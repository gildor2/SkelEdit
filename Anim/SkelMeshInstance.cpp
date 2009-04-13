#include "Core.h"

#include "AnimClasses.h"
#include "SkelMeshInstance.h"

#if EDITOR
#	include "GlViewport.h"
#endif


#define SHOW_BONE_UPDATES			1


/* NOTES for multi-AnimSet support:
 *	- requires multiple CMeshBoneData.BoneMap items
 *	- requires multiple AnimSet pointers
 *  * can check UT3/Engine/Classes/AnimSet.uc AnimSetMeshLinkup structure
 */

/* NOTES for multi-mesh support:
 *	a) attach meshes as separate CSkelMeshInstance, link with pointers (one mesh
 *	   can be attached only once)
 *	b) may be, use array of CSkeletalMesh pointers
 *	   - this will require exactly the same bone indices in all linked meshes, or
 *		 mechanism to remap bones
 */

/* NOTES for AnimTree support
 *	- replaces animation channels
 *	- will require
 *	  a) duplicate whole AnimTree for all mesh instances
 *	  b) use special holder for application-tured data (coefficients for animation
 *		 selection)
 *	- AnimSet pointers array will be placed in AnimTree, not in mesh instance
 *	* check UT3/Engine/SkeletalMeshComponent.uc for ideas
 */


/*-----------------------------------------------------------------------------
	Local skeleton state data
-----------------------------------------------------------------------------*/

struct CMeshBoneData
{
	// static data (computed after mesh loading)
	int			BoneMap;			// index of bone in AnimSet

	// dynamic data

	// skeleton configuration
	float		Scale;				// bone scale; 1=unscaled
	int			FirstChannel;		// first animation channel, affecting this bone
	// current pose
	CCoords		Coords;				// current coordinates of bone, model-space
	CCoords		Transform;			// used to transform vertex from reference pose to current pose
	// data for tweening; bone-space
	CVec3		Pos;				// current position of bone
	CQuat		Quat;				// current orientation quaternion
};


/*-----------------------------------------------------------------------------
	Create/destroy
-----------------------------------------------------------------------------*/

CSkelMeshInstance::~CSkelMeshInstance()
{
	if (pMesh)
	{
		delete BoneData;
		delete MeshVerts;
		delete MeshNormals;
	}
}


void CSkelMeshInstance::ClearSkelAnims()
{
	// init 1st animation channel with default pose
	for (int i = 0; i < MAX_SKELANIMCHANNELS; i++)
	{
		Channels[i].Anim1          = NULL;
		Channels[i].Anim2          = NULL;
		Channels[i].SecondaryBlend = 0;
		Channels[i].BlendAlpha     = 1;
		Channels[i].RootBone       = 0;
	}
}


void CSkelMeshInstance::SetMesh(const CSkeletalMesh *Mesh)
{
	guard(CSkelMeshInstance::SetMesh);

	bool prevMesh = pMesh != NULL;
	pMesh = Mesh;

	// get some counts
	int i;
	int NumBones = pMesh->Skeleton.Num();
	int NumVerts = pMesh->Lods[0].Points.Num();	// data for software skinning
	for (i = 1; i < pMesh->Lods.Num(); i++)
	{
		if (NumVerts < pMesh->Lods[i].Points.Num())
		{
			appNotify("Mesh with LOD[%d] vertex count greater, than LOD[0]", i);
			NumVerts = pMesh->Lods[i].Points.Num();
		}
	}

	// allocate some arrays
	if (prevMesh)
	{
		delete BoneData;
		delete MeshVerts;
		delete MeshNormals;
	}
	BoneData    = new CMeshBoneData[NumBones];
	MeshVerts   = new CVec3        [NumVerts];
	MeshNormals = new CVec3        [NumVerts];

	CMeshBoneData *data;
	for (i = 0, data = BoneData; i < NumBones; i++, data++)
	{
		// set reference bone in animation track (uninitialized value)
		data->BoneMap = -1;
		// initialize skeleton configuration
		data->Scale = 1.0f;			// default bone scale
	}

	ClearSkelAnims();
	PlayAnim(NULL);

	unguard;
}


