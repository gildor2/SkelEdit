// basic wxWidgets includes
#include <wx/wx.h>
#include <wx/xrc/xmlres.h>		// loading .xrc files
#include <wx/splitter.h>
#include <wx/notebook.h>
// wxGLCanvas
#include <wx/glcanvas.h>
// wxPropertyGrid
#include <wx/propgrid/propgrid.h>


#include "GlViewport.h"
#include "FileReaderStdio.h"


//?? move outside
//?? header: include "wx", "Core" ? (what about additional wxWidgets headers AFTER Core.h?)
void SetupPropGrid(wxPropertyGrid *Grid, CStruct *Type, void *Data);


#define CLEAR_COLOR		0.2, 0.3, 0.2, 0
#define TYPEINFO_FILE	"uc/typeinfo.bin"


/*!!---------------------------------------------------------------------------
	RTTI test
-----------------------------------------------------------------------------*/

#define DECLARE_BASE_CLASS(x)

struct FString
{
	int hack;
};


#include "uc/out.h"

/*struct FStruc
{
	CCoords		org;
	int			field1;
	float		field2;
	short		field3;
	bool		field4;
};*/

static UTest GEditStruc;

void InitTypeinfo2()
{
	guard(InitTypeinfo2);

	CFileReader Ar(TYPEINFO_FILE);
	InitTypeinfo(Ar);

	GEditStruc.Origin.Set(1, 2, 3);
	GEditStruc.FloatValue = 12345.666;
	GEditStruc.Orient.origin.Set(110, 112, 113);
	GEditStruc.Orient.axis[0].Set(1, 0, 0);
	GEditStruc.Orient.axis[1].Set(0, 1, 0);
	GEditStruc.Orient.axis[2].Set(0, 0, 1);
	GEditStruc.FinishField = 999777555;
/*	GEditStruc.field1 = 12345;
	GEditStruc.field2 = 222.12345;
	GEditStruc.field3 = 32767;
	GEditStruc.field4 = true;

	CStruct *Vec = new CStruct("Vec");	// == CVec3
		Vec->AddField("X", "float");
		Vec->AddField("Y", "float");
		Vec->AddField("Z", "float");
	Vec->Finalize();

	CStruct *Coord = new CStruct("Coord");	// == CCoords
		Coord->AddField("origin", "Vec");
		Coord->AddField("axis", "Vec", 3);
	Coord->Finalize();

	CStruct *Struc = new CStruct("FStruc");
		Struc->AddField("org",  "Coord");
		Struc->AddField("field1", "int");
		Struc->AddField("field2", "float");
		Struc->AddField("field3", "short");
		Struc->AddField("field4", "bool");
	Struc->Finalize(); */

	unguard;
}


void DumpStruc(FILE *f, CStruct *S, int indent = 0)
{
	if (indent > 32)
		indent = 32;
	char buf[256];
	memset(buf, ' ', 256);
	buf[indent+4] = 0;

	fprintf(f, "%s### %s (sizeof=%02X, align=%d):\n%s{\n", buf+4, S->TypeName, S->TypeSize, S->TypeAlign, buf+4);

	for (int i = 0; /* empty */ ; i++)
	{
		const CProperty *Prop = S->IterateProps(i);
		if (!Prop) break;
		fprintf(f, "%s[%2X] %-10s %s[%d]\n", buf, Prop->StructOffset, Prop->TypeInfo->TypeName, Prop->Name, Prop->ArrayDim);
		if (Prop->TypeInfo->IsStruc)
			DumpStruc(f, (CStruct*)Prop->TypeInfo, indent+4);
	}

	fprintf(f, "%s}\n", buf+4);
}


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
	wxPropertyGrid		*mMeshProps;

