#include "WebSocketServer.h"
#include "CRadio.h"
#include <thread>
#include <cmath>
#include <stddef.h>
#include <ipp.h>
#include <chrono>
#include <iostream>


void ProcessDataThread(int id, void* p) {
    CRadio* pRadio = (CRadio*)p;
    pRadio->DataThread();
    // Code to be executed in the new thread
}

typedef int PaStreamCallback(const void* input,
    void* output,
    unsigned long frameCount,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData);

//paTestData;
/* This routine will be called by the PortAudio engine when audio is needed.
 * It may called at interrupt level on some machines so don't do anything
 * that could mess up the system like calling malloc() or free().
*/
float DummyBuffer[64];
static int patestCallback(const void* inputBuffer, void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData)
{
    if (statusFlags)
        int j = statusFlags;
    // Cast data passed through stream to our structure. 
    CRadio* crp = (CRadio*)userData;
    float* out = (float*)outputBuffer;
    float* in = (float*)inputBuffer;
    unsigned int i;
 //   (void)inputBuffer; /* Prevent unused variable warning. */


    //for (int i = 0; i < 8; i++) memcpy(&out[i * 64], DummyBuffer, framesPerBuffer * 8);
    //return 0;

    float* obuf = &crp->audioOutBuf[crp->audioOutRdPtr];

    if  (crp->audioOutStarted)
    {
        for (int i = 0; i < framesPerBuffer; i++)
        {
            for (int q = 0; q < crp->AudioOutputChannels; q++)  *(out++) = *obuf;
            obuf++;
        }

        //memcpy(outputBuffer, &crp->audioOutBuf[crp->audioOutRdPtr], framesPerBuffer * 8);
        //crp->audioOutRdPtr += framesPerBuffer * 2;
        //if (crp->audioOutRdPtr >= 32768) crp->audioOutRdPtr = 0;
        crp->audioOutRdPtr += framesPerBuffer;
        if (crp->audioOutRdPtr >= 16384) crp->audioOutRdPtr = 0;
    }
    else
    {
        memset(outputBuffer, 0, framesPerBuffer * crp->AudioOutputChannels * 4);

        //for (int i = 0; i < 8; i++) memcpy(&out[i * 64], DummyBuffer, framesPerBuffer * 8);
        //memset(outputBuffer, 0, framesPerBuffer * 8);
    }
    if (crp->myStatus->mode == 1) // USB RX
    {
//        int IQSamples = crp->IQWriteAddr - crp->IQReadAddr;
//        if (crp->IQWriteAddr < crp->IQReadAddr) IQSamples += 16000;
        if ((!crp->audioOutStarted) && (crp->audioOutWrPtr > 4095)) crp->audioOutStarted = true;
    }
    //else // Not in RX mode
    //{
    //    crp->audioOutStarted = false;
    //    crp->audioOutWrPtr = 0;
    //    crp->audioOutRdPtr = 0;
    //}
    //if(crp->AudioInputChannels > 0)
    //    memcpy(&crp->audioInBuf[crp->audioInWrPtr], inputBuffer, framesPerBuffer * 4);
    crp->audioInWrPtr += framesPerBuffer;
    if (crp->audioInWrPtr >= 16384) crp->audioInWrPtr = 0;
    return 0;
}

#define SAMPLE_RATE (48000)
//#define SAMPLE_RATE (16000)
//static paTestData data;
PaStream* stream;
PaError err;
void InitStatus(RadioStatus* s)
{
    s->RXFreq = 146.56500;
    for (int i = 0; i < 256; i++) s->RFFreqPlot[i] = -20.0;
    for (int i = 0; i < 256; i++) s->RFFreqPlot2[i] = -20.0;
    s->antennaSpacing = 0.5f;
    s->angleOfArrival = 0.0f;
 
}

