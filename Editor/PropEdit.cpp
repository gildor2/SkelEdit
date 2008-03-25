#include <wx/wx.h>
#include <wx/propgrid/propgrid.h>
#include <wx/propgrid/advprops.h>		// wxColourProperty etc
#include <wx/filesys.h>					// for wxFileSystem

#include "Core.h"
#include "PropEdit.h"


#define MAX_STRUC_PROPS		1024		// in-stack array limit

//#define COLORIZE			1

#define CHECK_PROP_LINKS	1
//#define DEBUG_ARRAYS		1

//!! following part is copied from MainApp.cpp, should share in .h
#define ZIP_RESOURCES		"resource.bin"

#ifdef ZIP_RESOURCES
#	define RES_NAME(name)	ZIP_RESOURCES"#zip:" name
#else
#	define RES_NAME(name)	"xrc/" name
#endif
//!!^^^^^^^^^^^^^^^


#define DATA_REFRESH		"refresh"
#define DATA_GETLINK		"proplink"


// Asynchronous commands
enum
{
	ACMD_NULL,				// nop
	ACMD_DELETE,			// remove array item
	ACMD_APPEND				// append new array item
};


// bitmaps
static wxBitmap *bmpPropAdd    = NULL;
static wxBitmap *bmpPropRemove = NULL;

// colours
static wxColour propColourReadonly;


/*-----------------------------------------------------------------------------
	Miscellaneous helpers
-----------------------------------------------------------------------------*/

static wxColour AddColour(wxColour base, int r, int g, int b)
{
	int R, G, B;
	R = base.Red()   + r;
	G = base.Green() + g;
	B = base.Blue()  + b;
	return wxColour(bound(R, 0, 255), bound(G, 0, 255), bound(B, 0, 255));
}


/**
 *	Hack to get access from WPropEdit to protected wxPGProperty fields
 */
class wxPGPropAccessHack : public wxPGProperty
{
	friend WPropEdit;
};
#define PROP_UNHIDE(Prop,Field)	( ((wxPGPropAccessHack*)Prop)->Field )


static CPropLink *GetPropLink(wxPGProperty *prop)
{
	assert(prop);
	CPropLink *link = (CPropLink*)prop->DoGetAttribute(DATA_GETLINK).GetVoidPtr();
	assert(link);
	assert(link->wProp == prop);
	return link;
}


/*-----------------------------------------------------------------------------
	Special editor class "NoEditor" - will disable editor control for field
-----------------------------------------------------------------------------*/

class WNoEditor : public wxPGTextCtrlEditor
{
	WX_PG_DECLARE_EDITOR_CLASS(WNoEditor)
public:
	WNoEditor()
	{}
	virtual ~WNoEditor()
	{}

	wxPG_DECLARE_CREATECONTROLS
};

WX_PG_IMPLEMENT_EDITOR_CLASS(NoEditor, WNoEditor, wxPGTextCtrlEditor)

wxPGWindowList WNoEditor::CreateControls(wxPropertyGrid *grid, wxPGProperty *prop,
	const wxPoint &pos, const wxSize &sz) const
{
	return (wxWindow*)NULL;
}


/*-----------------------------------------------------------------------------
	Dynamic array editor
-----------------------------------------------------------------------------*/

class WDynArrayEditor : public wxPGTextCtrlEditor
{
	WX_PG_DECLARE_EDITOR_CLASS(WDynArrayEditor)

public:
	WDynArrayEditor()
	{}
	virtual ~WDynArrayEditor()
	{}

	virtual wxPGWindowList CreateControls(wxPropertyGrid *grid, wxPGProperty *prop, const wxPoint &pos, const wxSize &sz) const
	{
		guard(WDynArrayEditor::CreateControls);
		CPropLink *link = GetPropLink(prop);		//?? not needed?
		// create buttons
		wxPGMultiButton *buttons = new wxPGMultiButton(grid, sz);
		if (link->IsArray())
		{
			buttons->Add(*bmpPropAdd);
			buttons->GetButton(0)->SetToolTip("Append new entry");
		}
		else if (link->IsArrayItem())
		{
			buttons->Add(*bmpPropRemove);
			buttons->GetButton(0)->SetToolTip("Remove array entry");
		}
		buttons->FinalizePosition(pos);
		// attach to property editor
		wxPGWindowList wndList;
		wndList.SetSecondary(buttons);
		return wndList;
		unguard;
	}

	virtual bool OnEvent(wxPropertyGrid *propGrid, wxPGProperty *prop, wxWindow *ctrl, wxEvent &event) const
	{
		guard(WDynArrayEditor::OnEvent);

		if (event.GetEventType() == wxEVT_COMMAND_BUTTON_CLICKED)
		{
			CPropLink *link = GetPropLink(prop);
			WPropEdit *grid = static_cast<WPropEdit*>(propGrid);
			// NOTE: all operations are performed asynchronously, because wxPropertyGrid has
			// not capabilities to change property structure while handling event

			wxPGMultiButton *buttons = (wxPGMultiButton*) propGrid->GetEditorControlSecondary();
			if (event.GetId() == buttons->GetButtonId(0))
			{
				// remove item asynchronously
				if (link->IsArray())
				{
					grid->AsyncCommand(ACMD_APPEND, link);
				}
				else if (link->IsArrayItem())
				{
					grid->AsyncCommand(ACMD_DELETE, link);
				}
				return false;
			}
		}

		return wxPGTextCtrlEditor::OnEvent(propGrid, prop, ctrl, event);
		unguard;
	}
};

