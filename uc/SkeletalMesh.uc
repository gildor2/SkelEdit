class SkeletalMesh
	extends Object;


struct MeshAnimSeq
{
	var() editconst string[64] Name;
	var()	float	Rate;
	var		int		NumFrames;
};


/** Origin in original coordinate system */
var(Base)	Vec3	MeshOrigin;
/** Amount to scale mesh when importing */
var(Base)	Vec3	MeshScale;
/** Amount to rotate when importing */
var(Base)	Rotator	RotOrigin;
