/////////////////////////////////////////////////////////////////////////////
// Name:        myframe2.h
// Purpose:     
// Author:      
// Modified by: 
// Created:     18/12/2007 17:36:10
// RCS-ID:      
// Copyright:   
// Licence:     
/////////////////////////////////////////////////////////////////////////////

#ifndef _MYFRAME2_H_
#define _MYFRAME2_H_


/*!
 * Includes
 */

////@begin includes
#include "wx/xrc/xmlres.h"
#include "wx/aui/framemanager.h"
#include "wx/frame.h"
#include "wx/aui/auibook.h"
#include "wx/statusbr.h"
////@end includes

/*!
 * Forward declarations
 */

////@begin forward declarations
////@end forward declarations

/*!
 * Control identifiers
 */

////@begin control identifiers
#define ID_MAINFRAME2 10004
#define SYMBOL_MYFRAME2_STYLE wxDEFAULT_FRAME_STYLE
#define SYMBOL_MYFRAME2_TITLE _("Dialog")
#define SYMBOL_MYFRAME2_IDNAME ID_MAINFRAME2
#define SYMBOL_MYFRAME2_SIZE wxSize(500, 400)
#define SYMBOL_MYFRAME2_POSITION wxDefaultPosition
////@end control identifiers


/*!
 * MyFrame2 class declaration
 */

class MyFrame2: public wxFrame
{    
    DECLARE_CLASS( MyFrame2 )
    DECLARE_EVENT_TABLE()

public:
    /// Constructors
    MyFrame2();
    MyFrame2( wxWindow* parent, wxWindowID id = SYMBOL_MYFRAME2_IDNAME, const wxString& caption = SYMBOL_MYFRAME2_TITLE, const wxPoint& pos = SYMBOL_MYFRAME2_POSITION, const wxSize& size = SYMBOL_MYFRAME2_SIZE, long style = SYMBOL_MYFRAME2_STYLE );

    bool Create( wxWindow* parent, wxWindowID id = SYMBOL_MYFRAME2_IDNAME, const wxString& caption = SYMBOL_MYFRAME2_TITLE, const wxPoint& pos = SYMBOL_MYFRAME2_POSITION, const wxSize& size = SYMBOL_MYFRAME2_SIZE, long style = SYMBOL_MYFRAME2_STYLE );

    /// Destructor
    ~MyFrame2();

    /// Initialises member variables
    void Init();

    /// Creates the controls and sizers
    void CreateControls();

////@begin MyFrame2 event handler declarations
    /// wxEVT_LEFT_DOWN event handler for wxID_STATIC
    void OnLeftDown( wxMouseEvent& event );

////@end MyFrame2 event handler declarations

////@begin MyFrame2 member function declarations
    /// Returns the AUI manager object
    wxAuiManager& GetAuiManager() { return m_auiManager; }

    /// Retrieves bitmap resources
    wxBitmap GetBitmapResource( const wxString& name );

    /// Retrieves icon resources
    wxIcon GetIconResource( const wxString& name );
////@end MyFrame2 member function declarations

    /// Should we show tooltips?
    static bool ShowToolTips();

////@begin MyFrame2 member variables
    wxAuiManager m_auiManager;
////@end MyFrame2 member variables
};

#endif
    // _MYFRAME2_H_
