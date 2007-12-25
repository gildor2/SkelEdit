
//!!! TYPEINFO !!!

#define MAX_ANIM_NAME	64


struct CAnimBone
{
	TString<MAX_BONE_NAME> Name;
};


struct CMeshAnimSeq
{
	TString<MAX_ANIM_NAME> Name;
	float		Rate;
	int			NumFrames;
	//!! add notifications here
};


struct CAnalogTrack
{
	TArray<CQuat>			KeyQuat;	// count = 1 or KeyTime.Count
	TArray<CVec3>			KeyPos;		// count = 1 or KeyTime.Count
	TArray<float>			KeyTime;	// time of current key, measured from start of track

	friend CArchive& operator<<(CArchive &Ar, CAnalogTrack &T)
	{
		return Ar << T.KeyQuat << T.KeyPos << T.KeyTime;
	}
};


struct CMotionChunk
{
	TArray<CAnalogTrack>	Tracks;		// track per bone
};


class CAnimSet
{
public:
	TArray<CAnimBone>		RefBones;
	TArray<CMeshAnimSeq>	AnimSeqs;	// information about animation
	TArray<CMotionChunk>	Motion;		// same index, as corresponding AnimSeqs[] animation
};
