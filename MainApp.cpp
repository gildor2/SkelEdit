// basic wxWidgets includes
#include <wx/wx.h>
#include <wx/xrc/xmlres.h>		// loading .xrc files
#include <wx/splitter.h>
#include <wx/notebook.h>
#include <wx/glcanvas.h>		// wxGLCanvas

// Third-party wxPropertyGrid
#include <wx/propgrid/propgrid.h>


#include "GlViewport.h"
#include "FileReaderStdio.h"

//!! CHECK
#include "uc/SkelTypes.h"
//!! ^^^^^
#include "Import.h"

#include "SkelMeshInstance.h"

#include "PropEdit.h"


#define CLEAR_COLOR		0.2, 0.3, 0.2, 0
#define TYPEINFO_FILE	"uc/typeinfo.bin"

//!!!!!!
static CSkeletalMesh		*EditorMesh;
static CAnimSet				*EditorAnim;
static CSkelMeshInstance	*MeshInst;


/*!!---------------------------------------------------------------------------
	RTTI test
-----------------------------------------------------------------------------*/

//#define TEST_STRUC 1
#if TEST_STRUC
static CTest GEditStruc;

void InitTypeinfo2()
{
	guard(InitTypeinfo2);

	GEditStruc.Origin.Set(1, 2, 3);
	GEditStruc.IntegerValueEd = 7654321;
	GEditStruc.FloatValue = 12345.666;
	GEditStruc.Orient.origin.Set(110, 112, 113);
	GEditStruc.Orient.axis[0].Set(1, 0, 0);
	GEditStruc.Orient.axis[1].Set(0, 1, 0);
	GEditStruc.Orient.axis[2].Set(0, 0, 1);
	GEditStruc.FinishField = 999777555;
	GEditStruc.EnumValue = SE_VALUE3;
	GEditStruc.Description = "Sample string";

	unguard;
}
#endif


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

//?? change
inline unsigned GetMilliseconds()
{
#if _WIN32
	return GetTickCount();
#else
#error Port this!
#endif
}


class GLCanvas : public wxGLCanvas
{
public:
	bool		mShowWireframe;
	bool		mShowNormals;
	bool		mShowSkeleton;
	unsigned	mLastFrameTime;
	bool		mUpdateLocked;		//??????

	GLCanvas(wxWindow *parent)
	:	wxGLCanvas(parent, wxID_ANY, NULL, wxDefaultPosition, wxSize(300, 300), wxSUNKEN_BORDER)
	,	mShowWireframe(false)
	,	mShowNormals(false)
	,	mShowSkeleton(false)
	,	mLastFrameTime(GetMilliseconds())
	{
		mContext = new wxGLContext(this);	// destruction of current context is done in ~wxGLCanvas
		SetCurrent(*mContext);
	}

