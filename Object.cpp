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

	CProperty *last = NULL;
	for (CProperty *curr = FirstProp; curr; curr = curr->NextProp)
	{
		if (!strcmp(curr->Name, AName))
			appError("Structure \"%s\" already has property \"%s\"", TypeName, AName);
		last = curr;
	}

	const CType *Type = FindType(ATypeName);
	CProperty *prop = new CProperty(AName, Type, ArraySize);
	// link property
	if (last)
		last->NextProp = prop;
	else
		FirstProp = prop;

	// handle alignments, advance size
	unsigned propAlign = Type->TypeAlign;
	TypeSize = Align(TypeSize, propAlign);	// alignment for new prop
	prop->StructOffset = TypeSize;			// remember property offset
	if (ArraySize < 0)						//!! implement
		appError("dynamic arrays not yet implemented!");
	if (ArraySize > 0)						// update structure size
		TypeSize += Type->TypeSize * ArraySize;
	else
		TypeSize += Type->TypeSize;
	TypeAlign = max(TypeAlign, propAlign);	// update structure alignment requirement
	// update property counts
	NumProps++;
	TotalProps++;

	return prop;

	unguard;
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


//!! helper, should move
static void ReadString(CArchive &Ar, char *buf, int maxSize)
{
	int len;
	Ar << AR_INDEX(len);
	assert(len < maxSize);
	while (len-- > 0)
		Ar << *buf++;
	*buf = 0;
}


void ParseTypeinfoFile(CArchive &Ar)
{
	guard(ParseTypeinfoFile);

	while (true)
	{
		char TypeName[256];
		ReadString(Ar, ARRAY_ARG(TypeName));
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
//				appNotify("Reading enum [%s]", TypeName);	//!!
				while (true)
				{
					char EnumItem[256];
					ReadString(Ar, ARRAY_ARG(EnumItem));
					if (!EnumItem[0]) break;			// end marker
//					appNotify("  . [%s]", EnumItem);	//!!
					Enum->AddValue(EnumItem);
				}
			}
			break;

		case TYPE_CLASS:
		case TYPE_STRUCT:
			{
				CStruct *Struc;
				const CStruct *Parent = NULL;
				char ParentName[256];
				ReadString(Ar, ARRAY_ARG(ParentName));
				if (ParentName[0])
				{
					const CType *Parent2 = FindType(ParentName);
					if (!Parent2->IsStruc)
						appError("%s derived from non-structure type %s", TypeName, ParentName);
					Parent = (CStruct*)Parent2;
				}

				if (typeKind == TYPE_CLASS)
				{
					Struc = new (TypeChain) CClass(TypeName, (CClass*)Parent);	//!! here: CStruct->CSlass
				}
				else
				{
					Struc = new (TypeChain) CStruct(TypeName, Parent);
				}
//				appNotify("struct [%s] : [%s]", TypeName, ParentName);	//!!

				// read fields
				while (true)
				{
					char FieldName[256], FieldType[256], EditorGroup[256], Comment[1024];
					int ArrayDim;
					unsigned Flags;
					// read name
					ReadString(Ar, ARRAY_ARG(FieldName));
					if (!FieldName[0]) break;			// end marker
					// read other fields
					ReadString(Ar, ARRAY_ARG(FieldType));
					Ar << AR_INDEX(ArrayDim) << Flags;
					ReadString(Ar, ARRAY_ARG(Comment));
					ReadString(Ar, ARRAY_ARG(EditorGroup));
//					appNotify("  . field [%s] %s[%d] (F=%X) /* %s */ GRP=[%s]",
//						FieldName, FieldType, ArrayDim, Flags, Comment, EditorGroup);
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
			appError("unknown kind %d for type %s", typeKind, TypeName);
		}

		unguardf(("%s", TypeName));
	}

	unguard;
}


void InitTypeinfo(CArchive &Ar)
{
	TypeChain = new CMemoryChain;
	// register known types
	new CType("bool",   sizeof(bool)  );
	new CType("byte",   sizeof(byte)  );
	new CType("short",  sizeof(short) );
	new CType("int",    sizeof(int)   );
	new CType("float",  sizeof(float) );
	new CType("double", sizeof(double));
	new CType("string", 4);	//!!!!!!! IMPLEMENT STRINGS !!!!!
	// read typeinfo file
	ParseTypeinfoFile(Ar);
}


#if TEST
//!! remove this later, or make separate cpp

//!! NEEDS: ability to iterate all items + child subitems ...

void DumpStruc(CStruct *S, int indent = 0)
{
	if (indent > 32)
		indent = 32;
	char buf[256];
	memset(buf, ' ', 256);
	buf[indent+4] = 0;

	printf("%s### %s (sizeof=%02X, align=%d):\n%s{\n", buf+4, S->TypeName, S->TypeSize, S->TypeAlign, buf+4);

	for (int i = 0; /* empty */ ; i++)
	{
		const CProperty *Prop = S->IterateProps(i);
		if (!Prop) break;
		printf("%s[%2X] %-10s %s[%d]\n", buf, Prop->StructOffset, Prop->TypeInfo->TypeName, Prop->Name, Prop->ArrayDim);
		if (Prop->TypeInfo->IsStruc)
			DumpStruc((CStruct*)Prop->TypeInfo, indent+4);
	}

	printf("%s}\n", buf+4);
}


void main()
{
	try
	{
		CType BoolType  ("bool",   sizeof(bool)  );
		CType ByteType  ("byte",   sizeof(byte)  );
		CType IntType   ("short",  sizeof(short) );
		CType ShortType ("int",    sizeof(int)   );
		CType FloatType ("float",  sizeof(float) );
		CType DoubleType("double", sizeof(double));

		CStruct Vec("Vec");
			Vec.AddField("X", "float");
			Vec.AddField("Y", "float");
			Vec.AddField("Z", "float");
		Vec.Finalize();

		CStruct Coord("Coord");
			Coord.AddField("origin", "Vec");
			Coord.AddField("axis", "Vec", 3);
		Coord.Finalize();

		CStruct Struc("FStruc");
			Struc.AddField("org",  "Coord");
			Struc.AddField("field1", "int");
			Struc.AddField("field2", "float");
			Struc.AddField("field3", "short");
			Struc.AddField("field4", "bool");
		Struc.Finalize();

		DumpStruc(&Struc);
	}
	catch (...)
	{
		printf("ERROR: %s\n", GErrorHistory);
	}
}
#endif
