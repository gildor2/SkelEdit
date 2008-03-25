#ifndef __LOGWINDOW_H__
#define __LOGWINDOW_H__


#define MAX_LOG_LINE		256

// declare message type and handler for "log frame is closed" event
BEGIN_DECLARE_EVENT_TYPES()
	DECLARE_LOCAL_EVENT_TYPE(wxEVT_LOG_CLOSED, 2000)
END_DECLARE_EVENT_TYPES()

#define EVT_LOG_CLOSED(id, fn)			\
	DECLARE_EVENT_TABLE_ENTRY(			\
		wxEVT_LOG_CLOSED, id, wxID_ANY,	\
		(wxObjectEventFunction)(wxEventFunction) wxStaticCastEvent( wxCommandEventFunction, &fn ), \
		(wxObject *) NULL				\
	),


class WLogWindow : public wxLogWindow, public COutputDevice
{
public:
	wxFrame		*pParent;
	char		buf[MAX_LOG_LINE];
	int			cursor;

	WLogWindow(wxFrame *parent);

	/**
	 *	COutputDevice methods
	 */
	virtual void Write(const char *str);

	/**
	 *	wxLogWindow methods
	 */
	virtual bool OnFrameClose(wxFrame *frame);
	virtual void OnFrameDelete(wxFrame *frame);

	/**
	 *	Settings support
	 */
	void CollectSettings();
	void ApplySettings();
};


#endif // __LOGWINDOW_H__
