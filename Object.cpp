#include "Core.h"


static TArray<CType*>	GTypes;
static CMemoryChain		*TypeChain;


const CType *FindType(const char *Name, bool ShouldExist)
{
	for (int i = 0; i < GTypes.Num(); i++)
		if (!strcmp(GTypes[i]->TypeName, Name))
			return GTypes[i];
	if (ShouldExist)
		appError("unknown type \"%s\"", Name);
	return NULL;
}


CType::CType(const char *AName, unsigned ASize, unsigned AAlign)
:	TypeName(appStrdup(AName, TypeChain))
,	TypeSize(ASize)
,	TypeAlign(AAlign)
,	IsStruc(false)
,	IsEnum(false)
{
	guard(RegisterType);

	if (!AAlign)
		TypeAlign = ASize;
	if (FindType(AName, false))
		appError("Type %s is already registered", AName);
	GTypes.AddItem(this);

	unguard;
}


void CEnum::AddValue(const char *Name)
{
	Names.AddItem(appStrdup(Name, TypeChain));
}


CProperty *CStruct::AddField(const char *AName, const char *ATypeName, int ArraySize)
{
	guard(AddField);

	const CType *Type = FindType(ATypeName);
	CProperty *Prop = new CProperty(AName, Type, ArraySize);
	AddField(Prop);
	return Prop;

	unguard;
}


void CStruct::AddField(CProperty *Prop)
{
	guard(CStruct::AddField2);
	CProperty *last = NULL;
	const CType *Type = Prop->TypeInfo;
	for (CProperty *curr = FirstProp; curr; curr = curr->NextProp)
	{
		if (!strcmp(curr->Name, Prop->Name))
			appError("Structure \"%s\" already has property \"%s\"", Type->TypeName, Prop->Name);
		last = curr;
	}

	// link property
	if (last)
		last->NextProp = Prop;
	else
		FirstProp = Prop;

	// handle alignments, advance size
	unsigned propAlign = Type->TypeAlign;
	if (Prop->ArrayDim < 0)
		propAlign = max(sizeof(void*), sizeof(int)); // alignment of CArray
	TypeSize = Align(TypeSize, propAlign);	// alignment for new prop
	Prop->StructOffset = TypeSize;			// remember property offset
	if (Prop->ArrayDim < 0)					// dynamic array (TArray<>)
	{
		TypeSize += sizeof(CArray);
	}
	else if (Prop->ArrayDim > 0)			// update structure size for array
	{
		TypeSize += Type->TypeSize * Prop->ArrayDim;
	}
	else									// scalar property
	{
		TypeSize += Type->TypeSize;
	}
	TypeAlign = max(TypeAlign, propAlign);	// update structure alignment requirement
	// update property counts
	NumProps++;
	TotalProps++;

	unguardf(("%s", Prop->Name));
}


void CStruct::Finalize()
{
	TypeSize = Align(TypeSize, TypeAlign);
}


const CProperty *CStruct::IterateProps(int Index) const
{
	if (Index < 0 || Index >= TotalProps)
		return NULL;				// out of property array bounds
	// check parent class
	int numParentProps = TotalProps - NumProps;
	if (Index < numParentProps)
	{
		assert(ParentStruct);
		return ParentStruct->IterateProps(Index);
	}
	// find property
	CProperty *Prop;
	int i;
	for (i = numParentProps, Prop = FirstProp; i < Index; i++, Prop = Prop->NextProp)
	{
		// empty
		assert(Prop);
	}
	return Prop;
}


CProperty::CProperty(const char *AName, const CType *AType, int AArrayDim)
:	Name(appStrdup(AName, TypeChain))
,	TypeInfo(AType)
,	ArrayDim(AArrayDim)
,	NextProp(NULL)
,	StructOffset(0)
,	IsEditable(false)
,	IsReadonly(false)
,	Comment(NULL)
,	Category(NULL)
{}


void ParseTypeinfoFile(CArchive &Ar)
{
	guard(ParseTypeinfoFile);

	while (true)
	{
		TString<256> TypeName;
		Ar << TypeName;
		if (!TypeName[0]) break;					// end marker

		guard(ParseType);

		int typeKind;
		Ar << AR_INDEX(typeKind);
		switch (typeKind)
		{
		case TYPE_SCALAR:
			appError("TYPE_SCALAR should not appear!");
			break;

		case TYPE_ENUM:
			{
				CEnum *Enum = new (TypeChain) CEnum(TypeName);
//				appNotify("Reading enum [%s]", *TypeName);	//!!
				while (true)
				{
					TString<256> EnumItem;
					Ar << EnumItem;
					if (!EnumItem[0]) break;			// end marker
//					appNotify("  . [%s]", *EnumItem);	//!!
					Enum->AddValue(EnumItem);
				}
			}
			break;

		case TYPE_CLASS:
		case TYPE_STRUCT:
			{
				CStruct *Struc;
				const CStruct *Parent = NULL;
				TString<256> ParentName;
				Ar << ParentName;
				if (ParentName[0])
				{
					const CType *Parent2 = FindType(ParentName);
					if (!Parent2->IsStruc)
						appError("%s derived from non-structure type %s", *TypeName, *ParentName);
					Parent = (CStruct*)Parent2;
				}

				if (typeKind == TYPE_CLASS)
				{
					Struc = new (TypeChain) CStruct(TypeName, Parent);	//!! here: use CSlass ?
				}
				else
				{
					Struc = new (TypeChain) CStruct(TypeName, Parent);
				}
//				appNotify("struct [%s] : [%s]", *TypeName, *ParentName);	//!!

				// read fields
				while (true)
				{
					TString<256> FieldName, FieldType, EditorGroup;
					TString<1024> Comment;
					int ArrayDim;
					unsigned Flags;
					// read name
					Ar << FieldName;
					if (!FieldName[0]) break;			// end marker
					// read other fields
					Ar << FieldType << AR_INDEX(ArrayDim) << Flags << Comment << EditorGroup;
//					appNotify("  . field [%s] %s[%d] (F=%X) /* %s */ GRP=[%s]",
//						*FieldName, *FieldType, ArrayDim, Flags, *Comment, *EditorGroup);
					// create property
					CProperty *Prop = Struc->AddField(FieldName, FieldType, ArrayDim);
					// fill other fields
					Prop->IsEditable = Flags & PROP_EDITABLE;
					Prop->IsReadonly = Flags & PROP_EDITCONST;
					Prop->Category   = (Prop->IsEditable && EditorGroup[0]) ?
										appStrdup(EditorGroup) : NULL;
					Prop->Comment    = (Prop->IsEditable && Comment[0]) ?
										appStrdup(Comment) : NULL;
				}
				Struc->Finalize();
			}
			break;

		default:
			appError("unknown kind %d for type %s", typeKind, *TypeName);
		}

		unguardf(("%s", *TypeName));
	}

	unguard;
}


void InitTypeinfo(CArchive &Ar)
{
	TypeChain = new CMemoryChain;
	// register known types
#define T(name)					new CType(#name,  sizeof(name ));
#define T2(name1, name2)		new CType(#name1, sizeof(name2));
	T(bool)
	T(byte)
	T(short)
	T2(ushort, word)
	T(int)
	T(unsigned)
	T(float)
	T(double)
#undef T
	new CType("string", 1);			// real length is stored as array size
	// read typeinfo file
	ParseTypeinfoFile(Ar);
}
