// wxWidgets includes
#include "wx/wx.h"
#include "wx/xrc/xmlres.h"		// loading .xrc files
#include "wx/splitter.h"
#include "wx/glcanvas.h"

#include "GlViewport.h"


#define CLEAR_COLOR		0.2, 0.3, 0.2, 0


/*-----------------------------------------------------------------------------
	OpenGL canvas
-----------------------------------------------------------------------------*/

class GLCanvas : public wxGLCanvas
{
public:
	GLCanvas(wxWindow *parent)
	:	wxGLCanvas(parent, wxID_ANY, NULL, wxDefaultPosition, wxSize(300, 300), wxSUNKEN_BORDER)
	{
		mContext = new wxGLContext(this);	// destruction of current context is done in ~wxGLCanvas
		SetCurrent(*mContext);
	}

protected:
	void OnPaint(wxPaintEvent &event)
	{
		// The following line is required; strange bugs without it (messageboxes not works,
		// app cannot exit etc)
		// This is a dummy, to avoid an endless succession of paint messages.
		// OnPaint handlers must always create a wxPaintDC.
		wxPaintDC dc(this);
		Render();
	}

	void Render()
	{
		// prepare frame
		glClearColor(CLEAR_COLOR);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		GL::BuildMatrices();
		GL::Set3Dmode();

		// draw axis
		glDisable(GL_TEXTURE_2D);
		glBegin(GL_LINES);
		for (int i = 0; i < 3; i++)
		{
			CVec3 tmp = nullVec3;
			tmp[i] = 1;
			glColor3fv(tmp.v);
			tmp[i] = 70;
			glVertex3fv(tmp.v);
			glVertex3fv(nullVec3.v);
		}
		glEnd();
		glColor3f(1, 1, 1);

		GL::Set2Dmode();
		static int frame = 0;
		GL::textf("Frame: "S_GREEN"%d\n", frame++);	//!!

		// finish frame
		SwapBuffers();
	}

	void OnSize(wxSizeEvent &event)
	{
		wxGLCanvas::OnSize(event);
		GL::OnResize(event.GetSize().GetWidth(), event.GetSize().GetHeight());
	}

	void OnEraseBackground(wxEraseEvent &event)
	{
		// empty
	}

	void OnMouse(wxMouseEvent &event)
	{
		int dX = event.m_x - mMouseX;
		int dY = event.m_y - mMouseY;
		mMouseX = event.m_x;
		mMouseY = event.m_y;

		// update button states
		bool left  = event.LeftIsDown();
		bool right = event.RightIsDown();
		if (left != mMouseLeft)
		{
			mMouseLeft = left;
			GL::OnMouseButton(left, MOUSE_LEFT);
		}
		if (right != mMouseRight)
		{
			mMouseRight = right;
			GL::OnMouseButton(right, MOUSE_RIGHT);
		}
		// update movement
		GL::OnMouseMove(dX, dY);
		//?? if OnIdle() viewport update disabled, may call Refresh() here
		//?? when view origin changed
	}

	void OnIdle(wxIdleEvent &event)
	{
		//?? keep viewport constantly updating; check for other way?
		//?? update when animation launched or view rotated only
#if 0
		Refresh(false);			// render via messaging system
#else
		Render();				// directly render scene
		event.RequestMore();	// without this, only single OnIdle() will be generated
#endif
	}

private:
	wxGLContext		*mContext;
	long			mMouseX, mMouseY;
	bool			mMouseLeft, mMouseRight;

	DECLARE_EVENT_TABLE()
};


BEGIN_EVENT_TABLE(GLCanvas, wxGLCanvas)
	EVT_SIZE(GLCanvas::OnSize)
	EVT_PAINT(GLCanvas::OnPaint)
	EVT_ERASE_BACKGROUND(GLCanvas::OnEraseBackground)
	EVT_MOTION(GLCanvas::OnMouse)
	EVT_IDLE(GLCanvas::OnIdle)
END_EVENT_TABLE()


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
	GLCanvas			*mCanvas;

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
			wxLogFatalError("Incorrect frame resource");
		// replace right pane with GLCanvas control
		mCanvas = new GLCanvas(mMainSplitter);
		if (!mMainSplitter->ReplaceWindow(mRightFrame, mCanvas))
			wxLogFatalError("Cannot replace control right pane with GLCanvas");
		delete mRightFrame;
		mRightFrame = mCanvas;
		// init some controls
		mLeftSplitter->SetSashPosition(0);
	}

protected:
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
