#include "Core.h"
#include "AnimClasses.h"


CArchive& operator<<(CArchive &Ar, CAnimNodeChild &C)
{
	return Ar << C.Label << C.Node;
}
