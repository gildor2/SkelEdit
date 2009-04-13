#include <wx/wx.h>
#include <wx/propgrid/propgrid.h>

#include "Core.h"
#include "OutputDeviceFile.h"

#include "PropEdit.h"

#include "EditorClasses.h"
#include "AppSettings.h"


/*-----------------------------------------------------------------------------
	Application settings
-----------------------------------------------------------------------------*/

CAppSettings GCfg;

void PutFramePos(wxTopLevelWindow *Frame, CWndPos &Pos)
{
	if (Frame->IsFullScreen())
	{
		// There is no way to get internal "old position" (see wxTopLevelWindow*::m_fsOldSize).
		// Better is to not change previously saved window position at all
		return;
	}
#if !_WIN32
	Frame->GetPosition(&Pos.x, &Pos.y);
	Frame->GetSize(&Pos.w, &Pos.h);
	Pos.maximized = Frame->IsMaximized();
#else
	// GetWindowPlacement() allows to request non-maximized position of window
	WINDOWPLACEMENT wpl;
	GetWindowPlacement((HWND)Frame->GetHandle(), &wpl);
	Pos.x = wpl.rcNormalPosition.left;
	Pos.y = wpl.rcNormalPosition.top;
	Pos.w = wpl.rcNormalPosition.right  - wpl.rcNormalPosition.left;
	Pos.h = wpl.rcNormalPosition.bottom - wpl.rcNormalPosition.top;
	Pos.maximized = (wpl.showCmd == SW_MAXIMIZE);
#endif
}

void SetFramePos(wxTopLevelWindow *Frame, CWndPos &Pos)
{
	if (Pos.w == 0) return;		// wrong (or previously absent) settings
	Frame->SetSize(Pos.x, Pos.y, Pos.w, Pos.h);
	if (Pos.maximized) Frame->Maximize();
}

void SaveSettings()
{
	guard(SaveSettings);
	COutputDeviceFile Out(CONFIG_FILE, true);
	FindStruct("AppSettings")->WriteText(&Out, &GCfg);
	unguard;
}

bool LoadSettings()
{
	guard(LoadSettings);
	char *file = (char*)LoadFile(CONFIG_FILE);
	if (!file)
		return false;
	FindStruct("AppSettings")->ReadText(file, &GCfg);
	delete file;
	GRootDir = GCfg.ResourceRoot;
	return true;
	unguard;
}


/*-----------------------------------------------------------------------------
	Settings editor
-----------------------------------------------------------------------------*/

class WSettingsEditor : public wxDialog
{
public:
	WPropEdit	*m_editor;

	WSettingsEditor(wxFrame *parent)
	:	wxDialog(parent, wxID_ANY, "Preferences", wxDefaultPosition, wxSize(300, 200),
				 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
	{
		guard(WSettingsEditor::WSettingsEditor);
		m_editor = new WPropEdit(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
			/*wxPG_BOLD_MODIFIED |*/ wxSUNKEN_BORDER | wxPG_DEFAULT_STYLE);
		m_editor->SetExtraStyle(wxPG_EX_HELP_AS_TOOLTIPS);
		m_editor->AttachObject(FindStruct("AppSettings"), &GCfg);
		// when window is not resized after creation, PropEdit will be created
		// in incorrect size (very small window); to fix it, we should resize
		// dialog
		SetSize(320, 200);
		// restore splitter position
		if (GCfg.PrefsPropSplitter)
			m_editor->SetSplitterPosition(GCfg.PrefsPropSplitter);
		unguard;
	}
	~WSettingsEditor()
	{
		delete m_editor;
	}
};


static WSettingsEditor *SettingsEditor;

void EditSettings(wxFrame *parent)
{
	// create dialog
	if (!SettingsEditor)
	{
		SettingsEditor = new WSettingsEditor(parent);
		SetFramePos(SettingsEditor, GCfg.SettingsFramePos);
	}
	// show it
	SettingsEditor->ShowModal();
	// save settings
	GRootDir = GCfg.ResourceRoot;
	PutFramePos(SettingsEditor, GCfg.SettingsFramePos);
	GCfg.PrefsPropSplitter = SettingsEditor->m_editor->GetSplitterPosition();
}
