// basic wxWidgets includes
#include <wx/wx.h>
#include <wx/xrc/xmlres.h>		// loading .xrc files
#include <wx/splitter.h>
#include <wx/notebook.h>
#include <wx/fs_zip.h>			// for zip file system
// WX GL Canvas
#include "GLCanvas.h"
// Third-party wxPropertyGrid
#include <wx/propgrid/propgrid.h>

// Core includes
#include "GlViewport.h"
#include "FileReaderStdio.h"

// Skeletal mesh support
#include "AnimClasses.h"
#include "SkelMeshInstance.h"

#include "EditorClasses.h"
#include "Import.h"

// Property editor
#include "PropEdit.h"
// Log window class
#include "LogWindow.h"
// Settings management
#include "AppSettings.h"

//!! TEST
#include "OutputDeviceMem.h"


#define ZIP_RESOURCES		"resource.bin"

#ifdef ZIP_RESOURCES
#	define RES_NAME(name)	ZIP_RESOURCES"#zip:" name
#else
#	define RES_NAME(name)	"xrc/" name
#endif



#define CLEAR_COLOR			0.2, 0.3, 0.2, 0
#define TYPEINFO_FILE		"typeinfo.bin"

#define MESH_EXTENSION		"skm"
#define ANIM_EXTENSION		"ska"


// Global editing data
static CSkeletalMesh		*EditorMesh;
static CAnimSet				*EditorAnim;
static CSkelMeshInstance	*MeshInst;

static WLogWindow			*GLogWindow;


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


class WCanvas : public GLCanvas
{
public:
	bool		mShowWireframe;
	bool		mShowNormals;
	bool		mShowSkeleton;
	bool		mShowBoneNames;
	bool		mShowAxis;
	bool		mNoTexturing;
	unsigned	mLastFrameTime;

	WCanvas(wxWindow *parent)
	:	GLCanvas(parent)
	,	mShowWireframe(false)
	,	mShowNormals(false)
	,	mShowSkeleton(false)
	,	mShowBoneNames(false)
	,	mShowAxis(true)
	,	mNoTexturing(false)
	,	mLastFrameTime(GetMilliseconds())
	{}

	virtual void Render()
	{
		guard(WCanvas::Render);

		unsigned currTime = GetMilliseconds();
		float frameTime = (currTime - mLastFrameTime) / 1000.0f;
		mLastFrameTime = currTime;
		DrawTextRight("FPS: "S_GREEN"%5.1f", 1.0f / frameTime);

		// prepare frame
		glClearColor(CLEAR_COLOR);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDisable(GL_TEXTURE_2D);

		GL::BuildMatrices();
		GL::Set3Dmode();

		// draw axis
		if (mShowAxis)
		{
			glBegin(GL_LINES);
			for (int i = 0; i < 3; i++)
			{
				CVec3 tmp = nullVec3;
				tmp[i] = 1;
				glColor3fv(tmp.v);
				tmp[i] = 70;
				glVertex3fv(tmp.v);
				glVertex3fv(nullVec3.v);
				tmp[i] = 75;
				static const char* labels[3] = {
					"X", "Y", "Z"
				};
				DrawText3D(tmp, S_CYAN"%s", labels[i]);
			}
			glEnd();
		}
		glColor3f(1, 1, 1);

		//!! should change this !!
		if (MeshInst)
		{
			MeshInst->UpdateAnimation(frameTime);
			MeshInst->DrawMesh(mShowWireframe, mShowNormals, !mNoTexturing);
			if (mShowSkeleton)
				MeshInst->DrawSkeleton(mShowBoneNames);
		}

		GL::Set2Dmode();
		FlushTexts();

		// finish frame
//		glFinish();			-- will set CPU usage to 100% (even if vsync is ON ...)
		SwapBuffers();

		unguard;
	}
};


/*-----------------------------------------------------------------------------
	Main application frame
-----------------------------------------------------------------------------*/

