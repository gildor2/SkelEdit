#ifndef __MESHINSTANCE_H__
#define __MESHINSTANCE_H__


/*-----------------------------------------------------------------------------
	CSkelMeshInstance class
-----------------------------------------------------------------------------*/

#define MAX_SKELANIMCHANNELS	32


class CSkelMeshInstance
{
	struct CAnimChan
	{
		const CMeshAnimSeq *Anim1;	// current animation sequence; NULL for default pose
		const CMeshAnimSeq *Anim2;	// secondary animation for blending with; -1 = not used
		float		Time;			// current animation frame for primary animation
		float		SecondaryBlend;	// value = 0 (use primary animation) .. 1 (use secondary animation)
		float		BlendAlpha;		// blend with previous channels; 0 = not affected, 1 = fully affected
		int			RootBone;		// root animation bone
		float		Rate;			// animation rate multiplier for Anim.Rate
		float		TweenTime;		// time to stop tweening; 0 when no tweening at all
		float		TweenStep;		// fraction between current pose and desired pose; updated in UpdateAnimation()
		bool		Looped;
	};

public:
	// mesh state
	int					LodNum;
	// linked data
	const CSkeletalMesh	*pMesh;
	const CAnimSet		*pAnim;

	CSkelMeshInstance()
	:	LodNum(-1)
	,	pMesh(NULL)
	,	pAnim(NULL)
	,	MaxAnimChannel(-1)
	,	BoneData(NULL)
	,	MeshVerts(NULL)
	,	MeshNormals(NULL)
	{
		ClearSkelAnims();
	}

	virtual ~CSkelMeshInstance();

	void SetMesh(const CSkeletalMesh *Mesh);
	void SetAnim(const CAnimSet *Anim);

	void ClearSkelAnims();
//??	void StopAnimating(bool ClearAllButBase);

#if EDITOR
	void DrawSkeleton(bool ShowLabels);
	void DrawBoxes(int highlight = -1);
	void DrawMesh(bool Wireframe, bool Normals, bool Texturing);
#endif

	// skeleton configuration
	void SetBoneScale(const char *BoneName, float scale = 1.0f);
	//!! SetBone[Direction|Location|Rotation]()

	// animation control
	void PlayAnim(const char *AnimName, float Rate = 1, float TweenTime = 0, int Channel = 0)
	{
		PlayAnimInternal(AnimName, Rate, TweenTime, Channel, false);
	}
	void LoopAnim(const char *AnimName, float Rate = 1, float TweenTime = 0, int Channel = 0)
	{
		PlayAnimInternal(AnimName, Rate, TweenTime, Channel, true);
	}
	void TweenAnim(const char *AnimName, float TweenTime, int Channel = 0)
	{
		PlayAnimInternal(AnimName, 0, TweenTime, Channel, false);
	}
	void StopLooping(int Channel = 0)
	{
		GetStage(Channel).Looped = false;
	}
	void FreezeAnimAt(float Time, int Channel = 0)
	{
		CAnimChan &Chn = GetStage(Channel);
		Chn.Time = Time;
		Chn.Rate = 0;
	}

	// animation state

	// get current animation information:
	// Frame     = 0..NumFrames
	// NumFrames = animation length, frames
	// Rate      = frames per seconds
	void GetAnimParams(int Channel, const char *&AnimName,
		float &Frame, float &NumFrames, float &Rate) const;

	bool HasAnim(const char *AnimName) const
	{
		return FindAnim(AnimName) >= 0;
	}
	bool IsAnimating(int Channel = 0);
	bool IsTweening(int Channel = 0)
	{
		return GetStage(Channel).TweenTime > 0;
	}

	// animation blending
	void SetBlendParams(int Channel, float BlendAlpha, const char *BoneName = NULL);
	void SetBlendAlpha(int Channel, float BlendAlpha);
	void SetSecondaryAnim(int Channel, const char *AnimName = NULL);
	void SetSecondaryBlend(int Channel, float BlendAlpha);
	//?? -	AnimBlendToAlpha() - animate BlendAlpha coefficient
	//?? -	functions to smoothly replace current animation with another in a fixed time
	//??	(run new anim as secondary, ramp alpha from 0 to 1, and when blend becomes 1.0f
	//??	- replace 1st anim with 2nd, and clear 2nd

	// animation enumeration
	int GetAnimCount() const
	{
		return 0;
		//!!! IMPLEMENT OR DELETE
	}
	const char *GetAnimName(int Index) const
	{
		guard(CSkelMeshInstance::GetAnimName);
		return "None";
		//!!! IMPLEMENT OR DELETE
		/*
		if (Index < 0) return "None";
		const USkeletalMesh *Mesh = GetMesh();
		assert(Mesh->Animation);
		return Mesh->Animation->AnimSeqs[Index].Name;
		*/
		unguard;
	}

	const CCoords &GetBoneCoords(int BoneIndex) const;
	const CCoords &GetBoneTransform(int BoneIndex) const;

	void UpdateAnimation(float TimeDelta);

protected:
	// mesh data
	struct CMeshBoneData *BoneData;
	CVec3		*MeshVerts;			//!! combine with MeshNormals
	CVec3		*MeshNormals;
	// animation state
	CAnimChan	Channels[MAX_SKELANIMCHANNELS];
	int			MaxAnimChannel;

	CAnimChan &GetStage(int StageIndex)
	{
		assert(StageIndex >= 0 && StageIndex < MAX_SKELANIMCHANNELS);
		return Channels[StageIndex];
	}
	const CAnimChan &GetStage(int StageIndex) const
	{
		assert(StageIndex >= 0 && StageIndex < MAX_SKELANIMCHANNELS);
		return Channels[StageIndex];
	}
	const CMeshAnimSeq *FindAnim(const char *AnimName) const;
	void PlayAnimInternal(const char *AnimName, float Rate, float TweenTime, int Channel, bool Looped);
	void UpdateSkeleton();
};


#endif // __MESHINSTANCE_H__
