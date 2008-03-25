#ifndef __CORE_H__
#define __CORE_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>


// undefine some windows defines
#undef min
#undef max
#undef assert
#undef M_PI


#define RIPOSTE_COORDS		1			// Y=up, 1 unit ~ 1 meter etc


// forwards
class CArchive;
class CMemoryChain;
class CObject;


/*-----------------------------------------------------------------------------
	Some macros
-----------------------------------------------------------------------------*/

template<class T> inline T Align(const T ptr, int alignment)
{
	return (T) (((unsigned)ptr + alignment - 1) & ~(alignment - 1));
}

template<class T> inline T OffsetPointer(const T ptr, int offset)
{
	return (T) ((unsigned)ptr + offset);
}

template<class T> inline void Exchange(T& A, T& B)
{
	const T tmp = A;
	A = B;
	B = tmp;
}

#if !RIPOSTE
template<class T> inline void QSort(T* array, int count, int (*cmpFunc)(const T*, const T*))
{
	qsort(array, count, sizeof(T), (int (*)(const void*, const void*)) cmpFunc);
}
#endif

// field offset macros
// get offset of the field in struc
#ifdef offsetof
#	define FIELD2OFS(struc, field)		(offsetof(struc, field))				// more compatible
#else
#	define FIELD2OFS(struc, field)		((unsigned) &((struc *)NULL)->field)	// just in case
#endif
// get field of type by offset inside struc
#define OFS2FIELD(struc, ofs, type)	(*(type*) ((byte*)(struc) + ofs))


#define EXEC_ONCE(code)		\
	{						\
		static bool _flg = false; \
		if (!_flg) {		\
			_flg = true;	\
			code;			\
		}					\
	}


#define EXPAND_BOUNDS(a,minval,maxval)	\
	if (a < minval) minval = a;			\
	if (a > maxval) maxval = a;

#define VECTOR_ARG(name)	name[0],name[1],name[2]
#define ARRAY_ARG(array)	array, sizeof(array)/sizeof(array[0])
#define ARRAY_COUNT(array)	(sizeof(array)/sizeof(array[0]))

// use "STR(any_value)" to convert it to string (may be float value)
#define STR2(s) #s
#define STR(s) STR2(s)

#define BYTES4(a,b,c,d)	((a) | ((b)<<8) | ((c)<<16) | ((d)<<24))


