//!! TYPEINFO !!


#define MAX_MESH_BONES			256
#define MAX_BONE_NAME			32
#define MAX_MESH_MATERIALS		256

#define MAX_VERTEX_INFLUENCES	4
#define NO_INFLUENCE			-1


// wedge structure
struct CMeshPoint
{
	struct CWeight
	{
		short		BoneIndex;		// -1 == entry not used
		word		Weight;			// 0==0.0f, 65535 == 1.0f
		friend CArchive& operator<<(CArchive &Ar, CWeight &W)
		{
			return Ar << W.BoneIndex << W.Weight;
		}
	};

	CVec3			Point;
	CVec3			Normal;
	float			U, V;
	CWeight			Influences[MAX_VERTEX_INFLUENCES];	// 1st entry with BoneIndex==NO_INFLUENCE will
														// mark end of array

	friend CArchive& operator<<(CArchive &Ar, CMeshPoint &P)
	{
		Ar << P.Point << P.Normal << P.U << P.V;
		for (int i = 0; i < MAX_VERTEX_INFLUENCES; i++)
			Ar << P.Influences[i];
		return Ar;
	}
};


struct CMeshBone
{
	char			Name[MAX_BONE_NAME];		//?? string; REQUIRED FOR SERIALIZATION !
	CVec3			Position;
	CQuat			Orientation;
	int				ParentIndex;
};


struct CMeshSection
{
	int				MaterialIndex;
	int				FirstIndex;
	int				NumIndices;
};


struct CSkeletalMeshLod
{
	TArray<CMeshSection>	Sections;
	TArray<CMeshPoint>		Points;
	TArray<int>				Indices;
};


//?? move outside
inline void SetAxis(const CRotator &Rot, CAxis &Axis)
{
	CVec3 angles;
	angles[YAW]   = -Rot.Yaw   / 32768.0f * 180;
	angles[ROLL]  = -Rot.Pitch / 32768.0f * 180;
	angles[PITCH] =  Rot.Roll  / 32768.0f * 180;
	Axis.FromEuler(angles);
}


class CSkeletalMesh : public CObject
{
public:
	CSkeletalMesh()
	{
		MeshScale.Set(1, 1, 1);
		MeshOrigin.Set(0, 0, 0);
		RotOrigin.Set(0, 0, 0);
		UpdateProperties();
	}

	CVec3					MeshOrigin;
	CVec3					MeshScale;
	CRotator				RotOrigin;

	TArray<CSkeletalMeshLod> Lods;
	TArray<CMeshBone>		Skeleton;

	// computed properties
	CCoords					BaseTransform;			// rotation for mesh; have identity axis
	CCoords					BaseTransformScaled;	// rotation for mesh with scaled axis

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
};
