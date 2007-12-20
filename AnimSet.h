
//!!! TYPEINFO !!!

#define MAX_ANIM_NAME	64


struct CMeshAnimSeq
{
	char		Name[MAX_ANIM_NAME];	//?? make string (size-safe serialization)
	float		Rate;
	int			NumFrames;
	//!! notifications here
};


class CAnimSet
{
public:
	TArray<CMeshBone>		RefBones;
	TArray<CMeshAnimSeq>	AnimSeqs;
	//!! MotionChunk
};
