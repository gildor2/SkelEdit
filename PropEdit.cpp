#include <wx/wx.h>
#include <wx/propgrid/propgrid.h>

#include "Core.h"
#include "PropEdit.h"


#define MAX_STRUC_PROPS		1024		// in-stack array limit

//#define COLORIZE			1


static wxColour AddColour(wxColour base, int r, int g, int b)
{
	int R, G, B;
	R = base.Red()   + r;
	G = base.Green() + g;
	B = base.Blue()  + b;
	return wxColour(bound(R, 0, 255), bound(G, 0, 255), bound(B, 0, 255));
}


class wxPGPropAccessHack : public wxIntProperty
{
	friend WPropEdit;
};

#define PROP_UNHIDE(Prop,Field)	( ((wxPGPropAccessHack*)Prop)->Field )


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
	Special property types for wxPropertyGrid
-----------------------------------------------------------------------------*/

#define SIMPLE_PROPERTY(TypeName,BaseType,DataType,VariantType) \
class TypeName : public BaseType	\
{									\
public:								\
	DataType	*mPValue;			\
	TypeName(const char *Name, DataType *pValue) \
	:	BaseType(Name, wxPG_LABEL, *pValue) \
	,	mPValue(pValue)				\
	{}								\
	virtual void OnSetValue()		\
	{								\
		BaseType::OnSetValue();		\
		*mPValue = m_value.Get##VariantType(); \
	}								\
};

//!! add spins to all numeric props
//!! byte, short: limit range
//!! support for unsigned values
//!! add strings
//!! struc properties: should display prop text similar to (X=1,Y=0,Z=0) for some props, or "..." for all other
SIMPLE_PROPERTY(WIntProperty,   wxIntProperty,   int,   Long  )
SIMPLE_PROPERTY(WShortProperty, wxIntProperty,   short, Long  )
SIMPLE_PROPERTY(WFloatProperty, wxFloatProperty, float, Double)
SIMPLE_PROPERTY(WBoolProperty,  wxBoolProperty,  bool,  Bool  )


class WEnumProperty : public wxEnumProperty
{
public:
	byte		*mPValue;

	WEnumProperty(const CProperty *Prop, byte *pValue)
	:	wxEnumProperty(Prop->Name, wxPG_LABEL, NULL)
	,	mPValue(pValue)
	{
		// fill dropdown list with enumeration constants
		assert(Prop->TypeInfo->IsEnum);
		const CEnum * Type = (CEnum*)Prop->TypeInfo;
		for (int i = 0; i < Type->Names.Num(); i++)
			m_choices.Add(Type->Names[i]);
		// set value after filling dropdown list
		SetIndex(*pValue);
	}
	virtual void OnSetValue()
	{
		wxEnumProperty::OnSetValue();
		*mPValue = m_value.GetLong();
	}
};


/*-----------------------------------------------------------------------------
	Filling property grid using typeinfo information
-----------------------------------------------------------------------------*/

void WPropEdit::AppendProperty(wxPGPropertyWithChildren *Parent, const CProperty *Prop, wxPGProperty *wxProp)
{
	// add property to grid (root) or to parent property
	if (!Parent)
	{
		// top level
		if (Prop->Category)
			Append(new wxPropertyCategory(Prop->Category));
		Append(wxProp);
	}
	else
	{
		// append property to parent
		// note: there is no correct support for sub-categories
		AppendIn(Parent->GetId(), wxProp);
	}
	// setup property comment
	if (Prop->Comment)
		wxProp->SetHelpString(Prop->Comment);
}


void WPropEdit::MarkContainerProp(wxPGProperty *Prop)
{
	EXEC_ONCE(wxPGRegisterEditorClass(NoEditor));

	Prop->SetValue("...");
	Prop->SetEditor(wxPG_EDITOR(NoEditor));
	Prop->SetFlag(wxPG_PROP_MODIFIED);
	// note: here we desire to change colour of current property only
	// (without affecting children); SetPropertyBackgroundColour() will
	// set colour recursively, so - use internal wxPropertyGrid info ...
	PROP_UNHIDE(Prop, m_bgColIndex) = m_ContOnlyColIndex;
}


void WPropEdit::MarkReadonly(wxPGProperty *Prop)
{
	EXEC_ONCE(wxPGRegisterEditorClass(NoEditor));
	Prop->SetEditor(wxPG_EDITOR(NoEditor));

	wxColour bg = wxSystemSettings::GetColour(wxSYS_COLOUR_3DDKSHADOW);
	SetPropertyBackgroundColour(Prop->GetId(), AddColour(bg, 48, -32, -32));
	if (Prop->GetParentingType() != 0)
	{
		wxPGPropertyWithChildren *parent = (wxPGPropertyWithChildren*) Prop;
		for (int i = 0; i < parent->GetCount(); i++)
			MarkReadonly(parent->Item(i));
	}
}