WX_PG_IMPLEMENT_EDITOR_CLASS(ArrayEntry, WDynArrayEditor, wxPGTextCtrlEditor)


/*-----------------------------------------------------------------------------
	Special property types for wxPropertyGrid
-----------------------------------------------------------------------------*/

#define COMMON_NOCONSTRUC(TypeName,Parent)		\
	typedef Parent Super;						\
protected:										\
	CPropLink	*mInfo;							\
public:											\
	virtual ~TypeName()							\
	{											\
		mInfo->Release();						\
	}											\
	virtual wxVariant DoGetAttribute(const wxString &name) const \
	{											\
		if (name == DATA_GETLINK)				\
			return wxVariant((void*)mInfo);		\
		return Super::DoGetAttribute(name);		\
	}

#define COMMON_FIELDS(TypeName,Parent,Init)		\
	COMMON_NOCONSTRUC(TypeName,Parent)			\
public:											\
	TypeName(const char *Name, CPropLink *link)	\
	:	Parent Init								\
	,	mInfo(link)								\
	{}											\


#define SIMPLE_PROPERTY(ThisClass,SuperClass,DataType,VariantType) \
class ThisClass : public SuperClass				\
{												\
	COMMON_FIELDS(ThisClass, SuperClass, (Name, wxPG_LABEL, 0)) \
public:											\
	virtual void OnSetValue()					\
	{											\
		Super::OnSetValue();					\
		*(DataType*)mInfo->Data = m_value.Get##VariantType(); \
	}											\
	virtual bool DoSetAttribute(const wxString &name, wxVariant &value) \
	{											\
		if (name == DATA_REFRESH)				\
		{										\
			SetValue(*(DataType*)mInfo->Data);	\
			return false;						\
		}										\
		return Super::DoSetAttribute(name, value); \
	}											\
};

// NOTE: we use GetValueAsString() instead of m_value.GetString() to allow additional
//	preprocessing for field (to exactly match used value to displayed one)
#define STRING_HANDLERS							\
public:											\
	virtual void OnSetValue()					\
	{											\
		Super::OnSetValue();					\
/*		appStrncpyz((char*)mInfo->Data, m_value.GetString().c_str(), mInfo->Prop->ArrayDim); */ \
		appStrncpyz((char*)mInfo->Data, GetValueAsString(0).c_str(), mInfo->Prop->ArrayDim); \
	}											\
	virtual bool DoSetAttribute(const wxString &name, wxVariant &value) \
	{											\
		if (name == DATA_REFRESH)				\
		{										\
			SetValue((char*)mInfo->Data);		\
			return false;						\
		}										\
		return Super::DoSetAttribute(name, value); \
	}


//!! add spins to all numeric props
//!! byte, short: limit range
//!! struc properties: should display prop text similar to (X=1,Y=0,Z=0) for some types, or "..." for all other
SIMPLE_PROPERTY(WIntProperty,   wxIntProperty,   int,   Long  )
SIMPLE_PROPERTY(WShortProperty, wxIntProperty,   short, Long  )
SIMPLE_PROPERTY(WUIntProperty,  wxUIntProperty,  int,   Long  )
SIMPLE_PROPERTY(WFloatProperty, wxFloatProperty, float, Double)
SIMPLE_PROPERTY(WBoolProperty,  wxBoolProperty,  bool,  Bool  )


class WEnumProperty : public wxEnumProperty
{
	COMMON_FIELDS(WEnumProperty, wxEnumProperty, (Name, wxPG_LABEL, NULL))
public:
	virtual void OnSetValue()
	{
		Super::OnSetValue();
		*(byte*)mInfo->Data = m_value.GetLong();
	}
	virtual bool DoSetAttribute(const wxString &name, wxVariant &value)
	{
		if (name == DATA_REFRESH)
		{
			// fill dropdown list with enumeration constants
			assert(mInfo->Prop->TypeInfo->IsEnum);
			const CEnum * Type = (CEnum*)mInfo->Prop->TypeInfo;
			m_choices.AssignData(wxPGChoicesEmptyData);
			for (int i = 0; i < Type->Names.Num(); i++)
				m_choices.Add(Type->Names[i]);
			// update value
			SetIndex(*(byte*)mInfo->Data);
			return false;
		}
		return Super::DoSetAttribute(name, value);
	}
};


class WIntEnumProperty : public wxEnumProperty
{
	COMMON_FIELDS(WIntEnumProperty, wxEnumProperty, (Name, wxPG_LABEL, NULL))
public:
	virtual void OnSetValue()
	{
		Super::OnSetValue();
		*(int*)mInfo->Data = m_value.GetLong();
	}
	virtual bool DoSetAttribute(const wxString &name, wxVariant &value)
	{
		if (!mInfo->wGrid->m_enumCallback)
			appError("WIntEnumProperty: EnumCallback is not set");
		if (name == DATA_REFRESH)
		{
			// fill dropdown list with enumeration constants
			const char *str = strchr(mInfo->Prop->Comment, ' ');
			if (!str)
				appError("WIntEnumProperty: bad comment format for '%s'", mInfo->Prop->Name);
			while (*str == ' ')
				str++;								// skip spaces
			TString<256> EnumName; EnumName = str;
			if (char *cut = EnumName.chr('\n'))		// cut rest of comment
				*cut = 0;

			m_choices.AssignData(wxPGChoicesEmptyData);
			for (int i = 0; /* empty */; i++)
			{
				str = mInfo->wGrid->m_enumCallback(i, EnumName);
				if (!str) break;					// all items has been added
				m_choices.Add(str);
			}
			// update value
			SetIndex(*(int*)mInfo->Data);
			return false;
		}
		return Super::DoSetAttribute(name, value);
	}
};


