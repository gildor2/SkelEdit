#include "Core.h"

//#define SHOW_TYPEINFO	1				// define to 1 for debugging typeinfo reading

/*-----------------------------------------------------------------------------
	Type serialization consts
	Should correspond to TYPE_XXX and PROP_XXX declarations from "Tools/ucc"
-----------------------------------------------------------------------------*/

enum
{
	TYPE_SCALAR,
	TYPE_ENUM,
	TYPE_STRUCT,
	TYPE_CLASS
};


#define PROP_EDITABLE		1
#define PROP_EDITCONST		2
#define PROP_NORESIZE		4
#define PROP_NOADD			8
#define PROP_NOEXPORT		16
#define PROP_TRANSIENT		32
#define PROP_NATIVE			64


/*-----------------------------------------------------------------------------
	CType class
-----------------------------------------------------------------------------*/

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


bool CType::WriteProp(const CProperty *Prop, COutputDevice &Out, void *Data) const
{
	return false;
}


bool CType::ReadProp(const CProperty *Prop, const char *Text, void *Data) const
{
	return false;
}


/*-----------------------------------------------------------------------------
	CEnum class
-----------------------------------------------------------------------------*/

void CEnum::AddValue(const char *Name)
{
	Names.AddItem(appStrdup(Name, TypeChain));
}


/*-----------------------------------------------------------------------------
	CStruct class
-----------------------------------------------------------------------------*/

CProperty *CStruct::AddField(const char *AName, const char *ATypeName, int ArraySize)
{
	guard(AddField);

	// create new property
	const CType *Type = FindType(ATypeName);
	CProperty *Prop = new (TypeChain) CProperty(this, AName, Type, ArraySize);

	// find a place in structure and check for name duplicates
	CProperty *last = NULL;
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

	return Prop;
	unguardf(("%s", AName));
}


void CStruct::Finalize()
{
	TypeSize = Align(TypeSize, TypeAlign);
}


void CStruct::Dump(int indent) const
{
	if (indent > 32)
		indent = 32;
	char buf[48];
	memset(buf, ' ', sizeof(buf));
	buf[indent+4] = 0;

	appPrintf("%s### %s (sizeof=%02X, align=%d):\n%s{\n", buf+4, TypeName, TypeSize, TypeAlign, buf+4);

	for (int i = 0; /* empty */ ; i++)
	{
		const CProperty *Prop = IterateProps(i);
		if (!Prop) break;
		appPrintf("%s[%2X] %-10s %s[%d]\n", buf, Prop->StructOffset, Prop->TypeInfo->TypeName, Prop->Name, Prop->ArrayDim);
		if (Prop->TypeInfo->IsStruc)
			((CStruct*)Prop->TypeInfo)->Dump(indent+4);
	}

	appPrintf("%s}\n", buf+4);
}



void CStruct::WriteText(COutputDevice *Out, void *Data, int Indent) const
{
	guard(CStruct::WriteText);

	if (Indent > 32)
		Indent = 32;
	char buf[48];
	memset(buf, ' ', sizeof(buf));
	buf[Indent] = 0;

	for (int i = 0; /* empty */ ; i++)
	{
		const CProperty *Prop = IterateProps(i);
		if (!Prop) break;

		guard(WriteProp);

		const char *PropTypeName = Prop->TypeInfo->TypeName;
		if (Prop->IsDynamicArray() || Prop->IsStaticArray())
			continue;						//?? arrays are not yet unsupported ...

		void *varData = OffsetPointer(Data, Prop->StructOffset);
		Out->Printf("%s%s = ", buf, Prop->Name);
		if (Prop->TypeInfo->WriteProp(Prop, *Out, varData))
		{
			// do nothing
		}
		else if (Prop->TypeInfo->IsStruc)	//?? recurse; check for rework; no {} when indent<0 ??
		{
			Out->Printf("{\n", buf);
			((CStruct*)Prop->TypeInfo)->WriteText(Out, varData, Indent+2);
			Out->Printf("%s}", buf);
		}
		else
		{
			appNotify("WriteText for unsupported type %s (struct %s)", PropTypeName, TypeName);
			Out->Printf("(unsupported %s)", PropTypeName);
		}
		Out->Printf("\n");

		unguardf(("%s", *Prop->Name));
	}

	unguardf(("%s", TypeName));
}


bool CStruct::ReadText(const char* SrcText, void *Data) const
{
	CSimpleParser Text;
	Text.InitFromBuf(SrcText);
	return ReadText(Text, Data, 0);
}


