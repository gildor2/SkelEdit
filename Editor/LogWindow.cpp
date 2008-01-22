/*-----------------------------------------------------------------------------
	Log window support
-----------------------------------------------------------------------------*/

#include <wx/wx.h>
#include "Core.h"

#include "LogWindow.h"

#include "EditorClasses.h"
#include "AppSettings.h"


DEFINE_EVENT_TYPE(wxEVT_LOG_CLOSED)


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


void WLogWindow::OnFrameDelete(wxFrame *frame)
{
	CollectSettings();
}


void WLogWindow::CollectSettings()
{
	PutFramePos(GetFrame(), GCfg.LogFramePos);
}


void WLogWindow::ApplySettings()
{
	SetFramePos(GetFrame(), GCfg.LogFramePos);
}
