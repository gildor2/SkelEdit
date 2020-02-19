#ifndef __PROPEDIT_H__
#define __PROPEDIT_H__


class WPropEdit : public wxPropertyGrid
{
	friend struct CPropLink;

public:
	WPropEdit(wxWindow *parent, wxWindowID id = wxID_ANY,
			  const wxPoint &pos = wxDefaultPosition, const wxSize &size = wxDefaultSize,
			  long style = wxPG_DEFAULT_STYLE, const wxString &name = wxPropertyGridNameStr);
	~WPropEdit();

	/**
	 *	Attach data to edit in property grid
	 */
	void AttachObject(const CStruct *Type, void *Data);
	void AttachObject(CObject *Object);
	/**
	 *	Detach data
	 */
	inline void DetachObject();
	/**
	 *	Update grid data for specified property and all its subproperties. 'type'
	 *	should be specified for m_rootProp only, because there is no CProperty for
	 *	structure itself.
	 */
	void RefreshProperty(CPropLink *link, const CType *type = NULL);
	/**
	 *	Update grid data, when object modified not by property grid
	 */
	void RefreshGrid();
	/**
	 *	This function is used to queue asynchronous command when synchronous execution
	 *	is not possible due to limited wxPropertyGrid architecture
	 */
	void AsyncCommand(int cmd, CPropLink *link);
	/**
	 *	Some property manipulations using its name.
	 *	Note: name may be complex, for example: "Bones.[2].Name"
	 */
	bool SelectByName(const char *Name, bool ShouldExpand = false);
	wxString GetSelectionName() const;
	/**
	 *	Verify property selection. If property is selected, is array item, and 'index' is
	 *	not NULL, then '*index' will contain index of selected child property (or -1, if
	 *	concrete array item is not selected).
	 *	For example, you can call this function to determine, whether 'Bones' property is
	 *	selected, and if selected - retreive index of selected bone.
	 */
	bool IsPropertySelected(const char *Name, int *index = NULL) const;
	bool RefreshByName(const char *Name);
	/**
	 *	Callback function, used to fill enumeration values for 'int' properties, marked
	 *	with comment '#ENUM <EnumName>'
	 */
	const char* (*m_enumCallback)(int Index, const char *EnumName);

protected:
	/**
	 *	When set to 'true', CObject::PostEditChange() will be called after any property
	 *	modification
	 */
	bool			m_isObject;
	/**
	 *	Typeinfo for edited object
	 */
	const CStruct	*m_typeInfo;
	/**
	 *	Container property color
	 */
	wxColour		m_contPropColor;
	/**
	 *	Memory chain, used to allocate property links
	 */
	CMemoryChain	*m_chain;
	/**
	 *	Dual-linked lists for used and free property links
	 */
	CPropLink		*m_freeLinks;
	CPropLink		*m_usedLinks;
	/**
	 *	Root CPropLink, owner of all root m_typeInfo fields
	 */
	CPropLink		*m_rootProp;
	/**
	 *	When m_isRefreshing is 'true', AppendProperty() will refresh grid data instead of
	 *	appending new fields
	 */
	bool			m_isRefreshing;

	CPropLink *AllocLink();
	void FreeLink(CPropLink *link);

	void MarkArray(wxPGProperty *Prop);
	void MarkReadonly(wxPGProperty *Prop);

	// filling (or refreshing) property grid
	CPropLink *AppendProperty(CPropLink *Parent, const CProperty *Prop, int Type, void *Data, int ArrayIndex = -1);
	void PopulateStruct(CPropLink *Parent, const CStruct *Type, void *Data);
	CPropLink *PopulateProp(CPropLink *Parent, const CProperty *Prop, void *Data, int ArrayIndex = -1);
	void PopulateArrayProp(CPropLink *Parent, const CProperty *Prop, void *Data);

	// dynamic array manipulation (will modify grid structure)
	void RemoveArrayEntry(CPropLink *entry);
	void AppendArrayEntry(CPropLink *parent);
//	void InsertArrayEntry(CPropLink *link, int index);
	void FinishArrayEditing(CArray *Arr, const CType *Type, CPropLink *parent);

	/**
	 *	Event handlers
	 */
	void OnPropertyChange(wxPropertyGridEvent &event);
	void OnTimer(wxTimerEvent &event);

	/**
	 *	Asynchronous execution for event handlers
	 */
	wxTimer			*m_timer;
	int				m_asyncCmd;
	CPropLink		*m_asyncItem;

private:
	DECLARE_EVENT_TABLE()
};


struct CPropLink
{
	/// wxWidgets property
	wxPGProperty	*wProp;
	/// owner grid of WProp
	WPropEdit		*wGrid;

	/// type infornation pointer
	const CProperty	*Prop;
	/// real data pointer
	void			*Data;
	/// index in parent array property; -1 for non-array property
	int				ArrayIndex;

	/// parent property information
	CPropLink		*Parent;
	/// children properties
	CPropLink		*Children;

	// dual-linked list
	CPropLink		*prev, *next;
	CPropLink		*nextChild;

	inline void Release()
	{
		wGrid->FreeLink(this);
	}
	bool IsArray() const
	{
		return Prop->IsArray() && ArrayIndex == -1;
	}
	bool IsArrayItem() const
	{
		return Prop->IsArray() && ArrayIndex != -1;
	}
};


#endif // __PROPEDIT_H__
