// forwards
class CProperty;
class CObject;

#undef DECLARE_CLASS		// defined in wxWidgets

#define ARCHIVE_VERSION		1

/*-----------------------------------------------------------------------------
	Type serialization consts
	-- move to cpp ??
-----------------------------------------------------------------------------*/

enum
{
	TYPE_SCALAR,
	TYPE_ENUM,
	TYPE_STRUCT,
	TYPE_CLASS
};


// should correspond to parse.pl PROP_XXX declarations
#define PROP_EDITABLE		1
#define PROP_EDITCONST		2
#define PROP_NORESIZE		4
#define PROP_NOEXPORT		8
#define PROP_TRANSIENT		16
#define PROP_NATIVE			32


/*-----------------------------------------------------------------------------
	Type information
-----------------------------------------------------------------------------*/

/**
 *	Generic typeinfo class
 */
class CType
{
public:
	CType(const char *AName, unsigned ASize, unsigned AAlign = 0);

	/**
	 *	Name of type
	 */
	const char*	TypeName;

	/**
	 *	Size of single variable of this type
	 */
	short		TypeSize;

	/**
	 *	Required alignment of type in memory
	 */
	short		TypeAlign;

	/**
	 *	Set to "true" for complex types
	 */
	bool		IsStruc;
	/**
	 *	Set to "true" for enumeration types
	 */
	bool		IsEnum;
};


/**
 *	Enumeration declaration
 */
class CEnum : public CType
{
public:
	CEnum(const char *AName)
	:	CType(AName, 1, 1)
	{
		IsEnum = true;
	}

	void AddValue(const char *Name);

	TArray<const char*> Names;
};


/**
 *	Structure declaration
 */
class CStruct : public CType
{
public:
	CStruct(const char *AName, const CStruct *AParent = NULL)
	:	CType(AName, 0, 0)
	,	FirstProp(NULL)
	,	NumProps(0)
	,	TotalProps(0)
	,	ParentStruct(AParent)
	{
		IsStruc = true;
		// take into account parent class definition
		if (ParentStruct)
		{
			TotalProps = ParentStruct->NumProps;
			TypeSize   = ParentStruct->TypeSize;
			TypeAlign  = ParentStruct->TypeAlign;
		}
	}

	// structure creation
	CProperty *AddField(const char *AName, const char *ATypeName, int ArraySize = 0);
	void       AddField(CProperty *Prop);
	void Finalize();
	/**
	 *	Field enumeration
	 *	Properly handles inheritance from parent classes.
	 *	Returns NULL when no property with such index exists.
	 */
	const CProperty *IterateProps(int Index) const;

	/**
	 * Dump structure information to console (debugging)
	 */
	void Dump(int indent = 0) const;
	/**
	 * Write structure member values to provided text output
	 */
	void WriteText(COutputDevice *Out, void *Data, int Indent = 0) const;
	/**
	 * Read structure member values from text buffer. Return false when failed.
	 * Should successufully read text, created with WriteText().
	 */
	bool ReadText(const char* SrcText, void *Data) const;

protected:
	const CStruct *ParentStruct;
	int			NumProps;			// num of props in this struct excluding parent class
	int			TotalProps;			// num of props including parent class
	CProperty	*FirstProp;

	bool ReadText(CSimpleParser &Text, void *Data, int Level) const;
};


/*?? is separate "CStruct" and "CClass" needed?
	? class has "Constructor" and can be instantiated
	? class should be top, structs should be used inside classes only
	- class can have 'virtual void UpdateProperties()', called by editor
 */
#if 0
/**
 *	Class declaration
 */
class CClass : public CStruct
{
public:
	CClass(const char *AName, const CClass *AParent = NULL)
	:	CStruct(AName, AParent)
	,	ClassFlags(0)
	{}
	unsigned	ClassFlags;

protected:
	CObject* (*Constructor)();
};
#endif


/*-----------------------------------------------------------------------------
	Structure or class field descriptions
-----------------------------------------------------------------------------*/

/**
 *	Typeinfo for concrete structure/class single property
 */
class CProperty
{
	friend CStruct;

public:
	CProperty(const char *AName, const CType *AType, int AArrayDim = 0);

	/**
	 *	Name of property
	 */
	const char	*Name;

	/**
	 *	Type information for property
	 */
	const CType	*TypeInfo;

	/**
	 *	If this is an array property, its size specified here. Possible values are:
	 *		0		scalar
	 *	   -1		dynamic array
	 *		1..N	static array
	 */
	int			ArrayDim;

	/**
	 *	Offset in a owner structure/class
	 */
	unsigned	StructOffset;

	/* editor properties */

	bool		IsEditable;		// do not show in editor, when 'false'
	bool		IsReadonly;
	const char *Comment;
	const char *Category;

protected:
	/**
	 *	Link to next property inside structure of class
	 */
	CProperty	*NextProp;
};


/*-----------------------------------------------------------------------------
	Base object class
-----------------------------------------------------------------------------*/

/**
 *	The base class of all objects
 */
class CObject
{
public:
	virtual void PostLoad();
	virtual void PostEditChange();
	virtual const char* GetClassName() const;
	virtual void Serialize(CArchive &Ar);

protected:
	void InternalLoad(const char *From);
};


void InitTypeinfo(CArchive &Ar);
const CType *FindType(const char *Name, bool ShouldExist = true);

inline const CStruct *FindStruct(const char *Name)
{
	const CType *Type = FindType(Name);
	if (!Type->IsStruc)
		appError("FindStruct(%s): not structure type", Name);
	return (CStruct*)Type;
}


#define DECLARE_BASE_CLASS(Class)					\
	public:											\
		typedef Class	ThisClass;


#define DECLARE_CLASS(Class,Base)					\
	public:											\
		typedef Class	ThisClass;					\
		typedef Base	Super;						\
		virtual const char* GetClassName() const	\
		{											\
			return #Class+1;						\
		}											\
		static Class* LoadObject(const char *From)	\
		{											\
			guard(Class::LoadObject);				\
			Class *Obj = new Class;					\
			Obj->InternalLoad(From);				\
			return Obj;								\
			unguardf(("%s", From));					\
		}
