#include "Private.h"

#include "TMWorldGDB/wg_base.h"

#include "AnimClasses.h"
#include "SkelMeshInstance.h"
#include "Interface.h"


namespace skel
{


/*-----------------------------------------------------------------------------
  IDoll implementation
-----------------------------------------------------------------------------*/

void Doll::TimeTick(uint time)
{
  int i;

  float timeDelta = 0;
  if (time)
  {
    if (m_curTime)
      timeDelta = (time - m_curTime) / 1000.0f;
    m_curTime = time;
  }
  // if time == 0 -> tick with timeDelta = 0

  m_inst->UpdateAnimation(timeDelta);

  // copy bone matrices
  if (m_mesh)
  {
    int numBones  = m_mesh->Skeleton.Num();
    int numBBoxes = m_mesh->BoundingBoxes.Num();

    for (i = 0; i < numBones; i++)
      m_smdMatrices[i] = Convert(m_inst->GetBoneTransform(i));
    m_vProps.ApplyProperties(r::VISPROP_SMD_MATRICES);

    // update wgd ObbSet
    if (numBBoxes)
    {
      for (i = 0; i < numBBoxes; i++)
      {
        // get hitbox from model
        const CMeshHitBox &HitBox = m_mesh->BoundingBoxes[i];
        CCoords HC = HitBox.Coords;
        const CCoords &BC = m_inst->GetBoneCoords(HitBox.BoneIndex);
        // Convert to engine matrix:
        // - inside anim system box stored as coordinate system with box in {-0.5,-0.5,-0.5}-{0.5,0.5,0.5}
        // - inside riposte box stored as coordinate system with box in {-1,-1,-1}-{1,1,1}
        // So, scale axes with 0.5
        HC.axis[0].Scale(0.5f);
        HC.axis[1].Scale(0.5f);
        HC.axis[2].Scale(0.5f);
        BC.UnTransformCoords(HC, HC);            // transform from bone space to model space
        // remember box
        mt::MatrixAf boxAf = Convert(HC);
        m_hitBoxes[i] = mt::Matrix4(boxAf);
        // update bounding box
        mt::Obb tmpBox(boxAf * m_worldMatrix);   // oriented box in world space
        mt::Box3D box(tmpBox);
        if (!i)
          m_box = box;
        else
          m_box += box;
      }
      // finish update
      m_pObbSet->SetOBBsTransform(m_hitBoxes);
    }
  }
}


tmstring Doll::Debug_GetCaption(uint i)
{
  return EMPTY_STRING;
}


void Doll::SetTransform(const mt::MatrixAf &m)
{
  m_worldMatrix = m;
  m_vProps.ApplyProperties(r::VISPROP_BOX | r::VISPROP_TRANSFORM);
  m_pObbSet->SetTransform(m);
  //?? check doll::Doll::SetTransform(): has special case for ragdoll
}


void Doll::TurnRagdoll(const char *ragset, IDoll *pTransformSource)
{
  // ANIM_TODO
}


void Doll::TurnOffRagdoll()
{
  // ANIM_TODO
}


void Doll::SetRagDollSkipEntities(const std::vector<eg::EntityRef> &vEntities)
{
  // ANIM_TODO
}


bool Doll::RaiseRagDoll()
{
  // ANIM_TODO
  return false;
}


void Doll::TurnWGD(bool s)
{
  m_pObbSet->Turn(s);
}


void Doll::AccImpulse
(
  const mt::Vector3& vecImpulse,
  const mt::Vector3& vecPoint,
  uint iObb,
  eg::Damage::DAMAGE_TYPE dg
)
{
  // ANIM_TODO
}


void Doll::BinarySerialize(io::Stream& ar)
{
  // ANIM_TODO
}


/*-----------------------------------------------------------------------------
  IVisual implementation
-----------------------------------------------------------------------------*/

void Doll::SetShaderAttributes(const mt::Color4& prop, uint channel)
{
  mt::Vector4 vec(prop.r, prop.g, prop.b, prop.a);
  TM_VERIFYE(channel < MAX_DOLL_SHADER_PROPS)
  static const uint flags[MAX_DOLL_SHADER_PROPS] =
  {
    r::VISPROP_ATTRIBUTES0,
    r::VISPROP_ATTRIBUTES1,
    r::VISPROP_ATTRIBUTES2
  };
  m_shaderProps[channel] = vec;
  m_vProps.m_pObjectAttributes0 = &m_shaderProps[channel];
  m_vProps.ApplyProperties(flags[channel]);
}


void Doll::SetSkin(const tmstring& name)
{
  m_vProps.m_sSkinName = name;
  if (m_vProps.IsAttached())
    m_vProps.ApplyProperties(r::VISPROP_SKINNAME);
}


void Doll::Dissolve(float sec) const
{
  //?? ANIM_TODO
}


void Doll::AddToScene(fpp<r::Scene> scene)
{
  //?? ANIM_TODO : Reupdate() -- call TimeTick() to ensure correct skeleton pose
  m_vProps.Attach(scene);
  m_decalHolder.SetVisible(true);
}


void Doll::RemoveFromScene()
{
  m_vProps.Attach(NULL);
  m_decalHolder.SetVisible(false);
}


void Doll::SetRenderFlags(uint newflag, const tmstring &sLODSDist, const tmstring &sShadowLODSDist)
{
  m_renderFlags = newflag;
  m_vProps.SetSMDProperty
  (
    m_pEntity->GetObjectUID(),
    m_pEntity->GetObjectString(),
    &m_worldMatrix,
    m_renderFlags,
    m_vProps.m_uVisMode,
    m_vProps.m_fClipDistance,
    m_vProps.m_fFadeRange,
    0,
    m_vProps.m_sSkinName,
    sLODSDist,
    sShadowLODSDist,
    m_vProps.m_pModel,
    m_vProps.m_pBox,
    &m_smdMatrices
  );
}


/*-----------------------------------------------------------------------------
  Doll implementation
-----------------------------------------------------------------------------*/

Doll::Doll(fpp<r::Renderer> pRenderer, fpp<r::Scene> pScene, const doll::DollInitDesc &desc,
  wgd::IEngine* pWGEngine)
: m_pEntity(desc.m_pEntity)
, m_curTime(0)
, m_meshRef(NULL)
, m_animRef(NULL)
, m_mesh(NULL)
, m_anim(NULL)
{
  m_inst = new CSkelMeshInstance;

  // load animation resources
  m_meshRef = LoadMesh(desc.m_sModelName.c_str());
  if (m_meshRef)
  {
    m_meshRef->m_refCount++;
    m_mesh = m_meshRef->m_mesh;
    m_inst->SetMesh(m_mesh);
    m_smdMatrices.resize(m_mesh->Skeleton.Num());
  }

  fpp<r::Model> pModel = pRenderer->CreateSMDMesh(desc.m_sModelName, pScene, m_pEntity->GetBundle());

  /*
   * init IVisual
   */
  m_renderFlags = desc.m_uiRenderFlags;
  m_decalHolder.Init(&m_worldMatrix, this);
  m_decalHolder.SetVisible(false);

  m_vProps.SetSMDProperty
  (
    m_pEntity->GetObjectUID(),
    m_pEntity->GetObjectString(),
    &m_worldMatrix,
    desc.m_uiRenderFlags,
    desc.m_uVisMode,
    desc.m_fClipDistance,
    desc.m_fFadeRange,
    desc.m_uLightLimit,
    desc.m_sSkinName,
    desc.m_sLODSDist,
    desc.m_sShadowLODSDist,
    pModel,
    &m_box,
    &m_smdMatrices
  );

  /*
   * init WGD structures
   */
  m_pObbSet = pWGEngine->CreateOBBSet(wgd::IOBBSetCreateStuct(desc.m_pEntity));
  if (desc.m_pEntity)
  {
    // strange code, copied from doll_obbset_impl.cpp
    uint contents = desc.m_pEntity->GetContentFlags();
    if ((contents & eg::CONTENTS_HERO) && !(contents & eg::CONTENTS_NETPLAYER))
      m_pObbSet->SetContent(wgd::WGD_FIRST_PERSON_INVISIBLE);
  }
  if (m_mesh)
  {
    // init wgd materials
    std::vector<tmstring> matNames;
    int numBoxes = m_mesh->BoundingBoxes.Num();
    matNames.reserve(numBoxes);
    EXEC_ONCE(g_wrn.Printf("HACK: setting common physmaterial\n");)
    for (int i = 0; i < numBoxes; i++)
//!!      matNames.push_back(tmstring_pool(m_mesh->BoundingBoxes[i].PhysMaterial));
      matNames.push_back(tmstring_pool("basephmtrl/meat/vinous"));
    m_pObbSet->SetPhysMaterials(matNames);
    m_hitBoxes.resize(numBoxes);
  }
  m_pObbSet->Turn(true);

  /*
   * init internal doll state
   */
  TimeTick(0);
}


Doll::~Doll()
{
  delete m_inst;

  // free animation resources
  if (m_meshRef)
  {
    if (--m_meshRef->m_refCount <= 0)
      UnloadMesh(m_meshRef);
  }
  if (m_animRef)
  {
    if (--m_animRef->m_refCount <= 0)
      UnloadAnim(m_animRef);
  }
}


void Doll::SetAnim(doll::AnimRef anim)
{
  if (m_inst && anim.m_anim)
  {
    if (m_animRef)
    {
      if (--m_animRef->m_refCount <= 0)
        UnloadAnim(m_animRef);
    }
    m_animRef = anim.m_anim;
    m_animRef->m_refCount++;
    m_anim = m_animRef->m_anim;
    m_inst->SetAnim(m_anim);

    EXEC_ONCE(g_wrn.Printf("HACK: starting animation\n");)
    m_inst->LoopAnim("Idle_Rest2");
/*    m_inst->LoopAnim("Idle_Taunt02", 1, 0, 1);
    m_inst->SetBlendParams(1, 1.0f, "Bip01 Spine1");
    m_inst->LoopAnim("WalkF", 1, 0, 2);
    m_inst->SetBlendParams(2, 1.0f, "Bip01 R Thigh");
    m_inst->LoopAnim("Gesture_Cheer", 1, 0, 4);
    m_inst->SetBlendParams(4, 1.0f, "Bip01 L UpperArm"); */
  }
}


float Doll::GetMeshScale() const
{
  return m_mesh ? fabs(m_mesh->MeshScale[0]) : 1;
}


const char* Doll::GetHitboxName(int index) const
{
  return m_mesh ? *m_mesh->BoundingBoxes[index].Name : NULL;
}


int Doll::GetHitboxBone(int index) const
{
  return m_mesh ? m_mesh->BoundingBoxes[index].BoneIndex : 0;
}


mt::MatrixAf Doll::GetBoneMatrix(int index) const
{
  return m_inst ? Convert(m_inst->GetBoneCoords(index)) : mt::MatrixAf::ms_identity;
}


mt::MatrixAf Doll::GetRefBoneMatrixInv(int index) const
{
  return m_mesh ? Convert(m_mesh->Skeleton[index].InvRefCoords) : mt::MatrixAf::ms_identity;
}


doll::IDoll *CreateDoll(fpp<r::Renderer> pRenderer, fpp<r::Scene> pScene, const doll::DollInitDesc& desc,
  wgd::IEngine* pWGEngine)
{
  // verify filename here, return NULL when 'foreign' file
  const tmchar *ext = com::strrchr(desc.m_sModelName.c_str(), '.');
  if (!ext || com::stricmp(ext+1, MESH_EXTENSION) != 0)
    return NULL;      // create old smd doll

  return new Doll(pRenderer, pScene, desc, pWGEngine);
}


/*-----------------------------------------------------------------------------
  Debug rendering
-----------------------------------------------------------------------------*/

DECLARE_AND_DEFINE_BB_VAR_HELP(skm_showSkel, "0", "D", "show mesh skeleton; 2=show bone labels");
DECLARE_AND_DEFINE_BB_VAR_HELP(skm_showBox,  "0", "D", "show mesh bounding box");

DBG_BEGIN_CB_TABLE(Doll)
  DBG_CALLBACK_NO_COND(&Doll::ShowDebug, UM_ALWAYS)
DBG_END_CB_TABLE


void Doll::ShowDebug(dbg::Context* pCntx)
{
  pCntx->Clear();

  if (skm_showSkel.GetAsInt())
  {
    for (int i = 0; i < m_mesh->Skeleton.Num(); i++)
    {
      const CMeshBone &B  = m_mesh->Skeleton[i];
      const CCoords   &BC = m_inst->GetBoneCoords(i);

      // draw line
      CVec3 v1;
      if (i > 0)
      {
        v1 = m_inst->GetBoneCoords(B.ParentIndex).origin;
      }
      else
      {
        v1.Zero();
      }
      pCntx->AddLine(dbg::DSH_TRAN_GREEN, Convert(v1) * m_worldMatrix, Convert(BC.origin) * m_worldMatrix);
      if (skm_showSkel.GetAsInt() == 2)
      {
        // show bone label
        Lerp(v1, BC.origin, 0.5f, v1);      // center point between v1 and BC.origin
        pCntx->AddTextLabel(dbg::DSH_TRAN_YELLOW, Convert(v1) * m_worldMatrix, tmstring_pool(*B.Name));
      }
    }
  }

  if (skm_showBox.GetAsInt())
    pCntx->AddBox(dbg::DSH_TRAN_CYAN, m_box);
}


} // namespace
