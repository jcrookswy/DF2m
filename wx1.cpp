// Start of wxWidgets "Hello World" Program
#include <wx/wx.h>
//#include "MyProjectBase.h"
#include "frame1.h"
#include "CRadio.h"
#include "WebSocketServer.h"

static WebSocketServer* g_wsServer = nullptr;

class MyApp : public wxApp
{
public:
    bool OnInit() override;
};

wxIMPLEMENT_APP(MyApp);

/*
class MyFrame : public wxFrame
{
public:
    MyFrame();

private:
    void OnHello(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
};

enum
{
    ID_Hello = 1
};
*/
bool MyApp::OnInit()
{
    MyFrame* frame = new MyFrame(NULL);
    frame->Show(true);
    return true;
}

//BasicDrawPane::BasicDrawPane(wxFrame* parent) :
//    wxPanel(parent)
//{
//}
void BasicDrawPane::paintEvent(wxPaintEvent& evt)
{
    wxPaintDC dc(this);
    render(dc);
}
void BasicDrawPane::paintNow()
{
    wxClientDC dc(this);
    render(dc);
}

void DrawBearing(wxDC& dc, float angle)
{
    int xofs = 27;
    int yofs = 17;
    int xsize = 906;
    int ysize = 102;
    wchar_t text[16];
    char cText[16];
    if (angle < -180.0) angle = 180.0;
    if (angle > 180.0) angle = 180.0;

    sprintf_s(cText, "%.1f deg", angle);

    mbstowcs(text, cText, 16);


    //Just a nice big horizontal bar
    dc.SetBrush(wxBrush(wxColor(48, 48, 48))); //
    dc.SetPen(wxPen(wxColor(96, 96, 96), 3)); // 5-pixels-thick border
    dc.DrawRectangle(xofs, yofs, xsize, ysize); // Draw outer perimeter
    dc.SetPen(wxPen(wxColor(32, 32, 32), 3)); // 3-pixels-thick border
    dc.DrawLine(xofs, yofs + ysize, xofs + xsize, yofs + ysize);
    dc.DrawLine(xofs + xsize, yofs, xofs + xsize, yofs + ysize);
    xofs += 3;
    yofs += 3;
    xsize -= 6;
    ysize -= 6;

    dc.DrawText(text, xofs + 400, yofs + 3);

    sprintf_s(cText, "-90");
    mbstowcs(text, cText, 16);
    dc.DrawText(text, xofs , yofs + 3);

    sprintf_s(cText, "+90");
    mbstowcs(text, cText, 16);
    dc.DrawText(text, xofs + xsize - 32, yofs + 3);

    yofs += 24;
    ysize -= 24;

    //Lines at 15 degree increments
    for (int i = 0; i <= 12; i++)
    {
        dc.SetPen(wxPen(wxColor(255, 255, 255), (i==6)? 2:1)); // 1-pixels-thick except 0 deg

        int xpos = xofs + i * xsize / 12;
        dc.DrawLine(xpos, yofs, xpos, ysize + yofs); // draw line across the rectangle
    }

    dc.SetPen(wxPen(wxColor(255, 255, 128), 8)); // big fat yellow pen

    //Draw angle with big fat yellow pen
    int xpos = (int)(xofs + xsize * (angle + 90.0) / (180.0));
    dc.DrawLine(xpos, yofs+4, xpos, ysize + yofs-4); // draw line across the rectangle
//    dc.DrawLine(xpos+2, yofs+4, xpos+2, ysize + yofs-4); // draw line across the rectangle


}

