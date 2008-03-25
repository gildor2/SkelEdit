#undef DECLARE_CLASS		// defined in wxWidgets

#define ARCHIVE_VERSION		1

/*-----------------------------------------------------------------------------
	Base object class
-----------------------------------------------------------------------------*/

/**
 *	The base class of all objects
 */
class CObject
{
public:
	virtual ~CObject()
	{}
	virtual void PostLoad()
	{}
	virtual void PostEditChange()
	{}
	virtual const char* GetClassName() const;
	virtual void Serialize(CArchive &Ar);

protected:
	bool InternalLoad(const char *From);
};

void SerializeObject(CObject *Obj, CArchive &Ar);


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
			if (!Obj->InternalLoad(From))			\
			{										\
				delete Obj;							\
				return NULL;						\
			}										\
			return Obj;								\
			unguardf(("%s", From));					\
		}											\
		static Class* StaticConstructor()			\
		{											\
			return new Class;						\
		}											\
	private:										\
		friend CArchive& operator<<(CArchive &Ar, Class *&Res) \
		{											\
			return Ar << *(CObject**)&Res;			\
		}


/*-----------------------------------------------------------------------------
	Class registry
-----------------------------------------------------------------------------*/

//?? TODO: combine with CoreTypeinfo tables - but will require typeinfo
//??		to be loaded event in non-editor

struct CClassInfo
{
	const char *Name;
	CObject* (*Constructor)();
};

#define BEGIN_CLASS_TABLE						\
	{											\
		static CClassInfo Table[] = {
#define REGISTER_CLASS(Class)					\
			{ #Class+1, (CObject* (*) ())Class::StaticConstructor },
#define END_CLASS_TABLE							\
		};										\
		RegisterClasses(ARRAY_ARG(Table));		\
	}

void RegisterClasses(CClassInfo *Table, int Count);
CObject *CreateClass(const char *Name);
