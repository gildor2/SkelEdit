/*-----------------------------------------------------------------------------
	Implementation of Core function links to game engine
-----------------------------------------------------------------------------*/

#include "Private.h"
#include "AnimClasses.h"			// for REGISTER_ANIM_CLASSES only ...


/*-----------------------------------------------------------------------------
  Output device and error hook
-----------------------------------------------------------------------------*/

#define BUFFER_LEN      16384		// allocated in stack, so - not very small, but not too large ...

#define FORMAT_BUF(fmt,buf)     \
  va_list	argptr;               \
  va_start(argptr, fmt);        \
  char buf[BUFFER_LEN];         \
  vsnprintf(ARRAY_ARG(buf), fmt, argptr); \
  va_end(argptr);


class COutputDeviceRtl : public COutputDevice
{
private:
  int recurse;

public:
  virtual void Write(const char *str)
  {
    g_log << str;
  }
};


void appError(char *fmt, ...)
{
  FORMAT_BUF(fmt, buf);
  Sys_Error("%s", buf);
}

void appSetNotifyHeader(const char *fmt, ...)
{}


void appNotify(char *fmt, ...)
{
  FORMAT_BUF(fmt, buf);
  g_wrn << fmt << "\n";
}



/*-----------------------------------------------------------------------------
  Memory management
-----------------------------------------------------------------------------*/

#if SEPARATE_MEMSPACE

namespace skel
{
  rtl::MemSpace g_mem;
}

#if MEM_DEBUG
#define MEM_ALLOCATOR(msp, firstarg)   { if (!(msp).m_allocator)  (msp).m_allocator = GET_RETADDR(firstarg); }
#else
#define MEM_ALLOCATOR(msp, firstarg)
#endif

#endif // SEPARATE_MEMSPACE

// note: NOINLINE is used below for easier debugging and smaller code (whan calling appMalloc() from THIS module)

NOINLINE void *appMalloc(int size)
{
  assert(size >= 0);
#if SEPARATE_MEMSPACE
  assert(skel::g_mem.IsCreated());
  MEM_ALLOCATOR(g_mem, size);
  void *ptr = skel::g_mem.Alloc(size);
#else
  void *ptr = rtl::Alloc(size);
#endif
  memset(ptr, 0, size);    // required at least for TArray<> (nested arrays: constructor will not be called)
  return ptr;
}


NOINLINE void *appRealloc(void *ptr, int size)
{
  assert(size >= 0);
#if SEPARATE_MEMSPACE
  assert(skel::g_mem.IsCreated());
  MEM_ALLOCATOR(skel::g_mem, ptr);
  return skel::g_mem.Realloc(ptr, size);
#else
  return rtl::Realloc(ptr, size);
#endif
}


NOINLINE void appFree(void *ptr)
{
#if SEPARATE_MEMSPACE
  skel::g_mem.Free(ptr);
#else
  rtl::Free(ptr);
#endif
}


//------------------------------------------------------
// Hacky CMemoryChain -> rtl::MemChain link.
// Note: we cannot make 'new rtl::MemChain', because MemChain's 'operator new' is
// protected for security reasons; so - we will construct it 'by hands'.
//------------------------------------------------------

#define CHAIN(data)   ((rtl::MemChain*) ((data)->dummy))

ASSERT_CT(sizeof(CMemoryChain) >= sizeof(rtl::MemChain));


NOINLINE void* CMemoryChain::operator new(size_t size, int dataSize)
{
  // construct rtl::MemChain inside CMemoryChain structure (space is reserved as 'dummy' field)
#if SEPARATE_MEMSPACE
  rtl::MemChain *chain = (rtl::MemChain*)skel::g_mem.Alloc(size);
#else
  rtl::MemChain *chain = (rtl::MemChain*)rtl::Alloc(size);
#endif
  memset(chain, 0, size);
#if MEM_DEBUG
  chain->m_allocator = GET_RETADDR(size);
#endif
  return chain;
}


NOINLINE void CMemoryChain::operator delete(void *ptr)
{
  CHAIN((CMemoryChain*)ptr)->FreeAll();
#if SEPARATE_MEMSPACE
  skel::g_mem.Free(ptr);
#else
  rtl::Free(ptr);
#endif
}


NOINLINE void *CMemoryChain::Alloc(size_t size, int alignment)
{
  return CHAIN(this)->Alloc(size, alignment);
}


NOINLINE int CMemoryChain::GetSize() const
{
  return CHAIN(this)->GetSize();
}



/*-----------------------------------------------------------------------------
  CObject
-----------------------------------------------------------------------------*/

// this is a copy of CObject::InternalLoad() from Core/Object.cpp with replaced
// CFile -> CFileRtl
bool CObject::InternalLoad(const char *From)
{
	CFileRtl Ar(From, true, false);
	if (!Ar.IsOpen()) return false;
	SerializeObject(this, Ar);
	return true;
}


/*-----------------------------------------------------------------------------
  Math converters
-----------------------------------------------------------------------------*/

// verify vector size for Convert(CVec3) -> mt::Vector3 (inline)
ASSERT_CT(sizeof(CVec3) >= sizeof(mt::Vector3));


mt::MatrixAf Convert(const CCoords &c)
{
  return mt::MatrixAf
  (
    VECTOR_ARG(c.axis[0]),
    VECTOR_ARG(c.axis[1]),
    VECTOR_ARG(c.axis[2]),
    VECTOR_ARG(c.origin)
  );
}


/*-----------------------------------------------------------------------------
  Interfaces
-----------------------------------------------------------------------------*/

namespace skel
{


static COutputDeviceRtl *g_logger = NULL;


void Init()
{
  // init Core
  appInit();
  // register types
  BEGIN_CLASS_TABLE
    REGISTER_ANIM_CLASSES
  END_CLASS_TABLE
  // hook Core logging to rtl g_log
  g_logger = new COutputDeviceRtl();
  g_logger->Register();
#if SEPARATE_MEMSPACE
  // initialize memory management
  skel::g_mem.Create("skel");
#endif

  appPrintf("*** SkelSystem Core initialized ***\n");
}


void Done()
{
  // remove log hook
  delete g_logger;
  g_logger = NULL;
}


} // namespace
