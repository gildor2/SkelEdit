#include "Core.h"
#include "FileReaderStdio.h"			// for CObject::InternalLoad()


#define MAX_CLASS_NAME		256


/*-----------------------------------------------------------------------------
	CObject class
-----------------------------------------------------------------------------*/

const char* CObject::GetClassName() const
{
	return "Object";
}


void CObject::Serialize(CArchive &Ar)
{
	/* Streamed class layout:
	 *	string		ClassName
	 *	byte		PropCount	(reserved!!)
	 */
	//?? TODO: move ClassName serialization outside
	TString<MAX_CLASS_NAME> ClassName;
	ClassName = GetClassName();

	if (Ar.IsLoading)
	{
		// verify class name
		TString<MAX_CLASS_NAME> LoadName;
		Ar << LoadName;
		if (ClassName != LoadName)
			appError("Expected class \"%s\", but found \"%s\"", *ClassName, *LoadName);
	}
	else
	{
		// store object header
		Ar << ClassName;
	}
	//!! reserved place for props
	byte tmp = 0;
	Ar << tmp;
	assert(tmp == 0);
}


/*-----------------------------------------------------------------------------
	Object serialization support
-----------------------------------------------------------------------------*/

static TArray<CObject*> ObjSerialize;

/*
 *	Serialize object link. Format in CArchive:
 *		1) string	ObjectClassName
 *		2) index	SerializeIndex
 *	All objects will be serialized immediately after main serializable object,
 *	in order of appearance. Link will be represented as position of object
 *	in serialization list.
 *	Note: ObjectClassName is stored for each link, but really required for
 *	1st object appearance only.
 *	Other approach is to create separate table with object information, and
 *	store this table in contiguous file space (current implementation spreads
 *	this 'table' between objects).
 */

CArchive& operator<<(CArchive &Ar, CObject *&Obj)
{
	guard(operator<<(CObject*));

	TString<MAX_CLASS_NAME> ClassName;
	int Index;

	if (Ar.IsLoading)
	{
		Ar << AR_INDEX(Index) << ClassName;
		if (Index >= ObjSerialize.Num())
		{
			// new object link, create empty object
			assert(Index == ObjSerialize.Num());
			Obj = CreateClass(ClassName);
			ObjSerialize.AddItem(Obj);
		}
		else
		{
			// object already created, duplicate link
			Obj = ObjSerialize[Index];
			assert(ClassName == Obj->GetClassName());
		}
	}
	else
	{
		// saving
		ClassName = Obj->GetClassName();
		// find object in ObjSerialize[]
		for (Index = 0; Index < ObjSerialize.Num(); Index++)
			if (ObjSerialize[Index] == Obj) break;
		if (Index >= ObjSerialize.Num())		// new object link
			Index = ObjSerialize.AddItem(Obj);
		Ar << ClassName << AR_INDEX(Index);
	}

	return Ar;
	unguard;
}


//?? try to reduce function count: InternalLoad(), SerializeObject() ...
void SerializeObject(CObject *Obj, CArchive &Ar)
{
	guard(SerializeObject);

	// NOTE: should not remove entries from ObjSerialize[] until end of process
	// (required by operator<<(Ar,Obj) function)

	// serialize archive version
	if (Ar.IsLoading)
	{
		// get archive version
		Ar << AR_INDEX(Ar.ArVer);
		if (Ar.ArVer <= 0 || Ar.ArVer > ARCHIVE_VERSION)
			appError("Loading file of a newer version %d, current version is "STR(ARCHIVE_VERSION), Ar.ArVer);
	}
	else
	{
		Ar.ArVer = ARCHIVE_VERSION;
		Ar << AR_INDEX(Ar.ArVer);
	}

	// serialize main object
	Obj->Serialize(Ar);
	// serialize subobjects, if needed
	int index = 0;
	while (index < ObjSerialize.Num())
		ObjSerialize[index++]->Serialize(Ar);

	// call PostLoad() for all loaded objects
	if (Ar.IsLoading)
	{
		Obj->PostLoad();
		for (index = 0; index < ObjSerialize.Num(); index++)
			ObjSerialize[index]->PostLoad();
	}

	// remove all objects from ObjSerialize list
	ObjSerialize.Empty();
	unguard;
}


#if EDITOR

bool CObject::InternalLoad(const char *From)
{
	CFile Ar(From);
	if (!Ar.IsOpen()) return false;
	SerializeObject(this, Ar);
	return true;
}

#endif


/*-----------------------------------------------------------------------------
	Class registry
-----------------------------------------------------------------------------*/

static CClassInfo* GClasses    = NULL;
static int         GClassCount = 0;

void RegisterClasses(CClassInfo *Table, int Count)
{
	assert(GClasses == NULL);		// no multiple tables
	GClasses    = Table;
	GClassCount = Count;
}


CObject *CreateClass(const char *Name)
{
	for (int i = 0; i < GClassCount; i++)
		if (!strcmp(GClasses[i].Name, Name))
		{
			CObject *Obj = GClasses[i].Constructor();
			return Obj;
		}
	return NULL;
}
