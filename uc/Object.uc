class Object;
	//!! noexport


struct Vec3
{
	var() float X;
	var() float Y;
	var() float Z;
};


struct Rotator
{
	var() int	Yaw;
	var() int	Pitch;
	var() int	Roll;
};


/*
 *	CObject internal layout
 */

var int VmtPtr;		// internal VMT pointer
var int TypeInfo;	// pointer to type information