#define SLIDER_TICKS_PER_FRAME		10

class WMainFrame : public wxFrame
{
private:
	// GUI variables
	bool				mFrameVisible[2];
	wxWindow			*mTopFrame, *mBottomFrame, *mRightFrame;
	wxSplitterWindow	*mLeftSplitter, *mMainSplitter;
	int					mMainSplitterPos;
	WCanvas				*mCanvas;
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
	// filenames
	wxString			mMeshFilename;
	wxString			mAnimFilename;
	wxTextCtrl			*mMeshFilenameEdit;
	wxTextCtrl			*mAnimFilenameEdit;

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
	WMainFrame()
	:	mCurrAnim(NULL)
	,	mLastFrameTime(GetMilliseconds())
	{
		guard(WMainFrame::WMainFrame);

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
		mMeshFilenameEdit = (wxTextCtrl*)   FindCtrl("ID_MESHNAME");
		mAnimFilenameEdit = (wxTextCtrl*)   FindCtrl("ID_ANIMNAME");
		wxNotebook *propBase    = (wxNotebook*)FindCtrl("ID_PROPBASE");
		wxWindow   *dummyCanvas = (wxWindow*)  FindCtrl("ID_CANVAS");
		// replace right pane with GLCanvas control
		mCanvas = new WCanvas(mRightFrame);
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
		FindCtrl("ID_CLOSE_TOP")->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(WMainFrame::OnHideTop2), NULL, this);
		FindCtrl("ID_CLOSE_BOTTOM")->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(WMainFrame::OnHideBottom2), NULL, this);

		// setup initially checked toggles
		SyncToggles("AXIS", mCanvas->mShowAxis);

		StopAnimation();

		unguard;
	}

	~WMainFrame()
	{
		CollectSettings();
	}

	/**
	 *	Settings management
	 */
	void CollectSettings()
	{
		PutFramePos(this, GCfg.MainFramePos);
		GCfg.Splitter1 = mMainSplitter->GetSashPosition();
		GCfg.Splitter2 = mLeftSplitter->GetSashPosition();
		GCfg.MeshPropSplitter    = mMeshPropEdit->GetSplitterPosition();
		GCfg.AnimSetPropSplitter = mAnimSetPropEdit->GetSplitterPosition();
		GCfg.AnimSeqPropSplitter = mAnimPropEdit->GetSplitterPosition();
	}

	void ApplySettings()
	{
		SetFramePos(this, GCfg.MainFramePos);
		mMainSplitter->SetSashPosition(GCfg.Splitter1);
		mLeftSplitter->SetSashPosition(GCfg.Splitter2);
		mMeshPropEdit->SetSplitterPosition(GCfg.MeshPropSplitter);
		mAnimSetPropEdit->SetSplitterPosition(GCfg.AnimSetPropSplitter);
		mAnimPropEdit->SetSplitterPosition(GCfg.AnimSeqPropSplitter);
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
		guard(WMainFrame::FillAnimList);
		assert(Anim);

		List->Clear();
		for (int i = 0; i < Anim->Sequences.Num(); i++)
		{
			CMeshAnimSeq &A = Anim->Sequences[i];
			List->Append(wxString::Format("%s [%d]", *A.Name, A.NumFrames), new WAnimInfo(A));
		}

		unguard;
	}

	void UseMesh(CSkeletalMesh *Mesh)
	{
		guard(WMainFrame::UseMesh);
		// create mesh instance
		if (MeshInst)
			delete MeshInst;
		MeshInst = new CSkelMeshInstance;
		MeshInst->SetMesh(Mesh);
		// link animation if loaded
		if (EditorAnim)
			MeshInst->SetAnim(EditorAnim);
		// setup UI
		mMeshPropEdit->AttachObject(EditorMesh);
		GetMenuBar()->Enable(XRCID("ID_MESHSAVE"), true);
		mMeshFilenameEdit->SetValue(mMeshFilename);
		GL::ResetView();
		unguard;
	}

	void UseAnimSet(CAnimSet *Anim)
	{
		guard(WMainFrame::UseAnimSet);
		// setup UI
		mAnimSetPropEdit->AttachObject(Anim);
		FillAnimList(mAnimList, Anim);
		mCurrAnim = NULL;
		mAnimPropEdit->DetachObject();
		GetMenuBar()->Enable(XRCID("ID_ANIMSAVE"), true);
		mAnimFilenameEdit->SetValue(mAnimFilename);
		// link animation to mesh
		if (MeshInst)
		{
			MeshInst->SetAnim(Anim);
			MeshInst->PlayAnim("None");
		}
		unguard;
	}

	void OnSelectAnim(wxCommandEvent &event)
	{
		guard(OnAnimSelect);

		// get animation info from item data
		if (event.GetInt() == -1)
		{
			mCurrAnim = NULL;
			mAnimPropEdit->DetachObject();
			return;
		}
		CMeshAnimSeq *A = ((WAnimInfo*) event.GetClientObject())->Anim;
		assert(A);
		//!! attach property editor
		mAnimPropEdit->AttachObject(FindStruct("MeshAnimSeq"), A);
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

	void OnDblCkickAnim(wxCommandEvent &event)
	{
		OnSelectAnim(event);
		if (mCurrAnim)
			OnPlayAnim(event);
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
		guard(WMainFrame::UpdateAnimations);
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

	void OnResetCam(wxCommandEvent&)
	{
		GL::ResetView();	//?? send via GLCanvas ?
	}

	/**
	 *	Hiding and showing frames
	 */
	void ShowFrame(int frameNum, bool value)
	{
		guard(WMainFrame::ShowFrame);

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

	void OnShowLog(wxCommandEvent &event)
	{
		GLogWindow->Show(event.IsChecked());
	}

	void OnLogClosed(wxCommandEvent&)
	{
		GetMenuBar()->FindItem(XRCID("ID_SHOWLOG"))->Check(false);
	}

	void OnSettings(wxCommandEvent&)
	{
		guard(WMainFrame::OnSettings);
		EditSettings(this);
		unguard;
	}

	/**
	 *	Test stuff, debugging
	 */
	void OnMenuTest(wxCommandEvent&)
	{
		const CStruct *Type = FindStruct("SkeletalMesh");
		Type->Dump();
		if (EditorMesh)
		{
			Type->WriteText(GLog, EditorMesh);
			COutputDeviceMem mem;
			Type->WriteText(&mem, EditorMesh);
			appPrintf("\n\n========DAMAGE=======\n\n");
			EditorMesh->MeshOrigin.Set(-1,-2,-3);
//			EditorMesh->TestString = "DAMAGED STRING!";
			Type->WriteText(GLog, EditorMesh);
			UseMesh(EditorMesh);
			appPrintf("\n\n========READ=======\n\n");
			Type->ReadText(mem.GetText(), EditorMesh);
			appPrintf("\n\n========RECOVERED=======\n\n");
			Type->WriteText(GLog, EditorMesh);
			UseMesh(EditorMesh);
		}
	}

	void OnDumpBones(wxCommandEvent&)
	{
		if (EditorMesh) EditorMesh->DumpBones();
	}

	/**
	 *	Mesh file operations
	 */
	void OnImportMesh(wxCommandEvent&)
	{
		guard(WMainFrame::OnImportMesh);

		StopAnimation();

		wxFileDialog dlg(this, "Import mesh from file ...", "", "",
			"ActorX mesh files (*.psk)|*.psk",
			wxFD_OPEN | wxFD_FILE_MUST_EXIST);
		if (dlg.ShowModal() == wxID_OK)
		{
			wxFileName fn = dlg.GetFilename();
			mMeshFilename = "Imported_" + fn.GetName() + "."MESH_EXTENSION;
			if (EditorMesh) delete EditorMesh;

			const char *filename = dlg.GetPath().c_str();
			appSetNotifyHeader("Importing mesh from %s", filename);
			EditorMesh = new CSkeletalMesh;
			CFile Ar(filename);			// note: will throw appError when failed
			ImportPsk(Ar, *EditorMesh);
			EditorMesh->PostLoad();		// generate extra data

			appSetNotifyHeader("");
			UseMesh(EditorMesh);
		}

		unguard;
	}

	void OnLoadMesh(wxCommandEvent&)
	{
#define MESH_FILESTRING			"Skeletal mesh files (*."MESH_EXTENSION")|*."MESH_EXTENSION
		guard(WMainFrame::OnLoadMesh);

		StopAnimation();

		wxFileDialog dlg(this, "Load mesh from file ...", "", "", MESH_FILESTRING, wxFD_OPEN | wxFD_FILE_MUST_EXIST);
		if (dlg.ShowModal() == wxID_OK)
		{
			mMeshFilename = dlg.GetPath();
			if (EditorMesh) delete EditorMesh;

			EditorMesh = CSkeletalMesh::LoadObject(mMeshFilename.c_str());
			UseMesh(EditorMesh);
		}

		unguard;
	}

	void OnSaveMesh(wxCommandEvent&)
	{
		guard(WMainFrame::OnSaveMesh);
		wxFileDialog dlg(this, "Save mesh to file ...", "", mMeshFilename, MESH_FILESTRING, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
		if (dlg.ShowModal() == wxID_OK)
		{
			mMeshFilename = dlg.GetPath();
			CFile Ar(mMeshFilename.c_str(), false);		// note: will throw appError when failed
			EditorMesh->Serialize(Ar);
		}
		unguard;
	}

	/**
	 *	AnimSet file operations
	 */
	void OnImportAnim(wxCommandEvent&)
	{
		guard(WMainFrame::OnImportAnim);

		StopAnimation();

		wxFileDialog dlg(this, "Import animations from file ...", "", "",
			"ActorX animation files (*.psa)|*.psa",
			wxFD_OPEN | wxFD_FILE_MUST_EXIST);
		if (dlg.ShowModal() == wxID_OK)
		{
			wxFileName fn = dlg.GetFilename();
			mAnimFilename = "Imported_" + fn.GetName() + "."ANIM_EXTENSION;
			if (EditorAnim) delete EditorAnim;

			const char *filename = dlg.GetPath().c_str();
			appSetNotifyHeader("Importing animations from %s", filename);
			EditorAnim = new CAnimSet;
			CFile Ar(filename);		// note: will throw appError when failed
			ImportPsa(Ar, *EditorAnim);

			appSetNotifyHeader("");
			UseAnimSet(EditorAnim);
		}

		unguard;
	}

	void OnLoadAnim(wxCommandEvent&)
	{
		guard(WMainFrame::OnLoadAnim);
#define ANIM_FILESTRING			"Skeletal animation files (*."ANIM_EXTENSION")|*."ANIM_EXTENSION

		wxFileDialog dlg(this, "Load animations from file ...", "", "", ANIM_FILESTRING, wxFD_OPEN | wxFD_FILE_MUST_EXIST);
		if (dlg.ShowModal() == wxID_OK)
		{
			mAnimFilename = dlg.GetPath();
			if (EditorAnim) delete EditorAnim;

			EditorAnim = CAnimSet::LoadObject(mAnimFilename.c_str());
			UseAnimSet(EditorAnim);
		}

		unguard;
	}

	void OnSaveAnim(wxCommandEvent&)
	{
		guard(WMainFrame::OnSaveAnim);
		wxFileDialog dlg(this, "Save animations to file ...", "", mAnimFilename, ANIM_FILESTRING, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
		if (dlg.ShowModal() == wxID_OK)
		{
			mAnimFilename = dlg.GetPath();
			CFile Ar(mAnimFilename.c_str(), false);		// note: will throw appError when failed
			EditorAnim->Serialize(Ar);
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
	EVT_MENU(XRCID("ID_"#name), WMainFrame::On_##name)

	// togglers ...
	HANDLE_TOGGLE(AXIS,      mCanvas->mShowAxis     )
	HANDLE_TOGGLE(TEXTURING, mCanvas->mNoTexturing  )
	HANDLE_TOGGLE(WIREFRAME, mCanvas->mShowWireframe)
	HANDLE_TOGGLE(NORMALS,   mCanvas->mShowNormals  )
	HANDLE_TOGGLE(SKELETON,  mCanvas->mShowSkeleton )
	HANDLE_TOGGLE(BONENAMES, mCanvas->mShowBoneNames)
	HANDLE_TOGGLE(REFPOSE,   mShowRefpose           )

	/**
	 *	Idle handler
	 */
	void OnIdle(wxIdleEvent &event)
	{
		guard(WMainFrame::OnIdle);

		// update local animation state
		unsigned currTime = GetMilliseconds();
		float frameTime = (currTime - mLastFrameTime) / 1000.0f;
		mLastFrameTime = currTime;
		UpdateAnimations(frameTime);
		if (!mAnimPlaying)
			wxMilliSleep(10);
		event.RequestMore();	// without this, only single idle event will be generated

		unguard;
	}

private:
	DECLARE_EVENT_TABLE()
};


BEGIN_EVENT_TABLE(WMainFrame, wxFrame)
	EVT_IDLE(WMainFrame::OnIdle)
	EVT_MENU(wxID_EXIT,                  WMainFrame::OnExit      )
	EVT_MENU(XRCID("ID_FULLSCREEN"),     WMainFrame::OnFullscreen)
	EVT_MENU(XRCID("ID_RESETCAMERA"),    WMainFrame::OnResetCam  )
	EVT_MENU(XRCID("ID_HIDETOPPANE"),    WMainFrame::OnHideTop   )
	EVT_MENU(XRCID("ID_HIDEBOTTOMPANE"), WMainFrame::OnHideBottom)
	EVT_MENU(XRCID("ID_SHOWLOG"),        WMainFrame::OnShowLog   )
	EVT_MENU(XRCID("ID_SETTINGS"),       WMainFrame::OnSettings  )
	EVT_MENU(XRCID("ID_MENUTEST"),       WMainFrame::OnMenuTest  )	//???
	EVT_MENU(XRCID("ID_MESHOPEN"),       WMainFrame::OnLoadMesh  )
	EVT_MENU(XRCID("ID_MESHSAVE"),       WMainFrame::OnSaveMesh  )
	EVT_MENU(XRCID("ID_MESHIMPORT"),     WMainFrame::OnImportMesh)
	EVT_MENU(XRCID("ID_ANIMOPEN"),       WMainFrame::OnLoadAnim  )
	EVT_MENU(XRCID("ID_ANIMSAVE"),       WMainFrame::OnSaveAnim  )
	EVT_MENU(XRCID("ID_ANIMIMPORT"),     WMainFrame::OnImportAnim)
	EVT_BUTTON(XRCID("ID_MESHOPEN"),     WMainFrame::OnLoadMesh  ) //?? MESH1OPEN
	EVT_BUTTON(XRCID("ID_ANIMOPEN"),     WMainFrame::OnLoadAnim  )
	EVT_MENU(XRCID("ID_DUMPBONES"),      WMainFrame::OnDumpBones )
	HOOK_TOGGLE(AXIS     )
	HOOK_TOGGLE(TEXTURING)
	HOOK_TOGGLE(WIREFRAME)
	HOOK_TOGGLE(NORMALS  )
	HOOK_TOGGLE(SKELETON )
	HOOK_TOGGLE(BONENAMES)
	HOOK_TOGGLE(REFPOSE  )
	EVT_LISTBOX(XRCID("ID_ANIMLIST"),    WMainFrame::OnSelectAnim)
	EVT_LISTBOX_DCLICK(XRCID("ID_ANIMLIST"), WMainFrame::OnDblCkickAnim)
	EVT_MENU(XRCID("ID_PLAYANIM"),       WMainFrame::OnPlayAnim  )
	EVT_MENU(XRCID("ID_STOPANIM"),       WMainFrame::OnStopAnim  )
	EVT_MENU(XRCID("ID_LOOPANIM"),       WMainFrame::OnLoopToggle)
	EVT_COMMAND_SCROLL(XRCID("ID_ANIMPOS"), WMainFrame::OnSetAnimPos)
	EVT_LOG_CLOSED(wxID_ANY,             WMainFrame::OnLogClosed )
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


class WApp : public wxApp
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
			guard(WApp::OnInit);

#ifdef ZIP_RESOURCES
			wxFileSystem::AddHandler(new wxZipFSHandler);
#endif
			wxInitAllImageHandlers();
			// initialize wxWidgets resource system
			wxXmlResource::Get()->InitAllHandlers();
			if (!wxXmlResource::Get()->Load(RES_NAME("main.xrc")))
				return false;

			// Create the main frame window
			WMainFrame *frame = new WMainFrame();

			GLogWindow = new WLogWindow(frame);
			GLogWindow->Register();		//?? should Unregister() at exit

			CFile Ar(TYPEINFO_FILE);
			InitTypeinfo(Ar);
			Ar.Close();

			// load settings
			bool hasSettigns = LoadSettings();
			if (hasSettigns)
			{
				frame->ApplySettings();
				GLogWindow->ApplySettings();
			}

			// Show the frame
			frame->Show(true);

			// enable if there will be bugs with displaying multiline hints:
#if 1
			// create tooltip control with multiline handling (all subsequent calls
			// to change tooltip will use window, created by following line)
			frame->SetToolTip("\n");
			frame->SetToolTip(NULL);
#endif

			// check for uninitialized settings
			if (!GCfg.ResourceRoot[0])
				EditSettings(frame);

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

	/*
	 * wxApp::OnRun() is overrided to catch exceptions
	 * NOTE!! GPF is not catched! Should use /EHa compiler option for both application and
	 *        wxWidgets library + all controls (TEST IT !)
	 * Current implementation: using SetUnhandledExceptionFilter() on Win32 platform to catch
	 *  exceptions, but this will not work if someone (third-party lib, driver ...) will override
	 *  this function.
	 */
	virtual int OnRun()
	{
		try
		{
			guard(MainLoop);
			int result = wxApp::OnRun();
			if (!result)
			{
				guard(SaveSettings);
				SaveSettings();
				unguard;
			}
			return result;
			unguard;
		}
		catch (...)
		{
			// display error message
			DisplayError();
			// emergency save edited data
			bool savedMesh = false, savedAnim = false;
			if (EditorMesh)
			{
				try
				{
					appNotify("Saving mesh to autosave");
					CFile Ar("autosave."MESH_EXTENSION, false);
					EditorMesh->Serialize(Ar);
					savedMesh = true;
				}
				catch (...)
				{}
			}
			if (EditorAnim)
			{
				try
				{
					appNotify("Saving animations to autosave");
					CFile Ar("autosave."ANIM_EXTENSION, false);
					EditorAnim->Serialize(Ar);
					savedAnim = true;
				}
				catch (...)
				{}
			}
			if (savedMesh && savedAnim)
				wxMessageBox("Animation and mesh were saved to \"autosave\"");
			else if (savedMesh)
				wxMessageBox("Mesh was saved to \"autosave\"");
			else if (savedAnim)
				wxMessageBox("Animations was saved to \"autosave\"");
			// exit
			return 1;
		}
	}

	virtual bool OnExceptionInMainLoop()
	{
		throw;
	}
};

IMPLEMENT_APP(WApp)
