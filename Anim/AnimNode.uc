class AnimNode
	extends Object
	abstract;


const MAX_NODE_NAME  = 64;
const MAX_NODE_LABEL = 32;


struct AnimNodeData
{
	var bool				Relevant;
	var float				TotalWeight;	//?? nonsense, when multiple parents!
	/** Value, controlling AnimNode blending. When scalar used, Y component is ignored. */
	var float				ControlX;		//?? make as Vec2
	var float				ControlY;
	//?? should also store deformed skeleton
};


/** This is the name used to find an AnimNode by name from a tree */
var(Node) string[MAX_NODE_NAME]	Name;

/** AnimTree this node belongs to */
var AnimTree				Owner;


/**
 * Structure describing children (by tree hierarchy) of this AnimNode.
 * Really, this is an one of input values for this AnimNode.
 */
struct AnimNodeChild
{
	var string[MAX_NODE_LABEL] Label;
	var AnimNode			Node;

	structcpptext
	{
		friend CArchive& operator<<(CArchive &Ar, CAnimNodeChild &C);
			// cannot embed code here: uses undefined class CAnimNode
	}
};

/**
 * Parent nodes by AnimTree hierarchy. These nodes uses output from this node
 * as input value.
 */
var array<AnimNode>			Parents;
/** Nodes, provides input values for this AnimNode */
var array<AnimNodeChild>	Children;


/**
 * Drawing parameters
 */

/** Rectangle */
var int						DrawX, DrawY;
var int						DrawWidth, DrawHeight;
/** Node output */
var int						OutDrawY;


cpptext
{
	friend class CSkelMeshInstance;

#define DECLARE_ANIMDATA(DataType)	\
	typedef C##DataType NodeData_t;	\
	virtual int GetDataSize() const	\
	{								\
		return sizeof(C##DataType);	\
	}
	/*?? also add to DECLARE_ANIMDATA():
	virtual void Tick(void *Data) -- virtual ticker (different function name!)
	{
		Tick((DataType*)Data); -- non-virtual local ticker
	}
	void Tick(CSkelMeshInstance *Inst, DataType *Data, CBoneAtom *Bones)
	{
		CBoneAtom BonesA[MAX_MESH_BONES], BonesB[MAX_MESH_BONES];
		-- find relevant Children A and B + weights
		-- call Child[A].Tick(BonesA) and Child[B].Tick(BonesB)
		-- blend(BonesA, BonesB) -> Bones
		--!! should also pass bool array of bones to compute (for BlendPerBone nodes)

		--!! AnimPlayer should simply copy data from animation track to Bones[],
		--!! but with remapping Mesh <-> AnimSet bones; note: bone remap is required
		--!! for AnimPlayer only.
	}
	*/

	DECLARE_ANIMDATA(AnimNodeData)

	virtual void Serialize(CArchive &Ar)
	{
		Super::Serialize(Ar);
		Ar << Name << Parents << Children;
	}
}