CRadio::CRadio()
{
    ippInit();                      // Initialize Intel� IPP library 
    m_1stLOisHS = false;
    m_2ndLOisHS = false;
    m_currentLO1 = 1440;

    audioInBuf = new Ipp32f[16384];
    audioOutBuf = new Ipp32f[16384];
    resampledAudioOut = new Ipp32f[8192];
    resampledAudioIn = new Ipp32f[1280];

    audioInWrPtr = 0;
    audioInRdPtr = 0;
    audioOutWrPtr = 0;
    audioOutRdPtr = 0;
    audioOutStarted = false;
 
    memset(audioInBuf, 0, 65536);
    memset(audioOutBuf, 0, 65536);
    connected = false;

    NewLOFreq = false;
    LOfreq = 146.565f;
 
    //Step 1: I/Q data comes in at 96 kHz
    CH1IFData = ippsMalloc_32f(64);
    CH2IFData = ippsMalloc_32f(64);
    IQDataWrAdr = 0;
    IQDataRdAdr = 0;

    int tapslen = 2048;      // Length of the filter taps
    Ch1IQData = ippsMalloc_32fc(1024);
    Ch2IQData = ippsMalloc_32fc(1024);
    Ch1IQFiltered = ippsMalloc_32fc(1024);
    Ch2IQFiltered = ippsMalloc_32fc(1024);
    Ch1IQFiltered48k = ippsMalloc_32fc(512);
    Ch2IQFiltered48k = ippsMalloc_32fc(512);
    SumIQFiltered = ippsMalloc_32fc(512);
    IQPhase = ippsMalloc_32f(512);
    IQFreq = ippsMalloc_32f(512);
    IQFreqFiltered = ippsMalloc_32f(512);

    CH1FIRDL = ippsMalloc_32fc(tapslen);
    CH2FIRDL = ippsMalloc_32fc(tapslen);
    FIRTaps = ippsMalloc_32fc(tapslen);

    //Build our FIR filter for +/- 12 kHz from center @96 kHz rate
    Ipp64f* ReFIRTaps = ippsMalloc_64f(tapslen);
    int sz = 0;
    ippsFIRGenGetBufferSize(tapslen, &sz);
    Ipp8u* pFWB = ippsMalloc_8u(sz);
    Ipp64f rFreq = 0.09375;     // Let's try 9 kHz * 2 = 18 kHz BW
    ippsFIRGenLowpass_64f(rFreq, ReFIRTaps, tapslen, ippWinBartlett, ippTrue, pFWB);
    ippsFree(pFWB);

    for (int i = 0; i < tapslen; i++)
    {
        FIRTaps[i].re = ReFIRTaps[i];
        FIRTaps[i].im = 0;
    }
    ippsFree(ReFIRTaps);

    // 2. Initialize input vector and delay line
    ippsZero_32fc(CH1FIRDL, tapslen);
    ippsZero_32fc(CH2FIRDL, tapslen);
    int SpecSize = 0;
    int BufSize = 0;
    ippsFIRSRGetSize(tapslen, ipp32fc, &SpecSize, &BufSize);
    pFIRSpec = (IppsFIRSpec_32fc*)ippsMalloc_8u(SpecSize);
    pFIRBuf = ippsMalloc_8u(BufSize);

    ippsFIRSRInit_32fc(FIRTaps, tapslen, ippAlgAuto, pFIRSpec);

    HannWindow = ippsMalloc_32f(512);
    WindowedData = ippsMalloc_32fc(512);
    FFTData = ippsMalloc_32fc(512);
    RawAudio = ippsMalloc_32f(512);

    MagData = ippsMalloc_32f(1024);
    LogMagData = ippsMalloc_32f(1024);

    TunerPhase = 0.0;
    TunerFreq = 19.0/128;
    TunerMag = 1.0;
    TunerData1 = new Ipp32fc[64];// 
    TunerData2 = new Ipp32fc[64];// 


    Ipp8u* pFFTInitBuf;

    // Query to get buffer sizes
    int sizeFFTSpec, sizeFFTInitBuf, sizeFFTWorkBuf;
    ippsFFTGetSize_C_32fc(9, IPP_FFT_DIV_FWD_BY_N,
        ippAlgHintAccurate, &sizeFFTSpec, &sizeFFTInitBuf, &sizeFFTWorkBuf);

    // Alloc FFT buffers
    pFFTSpecBuf = ippsMalloc_8u(sizeFFTSpec);
    pFFTInitBuf = ippsMalloc_8u(sizeFFTInitBuf);
    pFFTWorkBuf = ippsMalloc_8u(sizeFFTWorkBuf);

    // Initialize FFT
    ippsFFTInit_C_32fc(&pFFTSpec, 9, IPP_FFT_DIV_FWD_BY_N,
        ippAlgHintAccurate, pFFTSpecBuf, pFFTInitBuf);
    if (pFFTInitBuf) ippFree(pFFTInitBuf);
    // *** FFT is good to go ***

	//Build window
	for (int i = 0; i < 512; i++)
		HannWindow[i] = 0.5 - 0.5 * cos(IPP_2PI * i / 512);
 
    // Build 2048-tap bandpass FIR: 100 Hz to 3 kHz @ 48 kSPS
    AudioFIRTaps = ippsMalloc_32f(2048);
    AudioFIRDL   = ippsMalloc_32f(2048);
    ippsZero_32f(AudioFIRDL, 2048);
    {
        Ipp64f* pBPTaps64 = ippsMalloc_64f(2048);
        int bpSz = 0;
        ippsFIRGenGetBufferSize(2048, &bpSz);
        Ipp8u* pBPBuf = ippsMalloc_8u(bpSz);
        ippsFIRGenBandpass_64f(200.0 / 48000.0, 3000.0 / 48000.0,
            pBPTaps64, 2048, ippWinHamming, ippTrue, pBPBuf);
        ippsFree(pBPBuf);
        ippsConvert_64f32f(pBPTaps64, AudioFIRTaps, 2048);
        ippsFree(pBPTaps64);
    }
    int audioFIRSpecSize = 0, audioFIRBufSize = 0;
    ippsFIRSRGetSize(2048, ipp32f, &audioFIRSpecSize, &audioFIRBufSize);
    pAudioFIRSpec = (IppsFIRSpec_32f*)ippsMalloc_8u(audioFIRSpecSize);
    pAudioFIRBuf  = ippsMalloc_8u(audioFIRBufSize);
    ippsFIRSRInit_32f(AudioFIRTaps, 2048, ippAlgAuto, pAudioFIRSpec);

    // 4th-order Butterworth high-pass IIR for DC removal: 1 kHz cutoff @ 96 kSPS (2 biquad stages)
    {
        const int order = 2;          // 4th-order → 2 biquad sections
        const int numBiquads = order / 2;
        Ipp64f pTaps64[10];           // 5 coefficients per biquad [b0,b1,b2,a1,a2]
        int genBufSize = 0;
        ippsIIRGenGetBufferSize(order, &genBufSize);
        Ipp8u* pGenBuf = ippsMalloc_8u(genBufSize);
        ippsIIRGenHighpass_64f(4000.0 / 96000.0, 0.01, order, pTaps64, ippChebyshev1, pGenBuf);
        ippsFree(pGenBuf);
        // Keep taps in 64f — poles are near z=1, 32f rounding makes the filter unstable
        int dcStateSize = 0;
        ippsIIRGetStateSize_BiQuad_64f(numBiquads, &dcStateSize);
        pDCRemBufCH1 = ippsMalloc_8u(dcStateSize);
        pDCRemBufCH2 = ippsMalloc_8u(dcStateSize);
        ippsIIRInit_BiQuad_64f(&pDCRemCH1, pTaps64, numBiquads, nullptr, pDCRemBufCH1);
        ippsIIRInit_BiQuad_64f(&pDCRemCH2, pTaps64, numBiquads, nullptr, pDCRemBufCH2);
    }

	IQWriteAddr = 0; // We want I/Q data in chunks of 256
    IQReadAddr = 0;
    myStatus = new RadioStatus;
    InitStatus(myStatus);

}

