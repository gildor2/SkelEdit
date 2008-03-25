#include "Core.h"
#include "AnimClasses.h"


//#define DEBUG_COMPRESS		1


#define SAME(x,y)   ( fabs(x-y) < 0.001 )

static bool VectorSame(const CVec3 &V1, const CVec3 &V2)
{
	return SAME(V1[0], V2[0]) && SAME(V1[1], V2[1]) && SAME(V1[2], V2[2]);
}

static bool QuatsSame(const CQuat &Q1, const CQuat &Q2)
{
	return SAME(Q1.x, Q2.x) && SAME(Q1.y, Q2.y) && SAME(Q1.z, Q2.z) && SAME(Q1.w, Q2.w);
}


// remove same keys from KeyQuat and KeyPos arrays
void RemoveRedundantKeys(CAnimSet &Anim)
{
	guard(RemoveRedundantKeys);

	int numRemovedKeys = 0, numKeys = 0;		// statistics
	for (int seq = 0; seq < Anim.Sequences.Num(); seq++)
	{
		// for each sequence ...
		CMeshAnimSeq &Seq = Anim.Sequences[seq];
		for (int bone = 0; bone < Seq.Tracks.Num(); bone++)
		{
			// for each bone track
			CAnalogTrack &Track = Seq.Tracks[bone];

			// ensure at least 2 keys
			numKeys += Track.KeyQuat.Num() + Track.KeyPos.Num();
			if (Track.KeyQuat.Num() <= 1 || Track.KeyPos.Num() <= 1 || Track.KeyTime.Num() <= 1)
				continue;

			int i, numKeys;
			bool remove;

			// compare KeyQuat
			remove = true;
			CQuat &Q0 = Track.KeyQuat[0];
			numKeys = Track.KeyQuat.Num();
			for (i = 1; i < numKeys; i++)
			{
				CQuat &Q1 = Track.KeyQuat[i];
				if (!QuatsSame(Q0, Q1))
				{
					remove = false;
					break;
				}
			}
			if (remove)
			{
				// remove KeyQuat track
				Track.KeyQuat.Remove(1, numKeys - 1);
				numRemovedKeys += numKeys - 1;
			}
			//!! note: can remove KeyPos when Anim.AnimRotationOnly==true, but:
			//!! 1) this is unrecoverable, and should be performed manually
			//!! 2) should be done AFTER importing and some editing of AnimSet, i.e. executed manually
			//!! this may be done as part of key reduction with lerping
			// compare KeyPos
			remove = true;
			CVec3 &V0 = Track.KeyPos[0];
			numKeys = Track.KeyPos.Num();
			for (i = 1; i < numKeys; i++)
			{
				CVec3 &V1 = Track.KeyPos[i];
				if (!VectorSame(V0, V1))
				{
					remove = false;
					break;
				}
			}
			if (remove)
			{
				// remove KeyPos track
				Track.KeyPos.Remove(1, numKeys - 1);
				numRemovedKeys += numKeys - 1;
			}
			if (Track.KeyQuat.Num() == 1 && Track.KeyPos.Num() == 1)
			{
				// position and orientation tracks are single-entry, remove unnecessary
				// time keys for this sequence/bone
				Track.KeyTime.Remove(1, Track.KeyTime.Num() - 1);
			}
		}
	}
	appPrintf("Removed %d of %d (%.0f%%) redundant keys\n", numRemovedKeys, numKeys,
		numRemovedKeys * 100.0f / numKeys);

	unguard;
}


static bool CanLerpKeys(CAnalogTrack &T, int key1, int key, int key2)
{
	guard(CanLerpKeys);

	// get time fraction of current key between two neibor keys
	float t1 = T.KeyTime[key1];
	float t  = T.KeyTime[key ];
	float t2 = T.KeyTime[key2];
	float frac = (t - t1) / (t2 - t1);

	// compare position part of key
	if (T.KeyPos.Num() > 1)
	{
		CVec3 newVec;
		Lerp(T.KeyPos[key1], T.KeyPos[key2], frac, newVec);
		if (!VectorSame(newVec, T.KeyPos[key]))
			return false;
	}

	// compare rotation part of key
	if (T.KeyQuat.Num() > 1)
	{
		CQuat newQuat;
		Slerp(T.KeyQuat[key1], T.KeyQuat[key2], frac, newQuat);
		if (!QuatsSame(newQuat, T.KeyQuat[key]))
			return false;
	}

	// keys can be interpolated
	return true;

	unguard;
}