//Plot widget (float)
//inputs: array size, array pointer, x offset, y offset, x size, y size, y min, y max, horz grat count, vert grat count
void Plot(wxDC& dc, int xofs, int yofs, int xsize, int ysize, int numElements, float* data, float ymin, float ymax, int hgrat, int ygrat, wchar_t* text, int wfallPix = 0)
{
    // draw a rectangle
    dc.SetBrush(wxBrush(wxColor(48, 48, 48))); //
    dc.SetPen(wxPen(wxColor(96, 96, 96), 3)); // 5-pixels-thick border
    dc.DrawRectangle(xofs, yofs, xsize, ysize); // Draw outer perimeter
    dc.SetPen(wxPen(wxColor(32, 32, 32), 3)); // 3-pixels-thick border
    dc.DrawLine(xofs, yofs + ysize, xofs + xsize, yofs + ysize);
    dc.DrawLine(xofs + xsize, yofs, xofs + xsize, yofs + ysize);

    xofs += 3;
    yofs += 3;
    xsize -= 6;
    ysize -= 6;

    dc.DrawText(text, xofs + 10, yofs + 3);
    yofs += 24;
    ysize -= 24;

    //if (wfallPix > 0)
    //{
    //    dc.SetPen(wxPen(wxColor(32, 32, 32), 0));
    //    dc.SetBrush(wxBrush(wxColor(0, 32, 192))); //
    //    dc.DrawRectangle(xofs, yofs + ysize - wfallPix + 2, xsize, wfallPix - 4); // Draw outer perimeter
    //    ysize -= wfallPix; // Leave room for waterfall if specified
    //}


    //dc.SetBrush(*wxBLACK_BRUSH); // black filling

    dc.SetBrush(wxBrush(wxColor(64, 64, 64))); //
    dc.DrawRectangle(xofs + 2.5 * xsize / ygrat, yofs, 3 * xsize / ygrat, ysize); // To do: shade center 3 for filtered region

    dc.SetPen(wxPen(wxColor(255, 255, 255), 1)); // 1-pixels-thick outline
    //dc.DrawRectangle(xofs, yofs, xsize , ysize ); // Draw outer perimeter
    for (int i = 0; i <= hgrat; i++)
    {
        int ypos = yofs + i * ysize / hgrat;
        dc.DrawLine(xofs, ypos, xsize + xofs, ypos); // draw line across the rectangle
    }
    for (int i = 0; i <= ygrat; i++)
    {
        int xpos = xofs + i * xsize / ygrat;
        dc.DrawLine(xpos, yofs, xpos, ysize + yofs); // draw line across the rectangle
    }

    dc.SetPen(wxPen(wxColor(255, 255, 128), 2)); // yellow pen
    int xposLast = 0;
    int yposLast = 0;
    for (int i = 0; i < numElements; i++)
    {
        int xpos = (int)(xofs + 1.0 * i * xsize / (numElements-1));
        int ypos = (int)(yofs + (ymax - data[i] ) * ysize / (ymax - ymin));
        if (ypos < yofs) ypos = yofs;
        if (ypos > (yofs + ysize)) ypos = yofs + ysize;

        if(i > 0)
            dc.DrawLine(xposLast, yposLast, xpos, ypos); // draw line across the rectangle
        xposLast = xpos;
        yposLast = ypos;
    }

    // dc.SetPen(wxPen(wxColor(255, 0, 0), 1)); // 1-pixel-thick graticule

}

void Plot2(wxDC& dc, int xofs, int yofs, int xsize, int ysize, int numElements, float* data, float ymin, float ymax, int hgrat, int ygrat, wchar_t* text, float* data2)
{
    Plot(dc, xofs, yofs, xsize, ysize, numElements, data, ymin, ymax, hgrat, ygrat, text);

    xofs += 3; // Border and scale to match "plot"
    yofs += 27;
    xsize -= 6;
    ysize -= 30;
 
    dc.SetPen(wxPen(wxColor(160, 255, 160), 2)); // green pen

    int xposLast = 0;
    int yposLast = 0;
    for (int i = 0; i < numElements; i++)
    {
        int xpos = (int)(xofs + 1.0 * i * xsize / (numElements - 1));
        int ypos = (int)(yofs + (ymax - data2[i]) * ysize / (ymax - ymin));
        if (ypos < yofs) ypos = yofs;
        if (ypos > (yofs + ysize)) ypos = yofs + ysize;

        if (i > 0)
            dc.DrawLine(xposLast, yposLast, xpos, ypos); // draw line across the rectangle
        xposLast = xpos;
        yposLast = ypos;
    }

}

