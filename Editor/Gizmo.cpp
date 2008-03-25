#include "Core.h"
#include "GlViewport.h"
#include "Gizmo.h"


#define MAX_AXIS_VERTS				64			// maximal number of gizmo axis verts
#define GIZMO_SIZE					0.35f		// size of gizmo in global coordinate space
#define MAX_COLLISION_DIST			4			// effective thickness of gizmo lines
#define MOUSE_Y_OFFSET				4			// used to fix mouse hotspot
#define MIN_OBJECT_SCALE			0.01f		// minimal length of axis
#define ROTATE_SCALE				40.0f


//#define DEBUG_COLLISION				1


struct CGizmo
{
	int				type;			// one of GT_XXX consts

	// visual parameters
	int				hoverAxis;		// -1 when mouse cursor is not hover gizmo

	// which object to modify
	const CCoords	*baseCoords;	// if NULL - edit in world coordinate system, otherwise
									// editVec and editAxis are local to baseCoords
	CVec3			*editVec;		// move, scale
	CAxis			*editAxis;		// scale, rotate

	// mouse state
	int				dragAxis;		// -1 == not dragging; >= 0 == dragging
	int				mouseX;
	int				mouseY;

	void Draw();
	int CheckCollision(int x, int y);
	void Drag(int dx, int dy);

	// get verts for desired axis; will return 'false' when axis does not exists
	bool GetVerts(int axis, const CVec3 **verts, int *numVerts);
};


/*

Multiple gizmos:
	- track current frame number (increment with GizmoMouse())
	- when calling SetGizmo(), find type/edit_object/parent_coords in a list
		of created gizmos, if found -- update gizmo frame, else - create new gizmo
	- on tick (GizmoMouse()), increment frame number and remove gizmos, which are too old
		(not updated with SetGizmo())

*/


static CGizmo gizmo;
static float  gizmoScale;

inline bool HasGizmos()
{
	return gizmo.type != GT_NONE;
}


void SetGizmo(int type, const CCoords *parent, CVec3 *edVec, CAxis *edAxis)
{
	guard(SetGizmo);

	if (gizmo.type       == type   &&
		gizmo.baseCoords == parent &&
		gizmo.editVec    == edVec  &&
		gizmo.editAxis   == edAxis)
	{
		// nothing changed
		return;
	}
	gizmo.dragAxis   = -1;
	gizmo.hoverAxis  = -1;
	gizmo.type       = type;
	gizmo.baseCoords = parent;
	gizmo.editVec    = edVec;
	gizmo.editAxis   = edAxis;

	unguard;
}


void RemoveGizmo()
{
	gizmo.type = GT_NONE;
}


static bool prevMouseDown = false;
static CGizmo *dragGizmo  = NULL;

bool TickGizmo(bool mouseDown, int mouseX, int mouseY, int deltaX, int deltaY)
{
	guard(TickGizmo);
	if (!HasGizmos())
	{
		prevMouseDown = mouseDown;
		dragGizmo     = NULL;
		return false;
	}

	// fix mouse hotspot
	mouseY += MOUSE_Y_OFFSET;

	if (mouseDown && dragGizmo)
	{
		// pass movement to dragGizmo
		dragGizmo->Drag(deltaX, deltaY);
	}
	else
	{
		// check collision
		int axis = gizmo.CheckCollision(mouseX, mouseY);
		gizmo.hoverAxis = axis;
		if (!prevMouseDown && mouseDown && axis >= 0)
		{
			// start dragging
			dragGizmo = &gizmo;
		}
		else
		{
			dragGizmo = NULL;
		}
	}
	prevMouseDown = mouseDown;

	return dragGizmo != NULL;
	unguard;
}


void SetGizmoScale(float scale)
{
	gizmoScale = scale;
}


void DisplayGizmo()
{
	guard(DisplayGizmo);
	if (!HasGizmos()) return;
	gizmo.Draw();
	unguard;
}


/*-----------------------------------------------------------------------------
	Simple CVec2 class
-----------------------------------------------------------------------------*/

