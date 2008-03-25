// forwards
class CProperty;
//class CObject;


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
	 *	Set to "true" for structural types
	 */
	bool		IsStruc;
	/**
	 *	Set to "true" for enumeration types
	 */
	bool		IsEnum;
	/**
	 *	Reading/writting properties as text. Default implementation does nothing and
	 *	returns false. Overriden method should return true.
	 */
	virtual bool WriteProp(const CProperty *Prop, COutputDevice &Out, void *Data) const;
	virtual bool ReadProp(const CProperty *Prop, const char *Text, void *Data) const;
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
	 *	Search for specific field. When field does not exists, return NULL
	 */
	const CProperty *FindProp(const char *Name) const;
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
	/**
	 * Call (emulate) destructor for data pointer of known type
	 */
	void DestructObject(void *Data);

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
	CProperty(CStruct *AOwner, const char *AName, const CType *AType, int AArrayDim = 0);

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
	//!! FIX: strings uses ArrayDim as size

	/**
	 *	Owner structure
	 */
	CStruct		*Owner;
	/**
	 *	Offset in a owner structure/class
	 */
	unsigned	StructOffset;

	/* editor properties */

	bool		IsEditable;			// do not show in editor, when 'false'
	bool		IsReadonly;
	bool		IsAddAllowed;		// for dynamic arrays
	bool		IsDeleteAllowed;	// for dynamic arrays
	const char *Comment;
	const char *Category;

	inline bool IsArray() const
	{
		return ArrayDim != 0;
	}
	inline bool IsDynamicArray() const
	{
		return ArrayDim == -1;
	}
	inline bool IsStaticArray() const
	{
		return (ArrayDim > 1) && strcmp(TypeInfo->TypeName, "string") != 0;	//!! FIX: remove 'string' dependency
	}

protected:
	/**
	 *	Link to next property inside structure of class
	 */
	CProperty	*NextProp;
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
