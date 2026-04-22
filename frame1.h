///////////////////////////////////////////////////////////////////////////////
// AOA DF 2m — Angle-of-Arrival Direction Finder for 2m Amateur Radio Band
// File:    frame1.h
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

#pragma once

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/intl.h>
#include <wx/panel.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/string.h>
#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/slider.h>
#include <wx/frame.h>

class CRadio;

///////////////////////////////////////////////////////////////////////////
class BasicDrawPane : public wxPanel
{

public:
    BasicDrawPane(wxWindow* parent, void *vpRadio,
        wxWindowID winid = wxID_ANY,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxTAB_TRAVERSAL | wxNO_BORDER,
        const wxString& name = wxASCII_STR(wxPanelNameStr))
    {
        Create(parent, winid, pos, size, style, name);
        pRadio = (CRadio *) vpRadio;
        textModified = true;
        audioModified = true;
        VSWRModified = true;
        RFModified = true;
        isPartial = false;
    }

    bool textModified;
    bool audioModified;
    bool RFModified;
    bool VSWRModified;
    bool isPartial;

    void paintEvent(wxPaintEvent& evt);
    void paintNow();

    void render(wxDC& dc);
    CRadio* pRadio;
    // some useful events
    /*
     void mouseMoved(wxMouseEvent& event);
     void mouseDown(wxMouseEvent& event);
     void mouseWheelMoved(wxMouseEvent& event);
     void mouseReleased(wxMouseEvent& event);
     void rightClick(wxMouseEvent& event);
     void mouseLeftWindow(wxMouseEvent& event);
     void keyPressed(wxKeyEvent& event);
     void keyReleased(wxKeyEvent& event);
     */
    DECLARE_EVENT_TABLE()

};

BEGIN_EVENT_TABLE(BasicDrawPane, wxPanel)
// some useful events
/*
 EVT_MOTION(BasicDrawPane::mouseMoved)
 EVT_LEFT_DOWN(BasicDrawPane::mouseDown)
 EVT_LEFT_UP(BasicDrawPane::mouseReleased)
 EVT_RIGHT_DOWN(BasicDrawPane::rightClick)
 EVT_LEAVE_WINDOW(BasicDrawPane::mouseLeftWindow)
 EVT_KEY_DOWN(BasicDrawPane::keyPressed)
 EVT_KEY_UP(BasicDrawPane::keyReleased)
 EVT_MOUSEWHEEL(BasicDrawPane::mouseWheelMoved)
 */

 // catch paint events
EVT_PAINT(BasicDrawPane::paintEvent)


END_EVENT_TABLE()

#define TIMER_ID 1

enum {
    ID_FILE_LOAD_CONFIG = wxID_HIGHEST + 1,
    ID_FILE_SAVE_CONFIG,
    ID_COMPASS_XMIN,
    ID_COMPASS_XMAX,
    ID_COMPASS_YMIN,
    ID_COMPASS_YMAX,
    ID_COMPASS_ZMIN,
    ID_COMPASS_ZMAX,
    ID_COMPASS_SET_POSITION,
};
///////////////////////////////////////////////////////////////////////////////
/// Class MyFrame1
///////////////////////////////////////////////////////////////////////////////
class MyFrame : public wxFrame
{
private:

protected:
    BasicDrawPane* m_panel1;
	wxButton* m_button1;
//	wxButton* m_button2;
//	wxButton* m_button3;
//	wxButton* m_button4;
//    wxButton* m_button5;
    wxButton* m_button6;
    wxButton* m_button7;
    wxButton* m_button8;
    wxButton* m_buttonSaveConfig;
//    wxButton* m_button9;
//    wxButton* m_button10;
    wxSlider* m_slider1;
    wxTextCtrl* m_textCtrl1;
    wxTextCtrl* m_textCtrl2;
    wxTextCtrl* m_textDebug;
    wxCheckBox* m_LO1HS;
    wxCheckBox* m_LO2HS;
    wxCheckBox* m_RevPol;


public:

	MyFrame(
        wxWindow* parent, 
        wxWindowID id = wxID_ANY, 
        const wxString& title = wxEmptyString, 
        const wxPoint& pos = wxDefaultPosition, 
        const wxSize& size = wxSize(1280, 800), 
        long style = wxDEFAULT_FRAME_STYLE | wxTAB_TRAVERSAL);

    wxTimer m_timer;

	~MyFrame();
    void B1Click(wxCommandEvent& event);
//    void B2Click(wxCommandEvent& event);
//    void B3Click(wxCommandEvent& event);
//    void B4Click(wxCommandEvent& event);
//    void B5Click(wxCommandEvent& event);
    void B6Click(wxCommandEvent& event);
    void B7Click(wxCommandEvent& event);
    void B8Click(wxCommandEvent& event);
    void BSaveConfigClick(wxCommandEvent& event);
    void OnMenuLoadConfig(wxCommandEvent& event);
    void OnMenuSaveConfig(wxCommandEvent& event);
    void OnMenuCompassAxis(wxCommandEvent& event);
    void OnMenuSetPosition(wxCommandEvent& event);
    void OnPaint(wxPaintEvent& event);
    void OnTimer(wxTimerEvent& event);
    void UpdateDebugText(char* text);
    void OnLO1HSChanged(wxCommandEvent& event);
    void OnLO2HSChanged(wxCommandEvent& event);
    void OnRevPolChanged(wxCommandEvent& event);

    CRadio* myRadio;
    wxDECLARE_EVENT_TABLE();

};
wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
EVT_TIMER(TIMER_ID, MyFrame::OnTimer)
EVT_MENU(ID_FILE_LOAD_CONFIG, MyFrame::OnMenuLoadConfig)
EVT_MENU(ID_FILE_SAVE_CONFIG, MyFrame::OnMenuSaveConfig)
EVT_MENU(ID_COMPASS_XMIN,     MyFrame::OnMenuCompassAxis)
EVT_MENU(ID_COMPASS_XMAX,     MyFrame::OnMenuCompassAxis)
EVT_MENU(ID_COMPASS_YMIN,     MyFrame::OnMenuCompassAxis)
EVT_MENU(ID_COMPASS_YMAX,     MyFrame::OnMenuCompassAxis)
EVT_MENU(ID_COMPASS_ZMIN,     MyFrame::OnMenuCompassAxis)
EVT_MENU(ID_COMPASS_ZMAX,         MyFrame::OnMenuCompassAxis)
EVT_MENU(ID_COMPASS_SET_POSITION, MyFrame::OnMenuSetPosition)
wxEND_EVENT_TABLE()


#pragma once
