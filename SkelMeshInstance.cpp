#include "Core.h"

#include "SkeletalMesh.h"
#include "AnimSet.h"
#include "SkelMeshInstance.h"

#include "GlViewport.h"				//!! MOVE DRAWING CODE OUTSIDE FROM CLASS


struct CMeshBoneData
{
	// static data (computed after mesh loading)
	int			BoneMap;			// index of bone in animation tracks
	CCoords		RefCoords;			// coordinates of bone in reference pose
	CCoords		RefCoordsInv;		// inverse of RefCoordsInv
	int			SubtreeSize;		// count of all children bones (0 for leaf bone)
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
	if (BoneData)  delete BoneData;
	if (MeshVerts) delete MeshVerts;
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


/* Iterate bone (sub)tree and do following:
 *	- find all direct childs of bone 'Index', check sort order; bones should be
 *	  sorted in hierarchy order (1st child and its children first, other childs next)
 *	  Example of tree:
 *		Bone1
 *		+- Bone11
 *		|  +- Bone111
 *		+- Bone12
 *		|  +- Bone121
 *		|  +- Bone122
 *		+- Bone13
 *	  Sorted order: Bone1, Bone11, Bone111, Bone12, Bone121, Bone122, Bone13
 *	  Note: it seems, Unreal already have sorted bone list (assumed in other code,
 *	  verified by 'assert(currIndex == Index)')
 *	- compute subtree sizes ('Sizes' array)
 *	- compute bone hierarchy depth ('Depth' array, for debugging only)
 */
static int CheckBoneTree(const TArray<CMeshBone> &Bones, int Index,
	int *Sizes, int *Depth, int &numIndices, int maxIndices)
{
	assert(numIndices < maxIndices);
	static int depth = 0;
	// remember curerent index, increment for recustion
	int currIndex = numIndices++;
	// find children of Bone[Index]
	int treeSize = 0;
	for (int i = Index + 1; i < Bones.Num(); i++)
		if (Bones[i].ParentIndex == Index)
		{
			// recurse
			depth++;
			treeSize += CheckBoneTree(Bones, i, Sizes, Depth, numIndices, maxIndices);
			depth--;
		}
	// store gathered information
	assert(currIndex == Index);
	Sizes[currIndex] = treeSize;
	Depth[currIndex] = depth;
	return treeSize + 1;
}



void CSkelMeshInstance::SetMesh(CSkeletalMesh *Mesh)
{
	guard(CSkelMeshInstance::SetMesh);

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
	if (BoneData)  delete BoneData;
	if (MeshVerts) delete MeshVerts;
	BoneData  = new CMeshBoneData[NumBones];
	MeshVerts = new CVec3        [NumVerts];

	int i;
	CMeshBoneData *data;
	for (i = 0, data = BoneData; i < NumBones; i++, data++)
	{
		const CMeshBone &B = pMesh->Skeleton[i];
		// NOTE: assumed, that parent bones goes first
		assert(B.ParentIndex <= i);

		// find reference bone in animation track
		data->BoneMap = -1;
		if (pAnim)	//?? move code to SetAnim() ?
		{
			for (int j = 0; j < pAnim->RefBones.Num(); j++)
				if (!strcmp(B.Name, pAnim->RefBones[j].Name))
				{
					data->BoneMap = j;
					break;
				}
		}

		// compute reference bone coords
		CVec3 BP;
		CQuat BO;
		// get default pose
		BP = (CVec3&)B.Position;
		BO = (CQuat&)B.Orientation;
		if (!i) BO.Conjugate();

		CCoords &BC = data->RefCoords;
		BC.origin = BP;
		BO.ToAxis(BC.axis);
		// move bone position to global coordinate space
		if (i)	// do not rotate root bone
			BoneData[pMesh->Skeleton[i].ParentIndex].RefCoords.UnTransformCoords(BC, BC);
		// store inverted transformation too
		InvertCoords(data->RefCoords, data->RefCoordsInv);
		// initialize skeleton configuration
		data->Scale = 1.0f;			// default bone scale
	}

	//!! move following part to mesh importer
/**	// normalize VertInfluences: sum of all influences may be != 1
	// (possible situation, see SkaarjAnims/Skaarj2, SkaarjAnims/Skaarj_Skel, XanRobots/XanF02)
	float *VertSumWeights = new float[NumVerts];	// zeroed
	int   *VertInfCount   = new int  [NumVerts];	// zeroed
	// count sum of weights for all verts
	for (i = 0; i < Mesh->VertInfluences.Num(); i++)
	{
		const FVertInfluences &Inf = pMesh->VertInfluences[i];
		int PointIndex = Inf.PointIndex;
		assert(PointIndex < NumVerts);
		VertSumWeights[PointIndex] += Inf.Weight;
		VertInfCount  [PointIndex]++;
	}
#if 0
	// notify about wrong weights
	for (i = 0; i < NumVerts; i++)
	{
		if (fabs(VertSumWeights[i] - 1.0f) < 0.01f) continue;
		appNotify("Vert[%d] weight sum=%g (%d weights)", i, VertSumWeights[i], VertInfCount[i]);
	}
#endif
	// normalize weights
	for (i = 0; i < pMesh->VertInfluences.Num(); i++)
	{
		FVertInfluences &Inf = pMesh->VertInfluences[i];
		int PointIndex = Inf.PointIndex;
		float sum = VertSumWeights[PointIndex];
		if (fabs(sum - 1.0f) < 0.01f) continue;
		assert(sum > 0.01f);	// no division by zero
		Inf.Weight = Inf.Weight / sum;
	}

	delete VertSumWeights;
	delete VertInfCount;
	*/
	//!! END OF EDITOR CODE !!

	// check bones tree
	//!!!! REMOVE THIS CODE
	int treeSizes[MAX_MESH_BONES], depth[MAX_MESH_BONES];
	int numIndices = 0;
	CheckBoneTree(pMesh->Skeleton, 0, treeSizes, depth, numIndices, MAX_MESH_BONES);
	assert(numIndices == NumBones);
	for (i = 0; i < numIndices; i++)
		BoneData[i].SubtreeSize = treeSizes[i];	// remember subtree size

#if 0
	//?? dump tree; separate function (requires depth information)
	//?? DEPTH INFORMATION can be easily computed (simple loop by parent index)
	for (i = 0; i < numIndices; i++)
	{
		int parent = pMesh->Skeleton[i].ParentIndex;
		printf("bone#%2d (parent %2d); tree size: %2d   ", i, parent, treeSizes[i]);
		for (int j = 0; j < depth[i]; j++)
		{
	#if 0
			// simple picture
			printf("|  ");
	#else
			// graph-like picture
			bool found = false;
			for (int n = i+1; n < numIndices; n++)
			{
				if (depth[n] >  j+1) continue;
				if (depth[n] == j+1) found = true;
				break;
			}
		#if _WIN32
			// pseudographics
			if (j == depth[i]-1)
				printf(found ? "\xC3\xC4\xC4" : "\xC0\xC4\xC4");	// [+--] : [\--]
			else
                printf(found ? "\xB3  " : "   ");					// [|  ] : [   ]
		#else
			// ASCII
			if (j == depth[i]-1)
				printf(found ? "+--" : "\\--");
			else
				printf(found ? "|  " : "   ");
        #endif
	#endif
		}
		printf("%s\n", pMesh->Skeleton[i].Name);
	}
#endif

	ClearSkelAnims();
	PlayAnim(NULL);

	unguard;
}


/*-----------------------------------------------------------------------------
	Miscellaneous
-----------------------------------------------------------------------------*/

int CSkelMeshInstance::FindBone(const char *BoneName) const
{
	assert(pMesh);
	for (int i = 0; i < pMesh->Skeleton.Num(); i++)
		if (!strcmp(pMesh->Skeleton[i].Name, BoneName))
			return i;
	return -1;
}


int CSkelMeshInstance::FindAnim(const char *AnimName) const
{
	if (!pAnim || !AnimName)
		return -1;
	for (int i = 0; i < pAnim->AnimSeqs.Num(); i++)
		if (!strcmp(pAnim->AnimSeqs[i].Name, AnimName))
			return i;
	return -1;
}


void CSkelMeshInstance::SetBoneScale(const char *BoneName, float scale)
{
	int BoneIndex = FindBone(BoneName);
	if (BoneIndex < 0) return;
	BoneData[BoneIndex].Scale = scale;
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


/**
static void GetBonePosition(const AnalogTrack &A, float Frame, float NumFrames, bool Loop,
	CVec3 &DstPos, CQuat &DstQuat)
{

	guard(GetBonePosition);

	int i;

	// fast case: 1 frame only
	if (A.KeyTime.Num() == 1)
	{
		DstPos  = (CVec3&)A.KeyPos[0];
		DstQuat = (CQuat&)A.KeyQuat[0];
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
			DstPos  = (A.KeyPos.Num()  > 1) ? (CVec3&)A.KeyPos[i]  : (CVec3&)A.KeyPos[0];
			DstQuat = (A.KeyQuat.Num() > 1) ? (CQuat&)A.KeyQuat[i] : (CQuat&)A.KeyQuat[0];
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
	//!! --- checker ---
	int i1;
	for (i1 = 0; i1 < NumKeys; i1++)
	{
		float CurrKeyTime = A.KeyTime[i1];
		if (Frame == CurrKeyTime)
		{
			// exact key found
			DstPos  = (A.KeyPos.Num()  > 1) ? (CVec3&)A.KeyPos[i]  : (CVec3&)A.KeyPos[0];
			DstQuat = (A.KeyQuat.Num() > 1) ? (CQuat&)A.KeyQuat[i] : (CQuat&)A.KeyQuat[0];
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
		Lerp((CVec3&)A.KeyPos[X], (CVec3&)A.KeyPos[Y], frac, DstPos);
	else
		DstPos = (CVec3&)A.KeyPos[0];
	// get orientation
	if (A.KeyQuat.Num() > 1)
		Slerp((CQuat&)A.KeyQuat[X], (CQuat&)A.KeyQuat[Y], frac, DstQuat);
	else
		DstQuat = (CQuat&)A.KeyQuat[0];

	unguard;
}
*/

static int BoneUpdateCounts[MAX_MESH_BONES];	//!! remove later

void CSkelMeshInstance::UpdateSkeleton()
{
	guard(CSkelMeshInstance::UpdateSkeleton);

	// process all animation channels
	assert(MaxAnimChannel < MAX_SKELANIMCHANNELS);
	int Stage;
	CAnimChan *Chn;
	memset(BoneUpdateCounts, 0, sizeof(BoneUpdateCounts)); //!! remove later
DrawTextLeft("ANIM: %d stages", MaxAnimChannel+1);
	for (Stage = 0, Chn = Channels; Stage <= MaxAnimChannel; Stage++, Chn++)
	{
		if (Stage > 0 && (Chn->AnimIndex1 == ANIM_UNASSIGNED || Chn->BlendAlpha <= 0))
			continue;
DrawTextLeft("[%d] anim=%d + %d", Stage, Chn->AnimIndex1, Chn->AnimIndex2);

///		const MotionChunk  *Motion1  = NULL, *Motion2  = NULL;
		const CMeshAnimSeq *AnimSeq1 = NULL, *AnimSeq2 = NULL;
		float Time2;
		if (Chn->AnimIndex1 >= 0)
		{
///			Motion1  = &pAnim->Moves   [Chn->AnimIndex1];
			AnimSeq1 = &pAnim->AnimSeqs[Chn->AnimIndex1];
			if (Chn->AnimIndex2 >= 0 && Chn->SecondaryBlend)
			{
///				Motion2  = &pAnim->Moves   [Chn->AnimIndex2];
				AnimSeq2 = &pAnim->AnimSeqs[Chn->AnimIndex2];
				// compute time for secondary channel; always in sync with primary channel
				Time2 = Chn->Time / AnimSeq1->NumFrames * AnimSeq2->NumFrames;
			}
		}

		// compute bone range, affected by specified animation bone
		int firstBone = Chn->RootBone;
		int lastBone  = firstBone + BoneData[firstBone].SubtreeSize;
		assert(lastBone < pMesh->Skeleton.Num());

		int i;
		CMeshBoneData *data;
		for (i = firstBone, data = BoneData + firstBone; i <= lastBone; i++, data++)
		{
			if (Stage < data->FirstChannel)
			{
				// this bone position will be overrided in following channel(s); all
				// subhierarchy bones should be overrided too; skip whole subtree
				int skip = data->SubtreeSize;
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
/**			if (Motion1 && BoneIndex >= 0)
			{
				// get bone position from track
				if (!Motion2 || Chn->SecondaryBlend != 1.0f)
				{
					BoneUpdateCounts[i]++;		//!! remove later
					GetBonePosition(Motion1->AnimTracks[BoneIndex], Chn->Time, AnimSeq1->NumFrames,
						Chn->Looped, BP, BO);
				}
				// blend secondary animation
				if (Motion2)
				{
					CVec3 BP2;
					CQuat BO2;
					BoneUpdateCounts[i]++;		//!! remove later
					GetBonePosition(Motion2->AnimTracks[BoneIndex], Time2, AnimSeq2->NumFrames,
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
			}
			else */
			{
				// get default bone position
				const CMeshBone &B = pMesh->Skeleton[i];
				BP = (CVec3&)B.Position;
				BO = (CQuat&)B.Orientation;
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
			BoneData[pMesh->Skeleton[i].ParentIndex].Coords.UnTransformCoords(BC, BC);
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
		BC.UnTransformCoords(data->RefCoordsInv, data->Transform);
	}
	unguard;
}


void CSkelMeshInstance::UpdateAnimation(float TimeDelta)
{
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
			const CMeshAnimSeq *Seq1 = &pAnim->AnimSeqs[Chn->AnimIndex1];
			const CMeshAnimSeq *Seq2 = (Chn->AnimIndex2 >= 0 && Chn->SecondaryBlend)
				? &pAnim->AnimSeqs[Chn->AnimIndex2]
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


void CSkelMeshInstance::AnimStopLooping(int Channel)
{
	guard(CSkelMeshInstance::AnimStopLooping);
	GetStage(Channel).Looped = false;
	unguard;
}


void CSkelMeshInstance::FreezeAnimAt(float Time, int Channel)
{
	guard(CSkelMeshInstance::FreezeAnimAt);
	CAnimChan &Chn = GetStage(Channel);
	Chn.Time = Time;
	Chn.Rate = 0;
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
	const CMeshAnimSeq &AnimSeq = pAnim->AnimSeqs[Chn.AnimIndex1];
	AnimName  = AnimSeq.Name;
	Frame     = Chn.Time;
	NumFrames = AnimSeq.NumFrames;
	Rate      = AnimSeq.Rate * Chn.Rate;

	unguard;
}


/*-----------------------------------------------------------------------------
	Drawing
-----------------------------------------------------------------------------*/

void CSkelMeshInstance::DrawSkeleton()
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

//		glColor3f(1,0,0);
//		glVertex3fv(BC.origin.v);
//		glVertex3fv(v2.v);

		if (i > 0)
		{
			glColor3f(1,1,0.3);
			//!! REMOVE LATER:
			int t = BoneUpdateCounts[i];
			glColor3f(t & 1, (t >> 1) & 1, (t >> 2) & 1);
			//!! ^^^^^^^^^^^^^
			glVertex3fv(BoneData[B.ParentIndex].Coords.origin.v);
		}
		else
		{
			glColor3f(1,0,1);
			glVertex3f(0, 0, 0);
		}
		glVertex3fv(BC.origin.v);
	}
	glColor3f(1,1,1);
	glEnd();

	glLineWidth(1);
	glDisable(GL_LINE_SMOOTH);

	unguard;
}


void CSkelMeshInstance::DrawMesh(bool Wireframe)
{
	guard(CSkelMeshInstance::DrawMesh);
	int i;

	assert(pMesh);

	if (pMesh->Lods.Num() == 0) return;

	//?? can specify LOD number for drawing
	const CSkeletalMeshLod &Lod = pMesh->Lods[0];

	//?? try other tech: compute weighted sum of matrices, and then
	//?? transform vector (+ normal ?; note: normals requires normalized
	//?? transformations ?? (or normalization guaranteed by sum(weights)==1?))
	memset(MeshVerts, 0, sizeof(CVec3) * Lod.Points.Num());
/**
	for (i = 0; i < Mesh->VertInfluences.Num(); i++)
	{
		const FVertInfluences &Inf  = pMesh->VertInfluences[i];
		const CMeshBoneData   &data = BoneData[Inf.BoneIndex];
		int PointIndex = Inf.PointIndex;
		const CVec3 &Src = (CVec3&)Mesh->Points[PointIndex];
		CVec3 tmp;
#if 0
		// variant 1: ref pose -> local bone space -> transformed bone to world
		data.RefCoords.TransformPoint(Src, tmp);
		data.Coords.UnTransformPoint(tmp, tmp);
#else
		// variant 2: use prepared transformation (same result, but faster)
		data.Transform.UnTransformPoint(Src, tmp);
#endif
		VectorMA(MeshVerts[PointIndex], Inf.Weight, tmp);
	}
*/
	//!! TEMP CODE, UNTIL INFLUENCES SUPPORTED !!
	for (i = 0; i < Lod.Points.Num(); i++)
	{
		MeshVerts[i] = Lod.Points[i].Point;
	}

	// prepare GL
	glPolygonMode(GL_FRONT_AND_BACK, Wireframe ? GL_LINE : GL_FILL);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, /*?? sizeof(CMeshPoint)*/ sizeof(CVec3), MeshVerts);

	// draw all sections
	for (int secIdx = 0; secIdx < Lod.Sections.Num(); secIdx++)
	{
		const CMeshSection &Sec = Lod.Sections[secIdx];
		DrawTextLeft(S_GREEN"Section %d:"S_WHITE" %d tris", secIdx, Sec.NumIndices / 3);

		// SetMaterial()
		int color = Sec.MaterialIndex + 1;
		if (color > 7) color = 7;
#define C(n)	( ((color >> n) & 1) * 0.5f + 0.3f )
		glColor3f(C(0), C(1), C(2));
#undef C

		glDrawElements(GL_TRIANGLES, Sec.NumIndices, GL_UNSIGNED_INT, &Lod.Indices[Sec.FirstIndex]);
	}

	// restore GL state
	glDisableClientState(GL_VERTEX_ARRAY);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	unguard;
}


/*!!
void CSkelMeshInstance::Draw()
{
	guard(CSkelMeshInstance::Draw);

	const USkeletalMesh   *Mesh   = GetMesh();
	const CSkelMeshViewer *Viewer = static_cast<CSkelMeshViewer*>(Viewport);

	// show skeleton
	if (Viewer->ShowSkel)		//!! move this part to CSkelMeshViewer; call Inst->DrawSkeleton() etc
		DrawSkeleton();
	// show mesh
	if (Viewer->ShowSkel != 2)
	{
		if (LodNum < 0)
			DrawBaseSkeletalMesh();
		else
			DrawLodSkeletalMesh(&Mesh->LODModels[LodNum]);
	}

	unguard;
}
*/
