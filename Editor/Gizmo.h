#ifndef __GIZMO_H__
#define __GIZMO_H__


/// Gizmo type
enum
{
	GT_NONE,
	GT_MOVE,
	GT_ROTATE,
	GT_SCALE
};


/// Internal use function, used by Gizmo[Move|Scale|Rotate] functions
void SetGizmo(int type, const CCoords *parent, CVec3 *edVec, CAxis *edAxis);
/// Remove gizmo from screen
void RemoveGizmo();

inline void GizmoMove(CVec3 *origin, const CCoords *parent)
{
	SetGizmo(GT_MOVE, parent, origin, NULL);
}

inline void GizmoScale(CCoords *edit, const CCoords *parent)
{
	SetGizmo(GT_SCALE, parent, &edit->origin, &edit->axis);
}

inline void GizmoRotate(CCoords *edit, const CCoords *parent)
{
	SetGizmo(GT_ROTATE, parent, &edit->origin, &edit->axis);
}


/// Update gizmos from mouse information and display them. Return 'true' when
/// edited object has been modified by mouse, or 'false' when nothing changed.
bool TickGizmo(bool mouseDown, int mouseX, int mouseY, int deltaX, int deltaY);

void SetGizmoScale(float scale);
void DisplayGizmo();


#endif // __GIZMO_H__