struct CVec2
{
	float	v[2];
	inline float& operator[](int index)
	{
		return v[index];
	}
	inline const float& operator[](int index) const
	{
		return v[index];
	}
	// NOTE: for fnctions, which requires CVec3 -> float*, can easily do it using CVec3.v field
	// trivial setup functions
	inline void Zero()
	{
		memset(this, 0, sizeof(CVec2));
	}
	inline void Set(float x, float y)
	{
		v[0] = x; v[1] = y;
	}
	inline void Scale(float scale)
	{
		v[0] *= scale;
		v[1] *= scale;
	}
	float GetLength() const;
	// returns vector length
	float Normalize();
};


inline void VectorSubtract(const CVec2 &a, const CVec2 &b, CVec2 &c)
{
	c.v[0] = a.v[0] - b.v[0];
	c.v[1] = a.v[1] - b.v[1];
}


inline float dot(const CVec2 &a, const CVec2 &b)
{
	return a.v[0]*b.v[0] + a.v[1]*b.v[1];
}


float CVec2::GetLength() const
{
	return sqrt(dot(*this, *this));
}


float CVec2::Normalize()
{
	float length = sqrt(dot(*this, *this));
	if (length) Scale(1.0f/length);
	return length;
}


static float GetDistance2D(const CVec2 &p, const CVec2 &v1, const CVec2 &v2, float maxDist)
{
	int i;

	// compare with bounds
	CVec2 bounds[2];
	for (i = 0; i < 2; i++)
	{
		bounds[0][i] = min(v1[i], v2[i]);
		bounds[1][i] = max(v1[i], v2[i]);
	}
	if (p[0] < bounds[0][0] - maxDist ||
		p[0] > bounds[1][0] + maxDist ||
		p[1] < bounds[0][1] - maxDist ||
		p[1] > bounds[1][1] + maxDist)
		return maxDist + 100;				// large value
	// project point to v1-v2
	// convert v1->{0,0}, p->d2
	CVec2 d1, d2;
	VectorSubtract(v2, v1, d1);
	d1.Normalize();							// 'd1' is line direction (normalized)
	VectorSubtract(p, v1, d2);				// 'd2' is offset from 'v1' to 'p'
	float f = dot(d1, d2);					// length of projection of 'p' to line
	d1.Scale(f);							// now 'd1' is projection of 'p' to line
//	DrawTextPos(d1[0]+v1[0], d1[1]+v1[1], S_RED"*");
	VectorSubtract(d2, d1, d1);				// now 'd1' is perpendicular from 'p' to line
	return d1.GetLength();
}


/*-----------------------------------------------------------------------------
	CGizmo implementation
-----------------------------------------------------------------------------*/

#if DEBUG_COLLISION
static int __X, __Y;
#endif

void CGizmo::Draw()
{
	guard(CGizmo::Draw);

	glDisable(GL_TEXTURE_2D);
	glLineWidth(2);
	glEnable(GL_LINE_SMOOTH);
	glDepthRange(0, 0.1f);

	for (int axis = 0; /* empty */; axis++)
	{
		int numVerts;
		const CVec3 *verts;
		if (!GetVerts(axis, &verts, &numVerts))
			break;		// all axis drawn

		// compute axis color
		float c[3];
		if (axis != hoverAxis)
		{
			c[0] = c[1] = c[2] = 0;
			c[axis % 3] = 1;
		}
		else
		{
			c[0] = 1; c[1] = 1; c[2] = 0;
		}
		// draw axis
		glColor3fv(c);
		glBegin(GL_LINE_STRIP);
		for (int i = 0; i < numVerts; i++)
			glVertex3fv(verts[i].v);
		glEnd();
	}
	glLineWidth(1);
	glDisable(GL_LINE_SMOOTH);
	glDepthRange(0, 1);
#if DEBUG_COLLISION
	CheckCollision(__X, __Y);
#endif

	unguard;
}


int CGizmo::CheckCollision(int x, int y)
{
#if DEBUG_COLLISION
	__X = x; __Y = y;
#endif
	CVec2 v[MAX_AXIS_VERTS];
	int i;

	CVec2 p;
	p.Set(x, y);

	float bestDist = MAX_COLLISION_DIST;
	int bestAxis = -1;

	for (int axis = 0; /* empty */; axis++)
	{
		// get axis vertices in screen space
		int numVerts;
		const CVec3 *verts;
		if (!GetVerts(axis, &verts, &numVerts))
			break;		// all axis checked
		assert(numVerts <= MAX_AXIS_VERTS);
		for (i = 0; i < numVerts; i++)
		{
			if (!ProjectToScreen(verts[i], v[i].v))
				goto next;				// this point is outside of viewport
		}
		// check point vs line collision
		for (i = 0; i < numVerts; i++)
		{
			float dist = GetDistance2D(p, v[i], v[i < numVerts-1 ? i+1 : 0], MAX_COLLISION_DIST);
			if (dist < bestDist)
				bestAxis = axis;
		}

	next: ;
	}

	return bestAxis;
}