class WStringProperty : public wxStringProperty
{
	COMMON_FIELDS(WStringProperty, wxStringProperty, (Name, wxPG_LABEL))
	STRING_HANDLERS
};


// WDirProperty: similar to WStringProperty, but different base class
class WDirProperty : public wxDirProperty
{
	COMMON_FIELDS(WDirProperty, wxDirProperty, (Name, wxPG_LABEL))
	STRING_HANDLERS
};


// WFileProperty: similar to WStringProperty, but different base class
class WFileProperty : public wxFileProperty
{
	COMMON_FIELDS(WFileProperty, wxFileProperty, (Name, wxPG_LABEL))
	STRING_HANDLERS
	/*
	 * wxFileProperty (or, more likely, wxFileName, used by wxFileProperty)
	 * has a bug: when relative filename edited by hands (not using file
	 * selector dialog), it becomes relative to working directory, not to
	 * specified with wxPG_FILE_SHOW_RELATIVE_PATH option directory.
	 * Here we trying to recover it: when non-absolute filename found, append
	 * path and update value. Another one bug appeared, when setting empty
	 * string as value - relative path to working directory will be displayed.
	 * Here is a slightly modified copy of wxFileProperty::GetValueAsString(),
	 * fixing these issues.
	 */
	virtual wxString GetValueAsString(int argFlags) const
	{
		if (argFlags & wxPG_FULL_VALUE)
		{
			return m_filename.GetFullPath();
		}
		else if (m_flags & wxPG_PROP_SHOW_FULL_FILENAME)
		{
			if (m_basePath.Length() && m_filename.IsAbsolute())
			{
				wxFileName fn2(m_filename);
				fn2.MakeRelativeTo(m_basePath);
				return fn2.GetFullPath();
			}
			return m_filename.GetFullPath();
		}

		return m_filename.GetFullName();
	}
};


class WColor3fProperty : public wxColourProperty
{
	COMMON_FIELDS(WColor3fProperty, wxColourProperty, (Name, wxPG_LABEL))
public:
	virtual void OnSetValue()
	{
		Super::OnSetValue();
		CColor3f *dst = (CColor3f*) mInfo->Data;
		const wxColour &src = GetVal().m_colour;
		dst->Set(src.Red() / 255.0f, src.Green() / 255.0f, src.Blue() / 255.0f);
	}
	virtual bool DoSetAttribute(const wxString &name, wxVariant &value)
	{
		if (name == DATA_REFRESH)
		{
			const CColor3f &src = * ((CColor3f*) mInfo->Data);
			wxColour tmp(src[0] * 255, src[1] * 255, src[2] * 255);
			Init(tmp);
			return false;
		}
		return Super::DoSetAttribute(name, value);
	}
};


class WContainerProperty : public wxCustomProperty
{
	COMMON_NOCONSTRUC(WContainerProperty, wxCustomProperty)
public:
	WContainerProperty(const char *name, CPropLink *link)
	:	wxCustomProperty(name)
	,	mInfo(link)
	{
		SetValue("...");					//?? hook GetValueAsString(), and do not use SetValue()
		SetFlag(wxPG_PROP_MODIFIED | wxPG_PROP_COLLAPSED);
	}
	virtual const wxPGEditor *DoGetEditorClass() const
	{
		return wxPG_EDITOR(NoEditor);
	}
};


/*-----------------------------------------------------------------------------
	Filling property grid using typeinfo information
-----------------------------------------------------------------------------*/

// Property types
enum
{
	PT_UNKNOWN,
	PT_CONTAINER,
	PT_ENUM,
	PT_BOOL,
	PT_SHORT,
	PT_INT,
	PT_UINT,
	PT_FLOAT,
	PT_INT_ENUM,
	PT_STRING,
	PT_DIRNAME,
	PT_FILENAME,
	PT_COLOR3F
};


