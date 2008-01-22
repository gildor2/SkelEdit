#include "Core.h"
#include "FileReaderStdio.h"


//#define SHOW_TYPEINFO	1				// define to 1 for debugging typeinfo reading


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
	guard(CStruct::Dump);

	if (Indent > 32)
		Indent = 32;
	char buf[48];
	memset(buf, ' ', sizeof(buf));
	buf[Indent] = 0;

	for (int i = 0; /* empty */ ; i++)
	{
		const CProperty *Prop = IterateProps(i);
		if (!Prop) break;

		const char *PropTypeName = Prop->TypeInfo->TypeName;
#define IS(t)	( strcmp(PropTypeName, #t) == 0 )
		if ((Prop->ArrayDim > 1 || Prop->ArrayDim < 0) && !IS(string))
			continue;		//?? arrays are unsupported ...

		void *varData = OffsetPointer(Data, Prop->StructOffset);
		Out->Printf("%s%s = ", buf, Prop->Name);
		if (Prop->TypeInfo->IsStruc)
		{
			Out->Printf("{\n", buf);
			((CStruct*)Prop->TypeInfo)->WriteText(Out, varData, Indent+2);
			Out->Printf("%s}", buf);
		}
		else if (IS(string))
		{
			char buf[2048];
			appQuoteString(ARRAY_ARG(buf), (char*)varData);
			Out->Write(buf);
		}
		else if (IS(int))
			Out->Printf("%d", *((int  *)varData));
		else if (IS(float))
			Out->Printf("%g", *((float*)varData));
		else if (IS(bool))
			Out->Printf("%d", *((byte *)varData));
		else
		{
			appNotify("WriteText for unsupported type %s (struct %s)", PropTypeName, TypeName);
			Out->Printf("(unsupported %s)", PropTypeName);
		}
		Out->Printf("\n");
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
	while (const char *line = Text.GetLine())
	{
		TString<2048> FieldName;
//		appPrintf("Read: [%s]\n", line);
		FieldName = line;
		// check for end of structure block
		// WARNING: mismatched closing
		if (!strcmp(line, "}"))
		{
			if (Level == 0)
			{
				appNotify("CStruct::ReadText: mismatched closing brace (})");
				return false;
			}
			return true;
		}
		char *delim = FieldName.chr('=');
		const char *value = delim + 1;
		if (!delim)
		{
			appNotify("CStruct::ReadText: wrong line format");
			return false;
		}
		// trim trailing spaces
		while (delim > *FieldName && delim[-1] == ' ')
			delim--;
		*delim = 0;
		// seek for value
		while (*value == ' ')
			value++;
		// find field 'FieldName'
		const CProperty *Prop;
		for (int i = 0; /* empty */ ; i++)
		{
			Prop = IterateProps(i);
			if (!Prop) break;
			if (Prop->Name == FieldName)
				break;
		}
		if (!Prop)
		{
			appNotify("CStruct::ReadText: property \"%s\" was not found in \"%s\"", *FieldName, TypeName);
			continue;
		}
		// parse property
		void *varData = OffsetPointer(Data, Prop->StructOffset);
		const char *PropTypeName = Prop->TypeInfo->TypeName;
		if (Prop->TypeInfo->IsStruc)
		{
			if (strcmp(value, "{") != 0)
			{
				appNotify("CStruct::ReadText: '{' expected for \"%s %s\"", PropTypeName, *FieldName);
				return false;
			}
			bool result = ((CStruct*)Prop->TypeInfo)->ReadText(Text, varData, Level+1);
			if (!result) return false;
		}
		else if (IS(string))
		{
			char buf[2048];
			appUnquoteString((char*)varData, Prop->ArrayDim, value);
		}
		else if (IS(int))
			*((int*)varData)   = atoi(value);
		else if (IS(float))
			*((float*)varData) = atof(value);
		else if (IS(bool))
			*((byte*)varData)  = atoi(value);
		else
		{
			appNotify("ReadText for unsupported type %s (struct %s)", PropTypeName, TypeName);
			return false;
		}
	}
	return true;
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
	T2(pointer, void*)
#undef T
	new CType("string", 1);			// real length is stored as array size
	// read typeinfo file
	ParseTypeinfoFile(Ar);
}


/*-----------------------------------------------------------------------------
	CObject class
-----------------------------------------------------------------------------*/

void CObject::PostLoad()
{}

void CObject::PostEditChange()
{}

const char* CObject::GetClassName() const
{
	return "Object";
}

void CObject::Serialize(CArchive &Ar)
{
	TString<256> TempName;
	TempName = GetClassName();
	if (Ar.IsLoading)
	{
		// get archive version
		Ar << AR_INDEX(Ar.ArVer);
		if (Ar.ArVer <= 0 || Ar.ArVer > ARCHIVE_VERSION)
			appError("Loading file of a newer version %d, current version is "STR(ARCHIVE_VERSION), Ar.ArVer);
		// verify class name
		TString<256> LoadName;
		Ar << LoadName;
		if (TempName != LoadName)
			appError("Expected class \"%s\", but found \"%s\"", *TempName, *LoadName);
	}
	else
	{
		// store object header
		Ar.ArVer = ARCHIVE_VERSION;
		Ar << AR_INDEX(Ar.ArVer) << TempName;
	}
}


void CObject::InternalLoad(const char *From)
{
	CFile Ar(From);
	Serialize(Ar);
	PostLoad();
}
