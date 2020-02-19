#include <wx/wx.h>

#include "GLCanvas.h"

#include "GlViewport.h"
#include "Gizmo.h"


GLCanvas::GLCanvas(wxWindow *parent)
:	wxGLCanvas(parent, wxID_ANY, NULL, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER)
,	m_mouseLeft(false)
,	m_mouseRight(false)
,	m_mouseMid(false)
,	m_context(NULL)
{}


GLCanvas::~GLCanvas()
{
	delete m_context;
}


void GLCanvas::Render()
{}


void GLCanvas::CreateContext()
{
	if (!m_context)
	{
		m_context = new wxGLContext(this);	// destruction of current context is done in ~wxGLCanvas
		SetCurrent(*m_context);
		GL::OnResize(GetSize().GetWidth(), GetSize().GetHeight());
		GL::ResetView();
	}
}


void GLCanvas::OnPaint(wxPaintEvent &event)
{
	CreateContext();
	// The following line is required; strange bugs without it (messageboxes not works,
	// app cannot exit etc)
	// This is a dummy, to avoid an endless succession of paint messages.
	// OnPaint handlers must always create a wxPaintDC.
	wxPaintDC dc(this);
	Render();
}

void GLCanvas::OnSize(wxSizeEvent &event)
{
	if (!IsShownOnScreen()) return;			// ignore OnSize before window creation to avoid wx asserts
	CreateContext();
	GL::OnResize(GetSize().GetWidth(), GetSize().GetHeight());
}

void GLCanvas::OnEraseBackground(wxEraseEvent &event)
{
	// empty
}

void GLCanvas::OnMouse(wxMouseEvent &event)
{
	bool prevMouseDown = m_mouseLeft | m_mouseRight | m_mouseMid;

	// update button states
	bool left  = event.LeftIsDown();
	bool right = event.RightIsDown();
	bool mid   = event.MiddleIsDown();

	int deltaX = event.m_x - m_mouseX;
	int deltaY = event.m_y - m_mouseY;

	// ignore mouse movement when button state changed
	if (left != m_mouseLeft || right != m_mouseRight || mid != m_mouseMid)
		deltaX = deltaY = 0;

	// mouse hook
	bool process = !ProcessMouse(left, right, event.m_x, event.m_y, deltaX, deltaY);

	// rotate/move camera
	if (left != m_mouseLeft)
	{
		m_mouseLeft = left;
		if (process) GL::OnMouseButton(left, MOUSE_LEFT);
	}
	if (right != m_mouseRight)
	{
		m_mouseRight = right;
		if (process) GL::OnMouseButton(right, MOUSE_RIGHT);
	}
	if (mid != m_mouseMid)
	{
		m_mouseMid = mid;
		if (process) GL::OnMouseButton(mid, MOUSE_MIDDLE);
	}
	bool mouseDown = m_mouseLeft | m_mouseRight | m_mouseMid;

	// process mouse position
	if (!prevMouseDown)
	{
		m_mouseX = event.m_x;
		m_mouseY = event.m_y;
	}
	else if (mouseDown)
	{
		// dragging with one of mouse buttons
		// set mouse cursor position to last non-draw position
		WarpPointer(m_mouseX, m_mouseY);
	}
	// show/hide cursor
	/* Win32 has some bugs with changing cursor:
	 * 1) wxCURSOR_BLANK does not work
	 * 2) changing cursor may be delayed by system until mouse move
	 *    (simple mouse_down+mouse_up will not change pointer appearance)
	 * Including wx/msw/wx.rc (contains blank cursor resource) into build
	 * will help, but I don't like it.
	 */
	if (!prevMouseDown && mouseDown)
	{
		CaptureMouse();
#if !_WIN32
		SetCursor(wxCURSOR_BLANK);
#else
		::ShowCursor(FALSE);
#endif
	}
	else if (prevMouseDown && !mouseDown)
	{
		ReleaseMouse();
#if !_WIN32
		SetCursor(wxNullCursor);
#else
		::ShowCursor(TRUE);
#endif
	}

	// update movement
	if (process) GL::OnMouseMove(deltaX, deltaY);

	//?? if OnIdle() viewport update disabled, may call Refresh() here
	//?? when view origin changed
}

void GLCanvas::OnMouseLost(wxMouseCaptureLostEvent&)
{
	// don't know, what to do here ...
}

void GLCanvas::OnIdle(wxIdleEvent &event)
{
	//?? keep viewport constantly updating; check for other way?
	//?? update when animation launched or view rotated only
#if 0
	Refresh(false);				// render via messaging system
#else
	Render();					// directly render scene
	event.RequestMore();		// without this, only single OnIdle() will be generated
#endif
}


BEGIN_EVENT_TABLE(GLCanvas, wxGLCanvas)
	EVT_SIZE(GLCanvas::OnSize)
	EVT_PAINT(GLCanvas::OnPaint)
	EVT_ERASE_BACKGROUND(GLCanvas::OnEraseBackground)
	EVT_MOUSE_EVENTS(GLCanvas::OnMouse)
	EVT_MOUSE_CAPTURE_LOST(GLCanvas::OnMouseLost)
	EVT_IDLE(GLCanvas::OnIdle)
END_EVENT_TABLE()