CPropLink *WPropEdit::AppendProperty(CPropLink *Parent, const CProperty *Prop, int Type, void *Data, int ArrayIndex)
{
	guard(WPropEdit::AppendProperty);

	assert(Parent);
	assert(Prop);
	assert(Data);

	const char *Name;
	char nameBuf[32];
	if (ArrayIndex == -1)
	{
		Name = Prop->Name;
	}
	else
	{
		appSprintf(ARRAY_ARG(nameBuf), "[%d]", ArrayIndex);
		Name = nameBuf;
	}

	wxPGProperty *P;
	CPropLink    *link;

	if (m_isRefreshing)
	{
		// search appropriate CPropLink in Parent->Children
		link = NULL;
		for (link = Parent->Children; link; link = link->nextChild)
			if (link->Prop == Prop && link->ArrayIndex == ArrayIndex)
				break;
		assert(link);
		P = link->wProp;
	}
	else
	{
		// create new CPropLink and wxPGProperty
		link = AllocLink();
		link->Parent     = Parent;
		link->Prop       = Prop;
		link->ArrayIndex = ArrayIndex;
		// insert last into Parent->Children -->> nextChild list
		// note: Parent is always non-null, because has m_rootProp
		if (Parent->Children)
		{
			CPropLink *p;
			for (p = Parent->Children; p->nextChild; p = p->nextChild)
			{ /* empty */ }
			p->nextChild = link;
		}
		else
			Parent->Children = link;	// this is a first children

		switch (Type)
		{
#define M(t1, t2)	case t1: P = new t2(Name, link); break;
			M(PT_CONTAINER,	WContainerProperty)
			M(PT_ENUM,		WEnumProperty)
			M(PT_BOOL,		WBoolProperty)
			M(PT_SHORT,		WShortProperty)
			M(PT_INT,		WIntProperty)
			M(PT_UINT,		WUIntProperty)
			M(PT_FLOAT,		WFloatProperty)
			M(PT_INT_ENUM,	WIntEnumProperty)
			M(PT_STRING,	WStringProperty)
			M(PT_FILENAME,	WFileProperty)
			M(PT_DIRNAME,	WDirProperty)
			M(PT_COLOR3F,	WColor3fProperty)
#undef M
		default: // PT_UNKNOWN
			P = new wxStringProperty(Name, wxPG_LABEL, Prop->TypeInfo->TypeName);
			appNotify("AppendProperty: unknown type %s for %s", Prop->TypeInfo->TypeName, Prop->Name);
		}
		link->wProp = P;
		// insert into property grid
		if (Parent == m_rootProp)
		{
			// top level
			if (Prop->Category)
				Append(new wxPropertyCategory(Prop->Category));
			Append(P);
		}
		else
		{
			// append property to parent
			// note: there is no correct support for sub-categories
			AppendIn(Parent->wProp->GetId(), P);
		}
		// Set property background colour.
		// Note: doing this after appending to grid, because Append() will set
		// property background colour to parent property colour.
		// Index '0' is taken from wxPropertyGrid::SetCellBackgroundColour() - this
		// function updates m_arrBgBrushes.Item(0)
		PROP_UNHIDE(P, m_bgColIndex) = Type == PT_CONTAINER ? m_contPropColIndex : 0;
		// setup property comment
		if (Prop->Comment)
		{
			const char *cmt = Prop->Comment;
			if (Prop->Comment[0] == '#')
			{
				// extended property information
				cmt = strchr(cmt, '\n');
				if (cmt) cmt++;
			}
			if (cmt && cmt[0])
				P->SetHelpString(cmt);
		}
	}

	// refresh (initialize) grid value
	link->Data = Data;
	P->SetAttribute(DATA_REFRESH, 0);
	return link;

	unguard;
}


/*?? Notes:
 * 1) to support array<array<>> requires to combine buttons of array itself and array entry in
 *    a single property (currently such syntax construction is not possible)
 * 2) array of type, which has own buttons (filename etc) - should combine buttons too
 * Button combination may be performed by using single wxPG_EDITOR class for all situations,
 * which will automatically assing different buttons in different cases.
 */

void WPropEdit::MarkArray(wxPGProperty *Prop)
{
	Prop->SetEditor(wxPG_EDITOR(ArrayEntry));
}


void WPropEdit::MarkReadonly(wxPGProperty *Prop)
{
	Prop->SetEditor(wxPG_EDITOR(NoEditor));
	SetPropertyBackgroundColour(Prop->GetId(), propColourReadonly);
	for (int i = 0; i < Prop->GetChildCount(); i++)
		MarkReadonly(Prop->Item(i));		// recurse to children
}