//Adds boxes of 24 point text in a row.
int PrettyText(wxDC& dc, int xofs, int yofs, int numChars, wchar_t* text)
{
    int xsize = numChars * 20 + 32;
    dc.SetBrush(wxBrush(wxColor(64, 64, 64))); //
    dc.SetPen(wxPen(wxColor(96, 96, 96), 3)); // 3-pixels-thick border
    dc.DrawRectangle(xofs, yofs, xsize, 60);
    dc.SetPen(wxPen(wxColor(32, 32, 32), 3)); // 3-pixels-thick border
    dc.DrawLine(xofs, yofs + 60, xofs + xsize, yofs + 60);
    dc.DrawLine(xofs + xsize, yofs, xofs + xsize, yofs + 60);
    dc.DrawText(text, xofs + 20, yofs + 15);
    return xsize;
}

void BasicDrawPane::render(wxDC& dc)
{
    // draw some text in boxes
    wxFont f = dc.GetFont();
    f.SetPointSize(24);
    f.MakeBold();
    f.SetFamily(wxFONTFAMILY_TELETYPE);
    dc.SetFont(f);
    dc.SetTextForeground(wxColor(255, 255, 0));

 	int xofs = 12;
    float data[10] = { 0.1, 0.4, 0.2, 0.3, 0.8, 0.8, 0.3, 0.5, 0.2, 0.1 };
    int numElements = 10;

    f.SetPointSize(12);
    dc.SetFont(f);
    dc.SetTextForeground(wxColor(255, 255, 0));


 /*   if (audioModified)
    {
        audioModified = false;
        wchar_t plot3Label[20] = _T("Audio (freq) 1kHz/");
        Plot(dc, 10, 72, 260, 228, 16, pRadio->myStatus->AudioFreqPlot, 0.0, 1.0, 4, 3, plot3Label);
 
        wchar_t plot4Label[20] = _T("Audio (time) 1ms/");
        Plot(dc, 288, 72, 260, 228, 128, pRadio->myStatus->AudioTimePlot, -1.0, 1.0, 4, 5, plot4Label);
     }*/

    DrawBearing(dc, pRadio->myStatus->angleOfArrival);

    //if (RFModified)
    //{
    //    RFModified = false;
    //    char rfText[64];
    //    sprintf_s(rfText, "RF Power vs Freq, %.3f - %.3f MHz, 5 kHz/, 10 dB/", pRadio->myStatus->TunerFreq -0.02, pRadio->myStatus->TunerFreq + 0.02);
    //    wchar_t plot7Label[64];// = _T("RF Power vs Freq, 14-14.35 MHz, 50 kHz/, 10 dB/");
    
    //    mbstowcs(plot7Label, rfText, 64);
    //sprintf_s(cText, "%.1f deg", angle);
    char plot7Text[64];
    sprintf_s(plot7Text, "RF Power vs Freq, %.3f MHz, 6 kHz/, 10 dB/", pRadio->myStatus->RXFreq);
    wchar_t plot7Label[64];
    mbstowcs(plot7Label, plot7Text, 64);
        Plot2(dc, 27, 140, 906, 256, 256, pRadio->myStatus->RFFreqPlot, -90.0, -10.0, 8, 8, plot7Label, pRadio->myStatus->RFFreqPlot2);
    //}
    //isPartial = false;
};


    // draw a line
   // dc.SetPen(wxPen(wxColor(0, 0, 0), 3)); // black line, 3 pixels thick
   // dc.DrawLine(300, 100, 700, 300); // draw line across the rectangle

    // Look at the wxDC docs to learn how to draw other stuff


///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6)
// http://www.wxformbuilder.org/
//
///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////