CRadio::~CRadio()
{
    if (m_wsServer && m_wsServer->IsRunning())
        m_wsServer->Stop();

    if (connected)
    {
        myStatus->mode = IDLE_MODE;
        connected = false;
        myDThread.join();
        Sleep(16);
        Pa_StopStream(stream);
    }

    //ippsFIRFree_64f(pState);
    //ippsFree(pSrc);
    //ippsFree(pDst);
    //ippsFree(taps);
    //ippsFree(pDL);

    // ippsMalloc_* buffers
    ippsFree(CH1IFData);
    ippsFree(CH2IFData);
    ippsFree(Ch1IQData);
    ippsFree(Ch2IQData);
    ippsFree(Ch1IQFiltered);
    ippsFree(Ch2IQFiltered);
    ippsFree(Ch1IQFiltered48k);
    ippsFree(Ch2IQFiltered48k);
    ippsFree(SumIQFiltered);
    ippsFree(IQPhase);
    ippsFree(IQFreq);
    ippsFree(IQFreqFiltered);
    ippsFree(CH1FIRDL);
    ippsFree(CH2FIRDL);
    ippsFree(FIRTaps);
    ippsFree(pFIRSpec);
    ippsFree(pFIRBuf);
    ippsFree(HannWindow);
    ippsFree(WindowedData);
    ippsFree(FFTData);
    ippsFree(RawAudio);
    ippsFree(MagData);
    ippsFree(LogMagData);
    ippsFree(pFFTSpecBuf);
    ippsFree(pFFTWorkBuf);
    ippsFree(AudioFIRTaps);
    ippsFree(AudioFIRDL);
    ippsFree(pAudioFIRSpec);
    ippsFree(pAudioFIRBuf);
    ippsFree(pDCRemBufCH1);
    ippsFree(pDCRemBufCH2);

    // new[] allocations
    delete[] resampledAudioOut;
    delete[] resampledAudioIn;
    delete[] TunerData1;
    delete[] TunerData2;
    delete[] audioInBuf;
    delete[] audioOutBuf;
    delete myStatus;
}

