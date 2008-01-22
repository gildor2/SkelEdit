#include "Core.h"
#include "AnimClasses.h"

#include "GlViewport.h"


/*-----------------------------------------------------------------------------
	Simple texture wrapper
-----------------------------------------------------------------------------*/

class CRenderingMaterial
{
public:
	GLuint			Handle;
	TString<64>		Filename;

	CRenderingMaterial()
	:	Handle(0)
	{
		Filename[0] = 0;
	}

	CRenderingMaterial(const char *Name)
	{
		Filename = Name;
		Handle   = GL_LoadTexture(Filename);
	}

	~CRenderingMaterial()
	{
		if (Handle)
			glDeleteTextures(1, &Handle);
	}

	bool Bind()
	{
		if (!Handle)
			return false;
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, Handle);
		return true;
	}
};


/*-----------------------------------------------------------------------------
	CSkeletalMesh class
-----------------------------------------------------------------------------*/

CSkeletalMesh::CSkeletalMesh()
{
	MeshScale.Set(1, 1, 1);
	MeshOrigin.Set(0, 0, 0);
	RotOrigin.Set(0, 0, 0);
	PostLoad();
}


CSkeletalMesh::~CSkeletalMesh()
{
	for (int i = 0; i < Materials.Num(); i++)
		if (Materials[i].RenMaterial)
			delete Materials[i].RenMaterial;
}


//?? move outside
static void SetAxis(const CRotator &Rot, CAxis &Axis)
{
	CVec3 angles;
	angles[YAW]   = -Rot.Yaw   / 32768.0f * 180;
	angles[ROLL]  = -Rot.Pitch / 32768.0f * 180;
	angles[PITCH] =  Rot.Roll  / 32768.0f * 180;
	Axis.FromEuler(angles);
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


void CSkeletalMesh::PostLoad()
{
	guard(CSkeletalMesh::PostLoad);

	int i;

	// compute transforms for root bone
	SetAxis(RotOrigin, BaseTransform.axis);
	BaseTransform.origin[0] = MeshOrigin[0] * MeshScale[0];
	BaseTransform.origin[1] = MeshOrigin[1] * MeshScale[1];
	BaseTransform.origin[2] = MeshOrigin[2] * MeshScale[2];

	BaseTransformScaled.axis = BaseTransform.axis;
	CVec3 tmp;
	tmp[0] = 1.0f / MeshScale[0];
	tmp[1] = 1.0f / MeshScale[1];
	tmp[2] = 1.0f / MeshScale[2];
	BaseTransformScaled.axis.PrescaleSource(tmp);
	BaseTransformScaled.origin = MeshOrigin;

	// compute bone data
	int numBones = Skeleton.Num();
	if (!numBones) return;	// empty skeleton, just created

	// compute reference bone inverted coords
	CCoords RefCoords[MAX_MESH_BONES];
	for (i = 0; i < numBones; i++)
	{
		CMeshBone &B = Skeleton[i];
		// NOTE: assumed, that parent bones goes first
		assert(B.ParentIndex <= i);

		CVec3 BP;
		CQuat BO;
		// get default pose
		BP = B.Position;
		BO = B.Orientation;
		if (!i) BO.Conjugate();

		CCoords &BC = RefCoords[i];
		BC.origin = BP;
		BO.ToAxis(BC.axis);
		// move bone position to global coordinate space
		if (i)	// do not rotate root bone
			RefCoords[B.ParentIndex].UnTransformCoords(BC, BC);
		// store inverted transformation too
		InvertCoords(BC, B.InvRefCoords);
	}

	// check bones tree
	// get bone subtree sizes
	int treeSizes[MAX_MESH_BONES], depth[MAX_MESH_BONES];
	int numIndices = 0;
	CheckBoneTree(Skeleton, 0, treeSizes, depth, numIndices, MAX_MESH_BONES);
	assert(numIndices == numBones);
	for (i = 0; i < numBones; i++)
		Skeleton[i].SubtreeSize = treeSizes[i];	// remember subtree size

	// load or update textures
	for (i = 0; i < Materials.Num(); i++)
	{
		CMeshMaterial &M = Materials[i];
		if (M.RenMaterial && M.RenMaterial->Filename != M.Filename)
		{
			delete M.RenMaterial;
			M.RenMaterial = NULL;
		}
		if (M.Filename[0])
			M.RenMaterial = new CRenderingMaterial(M.Filename);
	}

	unguard;
}


void CSkeletalMesh::PostEditChange()
{
	// NOTE: some data, computed in PostLoad() are not editable, but recomputed anyway
	// (but, should not be a performance issue)
	PostLoad();
}


void CSkeletalMesh::DumpBones()
{
#if 1
	int treeSizes[MAX_MESH_BONES], depth[MAX_MESH_BONES];
	int numIndices = 0;
	CheckBoneTree(Skeleton, 0, treeSizes, depth, numIndices, MAX_MESH_BONES);
	//?? NOTE: DEPTH INFORMATION can be easily computed (simple loop by parent index)
	for (int i = 0; i < numIndices; i++)
	{
		const CMeshBone &B = Skeleton[i];
		int parent = B.ParentIndex;
		appPrintf("bone#%2d (parent %2d); tree size: %2d   ", i, parent, treeSizes[i]);
		for (int j = 0; j < depth[i]; j++)
		{
	#if 0
			// simple picture
			appPrintf("|  ");
	#else
			// graph-like picture
			bool found = false;
			for (int n = i+1; n < numIndices; n++)
			{
				if (depth[n] >  j+1) continue;
				if (depth[n] == j+1) found = true;
				break;
			}
		#if 0 // _WIN32
			// pseudographics
			if (j == depth[i]-1)
				appPrintf(found ? "\xC3\xC4\xC4" : "\xC0\xC4\xC4");	// [+--] : [\--]
			else
                appPrintf(found ? "\xB3  " : "   ");				// [|  ] : [   ]
		#else
			// ASCII
			if (j == depth[i]-1)
				appPrintf(found ? "+--" : "\\--");
			else
				appPrintf(found ? "|  " : "   ");
        #endif
	#endif
		}
		appPrintf("%s\n", *B.Name);
	}
#endif
}


bool CSkeletalMesh::BindMaterial(int index)
{
	guard(CSkeletalMesh::BindMaterial);
	if (index < 0 || index >= Materials.Num())
		return false;
	CRenderingMaterial* Mat = Materials[index].RenMaterial;
	if (!Mat)
		return false;
	return Mat->Bind();
	unguard;
}