MyFrame::MyFrame(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) : wxFrame(parent, id, title, pos, size, style)
{

    this->SetSizeHints(1180, 480, 1180, 480);
    this->SetTitle(_("AOA DF 2m"));

    myRadio = new CRadio();//Do this after frame exists
    g_wsServer = new WebSocketServer(myRadio);
    g_wsServer->Start();
    myRadio->m_wsServer = g_wsServer;

    wxBoxSizer* bSizer1;
    bSizer1 = new wxBoxSizer(wxHORIZONTAL);

    wxBoxSizer* bSizer2;
    bSizer2 = new wxBoxSizer(wxVERTICAL);

    m_panel1 = new BasicDrawPane(this, myRadio, wxID_ANY, wxDefaultPosition, wxSize(960, 480), wxTAB_TRAVERSAL);
    m_panel1->SetBackgroundColour(wxColour(64, 64, 64));

    bSizer2->Add(m_panel1, 1, wxEXPAND | wxALL, 5);


    bSizer1->Add(bSizer2, 0, wxEXPAND, 5);

    wxFlexGridSizer* gSizer1;
    gSizer1 = new wxFlexGridSizer(10, 1, 5, 5);
    gSizer1->SetFlexibleDirection(wxBOTH);
    gSizer1->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);


    //wxGridSizer* gSizer1;
    //gSizer1 = new wxGridSizer(8, 1, 5, 5);

    m_button1 = new wxButton(this, wxID_ANY, _("  CONNECT  "), wxDefaultPosition, wxSize(180, 40), 0);
    wxFont bf = m_button1->GetFont();
    bf.SetPointSize(16);
    bf.MakeBold();
    m_button1->SetFont(bf);
    m_button1->SetBackgroundColour(wxColor(32, 32, 32));
    m_button1->SetForegroundColour(wxColor(255, 255, 128));
    gSizer1->Add(m_button1, 0, wxALL, 5);

    m_button2 = new wxButton(this, wxID_ANY, _("SYNC"), wxDefaultPosition, wxSize(180, 40), 0);
    m_button2->SetFont(bf);
    m_button2->SetBackgroundColour(wxColor(32, 32, 32));
    m_button2->SetForegroundColour(wxColor(255, 255, 128));
    gSizer1->Add(m_button2, 0, wxALL, 5);

    wxGridSizer* gSizer2;
    gSizer2 = new wxGridSizer(2, 2, 0, 0);
 

    m_textCtrl1 = new wxTextCtrl(this, wxID_ANY, _("146.565"), wxDefaultPosition, wxSize(105, 40), 0);
    //m_textCtrl1 = new wxTextCtrl(this, wxID_ANY, _("162.55"), wxDefaultPosition, wxSize(105, 40), 0);
    m_textCtrl1->SetFont(bf);
    gSizer2->Add(m_textCtrl1, 0, wxALL, 5);

    m_button6 = new wxButton(this, wxID_ANY, _("MHz"), wxDefaultPosition, wxSize(60, 40), 0);
    m_button6->SetFont(bf);
    m_button6->SetBackgroundColour(wxColor(32, 32, 32));
    m_button6->SetForegroundColour(wxColor(255, 255, 128));
    gSizer2->Add(m_button6, 0, wxALL, 5);

    m_button7 = new wxButton(this, wxID_ANY, _(" - "), wxDefaultPosition, wxSize(60, 40), 0);
    m_button7->SetFont(bf);
    m_button7->SetBackgroundColour(wxColor(32, 32, 32));
    m_button7->SetForegroundColour(wxColor(255, 255, 128));
    gSizer2->Add(m_button7, 0, wxALL, 5);

    m_button8 = new wxButton(this, wxID_ANY, _(" + "), wxDefaultPosition, wxSize(60, 40), 0);
    m_button8->SetFont(bf);
    m_button8->SetBackgroundColour(wxColor(32, 32, 32));
    m_button8->SetForegroundColour(wxColor(255, 255, 128));
    gSizer2->Add(m_button8, 0, wxALL, 5);



    gSizer1->Add(gSizer2, 0, wxALL, 5);

    wxFlexGridSizer* gSizer3;
    gSizer3 = new wxFlexGridSizer(1, 3, 0, 0);
    gSizer3->SetFlexibleDirection(wxBOTH);
    gSizer3->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);


     gSizer1->Add(gSizer3, 0, wxALL, 5);
     
    m_textDebug = new wxTextCtrl(this, wxID_ANY, _("MESSAGES"), wxDefaultPosition, wxSize(180, 40), 0);
    m_textDebug->SetFont(bf);
    gSizer1->Add(m_textDebug, 0, wxALL, 2);

    m_LO2HS = new wxCheckBox(this, wxID_ANY, _("2nd LO HS"), wxDefaultPosition, wxDefaultSize, 0);
    m_LO2HS->SetFont(bf);
    m_LO2HS->SetForegroundColour(wxColor(255, 255, 128));
    gSizer1->Add(m_LO2HS, 0, wxALL, 5);

    bSizer1->Add(gSizer1, 1, wxALIGN_TOP, 5);


    this->SetSizer(bSizer1);
    this->Layout();

    //this->Centre(wxBOTH);
    this->SetPosition(wxPoint(0, 0));

    // Connect Events


    m_LO2HS->Connect(wxEVT_CHECKBOX, wxCommandEventHandler(MyFrame::OnLO2HSChanged), NULL, this);
    m_button1->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::B1Click), NULL, this);
    m_button2->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::B2Click), NULL, this);
    m_button6->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::B6Click), NULL, this);
    m_timer.SetOwner(this, TIMER_ID);
    m_timer.Connect(wxEVT_TIMER, wxTimerEventHandler(MyFrame::OnTimer));
}

