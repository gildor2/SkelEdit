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

template<class T> inline void QSort(T* array, int count, int (*cmpFunc)(const T*, const T*))
{
	qsort(array, count, sizeof(T), (int (*)(const void*, const void*)) cmpFunc);
}

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


#define VECTOR_ARG(name)	name[0],name[1],name[2]
#define ARRAY_ARG(array)	array, sizeof(array)/sizeof(array[0])
#define ARRAY_COUNT(array)	(sizeof(array)/sizeof(array[0]))

#define FVECTOR_ARG(v)		v.X, v.Y, v.Z
#define FROTATOR_ARG(r)		r.Yaw, r.Pitch, r.Roll

#define BYTES4(a,b,c,d)	((a) | ((b)<<8) | ((c)<<16) | ((d)<<24))


#define assert(x)	\
	if (!(x))		\
	{				\
		appError("assertion failed: %s\n", #x); \
	}


#define M_PI				(3.14159265358979323846)


#define min(a,b)  (((a) < (b)) ? (a) : (b))
#define max(a,b)  (((a) > (b)) ? (a) : (b))
#define bound(a,minval,maxval)  ( ((a) > (minval)) ? ( ((a) < (maxval)) ? (a) : (maxval) ) : (minval) )

#define appFloor(x)		( (int)floor(x) )
#define appCeil(x)		( (int)ceil(x) )


#if _MSC_VER
#	define vsnprintf		_vsnprintf
#	define FORCEINLINE		__forceinline
#	define NORETURN			__declspec(noreturn)
	// disable some warnings
#	pragma warning(disable : 4291)			// no matched operator delete found
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


union color_t
{
	byte	c[4];
	unsigned rgba;
};


void appError(char *fmt, ...);

// log some interesting information
void appSetNotifyHeader(const char *fmt, ...);
void appNotify(char *fmt, ...);

int  appSprintf(char *dest, int size, const char *fmt, ...);
void appStrncpyz(char *dst, const char *src, int count);
void appStrcatn(char *dst, int count, const char *src);

char *appStrdup(const char *str);
char *appStrdup(const char *str, CMemoryChain *chain);


/*-----------------------------------------------------------------------------
	Memory management
-----------------------------------------------------------------------------*/

void* appMalloc(int size);
void  appFree(void *ptr);

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

#define DEFAULT_ALIGNMENT	8
#define MEM_CHUNK_SIZE		0x2000		// 8Kb


class CMemoryChain
{
private:
	CMemoryChain *next;
	int		size;
	byte	*data;
	byte	*end;
public:
	void* Alloc(size_t size, int alignment = DEFAULT_ALIGNMENT);
	// creating chain
	void* operator new(size_t size, int dataSize = MEM_CHUNK_SIZE);
	// deleting chain
	void operator delete(void* ptr);
	// stats
	int GetSize();
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

#define guardSlow		guard
#define unguardSlow		unguard
#define unguardfSlow	unguardf

#define	THROW_AGAIN		throw
#define THROW			throw 1

void appUnwindPrefix(const char *fmt);		// not vararg (will display function name for unguardf only)
NORETURN void appUnwindThrow(const char *fmt, ...);

extern char GErrorHistory[2048];


/*-----------------------------------------------------------------------------
	FCompactIndex class for serializing objects in a compactly, mapping
	small values to fewer bytes.
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
	{}

	virtual ~CArchive()
	{}

	virtual void Seek(int Pos) = NULL;
	virtual bool IsEof() = NULL;
	virtual void Serialize(void *data, int size) = NULL;

	bool IsStopper()
	{
		return ArStopper == ArPos;
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
		Ar.Serialize(&B, 2);
		return Ar;
	}
	friend CArchive& operator<<(CArchive &Ar, word &B)
	{
		Ar.Serialize(&B, 2);
		return Ar;
	}
	friend CArchive& operator<<(CArchive &Ar, int &B)
	{
		Ar.Serialize(&B, 4);
		return Ar;
	}
	friend CArchive& operator<<(CArchive &Ar, unsigned &B)
	{
		Ar.Serialize(&B, 4);
		return Ar;
	}
	friend CArchive& operator<<(CArchive &Ar, float &B)
	{
		Ar.Serialize(&B, 4);
		return Ar;
	}

//	virtual CArchive& operator<<(FName &N) = NULL;
//	virtual CArchive& operator<<(UObject *&Obj) = NULL;
};

void SerializeChars(CArchive &Ar, char *buf, int length);

/*-----------------------------------------------------------------------------
	TArray template
-----------------------------------------------------------------------------*/

class CArray
{
public:
	CArray()
	:	DataCount(0)
	,	MaxCount(0)
	,	DataPtr(NULL)
	{}
	~CArray()
	{
		if (DataPtr)
			delete[] DataPtr;
		DataPtr   = NULL;
		DataCount = 0;
		MaxCount  = 0;
	}

	int Num() const
	{
		return DataCount;
	}

	void Empty(int count, int elementSize)
	{
		if (DataPtr)
			delete[] DataPtr;
		DataPtr   = NULL;
		DataCount = 0;
		MaxCount  = count;
		if (count)
		{
			DataPtr = new byte[count * elementSize];
			memset(DataPtr, 0, count * elementSize);
		}
	}

protected:
	void	*DataPtr;
	int		DataCount;
	int		MaxCount;

	void Insert(int index, int count, int elementSize)
	{
		assert(index >= 0);
		assert(index <= DataCount);
		assert(count > 0);
		// check for available space
		if (DataCount + count > MaxCount)
		{
			// not enough space, resize ...
			int prevCount = MaxCount;
			MaxCount = ((DataCount + count + 7) / 8) * 8 + 8;
			DataPtr = realloc(DataPtr, MaxCount * elementSize);
			// zero added memory
			memset(
				(byte*)DataPtr + prevCount * elementSize,
				0,
				(MaxCount - prevCount) * elementSize
			);
		}
		// move data
		memmove(
			(byte*)DataPtr + (index + count)     * elementSize,
			(byte*)DataPtr + index               * elementSize,
							 (DataCount - index) * elementSize
		);
		// last operation: advance counter
		DataCount += count;
	}

	void Remove(int index, int count, int elementSize)
	{
		assert(index >= 0);
		assert(count > 0);
		assert(index + count <= DataCount);
		// move data
		memcpy(
			(byte*)DataPtr + index                       * elementSize,
			(byte*)DataPtr + (index + count)             * elementSize,
							 (DataCount - index - count) * elementSize
		);
		// decrease counter
		DataCount -= count;
	}
};

// NOTE: this container cannot hold objects, required constructor/destructor
// (at least, Add/Insert/Remove functions are not supported, but can serialize
// such data)
template<class T> class TArray : public CArray
{
public:
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
		assert(Ar.IsLoading);	//?? saving requires more code
		A.Empty();
		int Count;
		Ar << AR_INDEX(Count);
		T* Ptr = new T[Count];
		A.DataPtr   = Ptr;
		A.DataCount = Count;
		A.MaxCount  = Count;
		for (int i = 0; i < Count; i++)
			Ar << *Ptr++;
		return Ar;
		unguard;
	}
};




#include "Math3D.h"
#include "Object.h"


#endif // __CORE_H__