CPropLink *WPropEdit::PopulateProp(CPropLink *Parent, const CProperty *Prop, void *Data, int ArrayIndex)
{
	guard(WPropEdit::PopulateProp);

	CPropLink *link = NULL;
	const CType *Type = Prop->TypeInfo;
	assert(Type);

	struct PropTypes
	{
		const char	*Name;
		int			Index;
	};
	static const PropTypes KnownTypes[] =
	{
		{ "short",		PT_SHORT	},
		{ "int",		PT_INT		},
		{ "float",		PT_FLOAT	},
		{ "bool",		PT_BOOL		},
		{ "string",		PT_STRING	},
		{ "Color3f",	PT_COLOR3F	}
	};

	int TypeIndex = PT_UNKNOWN;
	for (int i = 0; i < ARRAY_COUNT(KnownTypes); i++)
		if (!strcmp(Type->TypeName, KnownTypes[i].Name))
		{
			TypeIndex = KnownTypes[i].Index;
			break;
		}

	// choose property type and create it
	if (Type->IsStruc && TypeIndex == PT_UNKNOWN)
	{
		// embedded structure
		link = AppendProperty(Parent, Prop, PT_CONTAINER, Data, ArrayIndex);
		// note: AppendProperty() should be used BEFORE PopulateStruct() when inserting child props
		// (otherwise app will crash)
		PopulateStruct(link, (CStruct*)Prop->TypeInfo, Data);
	}
	else if (Type->IsEnum)
	{
		TypeIndex = PT_ENUM;
	}
	else if (TypeIndex == PT_STRING)
	{
		// check additional info in comment
		if (Prop->Comment && !strncmp(Prop->Comment, "#FILENAME ", 10))
		{
			// filename property
			TString<256> Title, Mask;
			Title = Prop->Comment + 10;
			char *s = Title.chr(':');
			if (!s)
			{
				appNotify("Wrong comment format for string %s", Prop->Name);
				Title = "??";
				Mask  = "*.*";
			}
			else
			{
				*s = 0;
				Mask = s+1;
				s = Mask.chr('\n');
				if (s) *s = 0;
			}
			// create and setup file property
			link = AppendProperty(Parent, Prop, PT_FILENAME, Data, ArrayIndex);
			link->wProp->SetAttribute(wxPG_FILE_WILDCARD, wxString::Format("%s Files (%s)|%s", *Title, *Mask, *Mask));
			link->wProp->SetAttribute(wxPG_FILE_DIALOG_TITLE, wxString::Format("Choose %s", *Title));
			if (GRootDir[0])
				link->wProp->SetAttribute(wxPG_FILE_SHOW_RELATIVE_PATH, *GRootDir);
			// wxProp->SetMaxLength(Prop->ArrayDim-1) -- has bug:
			//	when filename is too long, it will not be scrolled with cursor keys!
		}
		else if (Prop->Comment && !strncmp(Prop->Comment, "#DIRNAME", 8))
		{
			TypeIndex = PT_DIRNAME;
		}
		else
		{
			// ordinary string property
			link = AppendProperty(Parent, Prop, PT_STRING, Data, ArrayIndex);
			link->wProp->SetMaxLength(Prop->ArrayDim-1);			// do this after AppendProperty()!
		}
	}
	else if (TypeIndex == PT_INT && Prop->Comment && !strncmp(Prop->Comment, "#ENUM ", 6))
	{
		TypeIndex = PT_INT_ENUM;
	}

	// create simple properties
	if (!link)
	{
		// all other types
		link = AppendProperty(Parent, Prop, TypeIndex, Data, ArrayIndex);
		// setup some specific attributes
		if (TypeIndex == PT_BOOL)									// show bool prop as checkbox
			link->wProp->SetAttribute(wxPG_BOOL_USE_CHECKBOX, (long)1);	// do this after AppendProperty()!
		else if (TypeIndex == PT_UNKNOWN)
		{
			appNotify("PropEdit: unknown property type %s", Type->TypeName);
			MarkReadonly(link->wProp);
		}
	}
	if (Prop->IsReadonly)
		MarkReadonly(link->wProp);

	assert(link);
	return link;

	unguardf(("Prop=%s[%d], Type=%s", Prop->Name, ArrayIndex, Prop->TypeInfo->TypeName));
}


