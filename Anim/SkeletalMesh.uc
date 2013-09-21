class SkeletalMesh
	extends Object;

const MAX_MESH_BONES		= 256;
//const MAX_BONE_NAME		= 32;		// declared in AnimSet.uc, but should be here
const MAX_MESH_MATERIALS	= 256;

const MAX_FILE_PATH			= 64;

const MAX_VERTEX_INFLUENCES	= 4;
const NO_INFLUENCE			= -1;


const MESH_EXTENSION		= "skm";
const ANIM_EXTENSION		= "ska";



struct PointWeight
{
	/** NO_INFLUENCE == entry not used */
	var short   			BoneIndex;
	/** 0==0.0f, 65535==1.0f */
	var ushort  			Weight;

	structcpptext
	{
		friend CArchive& operator<<(CArchive &Ar, CPointWeight &W)
		{
			return Ar << W.BoneIndex << W.Weight;
		}
	}
};


/**
 * Rendering vertex structure
 */
struct MeshPoint
{
	var Vec3				Point;
	var Vec3				Normal;
	var float				U, V;
	var PointWeight			Influences[MAX_VERTEX_INFLUENCES];

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
	var() string[MAX_BONE_NAME]	Name;
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


struct MeshHitBox
{
	/**
	 * Name of this volume
	 */
	var() string[MAX_BONE_NAME] Name;
	/**
	 * #ENUM MeshBone
	 * Associates this box with bone in a skeletal mesh
	 */
	var() editconst int		BoneIndex;
	/**
	 * Bounding volume (oriented bounding box)
	 * Box occupy a volume (-0.5,-0.5,-0.5)-(0.5,0.5,0.5) in local coordinate system
	 */
	var() Coords			Coords;

	structcpptext
	{
		friend CArchive& operator<<(CArchive &Ar, CMeshHitBox &B)
		{
			return Ar << B.Name << AR_INDEX(B.BoneIndex) << B.Coords;
		}
	}
};


/**
 * Defines a named attachment location on the SkeletalMesh
 */
struct MeshSocket
{
	/**
	 * Name of attachment
	 */
	var() string[MAX_BONE_NAME] Name;
	/**
	 * #ENUM MeshBone
	 * Associates this socket with bone in a skeletal mesh
	 */
	var() editconst int		BoneIndex;
	/**
	 * Coordinate system of socket, relative to parent bone's coordinates
	 */
	var() Coords			Coords;

	structcpptext
	{
		friend CArchive& operator<<(CArchive &Ar, CMeshSocket &S)
		{
			return Ar << S.Name << AR_INDEX(S.BoneIndex) << S.Coords;
		}
	}
};


/**
 *	Structure, describing single mesh section (for rendering)
 */
struct MeshSection
{
	/** Index in Mesh.Materials array */
	var() int				MaterialIndex;
	/** First index, corresponding to this section in MeshLod.Indices array */
	var() int				FirstIndex;
	/** Number of section indices in MeshLod.Indices */
	var() int				NumIndices;

	structcpptext
	{
		friend CArchive& operator<<(CArchive &Ar, CMeshSection &S)
		{
			return Ar << AR_INDEX(S.MaterialIndex) << S.FirstIndex << S.NumIndices;
		}
	}
};


/**
 * Visualization mesh. Contains geometry only. Materials, skeleton etc
 * shared between all lod levels in SkeletalMesh class.
 */
struct SkeletalMeshLod
{
	/** Separate renderable mesh parts */
	var() editconst array<MeshSection> Sections;
	/** Vertices of whole mesh (for all sections) */
	var   array<MeshPoint>	Points;
	/** Index buffer for whole mesh */
	var   array<int>		Indices;

	structcpptext
	{
		friend CArchive& operator<<(CArchive &Ar, CSkeletalMeshLod &L)
		{
			return Ar << L.Sections << L.Points << L.Indices;
		}
	}
};


/** Origin in original coordinate system */
var(Orientation)	Vec3	MeshOrigin;
/** Amount to scale mesh when importing */
var(Orientation)	Vec3	MeshScale;
/** Amount to rotate when importing */
var(Orientation)	Rotator	RotOrigin;
/** Information for LOD levels */
var() editnoadd		array<SkeletalMeshLod> Lods;
/** Skeleton bones */
var					array<MeshBone> Skeleton;
/** Collision volumes */
var(Extra Data) editnoadd array<MeshHitBox> BoundingBoxes;
/** Attachment sockets */
var(Extra Data) editnoadd array<MeshSocket> Sockets;


struct MeshMaterial
{
	structcpptext
	{
		// internal class, encapsulated rendering material
		friend class CRenderingMaterial;
	}
	/**
	 * #FILENAME Texture: *.bmp;*.tga
	 * Name of the material file
	 */
	var() string[MAX_FILE_PATH] Filename;
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

/**
 * List of materials applied to this mesh
 */
var() editfixedsize array<MeshMaterial> Materials;


/**
 * Following data generated after loading
 */

var Coords BaseTransform;
var Coords BaseTransformScaled;


cpptext
{
	CSkeletalMesh();
	virtual ~CSkeletalMesh();
	virtual void PostLoad();
	virtual void PostEditChange();
	/**
	 * Case-insensitive bone search. If bone found, returns its index, or -1 otherwise.
	 */
	int FindBone(const char *BoneName) const;
	/**
	 * Dump bone hierarchy to console
	 */
	void DumpBones();
#if EDITOR
	/**
	 * Editor: bind material from 'Materials' array for rendering. Return false when
	 * material cannot be bound (for example, texture was not loaded).
	 */
	bool BindMaterial(int index) const;
#endif

	virtual void Serialize(CArchive &Ar)
	{
		Super::Serialize(Ar);
		Ar << MeshOrigin << MeshScale << RotOrigin << Lods << Skeleton << Materials
		   << BoundingBoxes << Sockets;
	}
}
