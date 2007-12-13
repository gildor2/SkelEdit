#include <wx/wx.h>
#include <wx/propgrid/propgrid.h>

#include "Core.h"


//static wxPropertyGrid *FillingGrid;	//??? change


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
		*mPValue = m_value.Get##VariantType(); \
	}								\
};

//!! add spins to all numeric props
//!! byte, short: limit range
//!! support for unsigned values
//!! add enums, strings
//!! struc properties: should display prop text similar to (X=1,Y=0,Z=0) for some props, or "..." for all other
SIMPLE_PROPERTY(WIntProperty,   wxIntProperty,   int,   Long  )
SIMPLE_PROPERTY(WShortProperty, wxIntProperty,   short, Long  )
SIMPLE_PROPERTY(WFloatProperty, wxFloatProperty, float, Double)
SIMPLE_PROPERTY(WBoolProperty,  wxBoolProperty,  bool,  Bool  )


/*-----------------------------------------------------------------------------
	Filling property grid using typeinfo information
-----------------------------------------------------------------------------*/

static void AppendProperty(wxPropertyGrid *Grid, wxPGPropertyWithChildren *Parent, const CProperty *Prop, wxPGProperty *wxProp)
{
	assert((Grid && !Parent) || (!Grid && Parent));
	// add property to grid (root) or to parent property
	//?? check: do we need non-topmost categories (deeper in type tree)
	if (Grid)
	{
		// top level
		if (Prop->Category)
		{
			// property has category, should use it
			Grid->Append(new wxPropertyCategory(Prop->Category));
			// append property
			Grid->Append(wxProp);
		}
		else
		{
			Grid->Insert(Grid->GetFirst(), wxProp);
		}
		// append property
	}
	else
	{
		// append property to parent
		Parent->AddChild(wxProp);
	}
	// setup property comment
	if (Prop->Comment)
		wxProp->SetHelpString(Prop->Comment);
}


static void MarkContainerProp(wxPGProperty *Prop)
{
	Prop->SetValue("...");
	EXEC_ONCE(wxPGRegisterEditorClass(NoEditor));
	Prop->SetEditor(wxPG_EDITOR(NoEditor));
	Prop->SetFlag(wxPG_PROP_MODIFIED);
#if 0
	-- does not works properly
	-- can easily do SetBackgroundColourIndex(wxProperty*, 0) if subclass from
	-- wxPropertyGrid (will allow access to protected fields/methods)
	FillingGrid->SetPropertyBackgroundColour(Prop->GetId(), wxColour(255,0,0));
	// see wxPropertyGrid::SetBackgroundColourIndex() for details ...
	if (Prop->GetParentingType() != 0)
	{
		wxPGPropertyWithChildren *parent = (wxPGPropertyWithChildren*) Prop;
		for (int i = 0; i < parent->GetCount(); i++)
		{
			wxPGProperty *p = parent->Item(i);
			if (p->GetParentingType() == 0)
				FillingGrid->SetPropertyColourToDefault(p->GetId());
		}
	}
#endif
}


// forward declaration (used indirect recursion)
static void PopulateStruct(wxPropertyGrid *Grid, wxPGPropertyWithChildren *Parent, const CStruct *Type, void *Data);

static void PopulateProp(wxPropertyGrid *Grid, wxPGPropertyWithChildren *Parent, const CProperty *Prop,
	void *Data, const char *PropName)
{
	guard(PopulateProp);

	wxPGProperty *wxProp;
	const CType *Type = Prop->TypeInfo;
	assert(Type);

	// choose property type and create it
	if (Type->IsStruc)
	{
		// embedded structure
		wxProp = new wxCustomProperty(PropName, wxPG_LABEL);
		PopulateStruct(NULL, (wxCustomProperty*)wxProp, (CStruct*)Prop->TypeInfo, Data);
		AppendProperty(Grid, Parent, Prop, wxProp);				// do this after PopulateStruct()!
		MarkContainerProp(wxProp);
	}
	else if (!strcmp(Type->TypeName, "int"))
	{
		wxProp = new WIntProperty(Prop->Name, (int*)Data);
		AppendProperty(Grid, Parent, Prop, wxProp);
	}
	else if (!strcmp(Type->TypeName, "float"))
	{
		wxProp = new WFloatProperty(Prop->Name, (float*)Data);
		AppendProperty(Grid, Parent, Prop, wxProp);
	}
	else if (!strcmp(Type->TypeName, "short"))
	{
		wxProp = new WShortProperty(Prop->Name, (short*)Data);
		AppendProperty(Grid, Parent, Prop, wxProp);
	}
	else if (!strcmp(Type->TypeName, "bool"))
	{
		wxProp = new WBoolProperty(Prop->Name, (bool*)Data);
		AppendProperty(Grid, Parent, Prop, wxProp);
//		wxProp->SetAttribute(wxPG_BOOL_USE_CHECKBOX, (long)1);	// do this after AppendProperty()!
	}
	else
	{
		// single property
		//!! should not do this
		appNotify("PropEdit: unknown property type %s", Type->TypeName);
		wxProp = new wxStringProperty(Prop->Name, wxPG_LABEL, Type->TypeName);
		AppendProperty(Grid, Parent, Prop, wxProp);
	}

	unguardf(("Prop=%s (%s), Type=%s", Prop->Name, PropName, Prop->TypeInfo->TypeName));
}


static void PopulateStruct(wxPropertyGrid *Grid, wxPGPropertyWithChildren *Parent, const CStruct *Type, void *Data)
{
	// enumerate class properties
	for (int PropIdx = 0; /* empty */ ; PropIdx++)
	{
		const CProperty *Prop = Type->IterateProps(PropIdx);
		if (!Prop) break;
		//!! skip non-editable props

		unsigned offset = Prop->StructOffset;
		if (Prop->ArrayDim <= 1)
		{
			// add single property
			PopulateProp(Grid, Parent, Prop, OffsetPointer(Data, offset), Prop->Name);
		}
		else
		{
			// add array property
			wxPGPropertyWithChildren *ArrayProp = new wxCustomProperty(Prop->Name);
			AppendProperty(Grid, Parent, Prop, ArrayProp);
			for (int Index = 0; Index < Prop->ArrayDim; Index++, offset += Prop->TypeInfo->TypeSize)
			{
				char PropName[32];
				appSprintf(ARRAY_ARG(PropName), "[%d]", Index);
				PopulateProp(NULL, ArrayProp, Prop, OffsetPointer(Data, offset), PropName);
			}
			MarkContainerProp(ArrayProp);
		}
	}
}


void SetupPropGrid(wxPropertyGrid *Grid, CStruct *Type, void *Data)
{
//	FillingGrid = Grid;	//??? change

	//!! use 'Data'
	// cleanup grid from previous properties
	Grid->Clear();
	PopulateStruct(Grid, NULL, Type, Data);
	Grid->SetPropertyAttributeAll(wxPG_BOOL_USE_CHECKBOX, (long)1);
		// cannot do this after property creation - there will be crash in
		// SetAttribute() when bool property is deep inside type hierarchy
	// redraw control
	Grid->Refresh();
}
