#ifndef __GLCANVAS_H__
#define __GLCANVAS_H__


#include <wx/glcanvas.h>		// wxGLCanvas


class GLCanvas : public wxGLCanvas
{
public:
	GLCanvas(wxWindow *parent);
	virtual void Render();

protected:
	void OnPaint(wxPaintEvent &event);
	void OnSize(wxSizeEvent &event);
	void OnEraseBackground(wxEraseEvent &event);
	void OnMouse(wxMouseEvent &event);
	void OnMouseLost(wxMouseCaptureLostEvent&);
	void OnIdle(wxIdleEvent &event);

private:
	wxGLContext		*mContext;
	long			mMouseX, mMouseY;
	bool			mMouseLeft, mMouseRight, mMouseMid;

	DECLARE_EVENT_TABLE()
};


#endif // __GLCANVAS_H__
