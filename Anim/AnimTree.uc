class AnimTree
	extends AnimNode;


struct AnimControl
{
	var string[MAX_NODE_LABEL] Name;

	structcpptext
	{
		friend CArchive &operator<<(CArchive &Ar, CAnimControl &A)
		{
			return Ar << A.Name;
		}
	}
};


var array<AnimNode>		AllNodes;
var array<AnimControl>	Controls;


cpptext
{
	virtual void Serialize(CArchive &Ar)
	{
		Super::Serialize(Ar);
		Ar << Controls << AllNodes;
	}

	virtual void PostLoad()
	{
		for (int i = 0; i < AllNodes.Num(); i++)
			AllNodes[i]->Owner = this;
	}
}
