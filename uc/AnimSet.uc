/**
 * This is a set of AnimSequences
 * All sequence have the same number of tracks, and they relate to the same bone names.
 */
class AnimSet
	extends Object;


const MAX_BONE_NAME = 32;
const MAX_ANIM_NAME = 64;

//!! NOTE: its better to use array<string[MAX_BONE_NAME]>' instead of
//!!		array<AnimBone>, but current implementation does not allows
//!!		to use arrays of strings
struct AnimBone
{
	var editconst string[MAX_BONE_NAME] Name;

	structcpptext
	{
		friend CArchive& operator<<(CArchive &Ar, CAnimBone &B)
		{
			return Ar << B.Name;
		}
	}
};


struct MeshAnimSeq
{
	/** Name of the animation sequence */
	var() editconst string[MAX_ANIM_NAME] Name;
	/** Number for tweaking playback rate of this animation globally */
	var()			float		Rate;
	/** Length (in seconds) of this AnimSequence if played back with a speed of 1 */
	var				int			NumFrames;

	structcpptext
	{
		friend CArchive &operator<<(CArchive &Ar, CMeshAnimSeq &A)
		{
			return Ar << A.Name << A.Rate << A.NumFrames;
		}
	}
};


/**
 * Raw keyframe data for one track.  Each array will contain either NumKey elements or 1 element.
 * One element is used as a simple compression scheme where if all keys are the same, they'll be
 * reduced to 1 key that is constant over the entire sequence.
 */
struct AnalogTrack
{
	/** Rotation keys */
	var() array<Quat>		KeyQuat;
	/** Position keys */
	var() array<Vec3>		KeyPos;
	/** Key times, in seconds */
	var() array<float>		KeyTime;

	structcpptext
	{
		friend CArchive& operator<<(CArchive &Ar, CAnalogTrack &T)
		{
			return Ar << T.KeyQuat << T.KeyPos << T.KeyTime;
		}
	}
};


struct MotionChunk
{
	var() array<AnalogTrack> Tracks;

	structcpptext
	{
		friend CArchive &operator<<(CArchive &Ar, CMotionChunk &M)
		{
			return Ar << M.Tracks;
		}
	}
};


/** Bone name that each track relates to. TrackBoneName.Num() == Number of tracks. */
var array<AnimBone>		TrackBoneName;
/** Actual animation sequence information. */
var array<MeshAnimSeq>	Sequences;
/** Raw keyframe data for each sequence. */
var array<MotionChunk>	Motion;
/**
 *	Indicates that only the rotation should be taken from the animation sequence and the translation
 *  should come from the SkeletalMesh ref pose. Note that the root bone always takes translation from
 *  the animation, even if this flag is set.
 */
var() bool				AnimRotationOnly;


cpptext
{
	CAnimSet()
	:	AnimRotationOnly(true)
	{}

	virtual void Serialize(CArchive &Ar)
	{
		Super::Serialize(Ar);
		Ar << TrackBoneName << Sequences << Motion << AnimRotationOnly;
	}
}