	void Render()
	{
		guard(GLCanvas::Render);

		unsigned currTime = GetMilliseconds();
		float frameTime = (currTime - mLastFrameTime) / 1000.0f;
		mLastFrameTime = currTime;
		DrawTextRight("FPS: "S_GREEN"%5.1f", 1.0f / frameTime);

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

		//!! should change this !!
		if (MeshInst)
		{
			MeshInst->UpdateAnimation(frameTime);
			MeshInst->DrawMesh(mShowWireframe, mShowNormals);
			if (mShowSkeleton)
				MeshInst->DrawSkeleton();
		}

		GL::Set2Dmode();
		FlushTexts();

		// finish frame
//		glFinish();			// will significantly reduce FPS
		SwapBuffers();

		unguard;
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
		if (mUpdateLocked) return;	//??????
		//?? keep viewport constantly updating; check for other way?
		//?? update when animation launched or view rotated only
#if 0
		Refresh(false);				// render via messaging system
#else
		Render();					// directly render scene
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

#define SLIDER_TICKS_PER_FRAME		10

class MyFrame : public wxFrame
{
private:
	// GUI variables
	bool				mFrameVisible[2];
	wxWindow			*mTopFrame, *mBottomFrame, *mRightFrame;
	wxSplitterWindow	*mLeftSplitter, *mMainSplitter;
	int					mMainSplitterPos;
	GLCanvas			*mCanvas;
	WPropEdit			*mMeshPropEdit, *mAnimSetPropEdit, *mAnimPropEdit;
	wxListBox			*mAnimList;
	wxSlider			*mAnimPosition;
	wxStaticText		*mAnimLabel;
	// animation control variables
	unsigned			mLastFrameTime;
	const CMeshAnimSeq	*mCurrAnim;
	bool				mAnimLooped;
	bool				mAnimPlaying;
	bool				mShowRefpose;
	float				mAnimFrame;

	wxWindow* FindCtrl(const char *Name)
	{
		int id = wxXmlResource::GetXRCID(Name, -12345);
		if (id == -12345)
			appError("Resource error:\nIdent \"%s\" is not registered", Name);
		wxWindow *res = FindWindow(id);
		if (!res)
			appError("Resource error:\nUnable to find control for \"%s\" (id=%d)", Name, id);
		return res;
	}

public:
	/**
	 *	Constructor
	 */

	MyFrame(const wxString &title)
	:	mCurrAnim(NULL)
	,	mLastFrameTime(GetMilliseconds())
	{
		guard(MyFrame::MyFrame);

		// set window icon
		// SetIcon(wxIcon(....));
		wxXmlResource::Get()->LoadFrame(this, NULL, "ID_MAINFRAME");
		mFrameVisible[0] = mFrameVisible[1] = true;
		SetMinSize(wxSize(400, 300));
		// find main controls
		mTopFrame     = (wxWindow*)			FindCtrl("ID_TOPPANE");
		mBottomFrame  = (wxWindow*)			FindCtrl("ID_BOTTOMPANE");
		mRightFrame   = (wxWindow*)			FindCtrl("ID_RIGHTPANE");
		mLeftSplitter = (wxSplitterWindow*) FindCtrl("ID_LEFTSPLITTER");
		mMainSplitter = (wxSplitterWindow*) FindCtrl("ID_MAINSPLITTER");
		mAnimList     = (wxListBox*)        FindCtrl("ID_ANIMLIST");
		mAnimPosition = (wxSlider*)         FindCtrl("ID_ANIMPOS");
		mAnimLabel    = (wxStaticText*)     FindCtrl("ID_ANIMFRAME");
		wxNotebook *propBase    = (wxNotebook*)FindCtrl("ID_PROPBASE");
		wxWindow   *dummyCanvas = (wxWindow*)  FindCtrl("ID_CANVAS");
		// replace right pane with GLCanvas control
		mCanvas = new GLCanvas(mRightFrame);
		if (!mRightFrame->GetSizer()->Replace(dummyCanvas, mCanvas))
			wxLogFatalError("Cannot replace control right pane with GLCanvas");
		delete dummyCanvas;

		// create property grid pages
		mMeshPropEdit = new WPropEdit(propBase, wxID_ANY, wxDefaultPosition, wxDefaultSize,
			wxPG_SPLITTER_AUTO_CENTER | wxPG_BOLD_MODIFIED | wxSUNKEN_BORDER | wxPG_DEFAULT_STYLE);
		mMeshPropEdit->SetExtraStyle(wxPG_EX_HELP_AS_TOOLTIPS);
		propBase->AddPage(mMeshPropEdit, "Mesh");

		mAnimSetPropEdit = new WPropEdit(propBase, wxID_ANY, wxDefaultPosition, wxDefaultSize,
			wxPG_SPLITTER_AUTO_CENTER | wxPG_BOLD_MODIFIED | wxSUNKEN_BORDER | wxPG_DEFAULT_STYLE);
		mAnimSetPropEdit->SetExtraStyle(wxPG_EX_HELP_AS_TOOLTIPS);
		propBase->AddPage(mAnimSetPropEdit, "AnimSet");

		mAnimPropEdit = new WPropEdit(propBase, wxID_ANY, wxDefaultPosition, wxDefaultSize,
			wxPG_SPLITTER_AUTO_CENTER | wxPG_BOLD_MODIFIED | wxSUNKEN_BORDER | wxPG_DEFAULT_STYLE);
		mAnimPropEdit->SetExtraStyle(wxPG_EX_HELP_AS_TOOLTIPS);
		propBase->AddPage(mAnimPropEdit, "AnimSequence");

		// init some controls
		mMainSplitter->SetSashPosition(250);
		mLeftSplitter->SetSashPosition(0);

		// attach message handlers
		FindCtrl("ID_CLOSE_TOP")->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(MyFrame::OnHideTop2), NULL, this);
		FindCtrl("ID_CLOSE_BOTTOM")->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(MyFrame::OnHideBottom2), NULL, this);

#if TEST_STRUC
		//!! TEST, REMOVE
		InitTypeinfo2();
#endif

		StopAnimation();

		unguard;
	}

protected:
	/**
	 *	Animation management
	 */
	class WAnimInfo : public wxClientData
	{
	public:
		CMeshAnimSeq *Anim;			// not 'conts' - can be edited
		WAnimInfo(CMeshAnimSeq& A)
		:	Anim(&A)
		{}
	};

