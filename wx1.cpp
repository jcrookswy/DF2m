///////////////////////////////////////////////////////////////////////////////
// AOA DF 2m — Angle-of-Arrival Direction Finder for 2m Amateur Radio Band
// File:    wx1.cpp
// Author:  Justin Crooks
// Copyright (C) 2025  Justin Crooks
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
///////////////////////////////////////////////////////////////////////////////
#include <wx/wx.h>
//#include "MyProjectBase.h"
#include "frame1.h"
#include "CRadio.h"
#include "WebSocketServer.h"
#include <cmath>

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

void DrawBearing(wxDC& dc, float angle, float range)
{
    const int panelX = 27, panelY = 17, panelW = 906, panelH = 102;
    char  buf[32];
    wchar_t wbuf[32];

    if (angle < -range) angle = -range;
    if (angle > range)  angle =  range;

    // Panel background
    dc.SetBrush(wxBrush(wxColor(26, 26, 26)));
    dc.SetPen(wxPen(wxColor(90, 90, 90), 3));
    dc.DrawRectangle(panelX, panelY, panelW, panelH);

    const int margin     = 30;
    const int ix         = panelX + margin;
    const int iw         = panelW - 2 * margin;
    const int trackY     = panelY + 46;
    const int ballR      = 12;
    const int majorH     = 22;
    const int minorH     = 11;
    const int labelY     = trackY + majorH + 4;

    // Track rail with 3-D border effect
    dc.SetPen(wxPen(wxColor(50, 50, 50), 1));
    dc.DrawLine(ix, trackY - 2, ix + iw, trackY - 2);
    dc.SetPen(wxPen(wxColor(185, 185, 185), 2));
    dc.DrawLine(ix, trackY,     ix + iw, trackY);
    dc.SetPen(wxPen(wxColor(75, 75, 75), 1));
    dc.DrawLine(ix, trackY + 2, ix + iw, trackY + 2);

    // Tick marks and labels
    wxFont f = dc.GetFont();
    f.SetPointSize(9);
    dc.SetFont(f);

    const float minorStep  = (range <= 90.0f) ?  5.0f : 10.0f;
    const float majorStep  = (range <= 90.0f) ? 10.0f : 30.0f;
    const int   majorEvery = (int)roundf(majorStep / minorStep);
    const int   totalSteps = (int)roundf(2.0f * range / minorStep);

    for (int i = 0; i <= totalSteps; i++)
    {
        float deg    = -range + i * minorStep;
        int   xp     = ix + (int)(1.0f * i * iw / totalSteps);
        bool  major  = (i % majorEvery == 0);
        bool  isZero = (i == totalSteps / 2);

        wxColor tc = isZero   ? wxColor(255, 255, 100)
                   : major    ? wxColor(200, 200, 200)
                              : wxColor(110, 110, 110);
        dc.SetPen(wxPen(tc, (major || isZero) ? 2 : 1));
        dc.DrawLine(xp, trackY, xp, trackY + (major ? majorH : minorH));

        if (major)
        {
            float absDeg = fabsf(deg);
            if (absDeg < 0.5f)
                sprintf_s(buf, "0");
            else
                sprintf_s(buf, "%.0f", absDeg);
            mbstowcs(wbuf, buf, 32);
            dc.SetTextForeground(isZero ? wxColor(255, 255, 100) : wxColor(190, 190, 190));
            wxSize ts = dc.GetTextExtent(wbuf);
            dc.DrawText(wbuf, xp - ts.x / 2, labelY);
        }
    }

    // End-of-range labels in the margin strips
    {
        wxFont fEnd = dc.GetFont();
        fEnd.SetPointSize(8);
        dc.SetFont(fEnd);
        dc.SetTextForeground(wxColor(155, 155, 155));
        sprintf_s(buf, "%.0f", range);
        mbstowcs(wbuf, buf, 32);
        wxSize ts = dc.GetTextExtent(wbuf);
        int yc = trackY - ts.y / 2;
        dc.DrawText(wbuf, panelX + 3,                    yc);
        dc.DrawText(wbuf, panelX + panelW - margin + 3,  yc);
        dc.SetFont(f);
    }

    // Ball indicator — drawn last so it sits on top of ticks
    int xp = ix + (int)((angle + range) / (2.0f * range) * iw);

    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(wxColor( 55,  48,   0))); dc.DrawCircle(xp, trackY, ballR + 5);
    dc.SetBrush(wxBrush(wxColor(120, 108,   0))); dc.DrawCircle(xp, trackY, ballR + 3);
    dc.SetBrush(wxBrush(wxColor(200, 180,  20))); dc.DrawCircle(xp, trackY, ballR + 1);
    dc.SetBrush(wxBrush(wxColor(245, 228,  50)));
    dc.SetPen(wxPen(wxColor(255, 245, 100), 1));
    dc.DrawCircle(xp, trackY, ballR);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(wxColor(255, 255, 230)));
    dc.DrawCircle(xp - 3, trackY - 4, 4);

    // Angle readout text (top-left of panel)
    sprintf_s(buf, "%.1f deg", angle);
    mbstowcs(wbuf, buf, 32);
    wxFont fBig = dc.GetFont();
    fBig.SetPointSize(11);
    fBig.MakeBold();
    dc.SetFont(fBig);
    dc.SetTextForeground(wxColor(255, 255, 64));
    wxSize ts = dc.GetTextExtent(wbuf);
    dc.DrawText(wbuf, panelX + (panelW - ts.x) / 2, panelY + 5);
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

    //dc.SetBrush(wxBrush(wxColor(64, 64, 64))); //
    //dc.DrawRectangle(xofs + 1 * xsize / ygrat, yofs, 6 * xsize / ygrat, ysize); // To do: shade center 3 for filtered region

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