void WPropEdit::PopulateArrayProp(CPropLink *Parent, const CProperty *Prop, void *Data)
{
	guard(WPropEdit::PopulateArrayProp);

	// Add array property.
	// Note: varData will point to 1st array element for static array and to CArray structure
	// for dynamic array.
	CPropLink *ArrayProp = AppendProperty(Parent, Prop, PT_CONTAINER, Data);
	bool canAddItems     = false;
	bool canRemoveItems  = false;
	bool oldRefreshing   = m_isRefreshing;
	// get array pointer and size
	int Count;
	if (Prop->IsDynamicArray())
	{
		CArray *Arr = (CArray*)Data;
		Count = Arr->DataCount;
		Data  = Arr->DataPtr;
		canAddItems    = Prop->IsAddAllowed && !Prop->IsReadonly;
		canRemoveItems = Prop->IsDeleteAllowed && !Prop->IsReadonly;

		if (m_isRefreshing)
		{
			// verify children property count
			int childCount = 0;
			CPropLink *link;
			for (link = ArrayProp->Children; link; link = link->nextChild)
				if (link->ArrayIndex >= 0)
					childCount++;
			if (childCount != Arr->DataCount)
			{
				// children count was changed not using grid - remove all props and append again
				for (link = ArrayProp->Children; link; link = link->nextChild)
					DeleteProperty(link->wProp);
				ArrayProp->Children = NULL;
				m_propHover = NULL;
				// temporarily disable refreshing mode
				m_isRefreshing = false;
			}
		}

#if DEBUG_ARRAYS
		CPropLink *link;
	#define T(name)	\
			static const CProperty dummy##name(NULL, #name, FindType("int"));		\
			link = AppendProperty(ArrayProp, &dummy##name, PT_UINT, &Arr->name);	\
			link->wProp->SetAttribute(wxPG_UINT_BASE, wxPG_BASE_HEXL);				\
			link->wProp->SetAttribute(wxPG_UINT_PREFIX, wxPG_PREFIX_DOLLAR_SIGN);	\
			MarkReadonly(link->wProp);
		T(DataCount); T(MaxCount); T(DataPtr);
	#undef T
#endif
	}
	else
	{
		Count = Prop->ArrayDim;
	}

	// add array items
	for (int Index = 0; Index < Count; Index++, Data = OffsetPointer(Data, Prop->TypeInfo->TypeSize))
	{
		CPropLink *link = PopulateProp(ArrayProp, Prop, Data, Index);
		if (canRemoveItems)
			MarkArray(link->wProp);
	}
	if (canAddItems)
		MarkArray(ArrayProp->wProp);
	if (Prop->IsReadonly)
		MarkReadonly(ArrayProp->wProp);

	m_isRefreshing = oldRefreshing;

	unguardf(("Prop=%s, Type=%s", Prop->Name, Prop->TypeInfo->TypeName));
}


// sort properties first by category then by offset
static int PropSort(const CProperty* const* _P1, const CProperty* const* _P2)
{
	const CProperty* P1 = *_P1;
	const CProperty* P2 = *_P2;
	// compare category absence
	if (!P1->Category && P2->Category)
		return -1;
	if (P1->Category && !P2->Category)
		return 1;
	// here: either both properties has category, or both has not
	int cmp;
	if (P1->Category)
	{
		// compare categories
		cmp = stricmp(P1->Category, P2->Category);
		if (cmp) return cmp;
	}
	// compare offsets
	return P1->StructOffset - P2->StructOffset;
}


void WPropEdit::PopulateStruct(CPropLink *Parent, const CStruct *Type, void *Data)
{
	guard(WPropEdit::PopulateStruct);

	// gather properties list
	const CProperty *propList[MAX_STRUC_PROPS];
	int numProps = 0;
	int i;
	for (i = 0; /* empty */ ; i++)
	{
		const CProperty *Prop = Type->IterateProps(i);
		if (!Prop) break;			// end of structure declaration
		// skip non-editable props
		if (!Prop->IsEditable) continue;
		propList[numProps++] = Prop;
	}
	// sort properties
	if (Parent == m_rootProp)			// sort root level only
		QSort(propList, numProps, PropSort);

	// enumerate class properties
	for (i = 0; i < numProps; i++)
	{
		const CProperty *Prop = propList[i];

		void *varData = OffsetPointer(Data, Prop->StructOffset);
		if (!Prop->IsDynamicArray() && !Prop->IsStaticArray())
			PopulateProp     (Parent, Prop, varData);
		else
			PopulateArrayProp(Parent, Prop, varData);
	}

	unguard;
}


/*-----------------------------------------------------------------------------
	Asynchronous command execution
-----------------------------------------------------------------------------*/

void WPropEdit::AsyncCommand(int cmd, CPropLink *link)
{
	if (m_asyncCmd) return;		// queued command is not yet executed
	m_asyncItem = link;
	m_asyncCmd  = cmd;
	m_timer->Start(1, wxTIMER_ONE_SHOT);
}


void WPropEdit::OnTimer(wxTimerEvent&)
{
	guard(WPropEdit::OnTimer);

	// get command
	int cmd = m_asyncCmd;
	m_asyncCmd = 0;
	if (!cmd)
	{
		appNotify("WPropEdit::OnTimer: null command");
		return;
	}

	switch (cmd)
	{
	case ACMD_DELETE:
		RemoveArrayEntry(m_asyncItem);
		break;
	case ACMD_APPEND:
		AppendArrayEntry(m_asyncItem);
		break;
	default:
		appError("Unknown asynchronous command %d", cmd);
	}

	unguard;
}


/*-----------------------------------------------------------------------------
	Dynamic array manipulation
-----------------------------------------------------------------------------*/

void WPropEdit::RemoveArrayEntry(CPropLink *link)
{
	guard(WPropEdit::RemoveArrayEntry);

	assert(link);
	CPropLink *parent = link->Parent;
	int index = link->ArrayIndex;
	assert(index >= 0);
	assert(parent);

	CArray *Arr = (CArray*)parent->Data;
	assert(Arr);
	assert(Arr->DataPtr && Arr->DataCount <= Arr->MaxCount);
	assert(index >= 0 && index < Arr->DataCount);
	const CType *Type = parent->Prop->TypeInfo;
	assert(Type);

	// remove CArray entry
	if (Type->IsStruc)
		((CStruct*)Type)->DestructObject(link->Data);	// destruct removing entry
	Arr->Remove(index, 1, Type->TypeSize);

	// delete grid property
	// note: this will also perform FreeLink(link), but is is still accessible until not allocated again
	DeleteProperty(link->wProp);
	// BUG in wxPropertyGrid: m_propHover updated with MouseMove or RightClick only, but used in DoubleClick;
	// so. when deleting m_propHover property, then immediately double-clicking on other property, which
	// takes place of just deleted prop, app will crash in handler of double-click for removed property!
	m_propHover = NULL;

	// remove from parent->Children list
	assert(parent->Children);
	CPropLink *p;
	CPropLink *newSelection = NULL;
	CPropLink *prev = NULL;
	for (p = parent->Children; p; prev = p, p = p->nextChild)
		if (p == link)
		{
			// choose new selection
			newSelection = link->nextChild;
			if (!newSelection) newSelection = prev;
			// remove from Parent->Children list
			if (prev)
				prev->nextChild = link->nextChild;
			else
				parent->Children = link->nextChild;
			break;
		}
	assert(p);
	// select another property
	SelectProperty(newSelection ? newSelection->wProp : parent->wProp);

	FinishArrayEditing(Arr, Type, parent);

	unguard;
}


void WPropEdit::AppendArrayEntry(CPropLink *parent)
{
	guard(WPropEdit::AppendArrayEntry);

	CArray *Arr = (CArray*)parent->Data;
	assert(Arr);
	assert(Arr->DataCount <= Arr->MaxCount);
	const CType *Type = parent->Prop->TypeInfo;
	assert(Type);

	// insert new CArray entry
	int Index = Arr->DataCount;
	Arr->Insert(Index, 1, Type->TypeSize);

	// create new grid property and append to link->Children
	CPropLink *link = PopulateProp(parent, parent->Prop, (byte*)Arr->DataPtr + Type->TypeSize * Index, Index);
	MarkArray(link->wProp);
	// select new item
	SelectProperty(link->wProp);
	Expand(link->wProp);

	FinishArrayEditing(Arr, Type, parent);

	unguard;
}


#if 0
void WPropEdit::InsertArrayEntry(CPropLink *link, int index)
{
	//?? Difference from AppendArrayEntry(): PopulateProp()->AppendProperty() should
	//??	insert CPropLink into appropriate place in parent->Children; currently, will
	//??	insert into end (and append wxPGProperty to end of parent's children)
	//?? But note: FinishArrayEditing() will update all property labels and data pointers.
}
#endif


void WPropEdit::FinishArrayEditing(CArray *Arr, const CType *Type, CPropLink *parent)
{
	guard(WPropEdit::FinishArrayEditing);

	// refresh grid, update data pointers
	void *varData = Arr->DataPtr;
	int i = 0;
	for (CPropLink *p = parent->Children; p; p = p->nextChild)
	{
		if (p->Prop != parent->Prop)
			continue;
		// update data pointer
		p->Data = varData;
		// update index
		p->ArrayIndex = i;
		// update grid property name (internal use) and label (displayed text)
		// -- should be the same
		char PropName[32];
		appSprintf(ARRAY_ARG(PropName), "[%d]", i);
		SetPropertyName(p->wProp, PropName);
		p->wProp->SetLabel(PropName);
		// proceed to next entry
		varData = OffsetPointer(varData, Type->TypeSize);
		i++;
	}
	assert(i == Arr->DataCount);
	// redraw
#if DEBUG_ARRAYS
	RefreshProperty(parent);
#else
	Refresh();
#endif

	unguard;
}


/*-----------------------------------------------------------------------------
	Initialization
-----------------------------------------------------------------------------*/

//!! move to separate cpp
static wxBitmap *LoadResBitmap(const char *name)
{
	wxFileSystem fsys;
	wxFSFile *f = fsys.OpenFile(name, wxFS_READ | wxFS_SEEKABLE);
	if (!f)
		appError("LoadResBitmap: bitmap %s was not found", name);
	wxImage img(*(f->GetStream()));
	delete f;
	return new wxBitmap(img);
}


static void InitStatics()
{
	static bool init = false;
	if (init) return;
	init = true;

	// register property editors
	wxPGRegisterEditorClass(NoEditor);
	wxPGRegisterEditorClass(ArrayEntry);
	// load bitmaps
	bmpPropAdd    = LoadResBitmap(RES_NAME("prop_add.png"));
	bmpPropRemove = LoadResBitmap(RES_NAME("prop_remove.png"));
	// init colours
	wxColour bg = wxSystemSettings::GetColour(wxSYS_COLOUR_3DDKSHADOW);
	propColourReadonly = AddColour(bg, 64, -48, -48);
}


WPropEdit::WPropEdit(wxWindow *parent, wxWindowID id, const wxPoint &pos,
	const wxSize &size, long style, const wxChar *name)
:	wxPropertyGrid(parent, id, pos, size, style, name)
,	m_isRefreshing(false)
,	m_enumCallback(NULL)
{
	InitStatics();
	// init some colours
#if COLORIZE
	SetCellBackgroundColour(wxColour(192, 192, 255));
	wxColour bgColour = wxColour(64, 96, 192);
	SetCaptionForegroundColour(wxColour(255, 255, 255));
#else
	SetCellBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
	wxColour bgColour = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNHIGHLIGHT);
	SetCaptionForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_CAPTIONTEXT));
