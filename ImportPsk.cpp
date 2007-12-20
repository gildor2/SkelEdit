#include "Core.h"
#include "Import.h"
#include "ImportPsk.h"


/*-----------------------------------------------------------------------------
	Sorting bones by hierarchy
-----------------------------------------------------------------------------*/

#define SORT_BONES		1		//!!! REMOVE

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

void ImportPsk(CArchive &Ar, CSkeletalMesh &Mesh)
{
	guard(ImportPsk);
	int i;

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
	if (MainHdr.TypeFlag != 1999801)
		appNotify("WARNING: found PSK version %d", MainHdr.TypeFlag);

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
	int numInfs = InfHdr.DataCount;
	Infs.Empty(numInfs);
	Infs.Add(numInfs);
	for (i = 0; i < numInfs; i++)
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
		appStrncpyz(B.Name, SB.Name, ARRAY_COUNT(B.Name));
		// trim trailing spaces
		for (int j = strlen(B.Name)-1; j >= 0; j--)
		{
			if (B.Name[j] == ' ')
				B.Name[j] = 0;
			else
				break;
		}
		B.Position    = SB.BonePos.Position;
		B.Orientation = SB.BonePos.Orientation;
		B.ParentIndex = SB.ParentIndex;
	}
	// sort bones by hierarchy
	//!! should remap weights
	int sortedBones[MAX_MESH_BONES];
	for (i = 0; i < numBones; i++)
		sortedBones[i] = i;
	bonesToSort = &Mesh.Skeleton[0];
	QSort(sortedBones+1, numBones - 1, SortBones);
	for (i = 0; i < numBones; i++)
		if (sortedBones[i] != i)
			appError("Found mesh with insorted bones, should sort and remap!");	//!! remap bones/weights
	unguard;

	unguard;
}


//!! AFTER LOADING (COMMON FOR ALL FORMATS):
//!! - compute normals (if none) -- add ability to recompute normals from menu
//!! - normalize weights (check USkeletalMeshInstance code)
//!! - optimize mesh for vertex cache; may be, not needed, if it is done in renderer
//!! - resort bones by hierarchy (check USkeletalMeshInstance code) -- will require to remap influences