void CSkelMeshInstance::SetAnim(const CAnimSet *Anim)
{
	if (!pMesh) return;				// may be, bad mesh ... should revise for multi-mesh support

	pAnim = Anim;
//??	assert(pMesh);
	assert(pAnim);

	// prepare animation <-> mesh bone map
	for (int i = 0; i < pMesh->Skeleton.Num(); i++)
	{
		const CMeshBone &B = pMesh->Skeleton[i];
		BoneData[i].BoneMap = -1;
		// find reference bone in animation track
		for (int j = 0; j < pAnim->TrackBoneName.Num(); j++)
			if (!stricmp(B.Name, pAnim->TrackBoneName[j].Name))	// case-insensitive compare
			{
				BoneData[i].BoneMap = j;
				break;
			}
	}
}


/*-----------------------------------------------------------------------------
	Miscellaneous
-----------------------------------------------------------------------------*/

const CMeshAnimSeq *CSkelMeshInstance::FindAnim(const char *AnimName) const
{
	if (!pAnim || !AnimName)
		return NULL;
	// note: when multiple AnimSet's will be used, handle them here
	// UT3 uses last-to-first search, so 2nd AnimSet will override animations of 1st
	return pAnim->FindAnim(AnimName);
}


void CSkelMeshInstance::SetBoneScale(const char *BoneName, float scale)
{
	guard(CSkelMeshInstance::SetBoneScale);
	assert(pMesh);
	int BoneIndex = pMesh->FindBone(BoneName);
	if (BoneIndex < 0) return;
	BoneData[BoneIndex].Scale = scale;
	unguard;
}


bool CSkelMeshInstance::IsAnimating(int Channel)
{
	const CAnimChan &chn = GetStage(Channel);
	if (!chn.Rate) return false;
	return (chn.Anim1 != NULL);
}


const CCoords &CSkelMeshInstance::GetBoneCoords(int BoneIndex) const
{
	assert(pMesh);
	assert(BoneIndex >= 0 && BoneIndex < pMesh->Skeleton.Num());
	return BoneData[BoneIndex].Coords;
}


const CCoords &CSkelMeshInstance::GetBoneTransform(int BoneIndex) const
{
	assert(pMesh);
	assert(BoneIndex >= 0 && BoneIndex < pMesh->Skeleton.Num());
	return BoneData[BoneIndex].Transform;
}


/*-----------------------------------------------------------------------------
	Skeletal animation itself
-----------------------------------------------------------------------------*/

#if SHOW_BONE_UPDATES
static int BoneUpdateCounts[MAX_MESH_BONES];
#endif

