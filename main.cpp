#include "wx/wx.h"
#include "wx/xrc/xmlres.h"		// loading .xrc files
#include "wx/splitter.h"


/*-----------------------------------------------------------------------------
	Main application frame
-----------------------------------------------------------------------------*/

// non-standard commands
enum
{
	ID_FULLSCREEN = wxID_HIGHEST + 1
};


class MyFrame : public wxFrame
{
private:
	bool				mFrameVisible[2];
	wxWindow			*mTopFrame, *mBottomFrame, *mRightFrame;
	wxSplitterWindow	*mLeftSplitter, *mMainSplitter;
	int					mMainSplitterPos;

public:
	MyFrame(const wxString &title)
	{
		// set window icon
		// SetIcon(wxIcon(....));
		wxXmlResource::Get()->LoadFrame(this, NULL, "ID_MAINFRAME");
		mFrameVisible[0] = mFrameVisible[1] = true;
		// find main controls
		mTopFrame     = (wxWindow*)			FindWindow(XRCID("ID_TOPPANE"));
		mBottomFrame  = (wxWindow*)			FindWindow(XRCID("ID_BOTTOMPANE"));
		mRightFrame   = (wxWindow*)			FindWindow(XRCID("ID_RIGHTPANE"));
		mLeftSplitter = (wxSplitterWindow*) FindWindow(XRCID("ID_LEFTSPLITTER"));
		mMainSplitter = (wxSplitterWindow*) FindWindow(XRCID("ID_MAINSPLITTER"));
		if (!mTopFrame || !mBottomFrame || !mRightFrame || !mLeftSplitter || !mMainSplitter)
		{
			wxLogError("Incorrect frame resource");
			exit(1);
		}
		// init some controls
		mLeftSplitter->SetSashPosition(0);
	}

	void OnExit(wxCommandEvent&)
	{
		Close(true);	//?? 'false'; should check for changes!
	}

	void OnFullscreen(wxCommandEvent &event)
	{
		ShowFullScreen(event.IsChecked(), wxFULLSCREEN_NOBORDER|wxFULLSCREEN_NOCAPTION);
	}

	void ShowFrame(int frameNum, bool value)
	{
		// buggy way to support frame hiding ...
		// too much operations pending, disable drawing on window
		Freeze();

		bool MainFrameWasVisible = mFrameVisible[0] || mFrameVisible[1];
		mFrameVisible[frameNum] = value;
		bool MainFrameVisible = mFrameVisible[0] || mFrameVisible[1];

		mTopFrame->Show(mFrameVisible[0]);
		mBottomFrame->Show(mFrameVisible[1]);

		if (!MainFrameVisible)
		{
			mMainSplitterPos = mMainSplitter->GetSashPosition();
			// both frames are hidden
			mMainSplitter->Unsplit(mLeftSplitter);
		}
		else if (MainFrameWasVisible && MainFrameVisible)
		{
			// left frame remains visible
			mLeftSplitter->Unsplit(mFrameVisible[0] ? mBottomFrame : mTopFrame);
			if (mFrameVisible[0] && mFrameVisible[1])
				mLeftSplitter->SplitHorizontally(mTopFrame, mBottomFrame);
			else if (mFrameVisible[0] && !mFrameVisible[1])
				mLeftSplitter->Initialize(mTopFrame);
			else
				mLeftSplitter->Initialize(mBottomFrame);
		}
		else
		{
			// left frame becomes visible with one pane
			mMainSplitter->SplitVertically(mLeftSplitter, mRightFrame, mMainSplitterPos);
			mLeftSplitter->Unsplit();
			mLeftSplitter->Initialize(mFrameVisible[0] ? mTopFrame : mBottomFrame);
			mLeftSplitter->UpdateSize();
			mMainSplitter->UpdateSize();
		}
		// draw the window
		Thaw();
	}

	void OnHideTop(wxCommandEvent &event)
	{
		ShowFrame(0, event.IsChecked());
	}

	void OnHideBottom(wxCommandEvent &event)
	{
		ShowFrame(1, event.IsChecked());
	}

private:
	DECLARE_EVENT_TABLE()
};


BEGIN_EVENT_TABLE(MyFrame, wxFrame)
	EVT_MENU(wxID_EXIT,                  MyFrame::OnExit      )
	EVT_MENU(XRCID("ID_FULLSCREEN"),     MyFrame::OnFullscreen)
	EVT_MENU(XRCID("ID_HIDETOPPANE"),    MyFrame::OnHideTop   )
	EVT_MENU(XRCID("ID_HIDEBOTTOMPANE"), MyFrame::OnHideBottom)
END_EVENT_TABLE()


/*-----------------------------------------------------------------------------
	Application
-----------------------------------------------------------------------------*/

class MyApp : public wxApp
{
public:
	// `Main program' equivalent
	virtual bool OnInit()
	{
		// initialize wxWidgets resource system
		wxXmlResource::Get()->InitAllHandlers();
		if (!wxXmlResource::Get()->Load("xrc/main.xrc"))
			return false;
		// Create the main frame window
		MyFrame *frame = new MyFrame("");
		// Show the frame
		frame->Show(true);
		// return true to continue
		return true;
	}
};

IMPLEMENT_APP(MyApp)
