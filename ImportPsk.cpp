#include "Core.h"
#include "Import.h"
#include "ImportPsk.h"


/*-----------------------------------------------------------------------------
	Temporaty structures
-----------------------------------------------------------------------------*/

struct CWeightInfo
{
	int		BoneIndex;
	float	Weight;
};

struct CVertexWeights
{
	TArray<CWeightInfo> W;
};

#define MIN_VERTEX_INFLUENCE		(1.0f / 255)	// will be converted to bytes in renderer ...


/*-----------------------------------------------------------------------------
	Sorting bones by hierarchy
-----------------------------------------------------------------------------*/

static const CMeshBone* bonesToSort;

static int SortBones(const int *_B1, const int *_B2)
{
	int B[2], i;
	B[0] = *_B1;
	B[1] = *_B2;

	int BoneTree[MAX_MESH_BONES][2];
	int BoneLeaf[2];

//	appNotify("compare %d and %d", B[0], B[1]);
	// get current bones chain
	for (i = 0; i < 2; i++)
	{
//		appNotify("  tree for %d", B[i]);
		int BoneIndex;
		const CMeshBone *Bone;
		int Leaf = 0;
		for (BoneIndex = B[i], Bone = &bonesToSort[BoneIndex];				// current current bone
			 BoneIndex;														// until not root
			 BoneIndex = Bone->ParentIndex, Bone = &bonesToSort[BoneIndex])	// get parent bone
		{
//			appNotify("    %d", BoneIndex);
			BoneTree[Leaf++][i] = BoneIndex;
			if (Leaf >= MAX_MESH_BONES)
				appError("Recursion in skeleton hierarchy found");
		}
		BoneLeaf[i] = Leaf;
	}
	// compare chains from root node until branches differs
	int depth = min(BoneLeaf[0], BoneLeaf[1]);
	assert(depth);
	int cmp;
	for (i = 1; i <= depth; i++)
	{
		int bone0 = BoneTree[BoneLeaf[0] - i][0];
		int bone1 = BoneTree[BoneLeaf[1] - i][1];
//		appNotify("(depth %d/%d -- cmp %d and %d)", i, depth, bone0, bone1);
		cmp = bone0 - bone1;
		if (cmp) break;
	}
	if (!cmp)
	{
		// one of bones is parent of another - sort by depth
//		appNotify("(one node is parent of other)");
		cmp = BoneLeaf[0] - BoneLeaf[1];
	}
//	appNotify("cmp = %d", cmp);
	return cmp;
}


/*-----------------------------------------------------------------------------
	Importing PSK mesh
-----------------------------------------------------------------------------*/

static int SortInfluences(const CWeightInfo *const W1, const CWeightInfo *const W2)
{
	float delta = W1->Weight - W2->Weight;
	if (delta < 0)
		return 1;
	if (delta > 0)
		return -1;
	return 0;
}


static void TrimSpaces(char *Str)
{
	// trim trailing spaces
	for (int i = strlen(Str)-1; i >= 0; i--)
	{
		if (Str[i] == ' ')
			Str[i] = 0;
		else
			break;
	}
}