bool CRadio::ProcessRawToIQ(char* data) // return true if FIR filter ran and new block available
{
	int16_t valCH1, valCH2;
	float DebugData[512];
	char DebugChar[256];
	memcpy(DebugChar, data, 256);

	for (int i = 0; i < 256; i++) data[i] -= 0x20; // Remove offset
	for (int i = 0; i < 64; i++) //Convert to float
	{
		valCH1 = (*(data++)) << 2;
		valCH1 |= (*(data++)) << 8;
		valCH2 = (*(data++)) << 2;
		valCH2 |= (*(data++)) << 8;
		CH1IFData[i] = (valCH1 - 8192) * 0.000128;
		CH2IFData[i] = (valCH2 - 8192) * 0.000128; // Zero pad missing samples - no simultaneous sampling
	}

    // Remove DC / long-term drift with the 1 kHz high-pass IIR (state preserved across calls)
    Ipp64f dcTemp[64];
    ippsConvert_32f64f(CH1IFData, dcTemp, 64);
    ippsIIR_64f_I(dcTemp, 64, pDCRemCH1);
    ippsConvert_64f32f(dcTemp, CH1IFData, 64);
    ippsConvert_32f64f(CH2IFData, dcTemp, 64);
    ippsIIR_64f_I(dcTemp, 64, pDCRemCH2);
    ippsConvert_64f32f(dcTemp, CH2IFData, 64);


    //Convert to IQ
    Ipp32f LastTunerPhase = TunerPhase;
    ippsTone_32fc(TunerData1, 64, 1.0, TunerFreq, &TunerPhase, ippAlgHintFast);
    LastTunerPhase += IPP_PI * TunerFreq * 0.5; // Channel 2 starts with 1/4 tuner freq offset from staggered sampling and downsampling. Also let's avoid accumulating error.
    ippsTone_32fc(TunerData2, 64, 1.0, TunerFreq, &LastTunerPhase, ippAlgHintFast);
    ippsMul_32f32fc(CH1IFData, TunerData1, &Ch1IQData[IQDataWrAdr], 64);
    ippsMul_32f32fc(CH2IFData, TunerData2, &Ch2IQData[IQDataWrAdr], 64);

    memcpy(DebugData, Ch1IQData, 64 * 4);
//    memcpy(DebugData, Ch2IQData, 64 * 4);


    IQDataWrAdr += 64;


//    memcpy(DebugData, Ch1IQData, 512 * 4);

    if (IQDataWrAdr >= 1024)
    {
        //Filter to 24 kHz max channel
        ippsFIRSR_32fc(Ch1IQData, Ch1IQFiltered, 1024, pFIRSpec, CH1FIRDL, CH1FIRDL, pFIRBuf);
        ippsFIRSR_32fc(Ch2IQData, Ch2IQFiltered, 1024, pFIRSpec, CH2FIRDL, CH2FIRDL, pFIRBuf);

        memcpy(DebugData, Ch1IQFiltered, 64 * 4);
        memcpy(DebugData, Ch2IQFiltered, 64 * 4);

        //Decimate to 48kSPS to match audio. There is probably an IPP function...
        for (int i = 0; i < 512; i++)
        {
            Ch1IQFiltered48k[i] = Ch1IQFiltered[i << 1];
            Ch2IQFiltered48k[i] = Ch2IQFiltered[i << 1];
        }
            //Calculate phase so we can FM demod
        ippsAdd_32fc(Ch1IQFiltered48k, Ch2IQFiltered48k, SumIQFiltered, 512);
        ippsPhase_32fc(SumIQFiltered, IQPhase, 512);
        //FIR filter data
        IQDataWrAdr = 0;

        // Compute average phase difference between Ch2 and Ch1 using complex conjugate product.
        // Accumulating Ch2*conj(Ch1) then taking atan2 avoids phase-wrap discontinuities.
        Ipp32fc phaseProd = { 0.0f, 0.0f };
        for (int i = 0; i < 512; i++)
        {
            phaseProd.re += Ch2IQFiltered48k[i].re * Ch1IQFiltered48k[i].re + Ch2IQFiltered48k[i].im * Ch1IQFiltered48k[i].im;
            phaseProd.im += Ch2IQFiltered48k[i].im * Ch1IQFiltered48k[i].re - Ch2IQFiltered48k[i].re * Ch1IQFiltered48k[i].im;
        }
        myStatus->phaseDelta = atan2f(phaseProd.im, phaseProd.re) * (180.0f / IPP_PI);
        if (m_2ndLOisHS ^ m_1stLOisHS) myStatus->phaseDelta *= -1.0;

        // Now compute angle of arrival from phase difference, based on antenna spacing
		float sinArg = myStatus->phaseDelta / (360.0f * myStatus->antennaSpacing);
		if (sinArg > 1.0f) sinArg = 1.0f;
		else if (sinArg < -1.0f) sinArg = -1.0f;
		myStatus->angleOfArrival = asinf(sinArg) * (180.0f / IPP_PI);

		ippsMul_32f32fc(HannWindow, Ch1IQData, Ch1IQData, 512);
        ippsFFTFwd_CToC_32fc(Ch1IQData, FFTData, pFFTSpec, pFFTWorkBuf);
        ippsMagnitude_32fc(FFTData, MagData, 512);
        for (int k = 0; k < 512; k++) if (MagData[k] == 0.0) MagData[k] = 0.000000001;
        ippsLog10_32f_A21(MagData, LogMagData, 512);
        ippsMulC_32f_I(20.0, LogMagData, 512);
        // High-side mixing inverts the spectrum; mirror both halves when active.
        if (m_2ndLOisHS ^ m_1stLOisHS) {
            ippsFlip_32f(&LogMagData[0],   &myStatus->RFFreqPlot[0],   128); // reversed +freqs → left
            ippsFlip_32f(&LogMagData[384], &myStatus->RFFreqPlot[128], 128); // reversed -freqs → right
        } else {
            memcpy(&myStatus->RFFreqPlot[0],   &LogMagData[384], 128 * sizeof(Ipp32f)); // last 128 (negative freqs)
            memcpy(&myStatus->RFFreqPlot[128], &LogMagData[0],   128 * sizeof(Ipp32f)); // first 128 (positive freqs)
        }

        ippsMul_32f32fc(HannWindow, Ch2IQData, WindowedData, 512);
        ippsFFTFwd_CToC_32fc(WindowedData, FFTData, pFFTSpec, pFFTWorkBuf);
        ippsMagnitude_32fc(FFTData, MagData, 512);
        for (int k = 0; k < 512; k++) if (MagData[k] == 0.0) MagData[k] = 0.000000001;
        ippsLog10_32f_A21(MagData, LogMagData, 512);
        ippsMulC_32f_I(20.0, LogMagData, 512);

        if (m_2ndLOisHS ^ m_1stLOisHS) {
            ippsFlip_32f(&LogMagData[0],   &myStatus->RFFreqPlot2[0],   128);
            ippsFlip_32f(&LogMagData[384], &myStatus->RFFreqPlot2[128], 128);
        } else {
            memcpy(&myStatus->RFFreqPlot2[0],   &LogMagData[384], 128 * sizeof(Ipp32f)); // last 128 (negative freqs)
            memcpy(&myStatus->RFFreqPlot2[128], &LogMagData[0],   128 * sizeof(Ipp32f)); // first 128 (positive freqs)
        }
      
        return true;

    }
 
    return false;

}

