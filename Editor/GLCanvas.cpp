#include <wx/wx.h>

#include "GLCanvas.h"

#include "GlViewport.h"


GLCanvas::GLCanvas(wxWindow *parent)
:	wxGLCanvas(parent, wxID_ANY, NULL, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER)
,	mMouseLeft(false)
,	mMouseRight(false)
,	mMouseMid(false)
{
	mContext = new wxGLContext(this);	// destruction of current context is done in ~wxGLCanvas
	SetCurrent(*mContext);
	GL::ResetView();
}

void GLCanvas::Render()
{}


void GLCanvas::OnPaint(wxPaintEvent &event)
{
	// The following line is required; strange bugs without it (messageboxes not works,
	// app cannot exit etc)
	// This is a dummy, to avoid an endless succession of paint messages.
	// OnPaint handlers must always create a wxPaintDC.
	wxPaintDC dc(this);
	Render();
}

void GLCanvas::OnSize(wxSizeEvent &event)
{
	wxGLCanvas::OnSize(event);
	GL::OnResize(event.GetSize().GetWidth(), event.GetSize().GetHeight());
}

void GLCanvas::OnEraseBackground(wxEraseEvent &event)
{
	// empty
}

void GLCanvas::OnMouse(wxMouseEvent &event)
{
	bool prevMouseDown = mMouseLeft | mMouseRight | mMouseMid;

	// update button states
	bool left  = event.LeftIsDown();
	bool right = event.RightIsDown();
	bool mid   = event.MiddleIsDown();
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
	if (mid != mMouseMid)
	{
		mMouseMid = mid;
		GL::OnMouseButton(mid, MOUSE_MIDDLE);
	}
	bool mouseDown = mMouseLeft | mMouseRight | mMouseMid;

	// process mouse position
	if (!prevMouseDown)
	{
		mMouseX = event.m_x;
		mMouseY = event.m_y;
	}
	else if (mouseDown)
	{
		// dragging with one of mouse buttons
		// set mouse cursor position to last non-draw position
		WarpPointer(mMouseX, mMouseY);
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
	GL::OnMouseMove(event.m_x - mMouseX, event.m_y - mMouseY);
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