void CSkelMeshInstance::UpdateSkeleton()
{
	guard(CSkelMeshInstance::UpdateSkeleton);

	// process all animation channels
	assert(MaxAnimChannel < MAX_SKELANIMCHANNELS);
	int Stage;
	CAnimChan *Chn;
#if SHOW_BONE_UPDATES
	memset(BoneUpdateCounts, 0, sizeof(BoneUpdateCounts));
#endif
	for (Stage = 0, Chn = Channels; Stage <= MaxAnimChannel; Stage++, Chn++)
	{
		if (Stage > 0 && (!Chn->Anim1 || Chn->BlendAlpha <= 0))
			continue;
		// NOTE: if Stage==0 and animation is not assigned, we will get here anyway

		float Time2;
		if (Chn->Anim1 && Chn->Anim2 && Chn->SecondaryBlend)
		{
			// compute time for secondary channel; always in sync with primary channel
			Time2 = Chn->Time / Chn->Anim1->NumFrames * Chn->Anim2->NumFrames;
		}

		// compute bone range, affected by specified animation bone
		int firstBone = Chn->RootBone;
		int lastBone  = firstBone + pMesh->Skeleton[firstBone].SubtreeSize;
		assert(lastBone < pMesh->Skeleton.Num());

		int i;
		CMeshBoneData *data;
		for (i = firstBone, data = BoneData + firstBone; i <= lastBone; i++, data++)
		{
			const CMeshBone &B = pMesh->Skeleton[i];

			if (Stage < data->FirstChannel)
			{
				// this bone position will be overrided in following channel(s); all
				// subhierarchy bones should be overrided too; skip whole subtree
				int skip = B.SubtreeSize;
				// note: 'skip' equals to subtree size; current bone is excluded - it
				// will be skipped by 'for' operator (after 'continue')
				i    += skip;
				data += skip;
				continue;
			}

			CVec3 BP;
			CQuat BO;
			int BoneIndex = data->BoneMap;
			// compute bone orientation
			if (Chn->Anim1 && BoneIndex >= 0)
			{
				// get bone position from track
				if (!Chn->Anim2 || Chn->SecondaryBlend != 1.0f)
				{
#if SHOW_BONE_UPDATES
					BoneUpdateCounts[i]++;
#endif
					Chn->Anim1->GetBonePosition(BoneIndex, Chn->Time, Chn->Looped, BP, BO);
				}
				// blend secondary animation
				if (Chn->Anim2 && Chn->SecondaryBlend > 0.0f)
				{
					CVec3 BP2;
					CQuat BO2;
#if SHOW_BONE_UPDATES
					BoneUpdateCounts[i]++;
#endif
					Chn->Anim2->GetBonePosition(BoneIndex, Time2, Chn->Looped, BP2, BO2);
					if (Chn->SecondaryBlend == 1.0f)
					{
						BO = BO2;
						BP = BP2;
					}
					else
					{
						Lerp (BP, BP2, Chn->SecondaryBlend, BP);
						Slerp(BO, BO2, Chn->SecondaryBlend, BO);
					}
				}
				if (i > 0 && pAnim->AnimRotationOnly)
					BP = B.Position;
			}
			else
			{
				// get default bone position
				BP = B.Position;
				BO = B.Orientation;
			}
			if (!i) BO.Conjugate();

			// tweening
			if (Chn->TweenTime > 0)
			{
				// interpolate orientation using AnimTweenStep
				// current orientation -> {BP,BO}
				Lerp (data->Pos,  BP, Chn->TweenStep, BP);
				Slerp(data->Quat, BO, Chn->TweenStep, BO);
			}
			// blending with previous channels
			if (Chn->BlendAlpha < 1.0f)
			{
				Lerp (data->Pos,  BP, Chn->BlendAlpha, BP);
				Slerp(data->Quat, BO, Chn->BlendAlpha, BO);
			}

			data->Quat = BO;
			data->Pos  = BP;
		}
	}

	// transform bones using skeleton hierarchy
	int i;
	CMeshBoneData *data;
	for (i = 0, data = BoneData; i < pMesh->Skeleton.Num(); i++, data++)
	{
		const CMeshBone &B = pMesh->Skeleton[i];
		CCoords &BC = data->Coords;
		BC.origin = data->Pos;
		data->Quat.ToAxis(BC.axis);

		// move bone position to global coordinate space
		if (!i)
		{
			// root bone - use BaseTransform
			// can use inverted BaseTransformScaled to avoid 'slow' operation
			pMesh->BaseTransformScaled.TransformCoordsSlow(BC, BC);
		}
		else
		{
			// other bones - rotate around parent bone
			BoneData[B.ParentIndex].Coords.UnTransformCoords(BC, BC);
		}
		// deform skeleton according to external settings
		if (data->Scale != 1.0f)
		{
			BC.axis[0].Scale(data->Scale);
			BC.axis[1].Scale(data->Scale);
			BC.axis[2].Scale(data->Scale);
		}
		// compute transformation of world-space model vertices from reference
		// pose to desired pose
		BC.UnTransformCoords(B.InvRefCoords, data->Transform);
	}
	unguard;
}


