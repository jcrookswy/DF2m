#pragma once
#include "portaudio.h"
#include <complex>
#include <thread>
#include <windows.h>
#include <ipp.h>

//modes
//0 = IDLE
//1 = RX USB
//2 = RX LSB
//3 = RX CW
//4 = TX USB
//5 = TX LSB
//6 = TX CW
#define IDLE_MODE 0
#define RX_MODE 1
#define TX_MODE 2
#define VNA_MODE 3

class MyFrame;

struct RadioStatus {
	float RXFreq;
	float LO1Freq; // Typ 144 MHz, 1.2 MHz multiples
	float LO2Freq; // Typ 500 kHz to 3 MHz, LO1Freq / ( Idiv + Fdiv / 64)
	float ErrorFreq; // Residual error, typ < 2 kHz
	int mode;		//0 for standby, 1 for receive?
	float RFFreqPlot[256];
};

class CRadio
{
public:
	CRadio();
	~CRadio() ;
	int Connect();
	bool connected;
	int UpdatePlot();
	int DataThread();
	void RXDataLoop();

	bool ProcessRawToIQ(char* data); // Change values into 2 arrays of float
	void FilterIQ(); // Change into complex float, correcting sampling phase offset

	void DoRXDSP(bool bypassALC); // Change 4, 6-bit values into a float
	int SetFreq(float freqMHz);

	std::thread myAThread;
	std::thread myDThread;

	int AudioInputChannels;
	int AudioOutputChannels;

	MyFrame* theFrame;
	RadioStatus* myStatus;
	HANDLE hSerial;

	Ipp32f* audioInBuf;
	Ipp32f* audioOutBuf;
	Ipp32f* resampledAudioOut;
	Ipp32f* resampledAudioIn;
	void Get1280AudioSamples(float gain);
	IppsResamplingPolyphase_32f* resample_state;

	float LOfreq;
	bool NewLOFreq;

	Ipp32f* CH1IFData;// = ippsMalloc_16s(8192);
	Ipp32f* CH2IFData;// = ippsMalloc_16s(8192);
	Ipp32fc* Ch1IQData;// = ippsMalloc_32fc(8192);
	Ipp32fc* Ch2IQData;// = ippsMalloc_32fc(8192);
	Ipp32fc* Ch1IQFiltered;// = ippsMalloc_32fc(1024);
	Ipp32fc* Ch2IQFiltered;// = ippsMalloc_32fc(1024);
	Ipp32fc* Ch1IQFiltered48k;// = ippsMalloc_32fc(512);
	Ipp32fc* Ch2IQFiltered48k;// = ippsMalloc_32fc(512);

	Ipp32f* Ch1IQPhase;// = ippsMalloc_32f(1024);
	Ipp32f* Ch2IQPhase;// = ippsMalloc_32f(1024);
	Ipp32f* Ch1IQFreq;// = ippsMalloc_32f(512);
	Ipp32f* Ch2IQFreq;// = ippsMalloc_32f(512);


	int IQDataWrAdr = 0;
	int IQDataRdAdr = 0;
	Ipp32fc* CH1FIRDL;// = ippsMalloc_32fc(tapslen);
	Ipp32fc* CH2FIRDL;// = ippsMalloc_32fc(tapslen);
	Ipp32fc* FIRTaps;// = ippsMalloc_32fc(tapslen);

	IppsFIRSpec_32fc* pFIRSpec;
	Ipp8u* pFIRBuf;

	Ipp32f* MagData ;
//	Ipp32f* MagMinAccumData;
//	Ipp32f* MagAccumData;
//	bool ClearMagAccum;
	Ipp32f* LogMagData;
//	int m_iFreq;

	Ipp32fc* RawIQData;// = new Ipp32fc[16000];
	Ipp32fc* TunerData1;// 
	Ipp32fc* TunerData2;// = new Ipp32fc[16000];
	Ipp32f TunerPhase;
	Ipp32f TunerMag;
	float TunerFreq;
	Ipp32f* HannWindow;// = new Ipp32f[250];
	Ipp32f* TXHannWindow;// = new Ipp32f[2048];
	Ipp32fc* WindowedData;// = new Ipp32fc[250];
	Ipp32fc* DFTData;// = new Ipp32fc[250];
	IppsDFTSpec_C_32fc* pDFTSpec;
	Ipp8u* pDFTWorkBuf;
	Ipp8u* pTXFFTWorkBuf;

	Ipp32fc* TXFFTData;
	Ipp32f* RaisedCosUpDown;// = new Ipp32f[2048];
	void BuildGainRamp(float* ramp, float GainPA, float GainAB, float GainBC);

	IppsFFTSpec_C_32fc* pFFTSpec;
	Ipp8u* pFFTSpecBuf, * pFFTWorkBuf;

	Ipp32fc* IFFTData;// = new Ipp32fc[16000];
	Ipp32fc* IFFTAccum;// = new Ipp32fc[16000];
	Ipp32f* RawAudio;


	int IQWriteAddr;
	int IQReadAddr;

	int audioInWrPtr;
	int audioInRdPtr;
	int audioOutWrPtr;
	int audioOutRdPtr;
	char dbgText[16];
	bool audioOutStarted;


};