public:
	MyFrame(const wxString &title)
	{
		guard(MyFrame::MyFrame);

		// set window icon
		// SetIcon(wxIcon(....));
		wxXmlResource::Get()->LoadFrame(this, NULL, "ID_MAINFRAME");
		mFrameVisible[0] = mFrameVisible[1] = true;
		SetMinSize(wxSize(400, 300));
		// find main controls
		mTopFrame     = (wxWindow*)			FindWindow(XRCID("ID_TOPPANE"));
		mBottomFrame  = (wxWindow*)			FindWindow(XRCID("ID_BOTTOMPANE"));
		mRightFrame   = (wxWindow*)			FindWindow(XRCID("ID_RIGHTPANE"));
		mLeftSplitter = (wxSplitterWindow*) FindWindow(XRCID("ID_LEFTSPLITTER"));
		mMainSplitter = (wxSplitterWindow*) FindWindow(XRCID("ID_MAINSPLITTER"));
		wxNotebook *propBase = (wxNotebook*)FindWindow(XRCID("ID_PROPBASE"));
		if (!mTopFrame || !mBottomFrame || !mRightFrame || !mLeftSplitter || !mMainSplitter || !propBase)
			wxLogFatalError("Incorrect frame resource");
		// replace right pane with GLCanvas control
		mCanvas = new GLCanvas(mMainSplitter);
		if (!mMainSplitter->ReplaceWindow(mRightFrame, mCanvas))
			wxLogFatalError("Cannot replace control right pane with GLCanvas");
		delete mRightFrame;
		mRightFrame = mCanvas;

		// create property grid pages
		mMeshProps = new wxPropertyGrid(
			propBase,
			wxID_ANY,
			wxDefaultPosition,
			wxDefaultSize,
			/*wxPG_AUTO_SORT |*/ wxPG_SPLITTER_AUTO_CENTER | wxPG_BOLD_MODIFIED | wxPG_DEFAULT_STYLE
		);
		mMeshProps->SetExtraStyle(wxPG_EX_HELP_AS_TOOLTIPS);
		propBase->AddPage(mMeshProps, "Mesh");

		//!! remove
		// Add int property
		mMeshProps->Append( new wxIntProperty(wxT("IntProperty"), wxPG_LABEL, 12345678) );
		// Add float property (value type is actually double)
		mMeshProps->Append( new wxFloatProperty(wxT("FloatProperty"), wxPG_LABEL, 12345.678) );
		// Add a bool property
		mMeshProps->Append( new wxBoolProperty(wxT("BoolProperty"), wxPG_LABEL, false) );



		// init some controls
		mLeftSplitter->SetSashPosition(0);

		//!! TEST, REMOVE
		InitTypeinfo2();

		unguard;
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
		guard(OnHideTop);
		ShowFrame(0, event.IsChecked());
		unguard;
	}

	void OnHideBottom(wxCommandEvent &event)
	{
		ShowFrame(1, event.IsChecked());
	}

	void OnMenuTest(wxCommandEvent &event)
	{
		//!! TEST STUFF
		SetupPropGrid(mMeshProps, (CStruct*)FindType("Test"), &GEditStruc);
#if 0
		FILE *f = fopen("test.log", "a");
		fprintf(f, "\n\n---------\n\n");
		DumpStruc(f, (CStruct*)FindType("Test"));
		fclose(f);
#endif
	}

private:
	DECLARE_EVENT_TABLE()
};


BEGIN_EVENT_TABLE(MyFrame, wxFrame)
	EVT_MENU(wxID_EXIT,                  MyFrame::OnExit      )
	EVT_MENU(XRCID("ID_FULLSCREEN"),     MyFrame::OnFullscreen)
	EVT_MENU(XRCID("ID_HIDETOPPANE"),    MyFrame::OnHideTop   )
	EVT_MENU(XRCID("ID_HIDEBOTTOMPANE"), MyFrame::OnHideBottom)
	EVT_MENU(XRCID("ID_MENUTEST"),       MyFrame::OnMenuTest  )
END_EVENT_TABLE()


/*-----------------------------------------------------------------------------
	Application
-----------------------------------------------------------------------------*/

#if _WIN32
/*
 *	Visual C++ uses by default synchronous exception handling, which separates
 *	SEH and C++ exceptions and does not allows us to catch SEH with C++ try/catch
 *	construction. If we will hook __try/__except in a main loop function, then
 *	stack will be unwound at a moment of hook execution, and there will be no
 *	way to get call stack of exception. So, we using SetUnhandledExceptionFilter()
 *	to catch SEH and hope, that nobody will use __try/__except inside or reset
 *	filter function.
 */
static long WINAPI ExceptFilter(struct _EXCEPTION_POINTERS *info)
{
	appError("General Protection Fault!");
	return 0;
}
#endif


class MyApp : public wxApp
{
public:
	// `Main program' equivalent
	virtual bool OnInit()
	{
#if _WIN32
		SetUnhandledExceptionFilter(ExceptFilter);
#endif
		try
		{
			guard(MyApp::OnInit);

			// initialize wxWidgets resource system
			wxXmlResource::Get()->InitAllHandlers();
			if (!wxXmlResource::Get()->Load("xrc/main.xrc"))
				return false;
			// Create the main frame window
			MyFrame *frame = new MyFrame("");
			// create tooltip control with multiline handling (all subsequent calls
			// to change tooltip will use window, created by following line)
			frame->SetToolTip("\n");
			// Show the frame
			frame->Show(true);
			// return true to continue
			return true;

			unguard;
		}
		catch (...)
		{
			//?? same code in OnRun(), try to combine (difference: 'return false')
			if (GErrorHistory[0])
				appNotify("ERROR: %s\n", GErrorHistory);
			else
				appNotify("Unknown error\n");
			wxMessageBox(GErrorHistory, "Fatal error", wxOK | wxICON_ERROR);
			return false;
		}
	}

	// override wxApp::OnRun() to catch exceptions
	// NOTE!! GPF exceptions are not catched! should use /EHa compiler option
	//		  for both application and wxWidgets library + all controls
	//		  (TEST IT !)
	virtual int OnRun()
	{
		try
		{
			guard(MainLoop);
			return wxApp::OnRun();
			unguard;
		}
		catch (...)
		{
			if (GErrorHistory[0])
				appNotify("ERROR: %s\n", GErrorHistory);
			else
				appNotify("Unknown error\n");
			wxMessageBox(GErrorHistory, "Fatal error", wxOK | wxICON_ERROR);
			return 1;
		}
	}

	virtual bool OnExceptionInMainLoop()
	{
		throw;
	}
};

IMPLEMENT_APP(MyApp)