void DrawCompass(wxDC& dc, int xofs, int yofs, int xsize, int ysize, double compassBearing, float aoa)
{
    static const double PI = 3.14159265358979323846;

    // Outer border — matches the style of the other plot panels
    dc.SetBrush(wxBrush(wxColor(48, 48, 48)));
    dc.SetPen(wxPen(wxColor(96, 96, 96), 3));
    dc.DrawRectangle(xofs, yofs, xsize, ysize);
    dc.SetPen(wxPen(wxColor(32, 32, 32), 3));
    dc.DrawLine(xofs, yofs + ysize, xofs + xsize, yofs + ysize);
    dc.DrawLine(xofs + xsize, yofs, xofs + xsize, yofs + ysize);
    xofs += 3;
    yofs += 3;
    xsize -= 6;
    ysize -= 6;

    int cx = xofs + xsize / 2;
    int cy = yofs + ysize / 2;
    int radius = (xsize < ysize ? xsize : ysize) / 2 - 8;

    // Dark compass card background
    dc.SetBrush(wxBrush(wxColor(24, 24, 36)));
    dc.SetPen(wxPen(wxColor(100, 100, 140), 2));
    dc.DrawCircle(cx, cy, radius);

    // Tick marks every 10°, rotating with compassBearing
    for (int tick = 0; tick < 36; tick++)
    {
        double tickAngle = (tick * 10.0 - compassBearing) * PI / 180.0;
        bool isCardinal = (tick % 9 == 0);
        int tickLen = isCardinal ? 12 : 5;
        dc.SetPen(wxPen(wxColor(160, 160, 160), isCardinal ? 2 : 1));
        int x1 = cx + (int)(radius * sin(tickAngle));
        int y1 = cy - (int)(radius * cos(tickAngle));
        int x2 = cx + (int)((radius - tickLen) * sin(tickAngle));
        int y2 = cy - (int)((radius - tickLen) * cos(tickAngle));
        dc.DrawLine(x1, y1, x2, y2);
    }

    // N/E/S/W labels rotate with compassBearing so that the facing direction stays at the top
    wxFont labelFont = dc.GetFont();
    labelFont.SetPointSize(9);
    labelFont.MakeBold();
    dc.SetFont(labelFont);

    const wchar_t* cardinals[] = { L"N", L"E", L"S", L"W" };
    const double cardinalDeg[] = { 0.0, 90.0, 180.0, 270.0 };
    int labelRadius = radius - 22;

    for (int i = 0; i < 4; i++)
    {
        double angle = (cardinalDeg[i] - compassBearing) * PI / 180.0;
        int lx = cx + (int)(labelRadius * sin(angle));
        int ly = cy - (int)(labelRadius * cos(angle));
        wxSize ts = dc.GetTextExtent(cardinals[i]);
        dc.SetTextForeground(i == 0 ? wxColor(255, 80, 80) : wxColor(220, 220, 220));
        dc.DrawText(cardinals[i], lx - ts.x / 2, ly - ts.y / 2);
    }

    // Fixed bright north pointer — always points straight up on screen
    dc.SetPen(wxPen(wxColor(255, 255, 255), 3));
    dc.DrawLine(cx, cy - 6, cx, cy - radius + 16);
    wxPoint arrowPts[] = {
        wxPoint(cx,     cy - radius + 10),
        wxPoint(cx - 7, cy - radius + 24),
        wxPoint(cx + 7, cy - radius + 24),
    };
    dc.SetBrush(wxBrush(wxColor(255, 255, 255)));
    dc.SetPen(wxPen(wxColor(255, 255, 255), 1));
    dc.DrawPolygon(3, arrowPts);

    // AOA indicator — yellow needle, -90° to +90° from straight up
    float clampedAoA = aoa < -90.0f ? -90.0f : aoa > 90.0f ? 90.0f : aoa;
    double aoaRad = clampedAoA * PI / 180.0;
    int needleLen = radius - 18;
    int nx = cx + (int)(needleLen * sin(aoaRad));
    int ny = cy - (int)(needleLen * cos(aoaRad));
    dc.SetPen(wxPen(wxColor(255, 255, 64), 3));
    dc.DrawLine(cx, cy, nx, ny);

    // Center hub
    dc.SetBrush(wxBrush(wxColor(210, 210, 210)));
    dc.SetPen(wxPen(wxColor(210, 210, 210), 1));
    dc.DrawCircle(cx, cy, 4);
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

    if (pRadio->m_showAoA)
        DrawBearing(dc, pRadio->myStatus->angleOfArrival, 90.0f);
    else
        DrawBearing(dc, pRadio->myStatus->phaseDelta, 180.0f);

    //if (RFModified)
    //{
    //    RFModified = false;
    //    char rfText[64];
    //    sprintf_s(rfText, "RF Power vs Freq, %.3f - %.3f MHz, 5 kHz/, 10 dB/", pRadio->myStatus->TunerFreq -0.02, pRadio->myStatus->TunerFreq + 0.02);
    //    wchar_t plot7Label[64];// = _T("RF Power vs Freq, 14-14.35 MHz, 50 kHz/, 10 dB/");
    
    //    mbstowcs(plot7Label, rfText, 64);
    //sprintf_s(cText, "%.1f deg", angle);
    char plot7Text[64];
    sprintf_s(plot7Text, "RF Power vs Freq, %.3f MHz, 3 kHz/, 12 dB/", pRadio->myStatus->RXFreq);
    wchar_t plot7Label[64];
    mbstowcs(plot7Label, plot7Text, 64);
        Plot2(dc, 27, 140, 606, 256, 128, pRadio->myStatus->RFFreqPlot, -100.0, -4.0, 8, 8, plot7Label, pRadio->myStatus->RFFreqPlot2);

    DrawCompass(dc, 649, 140, 284, 256, pRadio->mCompassBearing, pRadio->myStatus->angleOfArrival);
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

    // Menu bar
    wxMenuBar* menuBar = new wxMenuBar();

    wxMenu* menuFile = new wxMenu();
    menuFile->Append(ID_FILE_LOAD_CONFIG, _("Load Config\tCtrl+O"));
    menuFile->Append(ID_FILE_SAVE_CONFIG, _("Save Config\tCtrl+S"));
    menuBar->Append(menuFile, _("File"));

    wxMenu* menuCompass = new wxMenu();
    menuCompass->Append(ID_COMPASS_XMIN, _("Set MinX"));
    menuCompass->Append(ID_COMPASS_XMAX, _("Set MaxX"));
    menuCompass->Append(ID_COMPASS_YMIN, _("Set MinY"));
    menuCompass->Append(ID_COMPASS_YMAX, _("Set MaxY"));
    menuCompass->Append(ID_COMPASS_ZMIN, _("Set MinZ"));
    menuCompass->Append(ID_COMPASS_ZMAX, _("Set MaxZ"));
    menuCompass->AppendSeparator();
    menuCompass->Append(ID_COMPASS_SET_POSITION, _("Set Position..."));
    menuBar->Append(menuCompass, _("Compass"));

    wxMenu* menuRFAlign = new wxMenu();
    menuRFAlign->Append(ID_RFALIGN_ZERO, _("Zero"));
    menuRFAlign->Append(ID_RFALIGN_FLIP, _("Flip"));
    menuBar->Append(menuRFAlign, _("RF Align"));

    SetMenuBar(menuBar);

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

    m_LO1HS = new wxCheckBox(this, wxID_ANY, _("1st LO HS"), wxDefaultPosition, wxDefaultSize, 0);
    m_LO1HS->SetFont(bf);
    m_LO1HS->SetForegroundColour(wxColor(255, 255, 128));
    gSizer1->Add(m_LO1HS, 0, wxALL, 5);

    m_LO2HS = new wxCheckBox(this, wxID_ANY, _("2nd LO HS"), wxDefaultPosition, wxDefaultSize, 0);
    m_LO2HS->SetFont(bf);
    m_LO2HS->SetForegroundColour(wxColor(255, 255, 128));
    gSizer1->Add(m_LO2HS, 0, wxALL, 5);

    m_RevPol = new wxCheckBox(this, wxID_ANY, _("show AoA"), wxDefaultPosition, wxDefaultSize, 0);
    m_RevPol->SetFont(bf);
    m_RevPol->SetForegroundColour(wxColor(255, 255, 128));
    gSizer1->Add(m_RevPol, 0, wxALL, 5);

    m_buttonSaveConfig = new wxButton(this, wxID_ANY, _("Save Config"), wxDefaultPosition, wxSize(180, 40), 0);
    m_buttonSaveConfig->SetFont(bf);
    m_buttonSaveConfig->SetBackgroundColour(wxColor(32, 32, 32));
    m_buttonSaveConfig->SetForegroundColour(wxColor(255, 255, 128));
    gSizer1->Add(m_buttonSaveConfig, 0, wxALL, 5);

    m_textDebug = new wxTextCtrl(this, wxID_ANY, _("MESSAGES"), wxDefaultPosition, wxSize(180, 40), 0);
    m_textDebug->SetFont(bf);
    gSizer1->Add(m_textDebug, 0, wxALL, 2);

    bSizer1->Add(gSizer1, 1, wxALIGN_TOP, 5);


    this->SetSizer(bSizer1);
    this->Layout();

    //this->Centre(wxBOTH);
    this->SetPosition(wxPoint(0, 0));

    // Connect Events


    m_LO1HS->Connect(wxEVT_CHECKBOX, wxCommandEventHandler(MyFrame::OnLO1HSChanged), NULL, this);
    m_LO2HS->Connect(wxEVT_CHECKBOX, wxCommandEventHandler(MyFrame::OnLO2HSChanged), NULL, this);
    m_RevPol->Connect(wxEVT_CHECKBOX, wxCommandEventHandler(MyFrame::OnRevPolChanged), NULL, this);
    m_button1->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::B1Click), NULL, this);
    m_button6->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::B6Click), NULL, this);
    m_button7->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::B7Click), NULL, this);
    m_button8->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::B8Click), NULL, this);
    m_buttonSaveConfig->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::BSaveConfigClick), NULL, this);
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
    static int CompassCounter = 0;
    if (CompassCounter > 2)
    {
        myRadio->mNewCompassRequest = true; // Update compass every second
        CompassCounter = 0;
    }
    else CompassCounter++;

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