#endif
	SetLineColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW));
	SetCaptionBackgroundColour(bgColour);
	SetMarginColour(bgColour);
	SetSelectionBackground(wxColour(32, 255, 64));
	SetSelectionForeground(wxColour(0, 0, 0));
	// create dummy property, setup its colour and get its internal index
	wxPGProperty *Prop = new wxIntProperty("test", wxPG_LABEL, 0);
	Append(Prop);
#if COLORIZE
	SetPropertyBackgroundColour(Prop, wxColour(48, 64, 128));
#else
	SetPropertyBackgroundColour(Prop, wxSystemSettings::GetColour(wxSYS_COLOUR_3DDKSHADOW));
#endif
	m_contPropColIndex = PROP_UNHIDE(Prop, m_bgColIndex);	// container property colour
	// remove dummy property
	Clear();

	// propare prop links
	m_chain = new CMemoryChain;		//?? can make global chain for all PropGrids
	m_usedLinks = m_freeLinks = NULL;
	m_rootProp = AllocLink();
	// create timer for asynchronous execution
	m_timer = new wxTimer(this, 0);
}


BEGIN_EVENT_TABLE(WPropEdit, wxPropertyGrid)
	EVT_PG_CHANGED(wxID_ANY, WPropEdit::OnPropertyChange)
	EVT_TIMER(wxID_ANY,      WPropEdit::OnTimer)
END_EVENT_TABLE()


WPropEdit::~WPropEdit()
{
	FreeLink(m_rootProp);		// just in case
#if CHECK_PROP_LINKS
	Clear();
	for (CPropLink *link = m_usedLinks; link; link = link->next)
		appNotify("used link: %s %s", link->Prop->TypeInfo->TypeName, link->Prop->Name);
#endif
	delete m_chain;
	delete m_timer;
}


/*-----------------------------------------------------------------------------
	Object attachment
-----------------------------------------------------------------------------*/

void WPropEdit::OnPropertyChange(wxPropertyGridEvent &event)
{
	wxPGProperty *property = event.GetProperty();
	if (!property || !m_isObject)
		return;

#if 0
	CPropLink *link = (CPropLink*)property->DoGetAttribute(DATA_GETLINK).GetVoidPtr();
	assert(link);
	appPrintf("changed %s %s[%d]\n", link->Prop->TypeInfo->TypeName, link->Prop->Name, link->ArrayIndex);
#endif

	((CObject*)m_rootProp->Data)->PostEditChange();
}


