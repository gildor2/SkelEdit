#ifndef __APPSETTINGS_H__
#define __APPSETTINGS_H__


#define CONFIG_FILE			"config.cfg"

extern CAppSettings GCfg;

void PutFramePos(wxTopLevelWindow *Frame, CWndPos &Pos);
void SetFramePos(wxTopLevelWindow *Frame, CWndPos &Pos);

void SaveSettings();
bool LoadSettings();
void EditSettings(wxFrame *parent);


#endif // __APPSETTINGS_H__