	void FillAnimList(wxListBox *List, CAnimSet *Anim)
	{
		guard(MyFrame::FillAnimList);
		assert(Anim);

		List->Clear();
		for (int i = 0; i < Anim->Sequences.Num(); i++)
		{
			CMeshAnimSeq &A = Anim->Sequences[i];
			List->Append(wxString::Format("%s [%d]", *A.Name, A.NumFrames), new WAnimInfo(A));
		}

		unguard;
	}

	void UseAnimSet(CAnimSet *Anim)
	{
		guard(MyFrame::UseAnimSet);
		FillAnimList(mAnimList, Anim);
		mCurrAnim = NULL;
		if (MeshInst)
		{
			MeshInst->SetAnim(Anim);
			MeshInst->PlayAnim("None");
		}
		unguard;
	}

	void OnAnimSelect(wxCommandEvent &event)
	{
		guard(OnAnimSelect);

		// get animation info from item data
		if (event.GetInt() == -1)
		{
			mCurrAnim = NULL;
			return;		//!!! detach property editor
		}
		CMeshAnimSeq *A = ((WAnimInfo*) event.GetClientObject())->Anim;
		assert(A);
		//!! attach property editor
		mAnimPropEdit->AttachObject((CStruct*)FindType("MeshAnimSeq"), A);
		// setup mesh animation
		StopAnimation();
		mCurrAnim = A;
		if (MeshInst)
		{
			// show tweening effect, and allow looping (to allow interpolation between
			// last and first frame for local animation controller)
			MeshInst->LoopAnim(A->Name, 0, 0.25);
			// setup slider
			mAnimPosition->SetRange(0, (A->NumFrames - 1) * SLIDER_TICKS_PER_FRAME);
		}

		unguard;
	}

	void OnPlayAnim(wxCommandEvent&)
	{
		mAnimFrame   = 0;
		mAnimPlaying = true;
	}

	void OnStopAnim(wxCommandEvent&)
	{
		if (MeshInst && mCurrAnim)
			mAnimPlaying = !mAnimPlaying;
	}

	void OnLoopToggle(wxCommandEvent &event)
	{
		mAnimLooped = event.IsChecked();
	}

	void OnSetAnimPos(wxScrollEvent&)
	{
		if (mAnimPlaying || !MeshInst || !mCurrAnim || !mCurrAnim->Rate)
			return;
		mAnimFrame = (float)mAnimPosition->GetValue() / SLIDER_TICKS_PER_FRAME;
	}

	/**
	 *	Very basic animation controller
	 */
	void UpdateAnimations(float timeDelta)
	{
		guard(MyFrame::UpdateAnimations);
		if (!MeshInst || !mCurrAnim || !mCurrAnim->Rate)
			return;

		if (mAnimPlaying && !MeshInst->IsTweening())
		{
			// advance frame
			mAnimFrame += timeDelta * mCurrAnim->Rate;
			// clamp/wrap frame
			if (!mAnimLooped)
			{
				if (mAnimFrame > mCurrAnim->NumFrames - 1)
				{
					mAnimFrame = mCurrAnim->NumFrames - 1;
					mAnimPlaying = false;
				}
			}
			else
			{
				if (mAnimFrame > mCurrAnim->NumFrames)	// 1 more frame for lerp between last and first frames
					mAnimFrame = 0;
			}
			// update slider
			mAnimPosition->SetValue(appFloor(mAnimFrame * SLIDER_TICKS_PER_FRAME));
		}
		// update animation
		const char *wantAnim = mShowRefpose ? "None" : mCurrAnim->Name;
		float dummyFrame, dummyNumFrames, dummyRate;
		const char *playingAnim;
		MeshInst->GetAnimParams(0, playingAnim, dummyFrame, dummyNumFrames, dummyRate);
		if (strcmp(playingAnim, wantAnim) != 0)
		{
			mAnimFrame = 0;
			MeshInst->LoopAnim(wantAnim, 0, 0.25);
		}
		if (!MeshInst->IsTweening() && !mShowRefpose)
			MeshInst->FreezeAnimAt(mAnimFrame);
		mAnimLabel->SetLabel(wxString::Format("Frame: %4.1f", mAnimFrame));

		unguard;
	}