void CSkelMeshInstance::UpdateAnimation(float TimeDelta)
{
	guard(CSkelMeshInstance::UpdateAnimation);

	if (!pMesh) return;

	// prepare bone-to-channel map
	//?? optimize: update when animation changed only
	for (int i = 0; i < pMesh->Skeleton.Num(); i++)
		BoneData[i].FirstChannel = 0;

	assert(MaxAnimChannel < MAX_SKELANIMCHANNELS);
	int Stage;
	CAnimChan *Chn;
	for (Stage = 0, Chn = Channels; Stage <= MaxAnimChannel; Stage++, Chn++)
	{
		if (Stage > 0 && !Chn->Anim1)
			continue;
		// update tweening
		if (Chn->TweenTime)
		{
			Chn->TweenStep = TimeDelta / Chn->TweenTime;
			Chn->TweenTime -= TimeDelta;
			if (Chn->TweenTime < 0)
			{
				// stop tweening, start animation
				TimeDelta = -Chn->TweenTime;
				Chn->TweenTime = 0;
			}
			assert(Chn->Time == 0);
		}
		// note: TweenTime may be changed now, check again
		if (!Chn->TweenTime && Chn->Anim1)
		{
			// update animation time
			const CMeshAnimSeq *Seq1 = Chn->Anim1;
			const CMeshAnimSeq *Seq2 = Chn->SecondaryBlend ? Chn->Anim2 : NULL;

			float Rate1 = Chn->Rate * Seq1->Rate;
			if (Seq2)
			{
				// if blending 2 channels, should adjust animation rate
				Rate1 = Lerp(Seq1->Rate / Seq1->NumFrames,
							 Seq2->Rate / Seq2->NumFrames,
							 Chn->SecondaryBlend) * Seq1->NumFrames;
			}
			Chn->Time += TimeDelta * Rate1;

			if (Chn->Looped)
			{
				// wrap time
				if (Chn->Time >= Seq1->NumFrames)
				{
					int numSkip = appFloor(Chn->Time / Seq1->NumFrames);
					Chn->Time -= numSkip * Seq1->NumFrames;
				}
				else if (Chn->Time < 0)
				{
					// backward playback
					int numSkip = appFloor(- Chn->Time / Seq1->NumFrames);
					Chn->Time += numSkip * Seq1->NumFrames;
				}
			}
			else
			{
				// clamp time
				if (Chn->Time >= Seq1->NumFrames-1)
					Chn->Time = Seq1->NumFrames-1;
				if (Chn->Time < 0)		// 2 cases: NumFrames=0 or backward animation playback
					Chn->Time = 0;
			}
		}
		// assign bones to channel
		if (Chn->BlendAlpha >= 1.0f && Stage > 0) // stage 0 already set
		{
			// whole subtree will be skipped in UpdateSkeleton(), so - mark root bone only
			BoneData[Chn->RootBone].FirstChannel = Stage;
		}
	}

	UpdateSkeleton();

	unguard;
}


/*-----------------------------------------------------------------------------
	Animation setup
-----------------------------------------------------------------------------*/

void CSkelMeshInstance::PlayAnimInternal(const char *AnimName, float Rate, float TweenTime, int Channel, bool Looped)
{
	guard(CSkelMeshInstance::PlayAnimInternal);

	CAnimChan &Chn = GetStage(Channel);
	if (Channel > MaxAnimChannel)
		MaxAnimChannel = Channel;

	const CMeshAnimSeq *NewAnim = FindAnim(AnimName);
	if (!NewAnim)
	{
		// show default pose
		Chn.Anim1          = NULL;
		Chn.Anim2          = NULL;
		Chn.Time           = 0;
		Chn.Rate           = 0;
		Chn.Looped         = false;
		Chn.TweenTime      = TweenTime;
		Chn.SecondaryBlend = 0;
		return;
	}

	Chn.Rate   = Rate;
	Chn.Looped = Looped;

	if (NewAnim == Chn.Anim1 && Looped)
	{
		// animation not changed, just set some flags (above)
		return;
	}

	Chn.Anim1          = NewAnim;
	Chn.Anim2          = NULL;
	Chn.Time           = 0;
	Chn.SecondaryBlend = 0;
	Chn.TweenTime      = TweenTime;

	unguard;
}


void CSkelMeshInstance::SetBlendParams(int Channel, float BlendAlpha, const char *BoneName)
{
	guard(CSkelMeshInstance::SetBlendParams);
	CAnimChan &Chn = GetStage(Channel);
	Chn.BlendAlpha = BlendAlpha;
	if (Channel == 0)
		Chn.BlendAlpha = 1;		// force full animation for 1st stage
	Chn.RootBone = 0;
	if (BoneName)
	{
		Chn.RootBone = pMesh->FindBone(BoneName);
		if (Chn.RootBone < 0)	// bone not found -- ignore animation
			Chn.BlendAlpha = 0;
	}
	unguard;
}


void CSkelMeshInstance::SetBlendAlpha(int Channel, float BlendAlpha)
{
	guard(CSkelMeshInstance::SetBlendAlpha);
	GetStage(Channel).BlendAlpha = BlendAlpha;
	unguard;
}


