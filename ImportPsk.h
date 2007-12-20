/**
 *	PSK file format structures
 */

struct VChunkHeader
{
	char		ChunkID[20];
	int			TypeFlag;
	int			DataSize;
	int			DataCount;

	friend CArchive& operator<<(CArchive &Ar, VChunkHeader &H)
	{
		SerializeChars(Ar, ARRAY_ARG(H.ChunkID));
		return Ar << H.TypeFlag << H.DataSize << H.DataCount;
	}
};


struct VVertex
{
	int			PointIndex;				// short, padded to int
	float		U, V;
	byte		MatIndex;
	byte		Reserved;
	short		Pad;					// not used

	friend CArchive& operator<<(CArchive &Ar, VVertex &V)
	{
		Ar << V.PointIndex << V.U << V.V << V.MatIndex << V.Reserved << V.Pad;
		if (Ar.IsLoading)
			V.PointIndex &= 0xFFFF;		// clamp padding bytes
		return Ar;
	}
};


struct VTriangle
{
	word		WedgeIndex[3];			// Point to three vertices in the vertex list.
	byte		MatIndex;				// Materials can be anything.
	byte		AuxMatIndex;			// Second material (unused).
	unsigned	SmoothingGroups;		// 32-bit flag for smoothing groups.

	friend CArchive& operator<<(CArchive &Ar, VTriangle &T)
	{
		Ar << T.WedgeIndex[0] << T.WedgeIndex[1] << T.WedgeIndex[2];
		Ar << T.MatIndex << T.AuxMatIndex << T.SmoothingGroups;
		return Ar;
	}
};


struct VMaterial
{
	char		MaterialName[64];
	int			TextureIndex;
	unsigned	PolyFlags;
	int			AuxMaterial;
	unsigned	AuxFlags;
	int			LodBias;
	int			LodStyle;

	friend CArchive& operator<<(CArchive &Ar, VMaterial &M)
	{
		SerializeChars(Ar, ARRAY_ARG(M.MaterialName));
		return Ar << M.TextureIndex << M.PolyFlags << M.AuxMaterial <<
					 M.AuxFlags << M.LodBias << M.LodStyle;
	}
};


// A bone: an orientation, and a position, all relative to their parent.
struct VJointPos
{
	CQuat			Orientation;
	CVec3			Position;
	float			Length;
	CVec3			Size;

	friend CArchive& operator<<(CArchive &Ar, VJointPos &P)
	{
		return Ar << P.Orientation << P.Position << P.Length << P.Size;
	}
};


struct VBone
{
	char			Name[64];
	unsigned		Flags;
	int				NumChildren;
	int				ParentIndex;			// 0 if this is the root bone.
	VJointPos		BonePos;

	friend CArchive& operator<<(CArchive &Ar, VBone &B)
	{
		SerializeChars(Ar, ARRAY_ARG(B.Name));
		return Ar << B.Flags << B.NumChildren << B.ParentIndex << B.BonePos;
	}
};


struct VRawBoneInfluence
{
	float			Weight;
	int				PointIndex;
	int				BoneIndex;

	friend CArchive& operator<<(CArchive &Ar, VRawBoneInfluence &I)
	{
		return Ar << I.Weight << I.PointIndex << I.BoneIndex;
	}
};



#define LOAD_CHUNK(var, name)	\
	VChunkHeader var;			\
	Ar << var;					\
	if (strcmp(var.ChunkID, name) != 0) \
		appError("LoadChunk: expecting header \""name"\", but found \"%s\"", var.ChunkID);
