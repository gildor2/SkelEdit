#ifndef __GLWINDOW_H__
#define __GLWINDOW_H__


#if _WIN32
#	ifndef APIENTRY
#		define APIENTRY __stdcall
#	endif
#	ifndef WINGDIAPI
#		define WINGDIAPI
		typedef unsigned		HDC;
		typedef unsigned		HGLRC;
		typedef const char *	LPCSTR;
		typedef int				BOOL;
		typedef unsigned char	BYTE;
		typedef unsigned short	WORD;
		typedef unsigned int	UINT;
		typedef int (APIENTRY *PROC)();
		typedef void PIXELFORMATDESCRIPTOR;		// structure
		typedef PIXELFORMATDESCRIPTOR * LPPIXELFORMATDESCRIPTOR;
#	endif // WINGDIAPI
#	ifndef CONST
#		define CONST const
#	endif
#endif

#include <GL/gl.h>


#include "Core.h"


#define MOUSE_BUTTON(n)		(1 << (n))

#define MOUSE_LEFT			0
#define MOUSE_RIGHT			1
#define MOUSE_MIDDLE		2

#undef RGB

// constant colors
#define RGBA(r,g,b,a)		( (unsigned)((r)*255) | ((unsigned)((g)*255)<<8) | ((unsigned)((b)*255)<<16) | ((unsigned)((a)*255)<<24) )
#define RGB(r,g,b)			RGBA(r,g,b,1)
#define RGB255(r,g,b)		( (unsigned) ((r) | ((g)<<8) | ((b)<<16) | (255<<24)) )
#define RGBA255(r,g,b,a)	( (r) | ((g)<<8) | ((b)<<16) | ((a)<<24) )

// computed colors
//?? make as methods; or - constructor of CColor
#define RGBAS(r,g,b,a)		(appRound((r)*255) | (appRound((g)*255)<<8) | (appRound((b)*255)<<16) | (appRound((a)*255)<<24))
#define RGBS(r,g,b)			(appRound((r)*255) | (appRound((g)*255)<<8) | (appRound((b)*255)<<16) | (255<<24))


namespace GL
{
	void Set2Dmode(bool force = false);
	void Set3Dmode();
	void BuildMatrices();
	void SetDistScale(float scale);
	void SetViewOffset(const CVec3 &offset);
	void ResetView();
	void OnResize(int w, int h);
	void OnMouseButton(bool down, int button);
	void OnMouseMove(int dx, int dy);

	extern int   width;
	extern int   height;
	extern CVec3 viewOrigin;
	extern CAxis viewAxis;
	extern bool  invertXAxis;
};


void DrawTextLeft(const char *text, ...);
void DrawTextRight(const char *text, ...);
void DrawTextPos(int x, int y, const char *text);
void DrawText3D(const CVec3 &pos, const char *text, ...);
void FlushTexts();

// Project 3D point to screen coordinates; return false when not in view frustum
bool ProjectToScreen(const CVec3 &pos, float scr[2]);
bool ProjectToScreen(const CVec3 &pos, int scr[2]);


// GlTexture.cpp functions
int GL_LoadTexture(const char *name);


#endif // __GLWINDOW_H__
