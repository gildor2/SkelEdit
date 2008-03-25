#include "Private.h"
#include "AnimClasses.h"
#include "Interface.h"


#define MAX_MESHES      256
#define MAX_ANIM_SETS   256


namespace skel
{


// empty array items are marked with m_name==""
static Mesh g_meshes[MAX_MESHES];
static Anim g_anims[MAX_ANIM_SETS];


Mesh* LoadMesh(const char *name)
{
  if (!name || !name[0])
    return NULL;

  int i;
  Mesh *pMesh, *pFree = NULL;
  for (i = 0, pMesh = g_meshes; i < MAX_MESHES; i++, pMesh++)
  {
    if (pMesh->m_name.empty())
    {
      if (!pFree)
        pFree = pMesh;
      continue;
    }
    if (!com::stricmp(pMesh->m_name.c_str(), name))
      return pMesh;
  }
  if (!pFree)
  {
    g_wrn.Printf("skel::LoadMesh(%s): more than %d meshes\n", name, MAX_MESHES);
    return NULL;
  }

  CSkeletalMesh *mesh = CSkeletalMesh::LoadObject(name);
  if (!mesh)
  {
    g_err.Printf("skel::LoadMesh(%s): loading error\n", name);
    return NULL;
  }

  pMesh = pFree;
  pMesh->m_name     = name;
  pMesh->m_refCount = 0;
  pMesh->m_mesh     = mesh;
  return pMesh;
}


void UnloadMesh(Mesh* pMesh)
{
  if (!pMesh)
    return;
  TM_VERIFYE(pMesh->m_refCount == 0);
  if (pMesh->m_mesh)
    delete pMesh->m_mesh;
  pMesh->m_mesh = NULL;
  pMesh->m_name = "";
}


Anim* LoadAnim(const char *name)
{
  if (!name || !name[0])
    return NULL;

  int i;
  Anim *pAnim, *pFree = NULL;
  for (i = 0, pAnim = g_anims; i < MAX_ANIM_SETS; i++, pAnim++)
  {
    if (pAnim->m_name.empty())
    {
      if (!pFree)
        pFree = pAnim;
      continue;
    }
    if (!com::stricmp(pAnim->m_name.c_str(), name))
      return pAnim;
  }
  if (!pFree)
  {
    g_wrn.Printf("skel::LoadAnim(%s): more than %d animsets\n", name, MAX_ANIM_SETS);
    return NULL;
  }

  CAnimSet *anim = CAnimSet::LoadObject(name);
  if (!anim)
  {
    g_err.Printf("skel::LoadAnim(%s): loading error\n", name);
    return NULL;
  }
  pAnim = pFree;
  pAnim->m_name     = name;
  pAnim->m_refCount = 0;
  pAnim->m_anim     = anim;
  return pAnim;
}


void UnloadAnim(Anim* pAnim)
{
  if (!pAnim)
    return;
  TM_VERIFYE(pAnim->m_refCount == 0);
  if (pAnim->m_anim)
    delete pAnim->m_anim;
  pAnim->m_anim = NULL;
  pAnim->m_name = "";
}



} // namespace
