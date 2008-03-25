#include "Core.h"
#include "AnimClasses.h"


/*-----------------------------------------------------------------------------
	CMeshAnimSeq class
-----------------------------------------------------------------------------*/

#define MAX_LINEAR_KEYS		4

// debugging staff ...

//#define DEBUG_BIN_SEARCH	1

#if 0
#	define DBG		printf
#else
#	define DBG		if (1) {} else printf
#endif


void CMeshAnimSeq::GetBonePosition(int TrackIndex, float Frame, bool Loop, CVec3 &DstPos, CQuat &DstQuat) const
{
	guard(CMeshAnimSeq::GetBonePosition);

	int i;

	const CAnalogTrack &A = Tracks[TrackIndex];

	// fast case: 1 frame only
	if (A.KeyTime.Num() == 1)
	{
		DstPos  = A.KeyPos[0];
		DstQuat = A.KeyQuat[0];
		return;
	}

	// find index in time key array
	int NumKeys = A.KeyTime.Num();
	// *** binary search ***
	int Low = 0, High = NumKeys-1;
	DBG(">>> find %.5f\n", Frame);
	while (Low + MAX_LINEAR_KEYS < High)
	{
		int Mid = (Low + High) / 2;
		DBG("   [%d..%d] mid: [%d]=%.5f", Low, High, Mid, A.KeyTime[Mid]);
		if (Frame < A.KeyTime[Mid])
			High = Mid-1;
		else
			Low = Mid;
		DBG("   d=%f\n", A.KeyTime[Mid]-Frame);
	}

	// *** linear search ***
	DBG("   linear: %d..%d\n", Low, High);
	for (i = Low; i <= High; i++)
	{
		float CurrKeyTime = A.KeyTime[i];
		DBG("   #%d: %.5f\n", i, CurrKeyTime);
		if (Frame == CurrKeyTime)
		{
			// exact key found
			DstPos  = (A.KeyPos.Num()  > 1) ? A.KeyPos[i]  : A.KeyPos[0];
			DstQuat = (A.KeyQuat.Num() > 1) ? A.KeyQuat[i] : A.KeyQuat[0];
			return;
		}
		if (Frame < CurrKeyTime)
		{
			i--;
			break;
		}
	}
	if (i > High)
		i = High;

#if DEBUG_BIN_SEARCH
	EXEC_ONCE(appPrintf("!!! WARNING: DEBUG_BIN_SEARCH enabled !!!\n"))
	//!! --- checker ---
	int i1;
	for (i1 = 0; i1 < NumKeys; i1++)
	{
		float CurrKeyTime = A.KeyTime[i1];
		if (Frame == CurrKeyTime)
		{
			// exact key found
			DstPos  = (A.KeyPos.Num()  > 1) ? A.KeyPos[i]  : A.KeyPos[0];
			DstQuat = (A.KeyQuat.Num() > 1) ? A.KeyQuat[i] : A.KeyQuat[0];
			return;
		}
		if (Frame < CurrKeyTime)
		{
			i1--;
			break;
		}
	}
	if (i1 > NumKeys-1)
		i1 = NumKeys-1;
	if (i != i1)
	{
		appError("i=%d != i1=%d", i, i1);
	}
#endif

	int X = i;
	int Y = i+1;
	float frac;
	if (Y >= NumKeys)
	{
		if (!Loop)
		{
			// clamp animation
			Y = NumKeys-1;
			assert(X == Y);
			frac = 0;
		}
		else
		{
			// loop animation
			Y = 0;
			frac = (Frame - A.KeyTime[X]) / (NumFrames - A.KeyTime[X]);
		}
	}
	else
	{
		frac = (Frame - A.KeyTime[X]) / (A.KeyTime[Y] - A.KeyTime[X]);
	}

	assert(X >= 0 && X < NumKeys);
	assert(Y >= 0 && Y < NumKeys);

	// get position
	if (A.KeyPos.Num() > 1)
		Lerp(A.KeyPos[X], A.KeyPos[Y], frac, DstPos);
	else
		DstPos = A.KeyPos[0];
	// get orientation
	if (A.KeyQuat.Num() > 1)
		Slerp(A.KeyQuat[X], A.KeyQuat[Y], frac, DstQuat);
	else
		DstQuat = A.KeyQuat[0];

	unguard;
}


void CMeshAnimSeq::GetMemFootprint(int *Compressed, int *Uncompressed)
{
	int uncompr = sizeof(CMeshAnimSeq) + sizeof(CAnalogTrack);
	int compr   = uncompr;
	uncompr += (sizeof(CQuat) + sizeof(CVec3) + sizeof(CVec3) + sizeof(float)) * NumFrames * Tracks.Num();
	for (int track = 0; track < Tracks.Num(); track++)
	{
		const CAnalogTrack &T = Tracks[track];
		compr += sizeof(CQuat) * T.KeyQuat.Num()
			  +  sizeof(CVec3) * T.KeyPos.Num()
			  +  sizeof(CVec3) * T.KeyScale.Num()
			  +  sizeof(float) * T.KeyTime.Num();
	}
	if (Compressed)   *Compressed   = compr;
	if (Uncompressed) *Uncompressed = uncompr;
}


/*-----------------------------------------------------------------------------
	CAnimSet class
-----------------------------------------------------------------------------*/

void CAnimSet::GetMemFootprint(int *Compressed, int *Uncompressed)
{
	int uncompr = sizeof(CAnimSet) + sizeof(CAnimBone) * TrackBoneName.Num();
	int compr   = uncompr;
	for (int seq = 0; seq < Sequences.Num(); seq++)
	{
		int c, u;
		Sequences[seq].GetMemFootprint(&c, &u);
		compr   += c;
		uncompr += u;
	}
	if (Compressed)   *Compressed   = compr;
	if (Uncompressed) *Uncompressed = uncompr;
}



const CMeshAnimSeq *CAnimSet::FindAnim(const char *AnimName) const
{
	for (int i = 0; i < Sequences.Num(); i++)
	{
		const CMeshAnimSeq *Seq = &Sequences[i];
		if (!stricmp(Seq->Name, AnimName))
			return Seq;
	}
	return NULL;
}
