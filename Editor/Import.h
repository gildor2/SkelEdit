#ifndef __IMPORT_H__
#define __IMPORT_H__


/*
 *	Importing external content
 */
void ImportPsk(CArchive &Ar, CSkeletalMesh &Mesh);
void ImportPsa(CArchive &Ar, CAnimSet &Anim);

/*
 *	Utility functions
 */
void GenerateBoxes(CSkeletalMesh &Mesh);
bool GenerateBox(CSkeletalMesh &Mesh, int BoneIndex, CCoords &Box);


#endif