void WPropEdit::PopulateProp(wxPGPropertyWithChildren *Parent, const CProperty *Prop, void *Data, const char *PropName)
{
	guard(WPropEdit::PopulateProp);

	wxPGProperty *wxProp;
	const CType *Type = Prop->TypeInfo;
	assert(Type);

	// choose property type and create it
	if (Type->IsStruc)
	{
		// embedded structure
		wxProp = new wxCustomProperty(PropName, wxPG_LABEL);
		AppendProperty(Parent, Prop, wxProp);
		// note: AppendProperty() should be used AFTER PopulateStruct() when inserting child props
		// using AddChild() or BEFORE PopulateStruct() when using AppendIn() function
		PopulateStruct((wxCustomProperty*)wxProp, (CStruct*)Prop->TypeInfo, Data);
		MarkContainerProp(wxProp);
	}
	else if (Type->IsEnum)
	{
		wxProp = new WEnumProperty(Prop, (byte*)Data);
		AppendProperty(Parent, Prop, wxProp);
	}
	else if (!strcmp(Type->TypeName, "int"))
	{
		wxProp = new WIntProperty(Prop->Name, (int*)Data);
		AppendProperty(Parent, Prop, wxProp);
	}
	else if (!strcmp(Type->TypeName, "float"))
	{
		wxProp = new WFloatProperty(Prop->Name, (float*)Data);
		AppendProperty(Parent, Prop, wxProp);
	}
	else if (!strcmp(Type->TypeName, "short"))
	{
		wxProp = new WShortProperty(Prop->Name, (short*)Data);
		AppendProperty(Parent, Prop, wxProp);
	}
	else if (!strcmp(Type->TypeName, "bool"))
	{
		wxProp = new WBoolProperty(Prop->Name, (bool*)Data);
		AppendProperty(Parent, Prop, wxProp);
		wxProp->SetAttribute(wxPG_BOOL_USE_CHECKBOX, (long)1);	// do this after AppendProperty()!
	}
	else
	{
		// single property
		//!! should not go here
		appNotify("PropEdit: unknown property type %s", Type->TypeName);
		wxProp = new wxStringProperty(Prop->Name, wxPG_LABEL, Type->TypeName);
		AppendProperty(Parent, Prop, wxProp);
	}
	if (Prop->IsReadonly)
		MarkReadonly(wxProp);

	unguardf(("Prop=%s (%s), Type=%s", Prop->Name, PropName, Prop->TypeInfo->TypeName));
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
	// here: both properties has category or both has not
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


void WPropEdit::PopulateStruct(wxPGPropertyWithChildren *Parent, const CStruct *Type, void *Data)
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
	if (!Parent)					// sort root level only
		QSort(propList, numProps, PropSort);

	// enumerate class properties
	for (i = 0; i < numProps; i++)
	{
		const CProperty *Prop = propList[i];

		unsigned offset = Prop->StructOffset;
		if (Prop->ArrayDim <= 1)
		{
			// add single property
			PopulateProp(Parent, Prop, OffsetPointer(Data, offset), Prop->Name);
		}
		else
		{
			// add array property
			wxPGPropertyWithChildren *ArrayProp = new wxCustomProperty(Prop->Name);
			AppendProperty(Parent, Prop, ArrayProp);
			for (int Index = 0; Index < Prop->ArrayDim; Index++, offset += Prop->TypeInfo->TypeSize)
			{
				char PropName[32];
				appSprintf(ARRAY_ARG(PropName), "[%d]", Index);
				PopulateProp(ArrayProp, Prop, OffsetPointer(Data, offset), PropName);
			}
			MarkContainerProp(ArrayProp);
			if (Prop->IsReadonly)
				MarkReadonly(ArrayProp);
		}
	}

	unguard;
}


WPropEdit::WPropEdit(wxWindow *parent, wxWindowID id, const wxPoint &pos,
	const wxSize &size, long style, const wxChar *name)
:	wxPropertyGrid(parent, id, pos, size, style, name)
{
	// init some colours
#if COLORIZE
	SetCellBackgroundColour(wxColour(192, 192, 255));
#else
	SetCellBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
#endif
	SetLineColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW));
#if COLORIZE
	wxColour bgColour = wxColour(64, 96, 192);
#else
	wxColour bgColour = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNHIGHLIGHT);
#endif
	SetCaptionBackgroundColour(bgColour);
	SetMarginColour(bgColour);
#if COLORIZE
	SetCaptionForegroundColour(wxColour(255, 255, 255));
#else
	SetCaptionForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_CAPTIONTEXT));
#endif
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
	m_ContOnlyColIndex = PROP_UNHIDE(Prop, m_bgColIndex);	// container property colour
	// remove dummy property
	Clear();
}


void WPropEdit::AttachObject(CStruct *Type, void *Data)
{
	// cleanup grid from previous properties
	Clear();
	if (Type && Data)
		PopulateStruct(NULL, Type, Data);
	// redraw control
	Refresh();
}
