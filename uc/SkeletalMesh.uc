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
var(Base)	Vec3	MeshOrigin;
/** Amount to scale mesh when importing */
var(Base)	Vec3	MeshScale;
/** Amount to rotate when importing */
var(Base)	Rotator	RotOrigin;
/** Information for LOD levels */
var(Internals) array<SkeletalMeshLod> Lods;
/** Skeleton bones */
var(Internals) array<MeshBone> Skeleton;

/** Following data generated after loading */
var Coords BaseTransform;
var Coords BaseTransformScaled;


cpptext
{
	CSkeletalMesh()
	{
		MeshScale.Set(1, 1, 1);
		MeshOrigin.Set(0, 0, 0);
		RotOrigin.Set(0, 0, 0);
		UpdateProperties();
	}

	virtual void UpdateProperties()
	{
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
	}

	//?? move outside
	static inline void SetAxis(const CRotator &Rot, CAxis &Axis)
	{
		CVec3 angles;
		angles[YAW]   = -Rot.Yaw   / 32768.0f * 180;
		angles[ROLL]  = -Rot.Pitch / 32768.0f * 180;
		angles[PITCH] =  Rot.Roll  / 32768.0f * 180;
		Axis.FromEuler(angles);
	}

	virtual void Serialize(CArchive &Ar)
	{
		Super::Serialize(Ar);
		Ar << MeshOrigin << MeshScale << RotOrigin << Lods << Skeleton;
	}
}