void CSkelMeshInstance::SetSecondaryAnim(int Channel, const char *AnimName)
{
	guard(CSkelMeshInstance::SetSecondaryAnim);
	CAnimChan &Chn = GetStage(Channel);
	Chn.Anim2          = FindAnim(AnimName);
	Chn.SecondaryBlend = 0;
	unguard;
}


void CSkelMeshInstance::SetSecondaryBlend(int Channel, float BlendAlpha)
{
	guard(CSkelMeshInstance::SetSecondaryBlend);
	GetStage(Channel).SecondaryBlend = BlendAlpha;
	unguard;
}


void CSkelMeshInstance::GetAnimParams(int Channel, const char *&AnimName,
	float &Frame, float &NumFrames, float &Rate) const
{
	guard(CSkelMeshInstance::GetAnimParams);

	const CAnimChan &Chn  = GetStage(Channel);
	if (!pAnim || !Chn.Anim1 || Channel > MaxAnimChannel)
	{
		AnimName  = "None";
		Frame     = 0;
		NumFrames = 0;
		Rate      = 0;
		return;
	}
	AnimName  = Chn.Anim1->Name;
	Frame     = Chn.Time;
	NumFrames = Chn.Anim1->NumFrames;
	Rate      = Chn.Anim1->Rate * Chn.Rate;

	unguard;
}


/*-----------------------------------------------------------------------------
	Drawing
-----------------------------------------------------------------------------*/

#if EDITOR

void CSkelMeshInstance::DrawSkeleton(bool ShowLabels)
{
	guard(CSkelMeshInstance::DrawSkeleton);

	glDisable(GL_TEXTURE_2D);
	glLineWidth(3);
	glEnable(GL_LINE_SMOOTH);

	glBegin(GL_LINES);
	for (int i = 0; i < pMesh->Skeleton.Num(); i++)
	{
		const CMeshBone &B  = pMesh->Skeleton[i];
		const CCoords   &BC = BoneData[i].Coords;

		CVec3 v1;
		if (i > 0)
		{
#if SHOW_BONE_UPDATES
			int t = BoneUpdateCounts[i];
			glColor3f(t & 1, (t >> 1) & 1, (t >> 2) & 1);
#else
			glColor3f(1, 1, 0.3);
#endif
			v1 = BoneData[B.ParentIndex].Coords.origin;
		}
		else
		{
			glColor3f(1, 0, 1);
			v1.Zero();
		}
		glVertex3fv(v1.v);
		glVertex3fv(BC.origin.v);
		if (ShowLabels)
		{
			// show bone label
			Lerp(v1, BC.origin, 0.5f, v1);			// center point between v1 and BC.origin
			DrawText3D(v1, S_YELLOW"%s", *B.Name);
		}
	}
	glColor3f(1,1,1);
	glEnd();

	glLineWidth(1);
	glDisable(GL_LINE_SMOOTH);

	unguard;
}


void CSkelMeshInstance::DrawBoxes(int highlight)
{
	glDisable(GL_TEXTURE_2D);
	glEnableClientState(GL_VERTEX_ARRAY);
	for (int i = 0; i < pMesh->BoundingBoxes.Num(); i++)
	{
		// convert box to model space
		const CMeshHitBox &H = pMesh->BoundingBoxes[i];
		CCoords Box;
		BoneData[H.BoneIndex].Coords.UnTransformCoords(H.Coords, Box);

#define A	-0.5f
#define B	 0.5f
		static const CVec3 srcPoints[8] = {
			{A,A,A}, {A,B,A}, {B,B,A}, {B,A,A},
			{A,A,B}, {A,B,B}, {B,B,B}, {B,A,B}
		};
#undef A
#undef B
		static const int inds[24] = {
			0,1, 1,2, 2,3, 3,0,		// rect1
			4,5, 5,6, 6,7, 7,4,		// rect2
			0,4, 1,5, 2,6, 3,7		// connectors
		};
		CVec3 verts[8];
		for (int j = 0; j < 8; j++)
			Box.UnTransformPoint(srcPoints[j], verts[j]);
		if (i == highlight)
			glColor3f(0.7, 0.8, 1);
		else
			glColor3f(0.25, 0.32, 1);
		glVertexPointer(3, GL_FLOAT, sizeof(CVec3), verts);
		glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, inds);
	}
	glColor3f(1, 1, 1);
	glDisableClientState(GL_VERTEX_ARRAY);
}