void CGizmo::Drag(int dx, int dy)
{
	guard(CGizmo::Drag);
	assert(type != GT_NONE);
	assert(hoverAxis >= 0);

	float xDelta = (float)dx / GL::width;
	float yDelta = (float)dy / GL::height;
	if (GL::invertXAxis)
		xDelta = -xDelta;

	// compute distance to object
	float dist = 1;
	if (baseCoords)
	{
		CVec3 tmp;
		VectorSubtract(baseCoords->origin, GL::viewOrigin, tmp);
		dist = dot(tmp, GL::viewAxis[0]);
	}

	// convert 2D movement to 3D
	CVec3 move;
	VectorScale(GL::viewAxis[1], xDelta * dist / 2, move);
	VectorMA(move, -yDelta * dist / 2, GL::viewAxis[2]);

	switch (type)
	{
	case GT_MOVE:
	case GT_SCALE:
		assert(hoverAxis < 3);
		// transform movement to local space
		if (baseCoords)
			baseCoords->axis.TransformVectorSlow(move, move);
		// restrict movement
		if (hoverAxis != 0) move[0] = 0;
		if (hoverAxis != 1) move[1] = 0;
		if (hoverAxis != 2) move[2] = 0;
		// apply movement
		if (type == GT_MOVE && editVec)
		{
			editVec->Add(move);
		}
		else if (type == GT_SCALE && editAxis)
		{
			float len = (*editAxis)[hoverAxis].GetLength();
			float newLen = len + move[hoverAxis];
			if (newLen < MIN_OBJECT_SCALE) newLen = MIN_OBJECT_SCALE;
			(*editAxis)[hoverAxis].Scale(newLen / len);
		}
		break;

	case GT_ROTATE:
		{
			assert(hoverAxis < 3);
			float m = xDelta;
			if (fabs(yDelta) > fabs(xDelta)) m = yDelta;
			CVec3 angles;
			angles.Set(0, 0, 0);
			angles[hoverAxis] = m * ROTATE_SCALE;
			CAxis rotate;
			rotate.FromEuler(angles);
			if (editAxis)
				rotate.TransformAxis(*editAxis, *editAxis);
		}
		break;

	default:
		appError("Unknown gizmo type: %d", type);
	}

	unguard;
}


bool CGizmo::GetVerts(int axis, const CVec3 **verts, int *numVerts)
{
	guard(CGizmo::GetVerts);

	assert(type != GT_NONE);
	assert(axis >= 0);

	static CVec3 v[MAX_AXIS_VERTS];
	*verts = v;
	int i;

	switch (type)
	{
	case GT_MOVE:
	case GT_SCALE:
		if (axis >= 3) return false;	//?? add axis for square
		v[0].Set(0, 0, 0);
		v[1].Set(0, 0, 0);
		v[1][axis] = 1;
		*numVerts = 2;
		break;

	case GT_ROTATE:
		if (axis >= 3) return false;
		for (i = 0; i < MAX_AXIS_VERTS; i++)
		{
			float angle = i * M_PI * 2 / (MAX_AXIS_VERTS - 1);
			float x = sin(angle) / 2;
			float y = cos(angle) / 2;
			if (axis == 0)
				v[i].Set(x, 0, y);
			else if (axis == 1)
				v[i].Set(x, y, 0);
			else
				v[i].Set(0, x, y);
		}
		*numVerts = MAX_AXIS_VERTS;
		break;

	default:
		appError("Unknown gizmo type: %d", type);
	}
	// transform verts to global coordinate space
	for (i = 0; i < *numVerts; i++)
	{
		v[i].Scale(gizmoScale * GIZMO_SIZE);

		if (editVec)
			v[i].Add(*editVec);			//?? move slightly for better box visualization; may be, make box
										//?? {-1,-1,-1}-{1,1,1} instead of {0,0,0}-{1,1,1}
		if (baseCoords)
			baseCoords->UnTransformPoint(v[i], v[i]);
	}

	return true;

	unguard;
}
