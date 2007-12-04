#ifndef __GLWINDOW_H__
#define __GLWINDOW_H__


#include "Core.h"


#define MOUSE_BUTTON(n)		(1 << (n))
#define MOUSE_LEFT			0
#define MOUSE_RIGHT			1


namespace GL
{
	void text(const char *text);
	void textf(const char *fmt, ...);
	void OnResize(int w, int h);
	void Set2Dmode(bool force = false);
	void BuildMatrices();
	void Set3Dmode();
	void SetDistScale(float scale);
	void SetViewOffset(const CVec3 &offset);
	void ResetView();
	void OnMouseButton(bool down, int button);
	void OnMouseMove(int x, int y);

	extern bool invertXAxis;
};

#define SPEC_KEY(x)		(SDLK_##x)


#endif // __GLWINDOW_H__