	void StopAnimation()
	{
		mAnimPlaying = false;
		mAnimFrame   = 0;
	}

	/**
	 *	Event handlers
	 */
	void OnExit(wxCommandEvent&)
	{
		Close(true);	//?? 'false'; should check for changes!
	}

	void OnFullscreen(wxCommandEvent &event)
	{
		ShowFullScreen(event.IsChecked(), wxFULLSCREEN_NOBORDER|wxFULLSCREEN_NOCAPTION);
	}

	/**
	 *	Hiding and showing frames
	 */
	void ShowFrame(int frameNum, bool value)
	{
		guard(MyFrame::ShowFrame);

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

		unguard;
	}

	void OnHideTop(wxCommandEvent &event)
	{
		guard(OnHideTop);
		ShowFrame(0, event.IsChecked());
		unguard;
	}

	void OnHideTop2(wxMouseEvent&)
	{
		wxMenuItem *Item = GetMenuBar()->FindItem(XRCID("ID_HIDETOPPANE"));
		assert(Item);
		Item->Check(false);
		ShowFrame(0, false);
	}

	void OnHideBottom(wxCommandEvent &event)
	{
		ShowFrame(1, event.IsChecked());
	}

	void OnHideBottom2(wxMouseEvent&)
	{
		wxMenuItem *Item = GetMenuBar()->FindItem(XRCID("ID_HIDEBOTTOMPANE"));
		assert(Item);
		Item->Check(false);
		ShowFrame(1, false);
	}

	/*!!
	 *	TEST STUFF
	 */
	void OnMenuTest(wxCommandEvent&)
	{
#if TEST_STRUC
		mMeshPropEdit->AttachObject(&GEditStruc);
		FILE *f = fopen("test.log", "a");
		fprintf(f, "\n\n---------\n\n");
		DumpStruc(f, (CStruct*)FindType("Test"));
		fclose(f);
#endif
	}

	/**
	 *	Import mesh from foreign file
	 */
	void OnImportMesh(wxCommandEvent&)
	{
		guard(MyFrame::OnImportMesh);

		StopAnimation();

		wxFileDialog dlg(this, "Import mesh from file ...", "", "",
			"ActorX mesh files (*.psk)|*.psk",
			wxFD_OPEN | wxFD_FILE_MUST_EXIST);
		if (dlg.ShowModal() == wxID_OK)
		{
			const char *filename = dlg.GetPath().c_str();
			appSetNotifyHeader("Importing mesh from %s", filename);
			CFileReader Ar(filename);	// note: will throw appError when failed

			if (EditorMesh) delete EditorMesh;
			EditorMesh = new CSkeletalMesh;
			ImportPsk(Ar, *EditorMesh);

			if (MeshInst)
				delete MeshInst;
			MeshInst = new CSkelMeshInstance;
			MeshInst->SetMesh(EditorMesh);
			if (EditorAnim)
				MeshInst->SetAnim(EditorAnim);

			mMeshPropEdit->AttachObject(EditorMesh);

			appSetNotifyHeader("");
		}

		unguard;
	}

	/**
	 *	Import animations from foreign file
	 */
	void OnImportAnim(wxCommandEvent&)
	{
		guard(MyFrame::OnImportAnim);

		StopAnimation();

		wxFileDialog dlg(this, "Import animations from file ...", "", "",
			"ActorX animation files (*.psa)|*.psa",
			wxFD_OPEN | wxFD_FILE_MUST_EXIST);
		if (dlg.ShowModal() == wxID_OK)
		{
			const char *filename = dlg.GetPath().c_str();
			appSetNotifyHeader("Importing animations from %s", filename);
			CFileReader Ar(filename);	// note: will throw appError when failed

			if (EditorAnim) delete EditorAnim;
			EditorAnim = new CAnimSet;
			ImportPsa(Ar, *EditorAnim);

			mAnimSetPropEdit->AttachObject(EditorAnim);

			appSetNotifyHeader("");
			UseAnimSet(EditorAnim);
		}

		unguard;
	}

