/**
 * This is a set of AnimSequences
 * All sequence have the same number of tracks, and they relate to the same bone names.
 */
class AnimSet
	extends Object;


const MAX_BONE_NAME = 32;	//?? move const to SkeletalMesh.uc, requires "dependson" class keyword support
const MAX_ANIM_NAME = 64;

//!! NOTE: its better to use array<string[MAX_BONE_NAME]>' instead of
//!!		array<AnimBone>, but current RTTI implementation does not allows
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
	/*!! TODO: Scale keys */
	var() array<Vec3>		KeyScale;
	/** Key times, in seconds */
	var() array<float>		KeyTime;

	structcpptext
	{
		friend CArchive& operator<<(CArchive &Ar, CAnalogTrack &T)
		{
			return Ar << T.KeyQuat << T.KeyPos << T.KeyScale << T.KeyTime;
		}
	}
};


struct AnimNotify
{
	var() float				Time;
	//!! EXTEND STRUCTURE

	structcpptext
	{
		friend CArchive &operator<<(CArchive &Ar, CAnimNotify &N)
		{
			return Ar << N.Time;
		}
	}
};


struct MeshAnimSeq
{
	/** Name of the animation sequence */
	var() editconst string[MAX_ANIM_NAME] Name;
	/** Number for tweaking playback rate of this animation globally */
	var() float				Rate;
	/** Length (in seconds) of this AnimSequence if played back with a speed of 1 */
	var int					NumFrames;
	/** Raw keyframe data for this sequence */
	var array<AnalogTrack>	Tracks;
	/*!! TODO: Animation notifies */
	var() array<AnimNotify>	Notifies;

	structcpptext
	{
		/**
		 * Interpolate bone position from animation track for specified time
		 */
		void GetBonePosition(int TrackIndex, float Frame, bool Loop, CVec3 &DstPos, CQuat &DstQuat) const;
		/**
		 * Query size statistics about this animation sequence
		 */
		void GetMemFootprint(int *Compressed, int *Uncompressed = NULL);

		friend CArchive &operator<<(CArchive &Ar, CMeshAnimSeq &A)
		{
			return Ar << A.Name << A.Rate << A.NumFrames << A.Tracks << A.Notifies;
		}
	}
};


/** Bone name that each track relates to. TrackBoneName.Num() == Number of tracks. */
var array<AnimBone>			TrackBoneName;
/** Actual animation sequence information */
var array<MeshAnimSeq>		Sequences;
/**
 *	Indicates that only the rotation should be taken from the animation sequence and the translation
 *  should come from the SkeletalMesh ref pose. Note that the root bone always takes translation from
 *  the animation, even if this flag is set.
 */
var() bool					AnimRotationOnly;


cpptext
{
	CAnimSet()
	:	AnimRotationOnly(false)
	{}

	/**
	 * Query size statistics about all animation sequences
	 */
	void GetMemFootprint(int *Compressed, int *Uncompressed = NULL);
	/**
	 * Find animation sequence by name. Case-insensitive search. When animation is not
	 * found, returns NULL.
	 */
	const CMeshAnimSeq *FindAnim(const char *AnimName) const;

	virtual void Serialize(CArchive &Ar)
	{
		Super::Serialize(Ar);
		Ar << TrackBoneName << Sequences << AnimRotationOnly;
	}
}