void MyFrame::OnLO1HSChanged(wxCommandEvent& event)
{
    myRadio->m_1stLOisHS = m_LO1HS->IsChecked();
    myRadio->NewLOFreq = true;
}

void MyFrame::OnLO2HSChanged(wxCommandEvent& event)
{
    myRadio->m_2ndLOisHS = m_LO2HS->IsChecked();
    myRadio->NewLOFreq = true;
}

void MyFrame::OnRevPolChanged(wxCommandEvent& event)
{
    myRadio->m_showAoA = m_RevPol->IsChecked();
}

void MyFrame::B7Click(wxCommandEvent& event) // -
{
    myRadio->LOfreq -= 0.005f;
    myRadio->NewLOFreq = true;
    char buf[32];
    sprintf_s(buf, "%.3f", myRadio->LOfreq);
    m_textCtrl1->SetValue(wxString::FromAscii(buf));
}

void MyFrame::B8Click(wxCommandEvent& event) // +
{
    myRadio->LOfreq += 0.005f;
    myRadio->NewLOFreq = true;
    char buf[32];
    sprintf_s(buf, "%.3f", myRadio->LOfreq);
    m_textCtrl1->SetValue(wxString::FromAscii(buf));
}


void MyFrame::BSaveConfigClick(wxCommandEvent& event)
{
    myRadio->SaveConfig();
    m_textDebug->SetValue(_("Config saved"));
}