#define assert(x)							\
	if (!(x))								\
	{										\
		appError("assertion failed: %s\n"	\
				 "  file: %s\n"				\
				 "  function: %s\n"			\
				 "  line: %d",				\
				 #x, __FILE__, __FUNCTION__, __LINE__); \
	}

#ifdef RETAIL
// RETAIL build of game engine
//?? change macro?
#undef assert
#define assert(x)
#endif


#define M_PI				(3.14159265358979323846)
#define BIG_NUMBER			0x1000000


#define min(a,b)  (((a) < (b)) ? (a) : (b))
#define max(a,b)  (((a) > (b)) ? (a) : (b))
#define bound(a,minval,maxval)  ( ((a) > (minval)) ? ( ((a) < (maxval)) ? (a) : (maxval) ) : (minval) )

//!! make fast functions
#define appFloor(x)		( (int)floor(x) )
#define appCeil(x)		( (int)ceil(x) )
#define appRound(x)		( (int) (x >= 0 ? (x)+0.5f : (x)-0.5f) )


#if _MSC_VER

#	define vsnprintf		_vsnprintf
#	define FORCEINLINE		__forceinline
#	define NORETURN			__declspec(noreturn)
	// disable some warnings
#	pragma warning(disable : 4018)			// signed/unsigned mismatch
#	pragma warning(disable : 4127)			// conditional expression is constant
#	pragma warning(disable : 4244)			// conversion from 'int'/'double' to 'float'
//#	pragma warning(disable : 4251)			// class 'Name' needs to have dll-interface to be used by clients of class 'Class'
#	pragma warning(disable : 4291)			// no matched operator delete found
#	pragma warning(disable : 4305)			// truncation from 'const double' to 'const float'

#	if _MSC_VER >= 1300
#		define NOINLINE			__declspec(noinline)
#	else
#		define NOINLINE						// no way ...
#	endif

#endif

#ifndef _XBOX
#	define LITTLE_ENDIAN	1
#else
#	define LITTLE_ENDIAN	0
#endif

// necessary types
typedef unsigned char		byte;
typedef unsigned short		word;


#define COLOR_ESCAPE	'^'		// may be used for quick location of color-processing code

#define S_BLACK			"^0"
#define S_RED			"^1"
#define S_GREEN			"^2"
#define S_YELLOW		"^3"
#define S_BLUE			"^4"
#define S_MAGENTA		"^5"
#define S_CYAN			"^6"
#define S_WHITE			"^7"


void appPrintf(const char *fmt, ...);
void appError(char *fmt, ...);

// log some interesting information
void appSetNotifyHeader(const char *fmt, ...);
void appNotify(char *fmt, ...);

void* LoadFile(const char* filename);

void appInit();


// Output device
class COutputDevice
{
public:
	bool	FlushEveryTime;		// when true, Flush() will be called after every Write()
	COutputDevice()
	:	FlushEveryTime(false)
	{}
	virtual ~COutputDevice()
	{}
	virtual void Write(const char *str) = 0;
	virtual void Flush()
	{ /* empty */ }
	virtual void Close()		// may be used instead of destructor
	{ /* empty */ }
	void Printf(const char *fmt, ...);
	void Register();
	void Unregister();
};

extern COutputDevice	*GLog;			// output via appPrintf()
extern COutputDevice	*GNull;			// do not output


/*-----------------------------------------------------------------------------
	Memory management
-----------------------------------------------------------------------------*/

void* appMalloc(int size);
void* appRealloc(void *ptr, int size);
void  appFree(void *ptr);

#if !RIPOSTE

FORCEINLINE void* operator new(size_t size)
{
	return appMalloc(size);
}

FORCEINLINE void* operator new[](size_t size)
{
	return appMalloc(size);
}

FORCEINLINE void operator delete(void* ptr)
{
	appFree(ptr);
}

#endif

#define DEFAULT_ALIGNMENT	8
#define MEM_CHUNK_SIZE		0x2000		// 8Kb


class CMemoryChain
{
private:
#if EDITOR
	CMemoryChain *next;
	int		size;
	byte	*data;
	byte	*end;
#else
	// Safe hack to link to rtl::MemChain; MemChain 'operator new' is protected,
	// so we needs some kind of hack to link CMemoryChain -> MemChain.
	// Same (or greater) members size than MemChain.
	byte	dummy[sizeof(size_t) + sizeof(int)];
#endif
public:
	void* Alloc(size_t size, int alignment = DEFAULT_ALIGNMENT);
	// creating chain
	void* operator new(size_t size, int dataSize = MEM_CHUNK_SIZE);
	// deleting chain
	void operator delete(void* ptr);
	// stats
	int GetSize() const;
};


FORCEINLINE void* operator new(size_t size, CMemoryChain *chain, int alignment = DEFAULT_ALIGNMENT)
{
	return chain->Alloc(size, alignment);
}

FORCEINLINE void* operator new[](size_t size, CMemoryChain *chain, int alignment = DEFAULT_ALIGNMENT)
{
	return chain->Alloc(size, alignment);
}


/*-----------------------------------------------------------------------------
	Crash helpers
-----------------------------------------------------------------------------*/

#if EDITOR

// C++excpetion-based guard/unguard system
#define guard(func)						\
	{									\
		static const char __FUNC__[] = #func; \
		try {

#define unguard							\
		} catch (...) {					\
			appUnwindThrow(__FUNC__);	\
		}								\
	}

#define unguardf(msg)					\
		} catch (...) {					\
			appUnwindPrefix(__FUNC__);	\
			appUnwindThrow msg;			\
		}								\
	}

#else

// do not use guards in game engine ...
#define guard(func)		{
#define unguard			}
#define unguardf(msg)	}

#endif

#define guardSlow		guard
#define unguardSlow		unguard
#define unguardfSlow	unguardf

#define	THROW_AGAIN		throw
#define THROW			throw 1

void appUnwindPrefix(const char *fmt);		// not vararg (will display function name for unguardf only)
NORETURN void appUnwindThrow(const char *fmt, ...);

extern char GErrorHistory[2048];


/*-----------------------------------------------------------------------------
	Helper CCompactIndex class for serializing objects in a compactly,
	mapping small values to fewer bytes. Binary format is compatible with
	Unreal Engine FCompactIndex.
-----------------------------------------------------------------------------*/

class CCompactIndex
{
public:
	int		Value;
	friend CArchive& operator<<(CArchive &Ar, CCompactIndex &I);
};

#define AR_INDEX(intref)	(*(CCompactIndex*)&(intref))


/*-----------------------------------------------------------------------------
	CArchive class
-----------------------------------------------------------------------------*/

class CArchive
{
public:
	bool	IsLoading;
	int		ArVer;
	int		ArPos;
	int		ArStopper;

	CArchive()
	:	ArStopper(0)
	,	ArVer(9999)			//?? something large
	,	ArPos(0)
	{}

	virtual ~CArchive()
	{}

	virtual void Seek(int Pos) = NULL;
	virtual bool IsEof() = NULL;
	virtual void Serialize(void *data, int size) = NULL;
#if LITTLE_ENDIAN
	FORCEINLINE void ByteOrderSerialize(void *data, int size)
	{
		Serialize(data, size);
	}
#else
	void ByteOrderSerialize(void *data, int size);
#endif

	bool IsStopper()
	{
		return ArStopper == ArPos;
	}

