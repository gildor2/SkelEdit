#ifndef __GLCANVAS_H__
#define __GLCANVAS_H__


#include <wx/glcanvas.h>		// wxGLCanvas


class GLCanvas : public wxGLCanvas
{
public:
	GLCanvas(wxWindow *parent);
	virtual ~GLCanvas();
	virtual void Render();

protected:
	void OnPaint(wxPaintEvent &event);
	void OnSize(wxSizeEvent &event);
	void OnEraseBackground(wxEraseEvent &event);
	void OnMouse(wxMouseEvent &event);
	void OnMouseLost(wxMouseCaptureLostEvent&);
	void OnIdle(wxIdleEvent &event);

	/**
	 *	Hook function to process mouse events in derived classes. Function should
	 *	return 'true' to prevent from further mouse processing.
	 */
	virtual bool ProcessMouse(bool leftDown, bool rightDown, int mouseX, int mouseY, int deltaX, int deltaY)
	{
		return false;
	}

private:
	wxGLContext		*m_context;
	long			m_mouseX, m_mouseY;
	bool			m_mouseLeft, m_mouseRight, m_mouseMid;

	void CreateContext();

	DECLARE_EVENT_TABLE()
};


#endif // __GLCANVAS_H__