//int TimesThroughDebugGlobal = 0;

void CRadio::DoRXDSP(bool bypassALC) // 2000 bytes = 250 I/Q = 256 audio
{
    static Ipp32f LastPhase = 0;
    // 1st delta processed from last sample last time
    IQFreq[0] = (IQPhase[0] - LastPhase);
    LastPhase = IQPhase[511];
 
    for (int i = 1; i < 512; i++) IQFreq[i] = IQPhase[i] - IQPhase[i - 1]; // Hopefully compiler makes this faster
 
    for (int i = 0; i < 512; i++)
    {
        if (IQFreq[i] > IPP_PI) IQFreq[i] -= IPP_2PI;
        if (IQFreq[i] < -IPP_PI) IQFreq[i] += IPP_2PI;
        IQFreq[i] *= 0.25;  // Reduce volume a bit
    }

    ippsFIRSR_32f(IQFreq, RawAudio, 512, pAudioFIRSpec, AudioFIRDL, AudioFIRDL, pAudioFIRBuf);


    memcpy(&audioOutBuf[audioOutWrPtr], RawAudio, 512 * 4);
    audioOutWrPtr += 512;
    if (audioOutWrPtr > 16383) audioOutWrPtr = 0;

}

void CRadio::RXDataLoop()
{
    DWORD WriteData4[16];
    char writeData[64];
    char readData[256];
    DWORD bytesWritten = 0;
    DWORD bytesToWrite = 4;
    bool bypassALC = true;
    int ALCCounter = 0;
    writeData[0] = 'd'; // Let's make 'd' send 256 x 64 bytes ADC data, so this is 42 ms

    WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); // queue up 84 ms initially
    bool OKtoProcess = false;
    while (RX_MODE == myStatus->mode) // Continue until mode changes
    {
        WriteFile(hSerial, writeData, 1, &bytesWritten, NULL); //add'l 42 ms
        for (int k = 0; k < 64; k++)
        {
            //ReadFile(hSerial, readData, 4, &bytesWritten, NULL);
            //if(readData[0] != 's')
            //{
            //    ReadFile(hSerial, readData, 4, &bytesWritten, NULL);
            //    int z = 0;
            //}

            ReadFile(hSerial, readData, 256, &bytesWritten, NULL);
            if (bytesWritten < 256)
                    int h = 0;
                OKtoProcess = ProcessRawToIQ(readData);
            
            if (OKtoProcess) DoRXDSP(bypassALC); // every 250 I/Q
        }
        if (NewLOFreq)
        {
            SetFreq(LOfreq);
            myStatus->RXFreq = LOfreq;
            NewLOFreq = false;
        }
        if (ALCCounter < 80) ALCCounter++;
        else bypassALC = false;
    }
    //Final xfer
 /*   for (int k = 0; k < 64; k++)
    {
        ReadFile(hSerial, readData, 256, &bytesWritten, NULL);
        if (bytesWritten < 256)
            int h = 0;
    }*/
}