void CSkelMeshInstance::DrawMesh(bool Wireframe, bool Normals, bool Texturing)
{
	guard(CSkelMeshInstance::DrawMesh);
	int i;

	assert(pMesh);

	if (pMesh->Lods.Num() == 0) return;

	//?? can specify LOD number for drawing
	const CSkeletalMeshLod &Lod = pMesh->Lods[0];

	// enable lighting
	if (!Wireframe)
	{
		glEnable(GL_NORMALIZE);
		glEnable(GL_LIGHTING);
		static const float lightPos[4]      = {1000, -2000, 1000, 0};
		static const float lightAmbient[4]  = {0.3, 0.3, 0.3, 1};
		static const float specIntens[4]    = {1, 1, 1, 0};
		glEnable(GL_COLOR_MATERIAL);
		glEnable(GL_LIGHT0);
		glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
		glLightfv(GL_LIGHT0, GL_AMBIENT,  lightAmbient);
		glMaterialfv(GL_FRONT, GL_SPECULAR, specIntens);
		glMaterialf(GL_FRONT, GL_SHININESS, 12);
	}

	// transform verts
	for (i = 0; i < Lod.Points.Num(); i++)
	{
		CCoords Transform;
		Transform.Zero();
		const CMeshPoint &P = Lod.Points[i];
		// prepare weighted transfîrmation
		for (int j = 0; j < MAX_VERTEX_INFLUENCES; j++)
		{
			const CPointWeight &W = P.Influences[j];
			int BoneIndex = W.BoneIndex;
			if (BoneIndex == NO_INFLUENCE)
				break;					// end of list
			CoordsMA(Transform, W.Weight / 65535.0f, BoneData[BoneIndex].Transform);
		}
		// transform vertex
		Transform.UnTransformPoint(P.Point, MeshVerts[i]);
		// transform normal
		Transform.axis.UnTransformVector(P.Normal, MeshNormals[i]);
	}

	// prepare GL
	glPolygonMode(GL_FRONT_AND_BACK, Wireframe ? GL_LINE : GL_FILL);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(3, GL_FLOAT, sizeof(CVec3), MeshVerts);
	glNormalPointer(GL_FLOAT, sizeof(CVec3), MeshNormals);
	glTexCoordPointer(2, GL_FLOAT, sizeof(CMeshPoint), &Lod.Points[0].U);

	// draw all sections
	for (int secIdx = 0; secIdx < Lod.Sections.Num(); secIdx++)
	{
		const CMeshSection &Sec = Lod.Sections[secIdx];
		DrawTextLeft(S_GREEN"Section %d:"S_WHITE" %d tris", secIdx, Sec.NumIndices / 3);

		// SetMaterial()
		if (!Texturing || Wireframe)
		{
			int color = Sec.MaterialIndex + 1;
			if (color > 7) color = 7;
#define C(n)	( ((color >> n) & 1) * 0.5f + 0.1f )
			glColor3f(C(0), C(1), C(2));
#undef C
		}
		else
		{
			if (!pMesh->BindMaterial(Sec.MaterialIndex))
				glDisable(GL_TEXTURE_2D);
		}

		glDrawElements(GL_TRIANGLES, Sec.NumIndices, GL_UNSIGNED_INT, &Lod.Indices[Sec.FirstIndex]);
	}

	// restore GL state
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDisable(GL_LIGHTING);
	glDisable(GL_NORMALIZE);

	// draw mesh normals
	if (Normals)
	{
		glDisable(GL_TEXTURE_2D);
		glBegin(GL_LINES);
		glColor3f(0.5, 1, 0);
		for (i = 0; i < Lod.Points.Num(); i++)
		{
			glVertex3fv(MeshVerts[i].v);
			CVec3 tmp;
			VectorMA(MeshVerts[i], 2, MeshNormals[i], tmp);
			glVertex3fv(tmp.v);
		}
		glEnd();
	}

	unguard;
}

#endif