MyFrame::~MyFrame()
{
    m_timer.Stop();

    delete myRadio;
}
void MyFrame::UpdateDebugText(char* text)
{
    wchar_t label[16];// = _T("RF Power vs Freq, 14-14.35 MHz, 50 kHz/, 10 dB/");
    mbstowcs(label, text, 16);
    m_textDebug->SetLabelText(label);

}

void MyFrame::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);

    dc.DrawText(wxT("Testing"), 40, 60);

    // draw a circle
    dc.SetBrush(*wxGREEN_BRUSH); // green filling
    dc.SetPen(wxPen(wxColor(255, 0, 0), 5)); // 5-pixels-thick red outline
    dc.DrawCircle(wxPoint(200, 100), 25 /* radius */);

    // draw a rectangle
    dc.SetBrush(*wxBLUE_BRUSH); // blue filling
    dc.SetPen(wxPen(wxColor(255, 175, 175), 10)); // 10-pixels-thick pink outline
    dc.DrawRectangle(300, 100, 400, 200);

    // draw a line
    dc.SetPen(wxPen(wxColor(0, 0, 0), 3)); // black line, 3 pixels thick
    dc.DrawLine(300, 100, 700, 300); // draw line across the rectangle

 }

void MyFrame::OnTimer(wxTimerEvent& event)
{
 //   return;
    wchar_t label[16];// = _T("RF Power vs Freq, 14-14.35 MHz, 50 kHz/, 10 dB/");
    mbstowcs(label, myRadio->dbgText, 16);
    m_textDebug->SetLabelText(label);

    //myRadio->UpdatePlot();
    //m_panel1->isPartial = true;
    //m_panel1->audioModified = true;
    m_panel1->RFModified = true;
    m_panel1->Refresh(false);

}

void MyFrame::B1Click(wxCommandEvent& event) // CONNECT
{
    int retval = myRadio->Connect();

    if (retval)
        m_button1->SetLabelText(_(" CONNECTED "));

    m_panel1->Refresh(false);
    m_timer.Start(125);
}

void MyFrame::B2Click(wxCommandEvent& event) // CONNECT
{

}

void MyFrame::OnLO2HSChanged(wxCommandEvent& event)
{
    myRadio->m_2ndLOisHS = m_LO2HS->IsChecked();
}

void MyFrame::B6Click(wxCommandEvent& event) // MHz
{
    wxString value = m_textCtrl1->GetValue();
    double freq;
    value.ToDouble(&freq);
    myRadio->LOfreq = freq;
    myRadio->NewLOFreq = true;
}