//!! Options for this function:
//!! 1) error for rotation and position
//!! 2) which sequences to compress

void CompressAnimation(CAnimSet &Anim)
{
	guard(CompressAnimation);

	// get maximal sequence length
	int maxSequenceLen = 0;
	int seq;
	for (seq = 0; seq < Anim.Sequences.Num(); seq++)
	{
		CMeshAnimSeq &Seq = Anim.Sequences[seq];
		for (int bone = 0; bone < Seq.Tracks.Num(); bone++)
		{
			int len = Seq.Tracks[bone].KeyTime.Num();
			if (len > maxSequenceLen)
				maxSequenceLen = len;
		}
	}
//	appPrintf("Maximal sequence length is %d keys\n", maxSequenceLen);

	bool* removeKeyFlag = new bool[maxSequenceLen];


	int numRemovedKeys = 0, numKeys = 0;		// statistics
	for (seq = 0; seq < Anim.Sequences.Num(); seq++)
	{
		// for each sequence ...
		CMeshAnimSeq &Seq = Anim.Sequences[seq];
		for (int bone = 0; bone < Seq.Tracks.Num(); bone++)
		{
#if DEBUG_COMPRESS
			int wipedTrackKeys = 0;
#endif
			// for each bone track
			CAnalogTrack &Track = Seq.Tracks[bone];

			// ensure at least 3 keys
			numKeys += Track.KeyQuat.Num() + Track.KeyPos.Num();
			if (Track.KeyQuat.Num() < 3 && Track.KeyPos.Num() < 3)
				continue;

			int numKeys    = Track.KeyTime.Num();
			bool checkQuat = Track.KeyQuat.Num() >= 3;
			bool checkPos  = Track.KeyPos.Num()  >= 3;
			assert(checkQuat  || checkPos);
			assert(!checkQuat || Track.KeyQuat.Num() == numKeys);
			assert(!checkPos  || Track.KeyPos.Num()  == numKeys);

			removeKeyFlag[0] = removeKeyFlag[numKeys-1] = false;

			int key;
			int lastKey = 0;	// last available key
			bool hasRemovedKeys = false;
			for (key = 1; key < numKeys - 1; key++)
			{
				bool remove = CanLerpKeys(Track, lastKey, key, key+1);
				if (remove)
				{
					// can lerp current key, should check previous removed keys
					// (their error may be increased - should not remove current
					// key in that case)
					for (int k = lastKey+1; k < key; k++)
					{
						if (!CanLerpKeys(Track, lastKey, k, key+1))
						{
							remove = false;
							break;
						}
					}
				}
				// remember decision
				removeKeyFlag[key] = remove;
				if (!remove)
				{
					lastKey = key;
				}
				else
				{
					hasRemovedKeys = true;
					numRemovedKeys++;
#if DEBUG_COMPRESS
					wipedTrackKeys++;
#endif
				}
			}
			// remove marked keys
			if (hasRemovedKeys)
			{
				for (key = numKeys-1; key >= 0; key--)
				{
					if (removeKeyFlag[key])
					{
						Track.KeyTime.Remove(key, 1);
						if (checkQuat)
							Track.KeyQuat.Remove(key, 1);
						if (checkPos)
							Track.KeyPos.Remove(key, 1);
					}
				}
			}
#if DEBUG_COMPRESS
			if (wipedTrackKeys)
				appPrintf("%s / %s[%d]: removed %d keys\n",
					*Seq.Name, *Anim.TrackBoneName[bone].Name, bone, wipedTrackKeys);
#endif
		}
	}
	appPrintf("Compression: removed %d of %d (%.0f%%) keys\n", numRemovedKeys, numKeys,
		numRemovedKeys * 100.0f / numKeys);

	delete removeKeyFlag;

	unguard;
}