void WPropEdit::AttachObject(const CStruct *Type, void *Data)
{
	// cleanup grid from previous properties
	DetachObject();
	// remember information
	m_isObject       = false;
	m_typeInfo       = Type;
	m_rootProp->Data = Data;
	if (Type && Data)
		PopulateStruct(m_rootProp, m_typeInfo, m_rootProp->Data);
	// redraw control
	Refresh();
}


void WPropEdit::AttachObject(CObject *Object)
{
	guard(WPropEdit::AttachObject);
	assert(Object);
	// cleanup grid from previous properties
	DetachObject();
	// remember information
	m_isObject       = true;
	m_typeInfo       = FindStruct(Object->GetClassName());
	m_rootProp->Data = Object;
	PopulateStruct(m_rootProp, m_typeInfo, m_rootProp->Data);
	// redraw control
	Refresh();
	unguardf(("class=%s", Object->GetClassName()));
}


void WPropEdit::DetachObject()
{
	Clear();
	m_rootProp->Data     = NULL;
	m_rootProp->Children = NULL;
}


void WPropEdit::RefreshProperty(CPropLink *link, const CType *type)
{
	guard(WPropEdit::RefreshProperty);
	m_isRefreshing = true;
	Freeze();
	if (type)
	{
		// refreshing whole object
		assert(!link->Prop);
		PopulateStruct(link, (CStruct*)type, link->Data);
	}
	else
	{
		assert(link->Prop);
		const CProperty *Prop = link->Prop;
		if (!Prop->IsArray())
			PopulateProp(link->Parent, Prop, link->Data, link->ArrayIndex);
		else
			PopulateArrayProp(link->Parent, Prop, link->Data);
	}
	m_isRefreshing = false;
	Thaw();
	Refresh();
	unguard;
}


void WPropEdit::RefreshGrid()
{
	if (m_typeInfo && m_rootProp->Data)
		RefreshProperty(m_rootProp, m_typeInfo);
}


/*-----------------------------------------------------------------------------
	Manipulating properties by their names
-----------------------------------------------------------------------------*/

bool WPropEdit::SelectByName(const char *Name, bool ShouldExpand)
{
	wxPGProperty *prop = GetPropertyByName(Name);
	if (!prop) return false;				// not found
	SelectProperty(prop);
	if (ShouldExpand) Expand(prop);
	return true;
}


wxString WPropEdit::GetSelectionName() const
{
	wxPGProperty *prop = GetSelection();
	if (!prop) return "";
	return prop->GetName();
}


bool WPropEdit::IsPropertySelected(const char *Name, int *index) const
{
	if (index) *index = -1;
	// find selected property
	wxPGProperty *prop = GetSelection();
	if (!prop) return false;
	// compare names
	wxString SelName = prop->GetName();
	const char *SelName2 = SelName.c_str();
	int len = strlen(Name);
	if (strncmp(Name, SelName2, len) != 0)	// 'Name' is not a substring of 'SelName'
		return false;
	// check tail of SelName: if end of string or '.', then property is selected
	bool res = SelName2[len] == 0 || SelName2[len] == '.';
	if (!res || !index) return res;
	// here: res == true, and index != NULL
	if (SelName2[len] == '.' && SelName2[len+1] == '[') // array item
		*index = atoi(SelName2 + len + 2);
	return true;
}


bool WPropEdit::RefreshByName(const char *Name)
{
	wxPGProperty *prop = GetPropertyByName(Name);
	if (!prop) return false;				// not found
	CPropLink *link = GetPropLink(prop);	// NOTE: will assert() if wrong property
	RefreshProperty(link);
	return true;
}


/*-----------------------------------------------------------------------------
	CPropLink management
-----------------------------------------------------------------------------*/

CPropLink *WPropEdit::AllocLink()
{
	// get link from 'free' list, or allocate new one
	CPropLink *link;
	if (m_freeLinks)
	{
		// get from 'free' list
		link = m_freeLinks;
		m_freeLinks = link->next;
		if (m_freeLinks)
		{
			assert(m_freeLinks->prev == link);
			m_freeLinks->prev = NULL;
		}
	}
	else
	{
		// allocate new entry
		link = new (m_chain) CPropLink;
	}

	// initialize all fields with zeros
	memset(link, 0, sizeof(CPropLink));

	// insert into 'used' list
	if (m_usedLinks)
	{
		assert(m_usedLinks->prev == NULL);
		m_usedLinks->prev = link;
	}
	link->prev       = NULL;
	link->next       = m_usedLinks;
	m_usedLinks      = link;
	// setup some fields
	link->wGrid      = this;
	link->wProp      = NULL;
	link->Parent     = NULL;
	link->Children   = NULL;
	link->nextChild  = NULL;
	link->Prop       = NULL;
	link->ArrayIndex = -1;
	link->Data       = NULL;

	return link;
}


void WPropEdit::FreeLink(CPropLink *link)
{
	// remove from 'used' list
	if (link->prev)
	{
		link->prev->next = link->next;
	}
	else
	{
		assert(m_usedLinks == link);
		m_usedLinks = link->next;
	}
	if (link->next)
		link->next->prev = link->prev;
	// insert into 'free' list
	if (m_freeLinks)
	{
		assert(m_freeLinks->prev == NULL);
		m_freeLinks->prev = link;
	}
	link->prev = NULL;
	link->next = m_freeLinks;
	m_freeLinks = link;
}
