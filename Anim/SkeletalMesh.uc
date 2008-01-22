class SkeletalMesh
	extends Object;

const MAX_MESH_BONES		= 256;
//const MAX_BONE_NAME		= 32;		//?? check usage, remove
const MAX_MESH_MATERIALS	= 256;

const MAX_VERTEX_INFLUENCES	= 4;
const NO_INFLUENCE			= -1;


struct PointWeight
{
	/** NO_INFLUENCE == entry not used */
	var short   BoneIndex;
	/** 0==0.0f, 65535==1.0f */
	var ushort  Weight;

	structcpptext
	{
		friend CArchive& operator<<(CArchive &Ar, CPointWeight &W)
		{
			return Ar << W.BoneIndex << W.Weight;
		}
	}
};


struct MeshPoint
{
	var Vec3		Point;
	var Vec3		Normal;
	var float		U, V;
	var PointWeight	Influences[MAX_VERTEX_INFLUENCES];

	structcpptext
	{
		friend CArchive& operator<<(CArchive &Ar, CMeshPoint &P)
		{
			Ar << P.Point << P.Normal << P.U << P.V;
			for (int i = 0; i < MAX_VERTEX_INFLUENCES; i++)
				Ar << P.Influences[i];
			return Ar;
		}
	}
};


struct MeshBone
{
	var() string[MAX_BONE_NAME] Name;
	var() Vec3				Position;
	var() Quat				Orientation;
	var() editconst int		ParentIndex;

	/** following data generated after mesh loading */
	var   Coords			InvRefCoords;
	var   int				SubtreeSize;

	structcpptext
	{
		friend CArchive& operator<<(CArchive &Ar, CMeshBone &B)
		{
			return Ar << B.Name << B.Position << B.Orientation << AR_INDEX(B.ParentIndex);
		}
	}
};


struct MeshSection
{
	var() int		MaterialIndex;
	var() int		FirstIndex;
	var() int		NumIndices;

	structcpptext
	{
		friend CArchive& operator<<(CArchive &Ar, CMeshSection &S)
		{
			return Ar << AR_INDEX(S.MaterialIndex) << S.FirstIndex << S.NumIndices;
		}
	}
};


struct SkeletalMeshLod
{
	var() editconst array<MeshSection> Sections;
	var   array<MeshPoint>   Points;
	var   array<int>         Indices;

	structcpptext
	{
		friend CArchive& operator<<(CArchive &Ar, CSkeletalMeshLod &L)
		{
			return Ar << L.Sections << L.Points << L.Indices;
		}
	}
};


/** Origin in original coordinate system */
var(Orientation)	Vec3		MeshOrigin;
/** Amount to scale mesh when importing */
var(Orientation)	Vec3		MeshScale;
/** Amount to rotate when importing */
var(Orientation)	Rotator		RotOrigin;
/** Information for LOD levels */
var()				array<SkeletalMeshLod> Lods;
/** Skeleton bones */
var					array<MeshBone> Skeleton;


struct MeshMaterial
{
	structcpptext
	{
		friend class CRenderingMaterial;
	}
	/**
	 * #FILENAME Texture: *.bmp;*.tga
	 * Name of the material file
	 */
	var() string[64] Filename;
	/**
	 * Material, used internally in renderer
	 */
	var pointer<CRenderingMaterial> RenMaterial;

	structcpptext
	{
		friend CArchive& operator<<(CArchive &Ar, CMeshMaterial &M)
		{
			return Ar << M.Filename;
		}
	}
};

var()		array<MeshMaterial> Materials;


/**
 * Following data generated after loading
 */

var Coords BaseTransform;
var Coords BaseTransformScaled;


cpptext
{
	CSkeletalMesh();
	~CSkeletalMesh();
	virtual void PostLoad();
	virtual void PostEditChange();
	void DumpBones();
	bool BindMaterial(int index);

	virtual void Serialize(CArchive &Ar)
	{
		Super::Serialize(Ar);
		Ar << MeshOrigin << MeshScale << RotOrigin << Lods << Skeleton << Materials;
	}
}
