class Object
	noexport
	abstract;


/*-----------------------------------------------------------------------------
 *	Common structures (declared here for typeinfo information)
 *---------------------------------------------------------------------------*/

struct Vec2
{
	var() float X;
	var() float Y;
};


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


struct Color3f
{
	var() float R;
	var() float G;
	var() float B;
};


/*-----------------------------------------------------------------------------
 *	CObject internal layout
 *---------------------------------------------------------------------------*/

var int VmtPtr;		// internal VMT pointer
