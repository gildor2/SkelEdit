#ifndef __SKEL_INTERFACE_H__
#define __SKEL_INTERFACE_H__

/*-----------------------------------------------------------------------------
	Interface functions for game engine
-----------------------------------------------------------------------------*/

#include "TMDoll/doll_model.h"             // doll::IDoll
#include "TMDoll/doll_visual.h"            // doll::IVisual

#include "TMKernel/k_decals.h"             // eg::SMDDecalsHolder
#include "TMRenderer/r_staticobjects.h"    // r::VisualizeProps

#include "TMRtl/rtl_comptr.h"              // ref::ComPtr<> for obbset (wgd)


#define MAX_DOLL_SHADER_PROPS   3


/*-----------------------------------------------------------------------------
  Forward declarations for some classes
-----------------------------------------------------------------------------*/

class CSkelMeshInstance;

namespace wgd
{
  class IEngine;
  class IOBBSet;
}


/*-----------------------------------------------------------------------------
  IDoll wrapper
-----------------------------------------------------------------------------*/

namespace skel
{


void Init();
void Done();


// resource storage classes declared in doll_model.h
struct Mesh;
struct Anim;


Mesh* LoadMesh(const char *name);
void UnloadMesh(Mesh* pMesh);

Anim* LoadAnim(const char *name);
void UnloadAnim(Anim* pAnim);


class Doll
: public doll::IDoll
, public doll::IVisual
, public dbg::User3D
{
public:
  Doll(fpp<r::Renderer> pRenderer, fpp<r::Scene> pScene, const doll::DollInitDesc &desc,
    wgd::IEngine* pWGEngine);
  virtual ~Doll();

  virtual Doll* GetDoll2()
  {
    return this;
  }
  virtual doll::IVisual* GetVisual()
  {
    return this;
  }

  /*
   * IDoll interface
   */
  virtual void Release()
  {
    delete this;
  }
  virtual class doll::Doll* GetAdvancedInterface()
  {
    // obsolete
    return NULL;
  }
  virtual doll::ISkeleton* GetSkeleton()
  {
    // obsolete
    return NULL;
  }
  virtual doll::IProps* GetProps()
  {
    // obsolete
    return NULL;
  }
  virtual void TurnSubmesh(const tmstring& name, bool bEnable)
  {
    // obsolete
  }
  virtual void MaskSubmesh(const tmstring& name)
  {
    // obsolete
  }
  virtual eg::InstanceSMD* HACK_GetInstanceSMD()
  {
    // obsolete
    return NULL;
  }

  virtual eg::Entity* GetEntity()
  {
    return m_pEntity;
  }

  virtual tmstring Debug_GetCaption(uint i);

  virtual void TimeTick(uint time);
  virtual void SetTransform(const mt::MatrixAf &m);
  virtual void TurnRagdoll(const char *ragset, IDoll *pTransformSource = NULL);
  virtual void TurnOffRagdoll();
  virtual void SetRagDollSkipEntities(const std::vector<eg::EntityRef> &vEntities);
  virtual bool RaiseRagDoll();

  virtual void TurnWGD(bool s);

  virtual void AccImpulse
  (
    const mt::Vector3& vecImpulse,
    const mt::Vector3& vecPoint,
    uint iObb,
    eg::Damage::DAMAGE_TYPE dg
  );

  virtual void BinarySerialize(io::Stream& ar);

  /*
   * IVisual interface
   */
  virtual void SetSkin(const tmstring& name);
  virtual const tmstring& GetSkin() const
  {
    return m_vProps.m_sSkinName;
  }
  virtual void AddToScene(fpp<r::Scene> scene);
  virtual void RemoveFromScene();
  virtual bool GetBox(mt::Box3D *pBox) const
  {
    *pBox = *m_vProps.m_pBox;
    return true;
  }
  virtual void Dissolve(float sec) const;
  virtual void UpdateVisualData()
  {
    // obsolete; used internally by Doll; not needed to be a part of interface
  }
  virtual void SetRenderFlags(uint newflag, const tmstring &sLODSDist, const tmstring &sShadowLODSDist);
  virtual void SetShaderAttributes(const mt::Color4& prop, uint channel);
  virtual fpp<r::SceneObject> GetSceneObject() const
  {
    return m_vProps.GetSceneObject();
  }
  virtual eg::BaseDecalsHolder* QueryDecalsHolder()
  {
    return &m_decalHolder;
  }

  /*
   * New functions; virtual to allow work without linking to TMDoll (sigh ...)
   */
  virtual void SetAnim(doll::AnimRef anim);
  virtual float GetMeshScale() const;

  // hitbox access

  inline int GetHitboxCount() const
  {
    return m_hitBoxes.size();
  }
  inline const mt::Matrix4 &GetHitbox(int index) const
  {
    return m_hitBoxes[index];
  }
  virtual const char* GetHitboxName(int index) const;
  // get index of hitbox owner bone
  virtual int GetHitboxBone(int index) const;

  // get current skeleton configuration

  virtual mt::MatrixAf GetBoneMatrix(int index) const;
  // get inversed matrix of specified bone in reference (bind) pose
  virtual mt::MatrixAf GetRefBoneMatrixInv(int index) const;

protected:
  // animation system resources
  Mesh                     *m_meshRef;
  Anim                     *m_animRef;
  CSkelMeshInstance        *m_inst;        // real animation object
  const CSkeletalMesh      *m_mesh;        // == m_meshRef->m_mesh
  const CAnimSet           *m_anim;        // == m_animRef->m_anim
  // other subsystem pointers
  eg::Entity               *m_pEntity;
  // renderer fields
  r::VisualizeProps         m_vProps;
  uint                      m_renderFlags;
  std::vector<mt::MatrixAf> m_smdMatrices; // matrices for rendering, transforms vertices from bind pose to final pose
  eg::SMDDecalsHolder       m_decalHolder;
  // WGD state
  mt::Box3D                 m_box;         // world bounding box
  std::vector<mt::Matrix4>  m_hitBoxes;    // used to send hitboxes to wgd
  ref::ComPtr<wgd::IOBBSet> m_pObbSet;     // wgd object itself
  // state
  mt::MatrixAf              m_worldMatrix; // orientation in world
  mt::Vector4               m_shaderProps[MAX_DOLL_SHADER_PROPS];
  uint                      m_curTime;     // last time value, passed with TimeTick()

  /*
   * Debug rendering
   */
  void ShowDebug(dbg::Context* pCntx);
  DBG_DECLARE_CB_TABLE();
};


doll::IDoll *CreateDoll(fpp<r::Renderer> pRenderer, fpp<r::Scene> pScene, const doll::DollInitDesc& desc,
  wgd::IEngine* pWGEngine);

} // namespace


#endif // __SKEL_INTERFACE_H__
