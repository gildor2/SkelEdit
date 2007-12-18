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
	inline void DetachObject()
	{
		AttachObject(NULL, NULL);
	}

	/**
	 *	Index of container property colour
	 */
	byte	m_ContOnlyColIndex;
};