DWORD GetBytesAvailable(HANDLE hComm) {
    COMSTAT comStat;
    DWORD dwErrors;
    if (ClearCommError(hComm, &dwErrors, &comStat)) {
        return comStat.cbInQue; // Returns bytes in input buffer
    }
    return 0;
}

int CRadio::DataThread()
{
    DWORD WriteData4[16];
    char writeData[64];
    char readData[64];
    DWORD bytesWritten = 0;
    DWORD bytesToWrite = 4;
    FILE* f;
    int mode = 0;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    //fopen_s(&f, "debugData.csv", "w");
    while (connected)
    {
        if (myStatus->mode == IDLE_MODE)
        {
             std::this_thread::sleep_for(std::chrono::milliseconds(16));
		}
        else if (myStatus->mode == RX_MODE) // Receive
        {
            RXDataLoop();
        }

	}

	return 0;
}

int CRadio::SetFreq(float freqMHz)
{
    char writeData[8];
    DWORD bytesWritten = 0;
    int Idiv, Fdiv;
    //float FirstLO = 144.0f;
    float freqMHzx10d12 = (int)freqMHz * 10.0 / 12.0 ;
    freqMHzx10d12 = (m_1stLOisHS)? freqMHzx10d12 + 2.0f : freqMHzx10d12 - 2.0f;
 
    int FirstLO1M2Steps = (int)floor(freqMHzx10d12 + 0.5);
    float FirstLO = FirstLO1M2Steps * 1.2f;
    FirstLO1M2Steps *= 12;
    float SecondLOTarget = fabs(freqMHz - FirstLO);
    SecondLOTarget = (m_2ndLOisHS)? (SecondLOTarget - 0.01425) : (SecondLOTarget + 0.01425); // 14.25 kHz offset

    float ratio = FirstLO / SecondLOTarget;
    Idiv = (int)ratio;
    Fdiv = (int)roundf((ratio - Idiv) * 64.0f);
    if (Fdiv >= 64) { Idiv++; Fdiv = 0; }


//    Idiv += 1;
    LOfreq = freqMHz;
    writeData[0] = 'f';  
    writeData[1] = 0x20 + ((Idiv >> 6) & 0x3F);
    writeData[2] = 0x20 + (Idiv & 0x3F);
    writeData[3] = 0x20 + (Fdiv & 0x3F);
    WriteFile(hSerial, writeData, 4, &bytesWritten, NULL); // Set frequency
    Sleep(32);

    if (m_currentLO1 != FirstLO1M2Steps)
    {
        m_currentLO1 = FirstLO1M2Steps;
        writeData[0] = 'm';
        writeData[1] = 0x20 + ((FirstLO1M2Steps >> 6) & 0x3F);
        writeData[2] = 0x20 + (FirstLO1M2Steps & 0x3F);
        writeData[3] = 0x20 + (Fdiv & 0x3F);
        WriteFile(hSerial, writeData, 4, &bytesWritten, NULL); // Set frequency
        Sleep(32); 
    }

//    TunerFreq = (m_1stLOisHS) ? -19.0 / 128 : 19.0 / 128;
    return bytesWritten;
}

