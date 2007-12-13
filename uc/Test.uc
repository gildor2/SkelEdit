class Test;

/*
	Class modifiers:
	----------------
	extends OtherClass
	abstract				do not allow instantiation, do not include in "new property instance" list ...
	noexport				do not create C++ header for this class (created separately)
	hidecategories(list)	do not show listed property categories in editor (for parent class fields)
	exportstructs			export all structs declared in this class to C++ header; this is equivalent to
							declaring all structs in the class as "native export"
	[no]editinlinenew		allow class creation from editor; propagated to all childs, can be overriden with no...
	dependson(class list)	force to compile script after this class(es)
	native					create C++ code for class; this class should be derived from another native class


	Variable modifiers:
	-------------------
	var						not editable prop
	var()					editable prop
	var(Group)				editable prop, displayed in property editor in "Group"
	editconst				do not allow to edit in editor
	editfixedsize			disallow resizing of dynamic array variable
	noexport				do not export field, should be explicitly descrived in [struct]cpptext
	transient				do not serialize property
	native					do not serialize as property, serialized by C++ code


	Struct modifiers:
	-----------------
	native					create structure in C++ header (C++ structure name = "F" + StructName)
	extends OtherStruct		derived structure


	Native types:
	-------------
	bool
	byte
	int
	float
	pointer					syntax: "pointer varname" or "pointer varname{type}"
							note: {type} will be used in C++ header, internally ignored
	string
	type[size]				static array
	array<type>				dynamic array
	struct
	[StructOrClassName]


	UE extra native types:
	----------------------
	name
	vector
	rotator
	class<ClassName>		RTTI object
	class'Name'				will load class instance from package


	Other notes:
	------------
	cpptext {}				describe C++ code for class
	structcpptext {}		describe C++ code for non-class structure
	structdefaultproperties {}		create constructor for native structure?
*/

/**
 * Sample variable with hint
 * (this is a multi-line comment test)
 */
var(Test) int	IntegerValue;

/**
 * Editable IntegerValueEd
 */
var()	int		IntegerValueEd;

/**
 * Editable float
 */
var()	float	FloatValue;


//!! should be exported
enum ESomeEnum
{
	SE_VALUE1,
	SE_VALUE2,
	SE_VALUE3
};

/**
 * Enumeration value
 */
var()	ESomeEnum EnumValue;

cpptext
{
//	void Serialize(FArchive &Ar);
}


struct /*native*/ Vector		// -> FVector
{
	var() float X, Y, Z;
structcpptext
{
	void Zero()
	{
		X = Y = Z = 0;
	}
	void Set(float _X, float _Y, float _Z)
	{
		X = _X; Y = _Y; Z = _Z;
	}
}
};

struct /*native*/ Plane extends Vector
{
	var() float W;
};

struct /*native*/ Coords
{
	/** this is a origin */
	var() Vector origin;
	/** this is an orientation */
	var() Vector axis[3];
	/** change alignment */
	var bool StrangeVar;
	/** native no-export variable */
	var noexport int SomeNativePointer;
structcpptext
{
	// native implementation of SomeNativePointer
	/*FSomeNativeStruc*/ void *SomePointer;
	// constructor
	FCoords()
	{}
	// Zero coords
	void Zero()
	{
		origin.Zero();
		axis[0].Zero();
		axis[1].Zero();
		axis[2].Zero();
	}
#if DEBUG
	void CheckCoords();
#endif
}
};


/** plane comment for Origin */
var(Orientation)	Vector	Origin;
var(Orientation)	Coords	Orient;
/**
 *	This is a very long, long, long comment. To make it longer:
 *	Class modifiers:
	----------------
	extends OtherClass
	abstract				do not allow instantiation, do not include in "new property instance" list ...
	noexport				do not create C++ header for this class (created separately)
	hidecategories(list)	do not show listed property categories in editor (for parent class fields)
	exportstructs			export all structs declared in this class to C++ header; this is equivalent to
							declaring all structs in the class as "native export"
	[no]editinlinenew		allow class creation from editor; propagated to all childs, can be overriden with no...
	dependson(class list)	force to compile script after this class(es)
	native					create C++ code for class; this class should be derived from another native class
 */
var()				string	Description;
var(Finish)			int		FinishField;
var(Test)			int		SecondTestField;

//var					BadType BadField;

/*!!
defaultproperties
{
	IntegerValue   = 1
	IntegerValueEd = 2
	FloatValue     = 1.2345
	Origin         = (X=1,Y=2,Z=3)
	EnumValue      = SE_VALUE1
	Description    = "Sample description"
}
*/
