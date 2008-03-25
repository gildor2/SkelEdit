/*-----------------------------------------------------------------------------
	Log window support
-----------------------------------------------------------------------------*/

#include <wx/wx.h>
#include "Core.h"

#include "LogWindow.h"

#include "EditorClasses.h"
#include "AppSettings.h"


DEFINE_EVENT_TYPE(wxEVT_LOG_CLOSED)


WLogWindow::WLogWindow(wxFrame *parent)
:	wxLogWindow(parent, "System Log", false)
,	pParent(parent)
,	cursor(0)
{
	// register logger in wxWidgets log chain and Core appPrintf() chain
	wxLog::SetActiveTarget(this);
	COutputDevice::Register();
}


void WLogWindow::OnFrameDelete(wxFrame *frame)
{
	// unregister logger
	wxLog::SetActiveTarget(NULL);
	COutputDevice::Unregister();
	// save window position etc
	CollectSettings();
}


void WLogWindow::Write(const char *str)
{
	// wxLog will automatically add "\n" in every call to wxLogMessage(),
	// so we need to accumulate message to allow printing single line within
	// a few appPrintf() calls
	while (true)
	{
		char c = *str++;
		if (!c) break;
		if (c == '\n' || cursor >= MAX_LOG_LINE-1)
		{
			// wxLog will automatically append "\n", so skip it
			if (cursor)
			{
				buf[cursor] = 0;
				wxLogMessage("%s", buf);
				cursor = 0;
			}
			if (c == '\n')
				continue;
		}
		buf[cursor++] = c;
	}
}


bool WLogWindow::OnFrameClose(wxFrame *frame)
{
	// send message to main frame about closing log frame
	wxCommandEvent event(wxEVT_LOG_CLOSED);
	wxPostEvent(pParent, event);
	return true;
}


void WLogWindow::CollectSettings()
{
	PutFramePos(GetFrame(), GCfg.LogFramePos);
}


void WLogWindow::ApplySettings()
{
	SetFramePos(GetFrame(), GCfg.LogFramePos);
}