int CRadio::Connect()
{
    if (connected) return 0;
    for (int i = 0; i < 64; i++) DummyBuffer[i] = 0.0;// 0.5 * cos(IPP_2PI * i / 64.0);

    sprintf_s(dbgText, "Connecting");
    FILE* f;
    fopen_s(&f, "comport.txt", "r");
    int port = 3;
    fscanf_s(f, "%d", &port);
//    fscanf_s(f, "%d", &AudioInputChannels);
//    fscanf_s(f, "%f", &LOfreq);
    fclose(f);
    
    //debug
    DWORD WriteData4[16];
    char writeData[64];
    char readData[64];
    DWORD bytesWritten = 0;
    DWORD bytesToWrite = 4;

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        sprintf_s(dbgText, "PA Init Fail");
        return 0;
    }
    PaDeviceIndex defaultIn = Pa_GetDefaultInputDevice();
    AudioInputChannels = 0;
    if (defaultIn != paNoDevice) {
        const PaDeviceInfo* paiIn = Pa_GetDeviceInfo(defaultIn);
        if (paiIn && paiIn->maxInputChannels > 0)
            AudioInputChannels = 1;
    }
    PaDeviceIndex defaultOut = Pa_GetDefaultOutputDevice();
    const PaDeviceInfo* pai = (defaultOut != paNoDevice) ? Pa_GetDeviceInfo(defaultOut) : nullptr;
    AudioOutputChannels = (pai && pai->maxOutputChannels > 0) ? pai->maxOutputChannels : 2;
    char msg[64];

    connected = true;
    err = Pa_OpenDefaultStream(&stream,
        AudioInputChannels,          // 1 input channels 
        AudioOutputChannels,          // stereo output 
        paFloat32,  // 32 bit floating point output 
        SAMPLE_RATE,
        256,        /* frames per buffer, i.e. the number
                           of sample frames that PortAudio will
                           request from the callback. Many apps
                           may want to use
                           paFramesPerBufferUnspecified, which
                           tells PortAudio to pick the best,
                           possibly changing, buffer size.*/
        patestCallback, /* this is your callback function */
        this); /*This is a pointer that will be passed to
                           your callback*/
    if (err != paNoError) {
        sprintf_s(dbgText, "PA Open Fail");
        return 0;
    }