void MyFrame::OnMenuLoadConfig(wxCommandEvent& event)
{
    myRadio->LoadConfig();
    m_textDebug->SetValue(_("Config loaded"));
}

void MyFrame::OnMenuSaveConfig(wxCommandEvent& event)
{
    myRadio->SaveConfig();
    m_textDebug->SetValue(_("Config saved"));
}

void MyFrame::OnMenuCompassAxis(wxCommandEvent& event)
{
    int* target = nullptr;
    switch (event.GetId())
    {
    case ID_COMPASS_XMIN: target = myRadio->mCompassExtrema.XMin; break;
    case ID_COMPASS_XMAX: target = myRadio->mCompassExtrema.XMax; break;
    case ID_COMPASS_YMIN: target = myRadio->mCompassExtrema.YMin; break;
    case ID_COMPASS_YMAX: target = myRadio->mCompassExtrema.YMax; break;
    case ID_COMPASS_ZMIN: target = myRadio->mCompassExtrema.ZMin; break;
    case ID_COMPASS_ZMAX: target = myRadio->mCompassExtrema.ZMax; break;
    }
    if (!target) return;
    target[0] = myRadio->mMagField[0];
    target[1] = myRadio->mMagField[1];
    target[2] = myRadio->mMagField[2];
}

class PositionDialog : public wxDialog
{
public:
    PositionDialog(MyFrame* parent)
        : wxDialog(parent, wxID_ANY, _("Set Position"),
                   wxDefaultPosition, wxDefaultSize,
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
        , m_radio(parent->myRadio)
    {
        wxFlexGridSizer* grid = new wxFlexGridSizer(2, wxSize(8, 6));
        grid->AddGrowableCol(1);

        auto addRow = [&](const wxString& label, wxTextCtrl*& ctrl, float val) {
            grid->Add(new wxStaticText(this, wxID_ANY, label),
                      0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
            ctrl = new wxTextCtrl(this, wxID_ANY,
                                  wxString::Format("%.6g", val));
            grid->Add(ctrl, 1, wxEXPAND | wxALL, 4);
        };

        addRow(_("Latitude:"),  m_lat,   m_radio->mLatitude);
        addRow(_("Longitude:"), m_lon,   m_radio->mLongitude);
        addRow(_("Pitch:"),     m_pitch, m_radio->mPitch);
        addRow(_("Roll:"),      m_roll,  m_radio->mRoll);
        addRow(_("Yaw:"),              m_yaw,   m_radio->mYaw);
        addRow(_("(opt) Declination:"), m_decl,    m_radio->mDeclination);
        addRow(_("Antenna Spacing (m):"), m_spacing, m_radio->myStatus->mSpacing);

        wxSizer* btns = CreateButtonSizer(wxOK | wxCANCEL);

        wxBoxSizer* top = new wxBoxSizer(wxVERTICAL);
        top->Add(grid, 1, wxEXPAND | wxALL, 8);
        top->Add(btns, 0, wxEXPAND | wxBOTTOM | wxLEFT | wxRIGHT, 8);
        SetSizerAndFit(top);

        Bind(wxEVT_BUTTON, &PositionDialog::OnOK, this, wxID_OK);
    }

private:
    void OnOK(wxCommandEvent&)
    {
        double v;
        if (m_lat->GetValue().ToDouble(&v))   m_radio->mLatitude  = (float)v;
        if (m_lon->GetValue().ToDouble(&v))   m_radio->mLongitude = (float)v;
        if (m_pitch->GetValue().ToDouble(&v)) m_radio->mPitch     = (float)v;
        if (m_roll->GetValue().ToDouble(&v))  m_radio->mRoll      = (float)v;
        if (m_yaw->GetValue().ToDouble(&v))    m_radio->mYaw         = (float)v;
        if (m_decl->GetValue().ToDouble(&v))    m_radio->mDeclination = (float)v;
        if (m_spacing->GetValue().ToDouble(&v)) m_radio->myStatus->mSpacing = (float)v;
        Destroy();
    }

    CRadio*     m_radio;
    wxTextCtrl* m_lat;
    wxTextCtrl* m_lon;
    wxTextCtrl* m_pitch;
    wxTextCtrl* m_roll;
    wxTextCtrl* m_yaw;
    wxTextCtrl* m_decl;
    wxTextCtrl* m_spacing;
};

void MyFrame::OnMenuSetPosition(wxCommandEvent&)
{
    PositionDialog* dlg = new PositionDialog(this);
    dlg->Show();
}

void MyFrame::OnRFAlignZero(wxCommandEvent& event)
{
}

void MyFrame::OnRFAlignFlip(wxCommandEvent& event)
{
}

void MyFrame::B6Click(wxCommandEvent& event) // MHz
{
    wxString value = m_textCtrl1->GetValue();
    double freq;
    value.ToDouble(&freq);
    myRadio->LOfreq = freq;
    myRadio->NewLOFreq = true;
}

