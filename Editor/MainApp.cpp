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
// Gizmos
#include "Gizmo.h"


#define ZIP_RESOURCES		"resource.bin"

#ifdef ZIP_RESOURCES
#	define RES_NAME(name)	ZIP_RESOURCES"#zip:" name
#else
#	define RES_NAME(name)	"xrc/" name
#endif



#define TYPEINFO_FILE		"typeinfo.bin"		//?? move to Core


// Global editing data
static CSkeletalMesh		*EditorMesh;
static CAnimSet				*EditorAnim;
static CSkelMeshInstance	*MeshInst;

static WLogWindow			*GLogWindow;


/*-----------------------------------------------------------------------------
	Creating object, attached to some mesh bone
-----------------------------------------------------------------------------*/

class WNewBoneObject : public wxDialog
{
protected:
	wxButton	*m_okButton;
	wxTextCtrl	*m_objectName;
	wxChoice	*m_boneSelect;

	wxWindow* FindCtrl(const char *Name)	//!! copy from WMainFrame
	{
		int id = wxXmlResource::GetXRCID(Name);
		wxWindow *res = FindWindow(id);
		if (!res)
			appError("Resource error:\nUnable to find control for \"%s\" (id=%d)", Name, id);
		return res;
	}

public:
	WNewBoneObject(wxFrame *parent)
	{
		wxXmlResource::Get()->LoadDialog(this, parent, "ID_BONEFRAME");
		m_objectName = (wxTextCtrl*) FindCtrl("ID_OBJNAME");
		m_boneSelect = (wxChoice*)   FindCtrl("ID_BONENAME");
		m_okButton   = (wxButton*)   FindCtrl("wxID_OK");
	}

	void OnBoneChanged(wxCommandEvent&)
	{
		m_okButton->Enable();
		if (m_objectName->GetValue().IsEmpty())
			m_objectName->SetValue(*EditorMesh->Skeleton[m_boneSelect->GetCurrentSelection()].Name);
	}

	static bool Show(wxFrame *parent, const char *ObjectTitle, wxString &ObjectName, int &BoneIndex)
	{
		guard(WNewBoneObject::Show);

		if (!EditorMesh) return false;	// no mesh - no bones

		static WNewBoneObject *Dialog;
		if (!Dialog) Dialog = new WNewBoneObject(parent);

		// setup dialog
		Dialog->SetTitle(wxString::Format("Create new %s", ObjectTitle));
		Dialog->m_boneSelect->Clear();
		for (int i = 0; i < EditorMesh->Skeleton.Num(); i++)
			Dialog->m_boneSelect->Append(*EditorMesh->Skeleton[i].Name);
		Dialog->m_objectName->SetValue("");
		Dialog->m_okButton->Enable(false);

		// show dialog
		if (Dialog->ShowModal() != wxID_OK)
			return false;
		ObjectName = Dialog->m_objectName->GetValue();
		BoneIndex  = Dialog->m_boneSelect->GetCurrentSelection();

		return true;

		unguard;
	}

private:
	DECLARE_EVENT_TABLE()
};


BEGIN_EVENT_TABLE(WNewBoneObject, wxDialog)
	EVT_CHOICE(XRCID("ID_BONENAME"), WNewBoneObject::OnBoneChanged)
END_EVENT_TABLE()


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
	bool		m_showWireframe;
	bool		m_showNormals;
	bool		m_showSkeleton;
	bool		m_showBoneNames;
	bool		m_showHitboxes;
	bool		m_showAxis;
	bool		m_noTexturing;
	unsigned	m_lastFrameTime;
	// some state
	int			m_highlightBox;

	WCanvas(wxWindow *parent)
	:	GLCanvas(parent)
	,	m_showWireframe(false)
	,	m_showNormals(false)
	,	m_showSkeleton(false)
	,	m_showBoneNames(false)
	,	m_showHitboxes(false)
	,	m_showAxis(true)
	,	m_noTexturing(false)
	,	m_lastFrameTime(GetMilliseconds())
	,	m_highlightBox(-1)
	{}

	virtual void Render()
	{
		guard(WCanvas::Render);

		unsigned currTime = GetMilliseconds();
		float frameTime = (currTime - m_lastFrameTime) / 1000.0f;
		m_lastFrameTime = currTime;
		DrawTextRight("FPS: "S_GREEN"%5.1f", 1.0f / frameTime);

		// prepare frame
		RenderBackground();
		glDisable(GL_TEXTURE_2D);

		GL::BuildMatrices();
		GL::Set3Dmode();

		// draw axis
		if (m_showAxis)
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
			MeshInst->DrawMesh(m_showWireframe, m_showNormals, !m_noTexturing);
			if (m_showSkeleton)
				MeshInst->DrawSkeleton(m_showBoneNames);
			if (m_showHitboxes)
				MeshInst->DrawBoxes(m_highlightBox);
		}

		DisplayGizmo();

		GL::Set2Dmode();
		FlushTexts();

		// finish frame
