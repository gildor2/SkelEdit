/////////////////////////////////////////////////////////////////////////////
// Name:        myframe2.cpp
// Purpose:     
// Author:      
// Modified by: 
// Created:     18/12/2007 17:36:10
// RCS-ID:      
// Copyright:   
// Licence:     
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

////@begin includes
////@end includes

#include "myframe2.h"

////@begin XPM images

////@end XPM images


/*!
 * MyFrame2 type definition
 */

IMPLEMENT_CLASS( MyFrame2, wxFrame )


/*!
 * MyFrame2 event table definition
 */

BEGIN_EVENT_TABLE( MyFrame2, wxFrame )

////@begin MyFrame2 event table entries
////@end MyFrame2 event table entries

END_EVENT_TABLE()


/*!
 * MyFrame2 constructors
 */

MyFrame2::MyFrame2()
{
    Init();
}

MyFrame2::MyFrame2( wxWindow* parent, wxWindowID id, const wxString& caption, const wxPoint& pos, const wxSize& size, long style )
{
    Init();
    Create( parent, id, caption, pos, size, style );
}


/*!
 * MyFrame2 creator
 */

bool MyFrame2::Create( wxWindow* parent, wxWindowID id, const wxString& caption, const wxPoint& pos, const wxSize& size, long style )
{
////@begin MyFrame2 creation
    SetParent(parent);
    CreateControls();
    Centre();
////@end MyFrame2 creation
    return true;
}


/*!
 * MyFrame2 destructor
 */

MyFrame2::~MyFrame2()
{
////@begin MyFrame2 destruction
    GetAuiManager().UnInit();
////@end MyFrame2 destruction
}


/*!
 * Member initialisation
 */

void MyFrame2::Init()
{
////@begin MyFrame2 member initialisation
////@end MyFrame2 member initialisation
}


/*!
 * Control creation for MyFrame2
 */

void MyFrame2::CreateControls()
{    
////@begin MyFrame2 content construction
    if (!wxXmlResource::Get()->LoadFrame(this, GetParent(), wxT("ID_MAINFRAME2")))
        wxLogError(wxT("Missing wxXmlResource::Get()->Load() in OnInit()?"));

    // Connect events and objects
    FindWindow(wxID_STATIC)->Connect(wxID_STATIC, wxEVT_LEFT_DOWN, wxMouseEventHandler(MyFrame2::OnLeftDown), NULL, this);
////@end MyFrame2 content construction

    // Create custom windows not generated automatically here.
////@begin MyFrame2 content initialisation
////@end MyFrame2 content initialisation
}


/*!
 * wxEVT_LEFT_DOWN event handler for wxID_STATIC
 */

void MyFrame2::OnLeftDown( wxMouseEvent& event )
{
////@begin wxEVT_LEFT_DOWN event handler for wxID_STATIC in MyFrame2.
    // Before editing this code, remove the block markers.
    event.Skip();
////@end wxEVT_LEFT_DOWN event handler for wxID_STATIC in MyFrame2. 
}


/*!
 * Should we show tooltips?
 */

bool MyFrame2::ShowToolTips()
{
    return true;
}

/*!
 * Get bitmap resources
 */

wxBitmap MyFrame2::GetBitmapResource( const wxString& name )
{
    // Bitmap retrieval
////@begin MyFrame2 bitmap retrieval
    wxUnusedVar(name);
    return wxNullBitmap;
////@end MyFrame2 bitmap retrieval
}

/*!
 * Get icon resources
 */

wxIcon MyFrame2::GetIconResource( const wxString& name )
{
    // Icon retrieval
////@begin MyFrame2 icon retrieval
    wxUnusedVar(name);
    return wxNullIcon;
////@end MyFrame2 icon retrieval
}
