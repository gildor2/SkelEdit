#include "Core.h"


struct VChunkHeader
{
	char		ChunkID[20];
	int			TypeFlag;
	int			DataSize;
	int			DataCount;

	friend CArchive& operator<<(CArchive &Ar, VChunkHeader &H)
	{
		for (int i = 0; i < 20; i++)
			Ar << H.ChunkID[i];
		return Ar << H.TypeFlag << H.DataSize << H.DataCount;
	}
};


struct VVertex
{
	int			PointIndex;				// short, padded to int
	float		U, V;
	byte		MatIndex;
	byte		Reserved;
	short		Pad;					// not used

	friend CArchive& operator<<(CArchive &Ar, VVertex &V)
	{
		return Ar << V.PointIndex << V.U << V.V << V.MatIndex << V.Reserved << V.Pad;
	}
};


struct VTriangle
{
	word		WedgeIndex[3];			// Point to three vertices in the vertex list.
	byte		MatIndex;				// Materials can be anything.
	byte		AuxMatIndex;			// Second material (unused).
	unsigned	SmoothingGroups;		// 32-bit flag for smoothing groups.

	friend CArchive& operator<<(CArchive &Ar, VTriangle &T)
	{
		Ar << T.WedgeIndex[0] << T.WedgeIndex[1] << T.WedgeIndex[2];
		Ar << T.MatIndex << T.AuxMatIndex << T.SmoothingGroups;
		return Ar;
	}
};


#define LOAD_CHUNK(var, name)	\
	VChunkHeader var;			\
	Ar << var;					\
	if (strcmp(var.ChunkID, name) != 0) \
		appError("LoadChunk: expecting header \""name"\", but found \"%s\"", var.ChunkID);

//!!! REMOVE !!!
static int	 		g_numVerts, g_numWedges, g_numTris;
static CVec3		*g_verts;
static VVertex		*g_wedges;
static VTriangle	*g_tris;

void ImportPsk(CArchive &Ar)
{
	guard(ImportPsk);
	int i;

	// load primary header
	LOAD_CHUNK(MainHdr, "ACTRHEAD");
	if (MainHdr.TypeFlag != 1999801)
		appError("Found PSK version %d", MainHdr.TypeFlag);

	// load points
	LOAD_CHUNK(PtsHdr, "PNTS0000");
	int numVerts = PtsHdr.DataCount;
	g_numVerts = numVerts;
	CVec3 *Verts = new CVec3[numVerts];
	g_verts = Verts;
	for (i = 0; i < numVerts; i++)
		Ar << Verts[i];

	// load wedges
	LOAD_CHUNK(WedgHdr, "VTXW0000");
	int numWedges = WedgHdr.DataCount;
	g_numWedges = numWedges;
	VVertex *Wedges = new VVertex[numWedges];
	g_wedges = Wedges;
	for (i = 0; i < numWedges; i++)
		Ar << Wedges[i];

	LOAD_CHUNK(FacesHdr, "FACE0000");
	int numTris = FacesHdr.DataCount;
	g_numTris = numTris;
	VTriangle *Tris = new VTriangle[numTris];
	g_tris = Tris;
	for (i = 0; i < numTris; i++)
		Ar << Tris[i];

	LOAD_CHUNK(MatrHdr, "MATT0000");

//	appNotify("%d %d %d", PtsHdr.TypeFlag, PtsHdr.DataSize, PtsHdr.DataCount);


	unguard;
}


//!!!! TESTING !!!!
#include "GlViewport.h"

void ShowMesh()
{
	for (int i = 0; i < g_numTris; i++)
	{
		const VTriangle &Face = g_tris[i];

		// SetMaterial()
		int color = Face.MatIndex + 1;
		if (color > 7) color = 7;
#define C(n)	( ((color >> n) & 1) * 0.5f + 0.3f )
		glColor3f(C(0), C(1), C(2));
#undef C

		// draw single triangle
		glBegin(GL_TRIANGLES);
		for (int j = 0; j < 3; j++)
		{
			const VVertex &W = g_wedges[Face.WedgeIndex[j]];
//			glTexCoord2f(W.U, W.V);
			glVertex3fv(&g_verts[W.PointIndex][0]);
		}
		glEnd();
	}
}
