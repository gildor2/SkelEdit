#ifndef __PROPEDIT_H__
#define __PROPEDIT_H__


class WPropEdit : public wxPropertyGrid
{
public:
	WPropEdit(wxWindow *parent, wxWindowID id = wxID_ANY,
			  const wxPoint &pos = wxDefaultPosition, const wxSize &size = wxDefaultSize,
			  long style = wxPG_DEFAULT_STYLE, const wxChar *name = wxPropertyGridNameStr);

	void AppendProperty(wxPGPropertyWithChildren *Parent, const CProperty *Prop, wxPGProperty *wxProp);
	void MarkContainerProp(wxPGProperty *Prop);
	void MarkReadonly(wxPGProperty *Prop);
	void PopulateStruct(wxPGPropertyWithChildren *Parent, const CStruct *Type, void *Data);
	void PopulateProp(wxPGPropertyWithChildren *Parent, const CProperty *Prop, void *Data, const char *PropName);

	void AttachObject(CStruct *Type, void *Data);
	void AttachObject(CObject *Object, CStruct *Type);
	inline void DetachObject()
	{
		AttachObject(NULL, (void*)NULL);
	}

	void OnPropertyChange(wxPropertyGridEvent &event);

	/**
	 *	When set, CObject::UpdateProperties() will be called after property modification
	 */
	CObject*	m_EditObject;
	/**
	 *	Index of container property colour
	 */
	byte		m_ContOnlyColIndex;
};


#endif // __PROPEDIT_H__
