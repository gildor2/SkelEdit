#ifndef __ANIM_PRIVATE_H__
#define __ANIM_PRIVATE_H__


#if EDITOR
#error EDITOR should not be defined to 1
#endif

// disable some VC8+ warnings
#define _SCL_SECURE_NO_WARNINGS
// disable some Core stuff
#define RIPOSTE			1


/*
 * Riposte engine headers
 */

#include "Personal.h"

#include "tmrtl/rtl_base.h"
#include "tmrtl/rtl_bstream.h"
#include "tmmath/mt_math.h"
#include "tmmath/mt_matrix.h"


/*
 * Core and animation headers
 */
#include "Core.h"


/*
 * File wrapper class
 */
class CFileRtl : public CArchive
{
public:
  CFileRtl(const char *FileName, bool Loading = true, bool isText = false)
  {
    uint mode = io::SM_NOEXCEPT;
    IsLoading = Loading;
    mode |= Loading ? io::SM_READ : io::SM_WRITE;
    if (isText)
      mode |= io::SM_TEXT;
    m_file.Open(FileName, mode);
  }

  virtual ~CFileRtl()
  {
    Close();
  }

  void Close()
  {
    m_file.Close();
  }

  virtual void Seek(int Pos)
  {
    ArPos = m_file.SetPos(Pos);
  }

  virtual bool IsEof()
  {
    return m_file.eof();
  }

  bool IsOpen()
  {
    return m_file.IsOpen();
  }

protected:
  io::FileStream m_file;

  virtual void Serialize(void *data, int size)
  {
    int res;
    if (IsLoading)
      res = m_file.RawRead(data, size);
    else
      res = m_file.RawWrite(data, size);
    ArPos = size;
    if (ArStopper > 0 && ArPos > ArStopper)
      appError("CFileRtl(%s): Serializing behind stopper", m_file.GetName().c_str());
    if (res != size)
      appError("CFileRtl(%s): Unable to serialize data", m_file.GetName().c_str());
  }
};


/*
 * Math type converters
 */

// note: has a copy of this function in msh_fileio.cpp
TMFORCE const mt::Vector3& Convert(const CVec3& v)
{
  return (mt::Vector3&)v;
}

mt::MatrixAf Convert(const CCoords& c);


//#define SEPARATE_MEMSPACE   1
/* Currently has problem: CObject::Load(), called from Riposte code, will create
 * object using local rtl::Alloc() (operator new, defined in rtl_new.h), but
 * destroy object on appFree() function (compiler will generate destructor in
 * animation area).
 */


#if SEPARATE_MEMSPACE

namespace skel
{
  extern rtl::MemSpace g_mem;
}

#define SKEL_MEM    (skel::g_mem)

#else  // SEPARATE_MEMSPACE

#define SKEL_MEM

#endif // SEPARATE_MEMSPACE


#endif // __ANIM_PRIVATE_H__