void ImportPsk(CArchive &Ar, CSkeletalMesh &Mesh)
{
	guard(ImportPsk);
	int i, j;

	/*
	 *	Load PSK file
	 */

	TArray<CVec3>		Verts;
	TArray<VVertex>		Wedges;
	TArray<VTriangle>	Tris;
	TArray<VMaterial>	Materials;
	TArray<VBone>		Bones;
	TArray<VRawBoneInfluence> Infs;

	// load primary header
	LOAD_CHUNK(MainHdr, "ACTRHEAD");
//	if (MainHdr.TypeFlag != 1999801)
//		appNotify("WARNING: found PSK version %d", MainHdr.TypeFlag);

	// load points
	LOAD_CHUNK(PtsHdr, "PNTS0000");
	int numVerts = PtsHdr.DataCount;
	Verts.Empty(numVerts);
	Verts.Add(numVerts);
	for (i = 0; i < numVerts; i++)
		Ar << Verts[i];

	// load wedges
	LOAD_CHUNK(WedgHdr, "VTXW0000");
	int numWedges = WedgHdr.DataCount;
	Wedges.Empty(numWedges);
	Wedges.Add(numWedges);
	for (i = 0; i < numWedges; i++)
		Ar << Wedges[i];

	LOAD_CHUNK(FacesHdr, "FACE0000");
	int numTris = FacesHdr.DataCount;
	Tris.Empty(numTris);
	Tris.Add(numTris);
	for (i = 0; i < numTris; i++)
		Ar << Tris[i];

	LOAD_CHUNK(MatrHdr, "MATT0000");
	int numMaterials = MatrHdr.DataCount;
	Materials.Empty(numMaterials);
	Materials.Add(numMaterials);
	for (i = 0; i < numMaterials; i++)
		Ar << Materials[i];

	LOAD_CHUNK(BoneHdr, "REFSKELT");
	int numBones = BoneHdr.DataCount;
	Bones.Empty(numBones);
	Bones.Add(numBones);
	for (i = 0; i < numBones; i++)
		Ar << Bones[i];

	LOAD_CHUNK(InfHdr, "RAWWEIGHTS");
	int numSrcInfs = InfHdr.DataCount;
	Infs.Empty(numSrcInfs);
	Infs.Add(numSrcInfs);
	for (i = 0; i < numSrcInfs; i++)
		Ar << Infs[i];

	if (!Ar.IsEof())
		appNotify("WARNING: extra bytes in source file");

	/*
	 *	Import data into Mesh
	 */

	// get lod model
	//?? it should be possible to import mesh into specific LOD index
	Mesh.Lods.Empty(1);
	Mesh.Lods.Add();
	CSkeletalMeshLod &Lod = Mesh.Lods[0];

	// copy vertex information
	guard(ImportVerts);
	Lod.Points.Empty(numWedges);
	Lod.Points.Add(numWedges);
	for (i = 0; i < numWedges; i++)
	{
		CMeshPoint &P = Lod.Points[i];
		const VVertex &W = Wedges[i];
		P.Point = Verts[W.PointIndex];
		P.U     = W.U;
		P.V     = W.V;
		// mark influences as unused
		for (j = 0; j < MAX_VERTEX_INFLUENCES; j++)
			P.Influences[j].BoneIndex = -1;
	}
	unguard;

	// check material usage
	assert(numMaterials <= MAX_MESH_MATERIALS);
	int matUsed[MAX_MESH_MATERIALS];	// counts of triangles using this material
	memset(matUsed, 0, sizeof(matUsed));
	for (i = 0; i < numTris; i++)
		matUsed[Tris[i].MatIndex]++;

	// create material index remap
	int matRemap[MAX_MESH_MATERIALS];	//!! use for materials array too !!
	int numUsedMaterials = 0;
	guard(RemapMaterials);
	for (i = 0; i < numMaterials; i++)
	{
		if (!matUsed[i])
		{
			appNotify("Unused material found: %s", Materials[i].MaterialName);
			continue;
		}
		matRemap[i] = numUsedMaterials++;
	}
	unguard;

	// create sections for materials
	guard(BuildSections);

	Lod.Sections.Empty(numUsedMaterials);
	Lod.Sections.Add(numUsedMaterials);
	Lod.Indices.Empty(numTris * 3);

	int section = 0;
	for (int Mat = 0; Mat < numMaterials; Mat++)
	{
		if (!matUsed[Mat]) continue;	// no tris for this material
		int secTris = 0;
		// prepare empty section
		CMeshSection &Section = Lod.Sections[section++];
		Section.MaterialIndex = matRemap[Mat];
		Section.FirstIndex    = Lod.Indices.Num();
		Section.NumIndices    = 0;
		// find section triangles
		for (int t = 0; t < numTris; t++)
		{
			const VTriangle &Face = Tris[t];
			if (Face.MatIndex != Mat)
				continue;				// different material
			// add indices for this triangle
			// NOTE: PSK has reverse order of points in triangle
			Lod.Indices.AddItem(Face.WedgeIndex[0]);
			Lod.Indices.AddItem(Face.WedgeIndex[2]);
			Lod.Indices.AddItem(Face.WedgeIndex[1]);
			Section.NumIndices += 3;

			secTris++;
		}
		assert(secTris == matUsed[Mat]);
	}
	assert(section == numUsedMaterials);
	if (Lod.Indices.Num() != numTris * 3)
		appNotify("WARNING: indices array size incorrect: %d instead of %d", Lod.Indices.Num(), numTris * 3);
	unguard;

	// import skeleton
	guard(ImportBones);
	Mesh.Skeleton.Empty(numBones);
	Mesh.Skeleton.Add(numBones);
	for (i = 0; i < numBones; i++)
	{
		CMeshBone   &B  = Mesh.Skeleton[i];
		const VBone &SB = Bones[i];
		B.Name = SB.Name;
		TrimSpaces(B.Name);
		B.Position    = SB.BonePos.Position;
		B.Orientation = SB.BonePos.Orientation;
		B.ParentIndex = SB.ParentIndex;
	}
	// sort bones by hierarchy
	//!! should remap weights, if it is possible to resort bones (or remove unused bones)
	int sortedBones[MAX_MESH_BONES];
	for (i = 0; i < numBones; i++)
		sortedBones[i] = i;
	bonesToSort = &Mesh.Skeleton[0];
	QSort(sortedBones+1, numBones - 1, SortBones);
	for (i = 0; i < numBones; i++)
		if (sortedBones[i] != i)
			appError("Found mesh with unsorted bones, should sort and remap!");	//!! remap bones/weights
	unguard;

	// import bone influences (vertex weight)
	// register all weights by vertex index
	TArray<CVertexWeights> Weights;
	Weights.Empty(numVerts);
	Weights.Add(numVerts);
	for (i = 0; i < numSrcInfs; i++)
	{
		const VRawBoneInfluence &Src = Infs[i];
		CWeightInfo Info;
		Info.BoneIndex = Src.BoneIndex;
		Info.Weight    = Src.Weight;
		Weights[Src.PointIndex].W.AddItem(Info);
	}
	// process weights per vertex
	guard(ProcessInfluences);
	for (i = 0; i < numVerts; i++)
	{
		TArray<CWeightInfo> &W = Weights[i].W;
		int numInfs = W.Num();
		if (!numInfs)
		{
			appError("Vertex %d has 0 bone influences", i);
			//?? what to do here? cannot assign bone automatically, because there are
			//?? many bones; cannot correctly render mesh, because point may produce hole;
			//?? should exclude vertex from mesh?
			continue;
		}
		if (numInfs > 1)
		{
			// sort influences by weights (lower weights last)
			QSort(&W[0], numInfs, SortInfluences);
		}
		if (numInfs > MAX_VERTEX_INFLUENCES)
		{
			appNotify("Vertex %d has too much influences (%d). Reducing.", i, numInfs);
			numInfs = MAX_VERTEX_INFLUENCES;
		}
		// compute total weight
		float totalWeight = 0;
		for (j = 0; j < numInfs; j++)
			totalWeight += W[j].Weight;
		// check for zero influence
		if (totalWeight == 0)
		{
			appNotify("Vertex %d has total weight equals to 0, replacing with 1", i);
			float v = 1.0f / numInfs;
			for (j = 0; j < numInfs; j++)
				W[j].Weight = v;
			totalWeight = 1.0f;
		}
		// normalize influences
		float scale = 1.0f / totalWeight;
		for (j = 0; j < numInfs; j++)
			W[j].Weight *= scale;
		// now, trim small influences
		totalWeight = 0;
		for (j = 0; j < numInfs; j++)
		{
			float weight = W[j].Weight;
			if (weight < MIN_VERTEX_INFLUENCE)
				break;
			totalWeight += weight;
		}
		if (j != numInfs)
		{
//			appNotify("Vertex %d: cutting redundant influences", i);
			numInfs = j;					// trim influences
			assert(numInfs);
			scale = 1.0f / totalWeight;		// should rescale influences again
			for (j = 0; j < numInfs; j++)
				W[j].Weight *= scale;
		}
		// resize influences array if needed
		if (numInfs != W.Num())
		{
			assert(numInfs < W.Num());
			W.Remove(numInfs, W.Num() - numInfs);
		}
	}
	unguard;
	// finally, put information to lod mesh verts (wedge -> vertex)
	guard(PutInfluences);
	for (i = 0; i < numWedges; i++)
	{
		CMeshPoint &P = Lod.Points[i];
		TArray<CWeightInfo> &W = Weights[Wedges[i].PointIndex].W;
		assert(W.Num() <= MAX_VERTEX_INFLUENCES);
		for (j = 0; j < W.Num(); j++)
		{
			CMeshPoint::CWeight &Dst = P.Influences[j];
			Dst.BoneIndex = W[j].BoneIndex;
			Dst.Weight    = appFloor(W[j].Weight * 65535.0f);
		}
		if (j < MAX_VERTEX_INFLUENCES)		// mark end of list
			P.Influences[j].BoneIndex = NO_INFLUENCE;
	}
	unguard;

	/*
	 *	Generate normals for mesh
	 */

	guard(GenerateNormals);
	// generate normals for triangles
	TArray<CVec3> Normals;
	Normals.Empty(numVerts);	// WARNING: assumed normals array will be zeroed
	Normals.Add(numVerts);
	for (i = 0; i < numTris; i++)
	{
		const VTriangle &Tri = Tris[i];
		// get vertex indices
		int i1 = Wedges[Tri.WedgeIndex[0]].PointIndex;
		int i2 = Wedges[Tri.WedgeIndex[1]].PointIndex;
		int i3 = Wedges[Tri.WedgeIndex[2]].PointIndex;
		// compute edges
		const CVec3 &V1 = Verts[i1];
		const CVec3 &V2 = Verts[i2];
		const CVec3 &V3 = Verts[i3];
		CVec3 D1, D2, D3;
		VectorSubtract(V2, V1, D1);
		VectorSubtract(V3, V2, D2);
		VectorSubtract(V1, V3, D3);
		// compute normal
		CVec3 norm;
		cross(D2, D1, norm);
		norm.Normalize();
		// compute angles
		D1.Normalize();
		D2.Normalize();
		D3.Normalize();
		float angle1 = acos(-dot(D1, D3));
		float angle2 = acos(-dot(D1, D2));
		float angle3 = acos(-dot(D2, D3));
		// add normals for triangle verts
		VectorMA(Normals[i1], angle1, norm);
		VectorMA(Normals[i2], angle2, norm);
		VectorMA(Normals[i3], angle3, norm);
	}
	// normalize normals
	for (i = 0; i < numVerts; i++)
		Normals[i].Normalize();
	// put normals to CMeshPoint
	for (i = 0; i < numWedges; i++)
	{
		Lod.Points[i].Normal = Normals[Wedges[i].PointIndex];
	}

	unguard;

	unguard;
}