bool CStruct::ReadText(CSimpleParser &Text, void *Data, int Level) const
{
	guard(CStruct::ReadText);

	while (const char *line = Text.GetLine())
	{
//		appPrintf("Read: [%s]\n", line);
		// check for end of structure block
		if (!strcmp(line, "}"))
		{
			if (Level == 0)
			{
				appNotify("CStruct::ReadText: mismatched closing brace (})");
				return false;
			}
			return true;
		}
		// here: line should be in one of following formats:
		//	FieldName = Value       -- for scalars
		//	FieldName = {           -- for structures
		// Separate FieldName and Valie
		TString<2048> FieldName;		// buffer for whole line, placed null char as delimiter
		FieldName = line;
		char *delim = FieldName.chr('=');
		const char *value = delim + 1;
		if (!delim)
		{
			appNotify("CStruct::ReadText: '=' expected in \"%s\"", line);
			return false;
		}
		// trim trailing spaces
		while (delim > *FieldName && delim[-1] == ' ')
			delim--;
		*delim = 0;
		// seek leading spaces for value
		while (*value == ' ')
			value++;
		// find field 'FieldName'
		const CProperty *Prop = FindProp(FieldName);
		if (!Prop)
		{
			appNotify("CStruct::ReadText: property \"%s\" was not found in \"%s\"", *FieldName, TypeName);
			continue;
		}
		// parse property
		void *varData = OffsetPointer(Data, Prop->StructOffset);
		const char *PropTypeName = Prop->TypeInfo->TypeName;

		if (Prop->TypeInfo->ReadProp(Prop, value, varData))
		{
			// do nothing
		}
		else if (Prop->TypeInfo->IsStruc)
		{
			if (strcmp(value, "{") != 0)
			{
				appNotify("CStruct::ReadText: '{' expected for \"%s %s\"", PropTypeName, *FieldName);
				return false;
			}
			bool result = ((CStruct*)Prop->TypeInfo)->ReadText(Text, varData, Level+1);
			if (!result) return false;
		}
		else
		{
			appNotify("ReadText for unsupported type %s (struct %s)", PropTypeName, TypeName);
			return false;
		}
	}
	return true;

	unguardf(("%s", TypeName));
}


const CProperty *CStruct::FindProp(const char *Name) const
{
	// check for local property
	for (const CProperty *Prop = FirstProp; Prop; Prop = Prop->NextProp)
		if (!strcmp(Prop->Name, Name))
			return Prop;
	// check for parent structure property
	if (ParentStruct)
		return ParentStruct->FindProp(Name);
	// not found ...
	return NULL;
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
	const CProperty *Prop;
	int i;
	for (i = numParentProps, Prop = FirstProp; i < Index; i++, Prop = Prop->NextProp)
	{
		// empty
		assert(Prop);
	}
	return Prop;
}


void CStruct::DestructObject(void *Data)
{
	guard(CStruct::DestructObject);

	for (int i = 0; /* empty */ ; i++)
	{
		const CProperty *Prop = IterateProps(i);
		if (!Prop) break;

		void *varData = OffsetPointer(Data, Prop->StructOffset);
		int varCount  = 1;

		// process arrays
		CArray *Arr = NULL;
		if (Prop->IsDynamicArray())
		{
			Arr = (CArray*)varData;
			varCount = Arr->DataCount;
			varData  = Arr->DataPtr;
		}
		else if (Prop->IsStaticArray())
			varCount = Prop->ArrayDim;
		// process structures recursively
		if (Prop->TypeInfo->IsStruc)
			for (int index = 0; index < varCount; index++, varData = OffsetPointer(varData, Prop->TypeInfo->TypeSize))
				((CStruct*)Prop->TypeInfo)->DestructObject(varData);
		// -- process types, which needs destruction, here ...
		// free dynamic array
		if (Arr)
			Arr->Empty(0, 0);
	}

	unguardf(("%s", TypeName));
}


/*-----------------------------------------------------------------------------
	CProperty class
-----------------------------------------------------------------------------*/

CProperty::CProperty(CStruct *AOwner, const char *AName, const CType *AType, int AArrayDim)
:	Name(appStrdup(AName, TypeChain))
,	TypeInfo(AType)
,	ArrayDim(AArrayDim)
,	NextProp(NULL)
,	Owner(AOwner)
,	StructOffset(0)
,	IsEditable(false)
,	IsReadonly(false)
,	IsAddAllowed(true)
,	IsDeleteAllowed(true)
,	Comment(NULL)
,	Category(NULL)
{}