	/**
	 *	Support for toggle some bool variable using menu and toolbar
	 */
	void SyncToggles(const char *name, bool value)
	{
		char buf[256];
		appSprintf(ARRAY_ARG(buf), "ID_%s", name);
		int id = XRCID(buf);
		GetMenuBar()->FindItem(id)->Check(value);
		GetToolBar()->ToggleTool(id, value);
	}

#define HANDLE_TOGGLE(name, var)	\
	void On_##name(wxCommandEvent&)	\
	{								\
		var = !var;					\
		SyncToggles(#name, var);	\
	}

#define HOOK_TOGGLE(name)			\
	EVT_MENU(XRCID("ID_"#name), MyFrame::On_##name)

	// togglers ...
	HANDLE_TOGGLE(WIREFRAME, mCanvas->mShowWireframe)
	HANDLE_TOGGLE(NORMALS,   mCanvas->mShowNormals  )
	HANDLE_TOGGLE(SKELETON,  mCanvas->mShowSkeleton )
	HANDLE_TOGGLE(REFPOSE,   mShowRefpose           )

	/**
	 *	Idle handler
	 */
	void OnIdle(wxIdleEvent &event)
	{
		guard(MyFrame::OnIdle);

		// update local animation state
		unsigned currTime = GetMilliseconds();
		float frameTime = (currTime - mLastFrameTime) / 1000.0f;
		mLastFrameTime = currTime;
		UpdateAnimations(frameTime);
		if (!mAnimPlaying)
			wxMilliSleep(10);
		event.RequestMore();	// without this, only single OnIdle() will be generated

		unguard;
	}

private:
	DECLARE_EVENT_TABLE()
};


BEGIN_EVENT_TABLE(MyFrame, wxFrame)
	EVT_IDLE(MyFrame::OnIdle)
	EVT_MENU(wxID_EXIT,                  MyFrame::OnExit      )
	EVT_MENU(XRCID("ID_FULLSCREEN"),     MyFrame::OnFullscreen)
	EVT_MENU(XRCID("ID_HIDETOPPANE"),    MyFrame::OnHideTop   )
	EVT_MENU(XRCID("ID_HIDEBOTTOMPANE"), MyFrame::OnHideBottom)
	EVT_MENU(XRCID("ID_MENUTEST"),       MyFrame::OnMenuTest  )	//???
	EVT_MENU(XRCID("ID_MESHIMPORT"),     MyFrame::OnImportMesh)
	EVT_MENU(XRCID("ID_ANIMIMPORT"),     MyFrame::OnImportAnim)
	HOOK_TOGGLE(WIREFRAME)
	HOOK_TOGGLE(NORMALS  )
	HOOK_TOGGLE(SKELETON )
	HOOK_TOGGLE(REFPOSE  )
	EVT_LISTBOX(XRCID("ID_ANIMLIST"),    MyFrame::OnAnimSelect)
	EVT_MENU(XRCID("ID_PLAYANIM"),       MyFrame::OnPlayAnim  )
	EVT_MENU(XRCID("ID_STOPANIM"),       MyFrame::OnStopAnim  )
	EVT_MENU(XRCID("ID_LOOPANIM"),       MyFrame::OnLoopToggle)
	EVT_COMMAND_SCROLL(XRCID("ID_ANIMPOS"), MyFrame::OnSetAnimPos)
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
	static void DisplayError()
	{
		if (GErrorHistory[0])
			appNotify("ERROR: %s\n", GErrorHistory);
		else
			appNotify("Unknown error\n");
		wxMessageBox(GErrorHistory, "Fatal error", wxOK | wxICON_ERROR);
	}

	/**
	 *	Initialize application. Return 'false' when failed.
	 */
	virtual bool OnInit()
	{
#if _WIN32
		SetUnhandledExceptionFilter(ExceptFilter);
#endif
		try
		{
			guard(MyApp::OnInit);

			CFileReader Ar(TYPEINFO_FILE);
			InitTypeinfo(Ar);
			Ar.Close();

			wxInitAllImageHandlers();
			// initialize wxWidgets resource system
			wxXmlResource::Get()->InitAllHandlers();
			if (!wxXmlResource::Get()->Load("xrc/main.xrc"))
				return false;
			// Create the main frame window
			MyFrame *frame = new MyFrame("");
			// Show the frame
			frame->Show(true);

			// enable if there will be bugs with displaying multiline hints:
#if 0
			// create tooltip control with multiline handling (all subsequent calls
			// to change tooltip will use window, created by following line)
			frame->SetToolTip("\n");
			frame->SetToolTip(NULL);
#endif
			// return true to continue
			return true;

			unguard;
		}
		catch (...)
		{
			DisplayError();
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
			DisplayError();
			return 1;
		}
	}

	virtual bool OnExceptionInMainLoop()
	{
		throw;
	}
};

IMPLEMENT_APP(MyApp)
