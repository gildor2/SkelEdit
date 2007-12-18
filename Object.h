// forwards
class CProperty;
class CObject;


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
#define PROP_NOEXPORT		4
#define PROP_TRANSIENT		8
#define PROP_NATIVE			16


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
	void Finalize();
	/**
	 *	Field enumeration
	 *	Properly handles inheritance from parent classes.
	 *	Returns NULL when no property with such index exists.
	 */
	const CProperty *IterateProps(int Index) const;

protected:
	const CStruct *ParentStruct;
	int			NumProps;			// num of props in this struct excluding parent class
	int			TotalProps;			// num of props including parent class
	CProperty	*FirstProp;
};


/*?? is separate "CStruct" and "CClass" needed?
	? class has "Constructor" and can be instantiated
	? class should be top, structs should be used inside classes only
	- class can have 'virtual void UpdateProperties()', called by editor
 */
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


/*-----------------------------------------------------------------------------
	Structure or class field descriptions
-----------------------------------------------------------------------------*/

/**
 *	Base typeinfo for concrete structure/class single property
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


#if 0
// UNUSED NOW

class CBoolProperty : public CProperty
{
public:
};


class CEnumProperty : public CProperty
{
public:
};


class CNumericProperty : public CProperty
{
public:
	//?? use for byte, [u]short, [u]int, float, double
};


//!! CStrProperty


/**
 *	Structure, embedded into class
 */
class CStructProperty : public CProperty
{
public:
};

#endif


/*-----------------------------------------------------------------------------
	Base object class
-----------------------------------------------------------------------------*/

/**
 *	The base class of all objects
 */

class CObject
{
private:
	CClass		*TypeInfo;
};


void InitTypeinfo(CArchive &Ar);
const CType *FindType(const char *Name, bool ShouldExist = true);