/*-----------------------------------------------------------------------------
	Typeinfo file support
-----------------------------------------------------------------------------*/

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
#if SHOW_TYPEINFO
				appPrintf("Reading enum [%s]\n", *TypeName);
#endif
				while (true)
				{
					TString<256> EnumItem;
					Ar << EnumItem;
					if (!EnumItem[0]) break;			// end marker
#if SHOW_TYPEINFO
					appPrintf("  . [%s]\n", *EnumItem);
#endif
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
					Struc = new (TypeChain) CStruct(TypeName, Parent);	//!! here: use CClass ?
				}
				else
				{
					Struc = new (TypeChain) CStruct(TypeName, Parent);
				}
#if SHOW_TYPEINFO
				appPrintf("struct [%s] : [%s]\n", *TypeName, *ParentName);
#endif

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
#if SHOW_TYPEINFO
					appPrintf("  . field [%s] %s[%d] (F=%X) /* %s */ GRP=[%s]\n",
						*FieldName, *FieldType, ArrayDim, Flags, *Comment, *EditorGroup);
#endif
					// create property
					CProperty *Prop = Struc->AddField(FieldName, FieldType, ArrayDim);
					// fill other fields
					Prop->IsEditable   = (Flags & PROP_EDITABLE)  != 0;
					Prop->IsReadonly   = (Flags & PROP_EDITCONST) != 0;
					Prop->IsAddAllowed = (Flags & (PROP_NOADD|PROP_NORESIZE)) == 0;
					Prop->IsDeleteAllowed = (Flags & PROP_NORESIZE) == 0;
					Prop->Category     = (Prop->IsEditable && EditorGroup[0]) ?
										  appStrdup(EditorGroup, TypeChain) : NULL;
					Prop->Comment      = (Prop->IsEditable && Comment[0]) ?
										  appStrdup(Comment, TypeChain) : NULL;
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


/*-----------------------------------------------------------------------------
	Support for standard types
-----------------------------------------------------------------------------*/

// here to disable warning ...
inline bool atob(const char *str)
{
	return atoi(str) != 0;
}


#define NUMERIC_TYPE(TypeName, NatType, WriteFormat, ReadFunc) \
	class C##TypeName##Type : public CType		\
	{											\
	public:										\
		C##TypeName##Type()						\
		:	CType(#TypeName, sizeof(NatType))	\
		{}										\
		virtual bool WriteProp(const CProperty *Prop, COutputDevice &Out, void *Data) const \
		{										\
			Out.Printf(WriteFormat, *(NatType*)Data); \
			return true;						\
		}										\
		virtual bool ReadProp(const CProperty *Prop, const char *Text, void *Data) const \
		{										\
			*(NatType*)Data = ReadFunc;			\
			return true;						\
		}										\
	};

NUMERIC_TYPE(bool,     bool,     "%d", atob(Text))
NUMERIC_TYPE(byte,     byte,     "%d", atoi(Text))
NUMERIC_TYPE(short,    short,    "%d", atoi(Text))
NUMERIC_TYPE(ushort,   word,     "%d", atoi(Text))
NUMERIC_TYPE(int,      int,      "%d", atoi(Text))
NUMERIC_TYPE(unsigned, unsigned, "%d", atoi(Text))
NUMERIC_TYPE(float,    float,    "%g", atof(Text))
NUMERIC_TYPE(double,   double,   "%g", strtod(Text, NULL))


class CStringType : public CType
{
public:
	CStringType()
	:	CType("string", 1)		// real length is stored as array size
	{}
	virtual bool WriteProp(const CProperty *Prop, COutputDevice &Out, void *Data) const
	{
		char buf[2048];
		appQuoteString(ARRAY_ARG(buf), (char*)Data);
		Out.Write(buf);
		return true;
	}
	virtual bool ReadProp(const CProperty *Prop, const char *Text, void *Data) const
	{
		appUnquoteString((char*)Data, Prop->ArrayDim, Text);
		return true;
	}
};


void InitTypeinfo(CArchive &Ar)
{
	TypeChain = new CMemoryChain;
	// register standard types
#define N(Name)					new (TypeChain) C##Name##Type;
	new (TypeChain) CStringType;
	N(bool)
	N(byte)
	N(short)
	N(ushort)
	N(int)
	N(unsigned)
	N(float)
	N(double)
#define T2(name1, name2)		new (TypeChain) CType(#name1, sizeof(name2));
	T2(pointer, void*)
#undef T2
#undef N
	// read typeinfo file
	ParseTypeinfoFile(Ar);
}