	friend CArchive& operator<<(CArchive &Ar, bool &B)
	{
		Ar.Serialize(&B, 1);
		return Ar;
	}
	friend CArchive& operator<<(CArchive &Ar, char &B)
	{
		Ar.Serialize(&B, 1);
		return Ar;
	}
	friend CArchive& operator<<(CArchive &Ar, byte &B)
	{
		Ar.Serialize(&B, 1);
		return Ar;
	}
	friend CArchive& operator<<(CArchive &Ar, short &B)
	{
		Ar.ByteOrderSerialize(&B, 2);
		return Ar;
	}
	friend CArchive& operator<<(CArchive &Ar, word &B)
	{
		Ar.ByteOrderSerialize(&B, 2);
		return Ar;
	}
	friend CArchive& operator<<(CArchive &Ar, int &B)
	{
		Ar.ByteOrderSerialize(&B, 4);
		return Ar;
	}
	friend CArchive& operator<<(CArchive &Ar, unsigned &B)
	{
		Ar.ByteOrderSerialize(&B, 4);
		return Ar;
	}
	friend CArchive& operator<<(CArchive &Ar, float &B)
	{
		Ar.ByteOrderSerialize(&B, 4);
		return Ar;
	}
	friend CArchive& operator<<(CArchive &Ar, CObject *&Obj);
};

void SerializeChars(CArchive &Ar, char *buf, int length);

/*-----------------------------------------------------------------------------
	TArray template
-----------------------------------------------------------------------------*/

/*
 * NOTES:
 *	- CArray/TArray should not contain objects with virtual tables (no
 *	  constructor/destructor support)
 *	- should not use new[] and delete[] here, because compiler will alloc
 *	  additional 'count' field for correct delete[], but we uses appMalloc/
 *	  appFree calls.
 */

class CArray
{
	friend class WPropEdit;		// access from editor's PropertyGrid
	friend class CStruct;		// full access to properties from typeinfo support classes

public:
	CArray()
	:	DataCount(0)
	,	MaxCount(0)
	,	DataPtr(NULL)
	{}
	~CArray()
	{
		if (DataPtr)
			appFree(DataPtr);
		DataPtr   = NULL;
		DataCount = 0;
		MaxCount  = 0;
	}

	int Num() const
	{
		return DataCount;
	}

	void Empty(int count, int elementSize);

protected:
	void	*DataPtr;
	int		DataCount;
	int		MaxCount;

	void Insert(int index, int count, int elementSize);
	void Remove(int index, int count, int elementSize);
};

// NOTE: this container cannot hold objects, required constructor/destructor
// (at least, Add/Insert/Remove functions are not supported, but can serialize
// such data)
template<class T> class TArray : public CArray
{
public:
	~TArray()
	{
		// destruct all array items
		T *P, *P2;
		for (P = (T*)DataPtr, P2 = P + DataCount; P < P2; P++)
			P->~T();
	}
	// data accessors
	T& operator[](int index)
	{
		assert(index >= 0 && index < DataCount);
		return *((T*)DataPtr + index);
	}
	const T& operator[](int index) const
	{
		assert(index >= 0 && index < DataCount);
		return *((T*)DataPtr + index);
	}

	int Add(int count = 1)
	{
		int index = DataCount;
		CArray::Insert(index, count, sizeof(T));
		return index;
	}

	void Insert(int index, int count = 1)
	{
		CArray::Insert(index, count, sizeof(T));
	}

	void Remove(int index, int count = 1)
	{
		// destruct specified array items
		T *P, *P2;
		for (P = (T*)DataPtr + index, P2 = P + count; P < P2; P++)
			P->~T();
		// remove items from array
		CArray::Remove(index, count, sizeof(T));
	}

	int AddItem(const T& item)
	{
		int index = Add();
		(*this)[index] = item;
		return index;
	}

	void Empty(int count = 0)
	{
		CArray::Empty(count, sizeof(T));
	}

	// serializer
	friend CArchive& operator<<(CArchive &Ar, TArray &A)
	{
		guard(TArray<<);
		T *Ptr;
		if (Ar.IsLoading)
		{
			// array loading
			A.Empty();
			int Count;
			Ar << AR_INDEX(Count);
			if (Count)
				Ptr = (T*)appMalloc(sizeof(T) * Count);
			else
				Ptr = NULL;
			A.DataPtr   = Ptr;
			A.DataCount = Count;
			A.MaxCount  = Count;
		}
		else
		{
			// array saving
			Ar << AR_INDEX(A.DataCount);
			Ptr = (T*)A.DataPtr;
		}
		for (int i = 0; i < A.DataCount; i++)
			Ar << *Ptr++;
		return Ar;
		unguard;
	}
};


template<class T> FORCEINLINE void* operator new(size_t size, TArray<T> &Array)
{
	guard(TArray::operator new);
	assert(size == sizeof(T));
	int index = Array.Add(1);
	return &Array[index];
	unguard;
}


#include "Strings.h"
#include "Math3D.h"
#include "Commands.h"
#include "ScriptParser.h"
#include "CoreTypeinfo.h"
#include "Object.h"

typedef CVec3 CColor3f;


extern TString<128> GRootDir;

#endif // __CORE_H__
