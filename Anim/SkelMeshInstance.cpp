#include "Core.h"

#include "AnimClasses.h"
#include "SkelMeshInstance.h"

#include "GlViewport.h"				//!! MOVE DRAWING CODE OUTSIDE FROM CLASS


/* NOTE for multi-AnimSet support:
 *	- requires multiple CMeshBoneData.BoneMap items
 *	- requires multiple AnimSet pointers
 *	- current animation should be stored by pointer, not by index
 *	  - or should make container with union of all AnimSet sequences)
 *	  - or store current animation as pair of AnimSet+AnimIndex
 */

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


#define ANIM_UNASSIGNED			-2


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
		Channels[i].AnimIndex1     = ANIM_UNASSIGNED;
		Channels[i].AnimIndex2     = ANIM_UNASSIGNED;
		Channels[i].SecondaryBlend = 0;
		Channels[i].BlendAlpha     = 1;
		Channels[i].RootBone       = 0;
	}
}


void CSkelMeshInstance::SetMesh(CSkeletalMesh *Mesh)
{
	guard(CSkelMeshInstance::SetMesh);

	bool prevMesh = pMesh != NULL;
	pMesh = Mesh;

	// get some counts
	int NumBones = pMesh->Skeleton.Num();
	int NumVerts = pMesh->Lods[0].Points.Num();	// data for software skinning
	for (int i = 1; i < pMesh->Lods.Num(); i++)
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

	int i;
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


void CSkelMeshInstance::SetAnim(CAnimSet *Anim)
{
	pAnim = Anim;
	assert(pMesh);
	assert(pAnim);

	// prepare animation <-> mesh bone map
	for (int i = 0; i < pMesh->Skeleton.Num(); i++)
	{
		const CMeshBone &B = pMesh->Skeleton[i];
		BoneData[i].BoneMap = -1;
		// find reference bone in animation track
		for (int j = 0; j < pAnim->TrackBoneName.Num(); j++)
			if (!stricmp(B.Name, pAnim->TrackBoneName[j].Name))
			{
				BoneData[i].BoneMap = j;
				break;
			}
	}
}


/*-----------------------------------------------------------------------------
	Miscellaneous
-----------------------------------------------------------------------------*/

int CSkelMeshInstance::FindBone(const char *BoneName) const
{
	assert(pMesh);
	for (int i = 0; i < pMesh->Skeleton.Num(); i++)
		if (pMesh->Skeleton[i].Name == BoneName)
			return i;
	return -1;
}


int CSkelMeshInstance::FindAnim(const char *AnimName) const
{
	if (!pAnim || !AnimName)
		return -1;
	for (int i = 0; i < pAnim->Sequences.Num(); i++)
		if (pAnim->Sequences[i].Name == AnimName)
			return i;
	return -1;
}


void CSkelMeshInstance::SetBoneScale(const char *BoneName, float scale)
{
	int BoneIndex = FindBone(BoneName);
	if (BoneIndex < 0) return;
	BoneData[BoneIndex].Scale = scale;
}


bool CSkelMeshInstance::IsAnimating(int Channel)
{
	const CAnimChan &chn = GetStage(Channel);
	if (!chn.Rate) return false;
	if (chn.AnimIndex1 == ANIM_UNASSIGNED)
		return false;
	return true;
}


/*-----------------------------------------------------------------------------
	Skeletal animation itself
-----------------------------------------------------------------------------*/

#define MAX_LINEAR_KEYS		4

//!! DEBUGGING, remove later
#define DEBUG_BIN_SEARCH	1
#if 0
#	define DBG		printf
#else
#	define DBG		if (1) {} else printf
#endif
//!! ^^^^^^


static void GetBonePosition(const CAnalogTrack &A, float Frame, float NumFrames, bool Loop,
	CVec3 &DstPos, CQuat &DstQuat)
{

	guard(GetBonePosition);

	int i;

	// fast case: 1 frame only
	if (A.KeyTime.Num() == 1)
	{
		DstPos  = A.KeyPos[0];
		DstQuat = A.KeyQuat[0];
		return;
	}

	// find index in time key array
	int NumKeys = A.KeyTime.Num();
	// *** binary search ***
	int Low = 0, High = NumKeys-1;
	DBG(">>> find %.5f\n", Frame);
	while (Low + MAX_LINEAR_KEYS < High)
	{
		int Mid = (Low + High) / 2;
		DBG("   [%d..%d] mid: [%d]=%.5f", Low, High, Mid, A.KeyTime[Mid]);
		if (Frame < A.KeyTime[Mid])
			High = Mid-1;
		else
			Low = Mid;
		DBG("   d=%f\n", A.KeyTime[Mid]-Frame);
	}

	// *** linear search ***
	DBG("   linear: %d..%d\n", Low, High);
	for (i = Low; i <= High; i++)
	{
		float CurrKeyTime = A.KeyTime[i];
		DBG("   #%d: %.5f\n", i, CurrKeyTime);
		if (Frame == CurrKeyTime)
		{
			// exact key found
			DstPos  = (A.KeyPos.Num()  > 1) ? A.KeyPos[i]  : A.KeyPos[0];
			DstQuat = (A.KeyQuat.Num() > 1) ? A.KeyQuat[i] : A.KeyQuat[0];
			return;
		}
		if (Frame < CurrKeyTime)
		{
			i--;
			break;
		}
	}
	if (i > High)
		i = High;

#if DEBUG_BIN_SEARCH
	EXEC_ONCE(appPrintf("!!! WARNING: DEBUG_BIN_SEARCH enabled !!!\n"))
	//!! --- checker ---
	int i1;
	for (i1 = 0; i1 < NumKeys; i1++)
	{
		float CurrKeyTime = A.KeyTime[i1];
		if (Frame == CurrKeyTime)
		{
			// exact key found
			DstPos  = (A.KeyPos.Num()  > 1) ? A.KeyPos[i]  : A.KeyPos[0];
			DstQuat = (A.KeyQuat.Num() > 1) ? A.KeyQuat[i] : A.KeyQuat[0];
			return;
		}
		if (Frame < CurrKeyTime)
		{
			i1--;
			break;
		}
	}
	if (i1 > NumKeys-1)
		i1 = NumKeys-1;
	if (i != i1)
	{
		appError("i=%d != i1=%d", i, i1);
	}
#endif

	int X = i;
	int Y = i+1;
	float frac;
	if (Y >= NumKeys)
	{
		if (!Loop)
		{
			// clamp animation
			Y = NumKeys-1;
			assert(X == Y);
			frac = 0;
		}
		else
		{
			// loop animation
			Y = 0;
			frac = (Frame - A.KeyTime[X]) / (NumFrames - A.KeyTime[X]);
		}
	}
	else
	{
		frac = (Frame - A.KeyTime[X]) / (A.KeyTime[Y] - A.KeyTime[X]);
	}

	assert(X >= 0 && X < NumKeys);
	assert(Y >= 0 && Y < NumKeys);

	// get position
	if (A.KeyPos.Num() > 1)
		Lerp(A.KeyPos[X], A.KeyPos[Y], frac, DstPos);
	else
		DstPos = A.KeyPos[0];
	// get orientation
	if (A.KeyQuat.Num() > 1)
		Slerp(A.KeyQuat[X], A.KeyQuat[Y], frac, DstQuat);
	else
		DstQuat = A.KeyQuat[0];

	unguard;
}


static int BoneUpdateCounts[MAX_MESH_BONES];	//!! remove later

void CSkelMeshInstance::UpdateSkeleton()
{
	guard(CSkelMeshInstance::UpdateSkeleton);

	// process all animation channels
	assert(MaxAnimChannel < MAX_SKELANIMCHANNELS);
	int Stage;
	CAnimChan *Chn;
	memset(BoneUpdateCounts, 0, sizeof(BoneUpdateCounts)); //!! remove later
	for (Stage = 0, Chn = Channels; Stage <= MaxAnimChannel; Stage++, Chn++)
	{
		if (Stage > 0 && (Chn->AnimIndex1 == ANIM_UNASSIGNED || Chn->BlendAlpha <= 0))
			continue;

		const CMeshAnimSeq *AnimSeq1 = NULL, *AnimSeq2 = NULL;
		float Time2;
		if (Chn->AnimIndex1 >= 0)
		{
			AnimSeq1 = &pAnim->Sequences[Chn->AnimIndex1];
			if (Chn->AnimIndex2 >= 0 && Chn->SecondaryBlend)
			{
				AnimSeq2 = &pAnim->Sequences[Chn->AnimIndex2];
				// compute time for secondary channel; always in sync with primary channel
				Time2 = Chn->Time / AnimSeq1->NumFrames * AnimSeq2->NumFrames;
			}
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
			if (AnimSeq1 && BoneIndex >= 0)
			{
				// get bone position from track
				if (!AnimSeq2 || Chn->SecondaryBlend != 1.0f)
				{
					BoneUpdateCounts[i]++;		//!! remove later
					GetBonePosition(AnimSeq1->Tracks[BoneIndex], Chn->Time, AnimSeq1->NumFrames,
						Chn->Looped, BP, BO);
				}
				// blend secondary animation
				if (AnimSeq2)
				{
					CVec3 BP2;
					CQuat BO2;
					BoneUpdateCounts[i]++;		//!! remove later
					GetBonePosition(AnimSeq2->Tracks[BoneIndex], Time2, AnimSeq2->NumFrames,
						Chn->Looped, BP2, BO2);
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

	// prepare bone-to-channel map
	//?? optimize: update when animation changed only
	for (int i = 0; i < pMesh->Skeleton.Num(); i++)
		BoneData[i].FirstChannel = 0;

	assert(MaxAnimChannel < MAX_SKELANIMCHANNELS);
	int Stage;
	CAnimChan *Chn;
	for (Stage = 0, Chn = Channels; Stage <= MaxAnimChannel; Stage++, Chn++)
	{
		if (Stage > 0 && Chn->AnimIndex1 == ANIM_UNASSIGNED)
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
		if (!Chn->TweenTime && Chn->AnimIndex1 >= 0)
		{
			// update animation time
			const CMeshAnimSeq *Seq1 = &pAnim->Sequences[Chn->AnimIndex1];
			const CMeshAnimSeq *Seq2 = (Chn->AnimIndex2 >= 0 && Chn->SecondaryBlend)
				? &pAnim->Sequences[Chn->AnimIndex2]
				: NULL;

			float Rate1 = Chn->Rate * Seq1->Rate;
			if (Seq2)
			{
				// if blending 2 channels, should adjust animation rate
				Rate1 = Lerp(Seq1->Rate / Seq1->NumFrames, Seq2->Rate / Seq2->NumFrames, Chn->SecondaryBlend)
					* Seq1->NumFrames;
			}
			Chn->Time += TimeDelta * Rate1;

			if (Chn->Looped)
			{
				if (Chn->Time >= Seq1->NumFrames)
				{
					// wrap time
					int numSkip = appFloor(Chn->Time / Seq1->NumFrames);
					Chn->Time -= numSkip * Seq1->NumFrames;
				}
			}
			else
			{
				if (Chn->Time >= Seq1->NumFrames-1)
				{
					// clamp time
					Chn->Time = Seq1->NumFrames-1;
					if (Chn->Time < 0)
						Chn->Time = 0;
				}
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

	int NewAnimIndex = FindAnim(AnimName);
	if (NewAnimIndex < 0)
	{
		// show default pose
		Chn.AnimIndex1     = -1;
		Chn.AnimIndex2     = -1;
		Chn.Time           = 0;
		Chn.Rate           = 0;
		Chn.Looped         = false;
		Chn.TweenTime      = TweenTime;
		Chn.SecondaryBlend = 0;
		return;
	}

	Chn.Rate   = Rate;
	Chn.Looped = Looped;

	if (NewAnimIndex == Chn.AnimIndex1 && Looped)
	{
		// animation not changed, just set some flags (above)
		return;
	}

	Chn.AnimIndex1     = NewAnimIndex;
	Chn.AnimIndex2     = -1;
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
		Chn.RootBone = FindBone(BoneName);
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
	Chn.AnimIndex2     = FindAnim(AnimName);
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
	if (!pAnim || Chn.AnimIndex1 < 0 || Channel > MaxAnimChannel)
	{
		AnimName  = "None";
		Frame     = 0;
		NumFrames = 0;
		Rate      = 0;
		return;
	}
	const CMeshAnimSeq &AnimSeq = pAnim->Sequences[Chn.AnimIndex1];
	AnimName  = AnimSeq.Name;
	Frame     = Chn.Time;
	NumFrames = AnimSeq.NumFrames;
	Rate      = AnimSeq.Rate * Chn.Rate;

	unguard;
}


/*-----------------------------------------------------------------------------
	Drawing
-----------------------------------------------------------------------------*/

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

		CVec3 v2;
		v2.Set(10, 0, 0);
		BC.UnTransformPoint(v2, v2);

		CVec3 v1;
		if (i > 0)
		{
			glColor3f(1,1,0.3);
			//!! REMOVE LATER:
			int t = BoneUpdateCounts[i];
			glColor3f(t & 1, (t >> 1) & 1, (t >> 2) & 1);
			//!! ^^^^^^^^^^^^^
			v1 = BoneData[B.ParentIndex].Coords.origin;
		}
		else
		{
			glColor3f(1,0,1);
			v1.Zero();
		}
		glVertex3fv(v1.v);
		glVertex3fv(BC.origin.v);
		if (ShowLabels)
		{
			// show bone label
			v1.Add(BC.origin);
			v1.Scale(0.5f);
			DrawText3D(v1, S_YELLOW"%s", *B.Name);
		}
	}
	glColor3f(1,1,1);
	glEnd();

	glLineWidth(1);
	glDisable(GL_LINE_SMOOTH);

	unguard;
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
		// prepare weighted transofrmation
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
