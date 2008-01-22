class Object
	noexport;


/*-----------------------------------------------------------------------------
 *	Common structures (declared here for typeinfo information)
 *---------------------------------------------------------------------------*/

struct Vec3
{
	var() float X;
	var() float Y;
	var() float Z;
};


struct Coords
{
	var() Vec3	Origin;
	var() Vec3	Axis[3];
};


struct Rotator
{
	var() int	Yaw;
	var() int	Pitch;
	var() int	Roll;
};


struct Quat
{
	var() float X;
	var() float Y;
	var() float Z;
	var() float W;
};


/*-----------------------------------------------------------------------------
 *	CObject internal layout
 *---------------------------------------------------------------------------*/

var int VmtPtr;		// internal VMT pointer