//    HANDLE hSerial;
    DCB dcbSerialParams = { 0 };
    COMMTIMEOUTS timeouts = { 0 };
    char data[256];
    DWORD bytesRead;

    // Open the serial port
    char cport[16];
    sprintf_s(cport, "\\\\.\\COM%d", port);

    hSerial = CreateFileA(cport, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (hSerial == INVALID_HANDLE_VALUE) {
        //perror("Error opening serial port");
        sprintf_s(dbgText, "NO DEVICE!");
        goto SkipComPortStuff;
    }

    // Set serial communication parameters
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        sprintf_s(dbgText, "COMSTATE ERR");
        CloseHandle(hSerial);
        goto SkipComPortStuff;
    }
    dcbSerialParams.BaudRate = 115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    dcbSerialParams.fRtsControl = 0;
    dcbSerialParams.fDtrControl = 1;
    
    if (!SetCommState(hSerial, &dcbSerialParams)) {
        sprintf_s(dbgText, "COMSTATE ERR");
        CloseHandle(hSerial);
        goto SkipComPortStuff;
    }

    PurgeComm(hSerial, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
    SetupComm(hSerial, 1048576, 1048576); // 1MB buffer

    // Set timeouts
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    if (!SetCommTimeouts(hSerial, &timeouts)) {
        sprintf_s(dbgText, "COMTMO ERR");
        CloseHandle(hSerial);
        goto SkipComPortStuff;
    }

    SetFreq(LOfreq); //DEBUG: SET FREQ FROM FILE
    myStatus->RXFreq = LOfreq;
    myStatus->phaseDelta = -1.0;
    Sleep(16);

    //writeData[0] = 'C';
    //writeData[1] = 0x20 + RelaySettings; 
    //writeData[2] = 0x20 + 0x3B; // FWD power
    //WriteFile(hSerial, writeData, 3, &bytesWritten, NULL); // queue up 8 * 250 IQ values
    //Sleep(16);

    PurgeComm(hSerial, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);


    myStatus->mode = 1;
    myDThread = std::thread(ProcessDataThread, 1, this); // Creates a thread executing myFunction with argument 1
    

SkipComPortStuff:
    err = Pa_StartStream(stream);
    //Pa_Sleep(1000);
    //err = Pa_StopStream(stream);
    if (err != paNoError) {
        sprintf_s(dbgText, "AUDIO ERR");
        return 0;
    }

    sprintf_s(dbgText, "Connect done");

//    DataThread();//debug
//    sprintf_s(dbgText, "stop data");

 	return 1;
}