//!! AFTER LOADING (COMMON FOR ALL FORMATS):
//!! ? optimize mesh for vertex cache; may be, not needed, if it is done in renderer
//!! ? find unused terminal bones
//!! ? resort bones by hierarchy (check USkeletalMeshInstance code) -- will require to remap influences


/*-----------------------------------------------------------------------------
	Importing PSA animations
-----------------------------------------------------------------------------*/

void ImportPsa(CArchive &Ar, CAnimSet &Anim)
{
	guard(ImportPsa);
	int i, j, k;

	/*
	 *	Load PSA file
	 */

	TArray<FNamedBoneBinary>	Bones;
	TArray<AnimInfoBinary>		AnimInfo;
	TArray<VQuatAnimKey>		Keys;

	// load primary header
	LOAD_CHUNK(MainHdr, "ANIMHEAD");
//	if (MainHdr.TypeFlag != 1999801)
//		appNotify("WARNING: found PSA version %d", MainHdr.TypeFlag);

	LOAD_CHUNK(BoneHdr, "BONENAMES");
	int numBones = BoneHdr.DataCount;
	Bones.Empty(numBones);
	Bones.Add(numBones);
	for (i = 0; i < numBones; i++)
		Ar << Bones[i];

	LOAD_CHUNK(AnimHdr, "ANIMINFO");
	int numAnims = AnimHdr.DataCount;
	AnimInfo.Empty(numAnims);
	AnimInfo.Add(numAnims);
	for (i = 0; i < numAnims; i++)
		Ar << AnimInfo[i];

	LOAD_CHUNK(KeyHdr, "ANIMKEYS");
	int numKeys = KeyHdr.DataCount;
	Keys.Empty(numKeys);
	Keys.Add(numKeys);
	for (i = 0; i < numKeys; i++)
		Ar << Keys[i];

	if (!Ar.IsEof())
		appNotify("WARNING: extra bytes in source file");

	/*
	 *	Import data to AnimSet
	 */

	Anim.RefBones.Empty(numBones);
	Anim.RefBones.Add(numBones);
	Anim.AnimSeqs.Empty(numAnims);
	Anim.AnimSeqs.Add(numAnims);
	Anim.Motion.Empty(numAnims);
	Anim.Motion.Add(numAnims);

	// import bones
	guard(ImportBones);
	for (i = 0; i < numBones; i++)
	{
		CAnimBone &B = Anim.RefBones[i];
		B.Name = Bones[i].Name;
		TrimSpaces(B.Name);
	}
	unguard;

	// setup animations
	guard(ImportAnims);
	int KeyIndex = 0;
	const VQuatAnimKey *SrcKey = &Keys[0];
	for (i = 0; i < numAnims; i++)
	{
		const AnimInfoBinary &Src = AnimInfo[i];
		CMeshAnimSeq &A = Anim.AnimSeqs[i];
		A.Name      = Src.Name;
		A.Rate      = Src.AnimRate;
		A.NumFrames = Src.NumRawFrames;
		assert(Src.TotalBones == numBones);

		// prepare motion chunk
		guard(ImportKeys);
		CMotionChunk &M = Anim.Motion[i];
		M.Tracks.Empty(numBones);
		M.Tracks.Add(numBones);
		// prepare analog track
		for (j = 0; j < numBones; j++)
		{
			CAnalogTrack &T = M.Tracks[j];
			T.KeyQuat.Empty(Src.NumRawFrames);
			T.KeyQuat.Add  (Src.NumRawFrames);
			T.KeyPos .Empty(Src.NumRawFrames);
			T.KeyPos .Add  (Src.NumRawFrames);
			T.KeyTime.Empty(Src.NumRawFrames);
			T.KeyTime.Add  (Src.NumRawFrames);
		}
		// copy keys
		float Time = 0;
		for (j = 0; j < Src.NumRawFrames; j++)
		{
			float frameTime = SrcKey->Time;
			for (k = 0; k < numBones; k++, SrcKey++, KeyIndex++)
			{
				assert(KeyIndex < numKeys);
				CAnalogTrack &T = M.Tracks[k];
				T.KeyQuat[j] = SrcKey->Orientation;
				T.KeyPos [j] = SrcKey->Position;
				T.KeyTime[j] = Time;
				assert(frameTime == SrcKey->Time);
			}
			Time += frameTime;
		}
		unguard;
	}
	if (KeyIndex != numKeys)
		appNotify("WARNING: Imported %d keys of %d", KeyIndex, numKeys);
	unguard;

	unguard;
}