//		glFinish();			-- will set CPU usage to 100% (even if vsync is ON ...)
		SwapBuffers();

		unguard;
	}

	virtual bool ProcessMouse(bool leftDown, bool rightDown, int mouseX, int mouseY, int deltaX, int deltaY)
	{
		return TickGizmo(leftDown, mouseX, mouseY, deltaX, deltaY);
	}

	void RenderBackground()
	{
		if (!GCfg.EnableGradient)
		{
			glClearColor(VECTOR_ARG(GCfg.MeshBackground), 1);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		}
		else
		{
			GL::Set2Dmode();
			glDisable(GL_TEXTURE_2D);
			glBegin(GL_QUADS);
			glColor4f(VECTOR_ARG(GCfg.MeshBackground), 1);
			glVertex2f(0, 0);
			glVertex2f(GL::width, 0);
			glColor4f(VECTOR_ARG(GCfg.MeshBackground2), 1);
			glVertex2f(GL::width, GL::height);
			glVertex2f(0, GL::height);
			glEnd();
			glClear(GL_DEPTH_BUFFER_BIT);
		}
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
	bool				m_frameVisible[2];
	wxWindow			*m_topFrame, *m_bottomFrame, *m_rightFrame;
	wxSplitterWindow	*m_leftSplitter, *m_mainSplitter;
	int					m_mainSplitterPos;
	WCanvas				*m_canvas;
	WPropEdit			*m_meshPropEdit, *m_animSetPropEdit, *m_animPropEdit;
	wxListBox			*m_animList;
	wxSlider			*m_animPosition;
	wxStaticText		*m_animLabel;
	// animation control variables
	unsigned			m_lastFrameTime;
	const CMeshAnimSeq	*m_currAnim;
	bool				m_animLooped;
	bool				m_animPlaying;
	bool				m_showRefpose;
	float				m_animFrame;
	// filenames
	wxString			m_meshFilename;
	wxString			m_animFilename;
	wxTextCtrl			*m_meshFilenameEdit;
	wxTextCtrl			*m_animFilenameEdit;
	// gizmo state
	int					m_gizmoMode;

	wxWindow* FindCtrl(const char *Name)
	{
		int id = wxXmlResource::GetXRCID(Name);
		wxWindow *res = FindWindow(id);
		if (!res)
			appError("Resource error:\nUnable to find control for \"%s\" (id=%d)", Name, id);
		return res;
	}

	static const char *MeshEnumCallback(int Index, const char *EnumName)
	{
		guard(MeshEnumCallback);
		if (!strcmp(EnumName, "MeshBone"))
		{
			assert(EditorMesh);
			if (Index >= EditorMesh->Skeleton.Num())
				return NULL;
			else
				return EditorMesh->Skeleton[Index].Name;
		}
		else
			appError("MeshEnumCallback: unknown enum '%s'", EnumName);
		return "<error>";		// shut up compiler
		unguard;
	}

public:
	/**
	 *	Constructor
	 */
	WMainFrame()
	:	m_currAnim(NULL)
	,	m_lastFrameTime(GetMilliseconds())
	,	m_gizmoMode(0)
	{
		guard(WMainFrame::WMainFrame);

		// set window icon
		// SetIcon(wxIcon(....));
		wxXmlResource::Get()->LoadFrame(this, NULL, "ID_MAINFRAME");
		m_frameVisible[0] = m_frameVisible[1] = true;
		SetMinSize(wxSize(400, 300));
		// find main controls
		m_topFrame     = (wxWindow*)           FindCtrl("ID_TOPPANE");
		m_bottomFrame  = (wxWindow*)           FindCtrl("ID_BOTTOMPANE");
		m_rightFrame   = (wxWindow*)           FindCtrl("ID_RIGHTPANE");
		m_leftSplitter = (wxSplitterWindow*)   FindCtrl("ID_LEFTSPLITTER");
		m_mainSplitter = (wxSplitterWindow*)   FindCtrl("ID_MAINSPLITTER");
		m_animList     = (wxListBox*)          FindCtrl("ID_ANIMLIST");
		m_animPosition = (wxSlider*)           FindCtrl("ID_ANIMPOS");
		m_animLabel    = (wxStaticText*)       FindCtrl("ID_ANIMFRAME");
		m_meshFilenameEdit = (wxTextCtrl*)     FindCtrl("ID_MESHNAME");
		m_animFilenameEdit = (wxTextCtrl*)     FindCtrl("ID_ANIMNAME");
		wxNotebook *propBase    = (wxNotebook*)FindCtrl("ID_PROPBASE");
		wxWindow   *dummyCanvas = (wxWindow*)  FindCtrl("ID_CANVAS");
		// replace right pane with GLCanvas control
		m_canvas = new WCanvas(m_rightFrame);
		if (!m_rightFrame->GetSizer()->Replace(dummyCanvas, m_canvas))
			wxLogFatalError("Cannot replace control right pane with GLCanvas");
		delete dummyCanvas;

		// create property grid pages
		m_meshPropEdit = new WPropEdit(propBase, wxID_ANY, wxDefaultPosition, wxDefaultSize,
			wxPG_SPLITTER_AUTO_CENTER | wxPG_BOLD_MODIFIED | wxSUNKEN_BORDER | wxPG_DEFAULT_STYLE);
		m_meshPropEdit->SetExtraStyle(wxPG_EX_HELP_AS_TOOLTIPS);
		propBase->AddPage(m_meshPropEdit, "Mesh");
		m_meshPropEdit->m_enumCallback = MeshEnumCallback;

		m_animSetPropEdit = new WPropEdit(propBase, wxID_ANY, wxDefaultPosition, wxDefaultSize,
			wxPG_SPLITTER_AUTO_CENTER | wxPG_BOLD_MODIFIED | wxSUNKEN_BORDER | wxPG_DEFAULT_STYLE);
		m_animSetPropEdit->SetExtraStyle(wxPG_EX_HELP_AS_TOOLTIPS);
		propBase->AddPage(m_animSetPropEdit, "AnimSet");

		m_animPropEdit = new WPropEdit(propBase, wxID_ANY, wxDefaultPosition, wxDefaultSize,
			wxPG_SPLITTER_AUTO_CENTER | wxPG_BOLD_MODIFIED | wxSUNKEN_BORDER | wxPG_DEFAULT_STYLE);
		m_animPropEdit->SetExtraStyle(wxPG_EX_HELP_AS_TOOLTIPS);
		propBase->AddPage(m_animPropEdit, "AnimSequence");

		// init some controls
		m_mainSplitter->SetSashPosition(250);
		m_leftSplitter->SetSashPosition(0);

		// attach message handlers
		FindCtrl("ID_CLOSE_TOP")->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(WMainFrame::OnHideTop2), NULL, this);
		FindCtrl("ID_CLOSE_BOTTOM")->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(WMainFrame::OnHideBottom2), NULL, this);

		// setup initially checked toggles
		SyncToggles("AXIS", m_canvas->m_showAxis);

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
		GCfg.Splitter1 = m_mainSplitter->GetSashPosition();
		GCfg.Splitter2 = m_leftSplitter->GetSashPosition();
		GCfg.MeshPropSplitter    = m_meshPropEdit->GetSplitterPosition();
		GCfg.AnimSetPropSplitter = m_animSetPropEdit->GetSplitterPosition();
		GCfg.AnimSeqPropSplitter = m_animPropEdit->GetSplitterPosition();
	}

	void ApplySettings()
	{
		SetFramePos(this, GCfg.MainFramePos);
		m_mainSplitter->SetSashPosition(GCfg.Splitter1);
		m_leftSplitter->SetSashPosition(GCfg.Splitter2);
		m_meshPropEdit->SetSplitterPosition(GCfg.MeshPropSplitter);
		m_animSetPropEdit->SetSplitterPosition(GCfg.AnimSetPropSplitter);
		m_animPropEdit->SetSplitterPosition(GCfg.AnimSeqPropSplitter);
	}

	/**
	 *	File operations
	 */

	void ImportMesh(const char *Filename)
	{
		guard(WMainFrame::ImportMesh);

		wxFileName fn = Filename;	// using wxFileName here for GetName() function
		m_meshFilename = "Imported_" + fn.GetName() + "."MESH_EXTENSION;
		if (EditorMesh) delete EditorMesh;

		appSetNotifyHeader("Importing mesh from %s", Filename);
		EditorMesh = new CSkeletalMesh;
		CFile Ar(Filename);			// note: will throw appError when failed
		ImportPsk(Ar, *EditorMesh);
		EditorMesh->PostLoad();		// generate extra data

		appSetNotifyHeader("");
		UseMesh(EditorMesh);

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
		if (MeshInst) delete MeshInst;
		MeshInst = NULL;

		MeshInst = new CSkelMeshInstance;
		MeshInst->SetMesh(Mesh);
		// link animation if loaded
		if (EditorAnim)
			MeshInst->SetAnim(EditorAnim);
		// setup UI
		m_meshPropEdit->AttachObject(EditorMesh);
		GetMenuBar()->Enable(XRCID("ID_MESHSAVE"), true);
		m_meshFilenameEdit->SetValue(m_meshFilename);
		// setup statusbar
		GetStatusBar()->SetStatusText(wxString::Format("SkeletalMesh: %s", m_meshFilename.c_str()), 0);

		GL::ResetView();
		unguard;
	}

	void UseAnimSet(CAnimSet *Anim)
	{
		guard(WMainFrame::UseAnimSet);
		// setup UI
		m_animSetPropEdit->AttachObject(Anim);
		FillAnimList(m_animList, Anim);
		m_currAnim = NULL;
		m_animPropEdit->DetachObject();
		GetMenuBar()->Enable(XRCID("ID_ANIMSAVE"), true);
		m_animFilenameEdit->SetValue(m_animFilename);
		// setup statusbar
		int comprMem, uncomprMem;
		Anim->GetMemFootprint(&comprMem, &uncomprMem);
		GetStatusBar()->SetStatusText(wxString::Format("AnimSet: %s   Mem: %.2fMb/%.2fMb",
			m_animFilename.c_str(), comprMem / (1024.0f*1024.0f), uncomprMem / (1024.0f*1024.0f)), 1);
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
			m_currAnim = NULL;
			m_animPropEdit->DetachObject();
			GetStatusBar()->SetStatusText("", 2);
			return;
		}
		CMeshAnimSeq *A = ((WAnimInfo*) event.GetClientObject())->Anim;
		assert(A);
		// attach property editor
		m_animPropEdit->AttachObject(FindStruct("MeshAnimSeq"), A);
		// setup statusbar
		int comprMem, uncomprMem;
		A->GetMemFootprint(&comprMem, &uncomprMem);
		GetStatusBar()->SetStatusText(wxString::Format("AnimSeq: %s   Len: %.2fs   Mem: %.2fKb/%.2fKb",
			*A->Name, A->NumFrames / A->Rate, comprMem / 1024.0, uncomprMem / 1024.0), 2);
		// setup mesh animation
		StopAnimation();
		m_currAnim = A;
		if (MeshInst)
		{
			// show tweening effect, and allow looping (to allow interpolation between
			// last and first frame for local animation controller)
			MeshInst->LoopAnim(A->Name, 0, 0.25);
			// setup slider
			m_animPosition->SetRange(0, (A->NumFrames - 1) * SLIDER_TICKS_PER_FRAME);
		}

		unguard;
	}

	void OnDblCkickAnim(wxCommandEvent &event)
	{
		OnSelectAnim(event);
		if (m_currAnim)
			OnPlayAnim(event);
	}

	void OnPlayAnim(wxCommandEvent&)
	{
		m_animFrame   = 0;
		m_animPlaying = true;
	}

	void OnStopAnim(wxCommandEvent&)
	{
		if (MeshInst && m_currAnim)
			m_animPlaying = !m_animPlaying;
	}

	void OnLoopToggle(wxCommandEvent &event)
	{
		m_animLooped = event.IsChecked();
	}

	void OnSetAnimPos(wxScrollEvent&)
	{
		if (m_animPlaying || !MeshInst || !m_currAnim || !m_currAnim->Rate)
			return;
		m_animFrame = (float)m_animPosition->GetValue() / SLIDER_TICKS_PER_FRAME;
	}

	/**
	 *	Very basic animation controller
	 */
	void UpdateAnimations(float timeDelta)
	{
		guard(WMainFrame::UpdateAnimations);
		if (!MeshInst || !m_currAnim || !m_currAnim->Rate)
			return;

		if (m_animPlaying && !MeshInst->IsTweening())
		{
			// advance frame
			m_animFrame += timeDelta * m_currAnim->Rate;
			// clamp/wrap frame
			if (!m_animLooped)
			{
				if (m_animFrame > m_currAnim->NumFrames - 1)
				{
					m_animFrame   = m_currAnim->NumFrames - 1;
					m_animPlaying = false;
				}
				else if (m_animFrame < 0)					// reverse playback
				{
					m_animFrame   = 0;
					m_animPlaying = false;
				}
			}
			else
			{
				if (m_animFrame >= m_currAnim->NumFrames)	// 1 more frame for lerp between last and first frames
					m_animFrame -= m_currAnim->NumFrames;
				else if (m_animFrame < 0)					// reverse playback
					m_animFrame += m_currAnim->NumFrames;
			}
			// update slider
			m_animPosition->SetValue(appFloor(m_animFrame * SLIDER_TICKS_PER_FRAME));
		}
		// update animation
		const char *wantAnim = m_showRefpose ? "None" : *m_currAnim->Name;
		float dummyFrame, dummyNumFrames, dummyRate;
		const char *playingAnim;
		MeshInst->GetAnimParams(0, playingAnim, dummyFrame, dummyNumFrames, dummyRate);
		if (strcmp(playingAnim, wantAnim) != 0)
		{
			m_animFrame = 0;
			MeshInst->LoopAnim(wantAnim, 0, 0.25);
		}
		if (!MeshInst->IsTweening() && !m_showRefpose)
			MeshInst->FreezeAnimAt(m_animFrame);
		m_animLabel->SetLabel(wxString::Format("Frame: %4.1f", m_animFrame));

		unguard;
	}

	void StopAnimation()
	{
		m_animPlaying = false;
		m_animFrame   = 0;
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

	void OnGizmoMode(wxCommandEvent &event)
	{
		int id = event.GetId();
		if (id == XRCID("ID_MOVEGIZMO"))
			m_gizmoMode = 0;
		else if (id == XRCID("ID_SCALEGIZMO"))
			m_gizmoMode = 1;
		else if (id == XRCID("ID_ROTATEGIZMO"))
			m_gizmoMode = 2;
		else
			appError("OnGizmoMode: unknown id %d", id);
		// sync toolbar and menu
		GetMenuBar()->FindItem(id)->Check(true);
		GetToolBar()->ToggleTool(id, true);
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

		bool MainFrameWasVisible = m_frameVisible[0] || m_frameVisible[1];
		m_frameVisible[frameNum] = value;
		bool MainFrameVisible = m_frameVisible[0] || m_frameVisible[1];

		m_topFrame->Show(m_frameVisible[0]);
		m_bottomFrame->Show(m_frameVisible[1]);

		if (!MainFrameVisible)
		{
			m_mainSplitterPos = m_mainSplitter->GetSashPosition();
			// both frames are hidden
			m_mainSplitter->Unsplit(m_leftSplitter);
		}
		else if (MainFrameWasVisible && MainFrameVisible)
		{
			// left frame remains visible
			m_leftSplitter->Unsplit(m_frameVisible[0] ? m_bottomFrame : m_topFrame);
			if (m_frameVisible[0] && m_frameVisible[1])
				m_leftSplitter->SplitHorizontally(m_topFrame, m_bottomFrame);
			else if (m_frameVisible[0] && !m_frameVisible[1])
				m_leftSplitter->Initialize(m_topFrame);
			else
				m_leftSplitter->Initialize(m_bottomFrame);
		}
		else
		{
			// left frame becomes visible with one pane
			m_mainSplitter->SplitVertically(m_leftSplitter, m_rightFrame, m_mainSplitterPos);
			m_leftSplitter->Unsplit();
			m_leftSplitter->Initialize(m_frameVisible[0] ? m_topFrame : m_bottomFrame);
			m_leftSplitter->UpdateSize();
			m_mainSplitter->UpdateSize();
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

		wxFileDialog dlg(this, "Import mesh from file ...", *GCfg.ImportDirectory, "",
			"ActorX mesh files (*.psk,*.pskx)|*.psk;*.pskx",
			wxFD_OPEN | wxFD_FILE_MUST_EXIST);
		if (dlg.ShowModal() == wxID_OK)
		{
			wxFileName fn = dlg.GetFilename();
			m_meshFilename = "Imported_" + fn.GetName() + "."MESH_EXTENSION;
			if (EditorMesh) delete EditorMesh;

			wxString filename = dlg.GetPath();
			const char *filename2 = filename.c_str();	// wxString.c_str() has known bugs with printf-like functions, so use intermediate variable
			appSetNotifyHeader("Importing mesh from %s", filename2);
			EditorMesh = new CSkeletalMesh;
			CFile Ar(filename2);		// note: will throw appError when failed
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

		wxFileDialog dlg(this, "Load mesh from file ...", *GCfg.AnimDataDirectory, "",
			MESH_FILESTRING,
			wxFD_OPEN | wxFD_FILE_MUST_EXIST);
		if (dlg.ShowModal() == wxID_OK)
		{
			m_meshFilename = dlg.GetPath();
			if (EditorMesh) delete EditorMesh;

			EditorMesh = CSkeletalMesh::LoadObject(m_meshFilename.c_str());
			if (EditorMesh)
			{
				UseMesh(EditorMesh);
			}
			else
			{
				wxMessageBox(m_meshFilename, "Unable to load mesh", wxOK | wxICON_ERROR);
			}
		}

		unguard;
	}

	void OnSaveMesh(wxCommandEvent&)
	{
		guard(WMainFrame::OnSaveMesh);
		wxFileDialog dlg(this, "Save mesh to file ...", "", m_meshFilename,
			MESH_FILESTRING,
			wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
		if (dlg.ShowModal() == wxID_OK)
		{
			m_meshFilename = dlg.GetPath();
			m_meshFilenameEdit->SetValue(m_meshFilename);
			CFile Ar(m_meshFilename.c_str(), false);		// note: will throw appError when failed
			SerializeObject(EditorMesh, Ar);
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

		wxFileDialog dlg(this, "Import animations from file ...", *GCfg.ImportDirectory, "",
			"ActorX animation files (*.psa)|*.psa",
			wxFD_OPEN | wxFD_FILE_MUST_EXIST);
		if (dlg.ShowModal() == wxID_OK)
		{
			wxFileName fn = dlg.GetFilename();
			m_animFilename = "Imported_" + fn.GetName() + "."ANIM_EXTENSION;
			if (EditorAnim) delete EditorAnim;

			wxString filename = dlg.GetPath();
			const char *filename2 = filename.c_str();
			appSetNotifyHeader("Importing animations from %s", filename2);
			EditorAnim = new CAnimSet;
			CFile Ar(filename.c_str());	// note: will throw appError when failed
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

		wxFileDialog dlg(this, "Load animations from file ...", *GCfg.AnimDataDirectory, "",
			ANIM_FILESTRING,
			wxFD_OPEN | wxFD_FILE_MUST_EXIST);
		if (dlg.ShowModal() == wxID_OK)
		{
			m_animFilename = dlg.GetPath();
			if (EditorAnim) delete EditorAnim;

			EditorAnim = CAnimSet::LoadObject(m_animFilename.c_str());
			if (!EditorAnim)
			{
				wxMessageBox(m_animFilename, "Unable to load AnimSet", wxOK | wxICON_ERROR);
				return;
			}
			UseAnimSet(EditorAnim);
		}

		unguard;
	}

	void OnSaveAnim(wxCommandEvent&)
	{
		guard(WMainFrame::OnSaveAnim);
		wxFileDialog dlg(this, "Save animations to file ...", "", m_animFilename,
			ANIM_FILESTRING,
			wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
		if (dlg.ShowModal() == wxID_OK)
		{
			m_animFilename = dlg.GetPath();
			m_animFilenameEdit->SetValue(m_animFilename);
			CFile Ar(m_animFilename.c_str(), false);		// note: will throw appError when failed
			SerializeObject(EditorAnim, Ar);
		}
		unguard;
	}

	/**
     *	Mesh bounding boxes support
     */
	void OnNewHitbox(wxCommandEvent&)
	{
		guard(WMainFrame::OnNewHitbox);
		if (!EditorMesh) return;

		wxString ObjName;
		int boneIdx;
		if (!WNewBoneObject::Show(this, "hitbox", ObjName, boneIdx))
			return;
		// create and setup new hitbox
		int boxIndex = EditorMesh->BoundingBoxes.Add();
		CMeshHitBox &H = EditorMesh->BoundingBoxes[boxIndex];
		H.Name      = ObjName.c_str();
		H.BoneIndex = boneIdx;
		if (!GenerateBox(*EditorMesh, boneIdx, H.Coords))	// try to compute hitbox
			H.Coords = identCoords;							// box is empty, fallback
		// update property grid and select new box
		m_meshPropEdit->RefreshByName("BoundingBoxes");
		TString<256> PropName;
		PropName.sprintf("BoundingBoxes.[%d]", boxIndex);
		m_meshPropEdit->SelectByName(PropName, true);

		unguard;
	}

	void OnRemoveHitboxes(wxCommandEvent&)
	{
		guard(WMainFrame::OnRemoveHitboxes);
		if (!EditorMesh) return;
		if (wxMessageBox("All hitboxes will be removed. Continue ?", "Warning",
			wxICON_QUESTION | wxYES_NO) == wxNO)
			return;

		EditorMesh->BoundingBoxes.Empty();
		m_meshPropEdit->RefreshByName("BoundingBoxes");

		unguard;
	}

	void OnRecreateHitboxes(wxCommandEvent&)
	{
		guard(WMainFrame::OnRecreateHitboxes);
		if (!EditorMesh) return;

		if (wxMessageBox("All hitboxes will be removed. Continue ?", "Warning",
			wxICON_QUESTION | wxYES_NO) == wxNO)
			return;

		EditorMesh->BoundingBoxes.Empty();
		GenerateBoxes(*EditorMesh);
		m_meshPropEdit->RefreshByName("BoundingBoxes");

		unguard;
	}

	/**
     *	Mesh sockets support
     */
	void OnNewSocket(wxCommandEvent&)
	{
		guard(WMainFrame::OnNewSocket);
		if (!EditorMesh) return;

		wxString ObjName;
		int boneIdx;
		if (!WNewBoneObject::Show(this, "socket", ObjName, boneIdx))
			return;
		// create and setup socket
		int socketIndex = EditorMesh->Sockets.Add();
		CMeshSocket &S = EditorMesh->Sockets[socketIndex];
		S.Name      = ObjName.c_str();
		S.BoneIndex = boneIdx;
		S.Coords    = identCoords;
		// update property grid and select new box
		m_meshPropEdit->RefreshByName("Sockets");
		TString<256> PropName;
		PropName.sprintf("Sockets.[%d]", socketIndex);
		m_meshPropEdit->SelectByName(PropName, true);

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
	HANDLE_TOGGLE(AXIS,        m_canvas->m_showAxis     )
	HANDLE_TOGGLE(TEXTURING,   m_canvas->m_noTexturing  )
	HANDLE_TOGGLE(WIREFRAME,   m_canvas->m_showWireframe)
	HANDLE_TOGGLE(NORMALS,     m_canvas->m_showNormals  )
	HANDLE_TOGGLE(SKELETON,    m_canvas->m_showSkeleton )
	HANDLE_TOGGLE(BONENAMES,   m_canvas->m_showBoneNames)
	HANDLE_TOGGLE(BBOXES,      m_canvas->m_showHitboxes )
	HANDLE_TOGGLE(REFPOSE,     m_showRefpose            )

	/**
	 *	Idle handler
	 */
	void OnIdle(wxIdleEvent &event)
	{
		guard(WMainFrame::OnIdle);

		// update local animation state
		unsigned currTime = GetMilliseconds();
		float frameTime = (currTime - m_lastFrameTime) / 1000.0f;
		m_lastFrameTime = currTime;
		UpdateAnimations(frameTime);
		if (!m_animPlaying)
			wxMilliSleep(10);
		event.RequestMore();	// without this, only single idle event will be generated

		// update some editing state
		if (EditorMesh)
		{
			int sel = -1;
			m_meshPropEdit->IsPropertySelected("BoundingBoxes", &sel);
			m_canvas->m_highlightBox = sel;
			CCoords *edCoords = NULL;
			int      edBone   = -1;
			if (sel >= 0)
			{
				// edit mesh bounding box
				CMeshHitBox &box = EditorMesh->BoundingBoxes[sel];
				edCoords = &box.Coords;
				edBone   = box.BoneIndex;
			}
			else
			{
				m_meshPropEdit->IsPropertySelected("Sockets", &sel);
				if (sel >= 0)
				{
					// edit mesh socket
					CMeshSocket &s = EditorMesh->Sockets[sel];
					edCoords = &s.Coords;
					edBone   = s.BoneIndex;
				}
			}

			if (edCoords)
			{
				SetGizmoScale(1.0f / EditorMesh->MeshScale[0]);
				if (m_gizmoMode == 0)
					GizmoMove(&edCoords->origin, &MeshInst->GetBoneCoords(edBone));
				else if (m_gizmoMode == 1)
					GizmoScale(edCoords,  &MeshInst->GetBoneCoords(edBone));
				else if (m_gizmoMode == 2)
					GizmoRotate(edCoords, &MeshInst->GetBoneCoords(edBone));
				else
					RemoveGizmo();
			}
			else
				RemoveGizmo();
		}

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
	EVT_MENU(XRCID("ID_NEWHITBOX"),      WMainFrame::OnNewHitbox )
	EVT_MENU(XRCID("ID_REMOVEHITBOXES"), WMainFrame::OnRemoveHitboxes)
	EVT_MENU(XRCID("ID_RECREATEHITBOXES"), WMainFrame::OnRecreateHitboxes)
	EVT_MENU(XRCID("ID_NEWSOCKET"),      WMainFrame::OnNewSocket )
	EVT_MENU(XRCID("ID_DUMPBONES"),      WMainFrame::OnDumpBones )
	HOOK_TOGGLE(AXIS       )
	HOOK_TOGGLE(TEXTURING  )
	HOOK_TOGGLE(WIREFRAME  )
	HOOK_TOGGLE(NORMALS    )
	HOOK_TOGGLE(SKELETON   )
	HOOK_TOGGLE(BONENAMES  )
	HOOK_TOGGLE(BBOXES     )
	HOOK_TOGGLE(REFPOSE    )
	EVT_MENU(XRCID("ID_MOVEGIZMO"),      WMainFrame::OnGizmoMode )
	EVT_MENU(XRCID("ID_SCALEGIZMO"),     WMainFrame::OnGizmoMode )
	EVT_MENU(XRCID("ID_ROTATEGIZMO"),    WMainFrame::OnGizmoMode )
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
		// init Core
		appInit();
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

			// init typeinfo
			CFile Ar(TYPEINFO_FILE);		//?? move to appInit()
			InitTypeinfo(Ar);
			Ar.Close();
			BEGIN_CLASS_TABLE
				REGISTER_ANIM_CLASSES
			END_CLASS_TABLE

			// setup some GCfg defaults
			GCfg.MeshBackground.Set(0, 0.3f, 0.5f);
			GCfg.EnableGradient = true;

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
			{
				wxMessageBox("Please specify a resource root directory", "Setup", wxOK | wxICON_EXCLAMATION);
				EditSettings(frame);
			}

			if (argc > 1)
			{
				wxString FileToOpen(argv[1]);
				const char *s = FileToOpen.c_str();
				const char *ext = strrchr(s, '.');
				if (ext && (!stricmp(ext, ".psk") || !stricmp(ext, ".pskx")))
					frame->ImportMesh(s);
			}

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
	 * Also note: exceptions in wxTimer event handler not catched (even assert's !)
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
			// free allocated animation resources
			if (EditorMesh) delete EditorMesh;
			if (EditorAnim) delete EditorAnim;
			if (MeshInst)   delete MeshInst;
			return result;
			unguard;
		}
		catch (...)
		{
			// display error message
			GLogWindow->Show(true);
			DisplayError();
			// emergency save edited data
			bool savedMesh = false, savedAnim = false;
			if (EditorMesh)
			{
				try
				{
					appNotify("Saving mesh to autosave");
					CFile Ar("autosave."MESH_EXTENSION, false);
					SerializeObject(EditorMesh, Ar);
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
					SerializeObject(EditorAnim, Ar);
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
