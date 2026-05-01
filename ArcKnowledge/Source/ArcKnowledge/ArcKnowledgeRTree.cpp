// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcKnowledgeRTree.h"

// ============================================================================
// FNode helpers
// ============================================================================

void FArcKnowledgeRTree::FNode::RecomputeBounds()
{
	Bounds = FArcKnowledgeRTreeBounds(); // reset to invalid

	if (bIsLeaf)
	{
		for (const FArcKnowledgeRTreeLeafEntry& Entry : LeafEntries)
		{
			Bounds.Expand(Entry.Position);
		}
	}
	else
	{
		for (const FNode* Child : Children)
		{
			Bounds.Expand(Child->Bounds);
		}
	}
}

void FArcKnowledgeRTree::FNode::RecomputeAggregatedMask()
{
	AggregatedMask.Low = 0;
	AggregatedMask.High = 0;

	if (bIsLeaf)
	{
		for (const FArcKnowledgeRTreeLeafEntry& Entry : LeafEntries)
		{
			AggregatedMask.Low |= Entry.TagBitmask.Low;
			AggregatedMask.High |= Entry.TagBitmask.High;
		}
	}
	else
	{
		for (const FNode* Child : Children)
		{
			AggregatedMask.Low |= Child->AggregatedMask.Low;
			AggregatedMask.High |= Child->AggregatedMask.High;
		}
	}
}

// ============================================================================
// Constructor / Destructor / Move
// ============================================================================

FArcKnowledgeRTree::FArcKnowledgeRTree() = default;

FArcKnowledgeRTree::~FArcKnowledgeRTree()
{
	Clear();
}

FArcKnowledgeRTree::FArcKnowledgeRTree(FArcKnowledgeRTree&& Other) noexcept
	: Root(Other.Root)
	, EntryCount(Other.EntryCount)
	, NodeCount(Other.NodeCount)
{
	Other.Root = nullptr;
	Other.EntryCount = 0;
	Other.NodeCount = 0;
}

FArcKnowledgeRTree& FArcKnowledgeRTree::operator=(FArcKnowledgeRTree&& Other) noexcept
{
	if (this != &Other)
	{
		Clear();
		Root = Other.Root;
		EntryCount = Other.EntryCount;
		NodeCount = Other.NodeCount;
		Other.Root = nullptr;
		Other.EntryCount = 0;
		Other.NodeCount = 0;
	}
	return *this;
}

// ============================================================================
// Insert
// ============================================================================

void FArcKnowledgeRTree::Insert(const FArcKnowledgeRTreeLeafEntry& Entry)
{
	if (Root == nullptr)
	{
		Root = new FNode();
		Root->bIsLeaf = true;
		++NodeCount;
	}

	// Build a point AABB for the entry
	FArcKnowledgeRTreeBounds EntryBounds;
	EntryBounds.Expand(Entry.Position);

	// Find the best leaf, building path from root
	TArray<FNode*> Path;
	FNode* Leaf = ChooseLeaf(Root, EntryBounds, Path);

	// Insert into the leaf
	Leaf->LeafEntries.Add(Entry);
	Leaf->Bounds.Expand(Entry.Position);
	Leaf->AggregatedMask.Low |= Entry.TagBitmask.Low;
	Leaf->AggregatedMask.High |= Entry.TagBitmask.High;
	++EntryCount;

	// If leaf overflows, split and propagate upward
	if (Leaf->LeafEntries.Num() > MaxChildren)
	{
		FNode* SplitSibling = SplitNode(Leaf);

		// Propagate split upward through the recorded path
		FNode* NewChild = SplitSibling;
		for (int32 I = Path.Num() - 2; I >= 0; --I)
		{
			FNode* Parent = Path[I];
			Parent->Children.Add(NewChild);
			Parent->RecomputeBounds();
			Parent->RecomputeAggregatedMask();

			if (Parent->Children.Num() > MaxChildren)
			{
				NewChild = SplitNode(Parent);
			}
			else
			{
				NewChild = nullptr;
				break;
			}
		}

		// If root was split, create new root
		if (NewChild != nullptr)
		{
			FNode* NewRoot = new FNode();
			NewRoot->bIsLeaf = false;
			NewRoot->Children.Add(Root);
			NewRoot->Children.Add(NewChild);
			NewRoot->RecomputeBounds();
			NewRoot->RecomputeAggregatedMask();
			Root = NewRoot;
			++NodeCount;
		}
	}
	else
	{
		// No split needed — propagate bounds/mask up through path
		for (int32 I = 0; I < Path.Num() - 1; ++I)
		{
			Path[I]->Bounds.Expand(EntryBounds);
			Path[I]->AggregatedMask.Low |= Entry.TagBitmask.Low;
			Path[I]->AggregatedMask.High |= Entry.TagBitmask.High;
		}
	}
}

// ============================================================================
// ChooseLeaf
// ============================================================================

FArcKnowledgeRTree::FNode* FArcKnowledgeRTree::ChooseLeaf(FNode* CurrentNode, const FArcKnowledgeRTreeBounds& EntryBounds, TArray<FNode*>& OutPath) const
{
	OutPath.Add(CurrentNode);

	while (!CurrentNode->bIsLeaf)
	{
		double BestIncrease = MAX_dbl;
		double BestVolume = MAX_dbl;
		FNode* BestChild = nullptr;

		for (FNode* Child : CurrentNode->Children)
		{
			double Increase = Child->Bounds.VolumeIncrease(EntryBounds);
			double ChildVolume = Child->Bounds.Volume();

			// Prefer minimum volume increase, break ties by smallest volume
			if (Increase < BestIncrease || (Increase == BestIncrease && ChildVolume < BestVolume))
			{
				BestIncrease = Increase;
				BestVolume = ChildVolume;
				BestChild = Child;
			}
		}

		CurrentNode = BestChild;
		OutPath.Add(CurrentNode);
	}

	return CurrentNode;
}

// ============================================================================
// SplitNode — Quadratic split
// ============================================================================

FArcKnowledgeRTree::FNode* FArcKnowledgeRTree::SplitNode(FNode* Node)
{
	FNode* NewNode = new FNode();
	NewNode->bIsLeaf = Node->bIsLeaf;
	++NodeCount;

	if (Node->bIsLeaf)
	{
		// Pick seeds: for point data use maximum distance between entries
		int32 Seed1 = 0;
		int32 Seed2 = 1;
		double MaxDistSq = -1.0;
		const int32 EntryNum = Node->LeafEntries.Num();

		for (int32 i = 0; i < EntryNum; ++i)
		{
			for (int32 j = i + 1; j < EntryNum; ++j)
			{
				double DistSq = FVector::DistSquared(Node->LeafEntries[i].Position, Node->LeafEntries[j].Position);
				if (DistSq > MaxDistSq)
				{
					MaxDistSq = DistSq;
					Seed1 = i;
					Seed2 = j;
				}
			}
		}

		// Collect all entries, clear node
		TArray<FArcKnowledgeRTreeLeafEntry> AllEntries;
		AllEntries.Reserve(Node->LeafEntries.Num());
		for (FArcKnowledgeRTreeLeafEntry& E : Node->LeafEntries)
		{
			AllEntries.Add(MoveTemp(E));
		}
		Node->LeafEntries.Reset();

		// Assign seeds
		Node->LeafEntries.Add(AllEntries[Seed1]);
		NewNode->LeafEntries.Add(AllEntries[Seed2]);

		// Distribute remaining entries
		for (int32 i = 0; i < AllEntries.Num(); ++i)
		{
			if (i == Seed1 || i == Seed2)
			{
				continue;
			}

			// Count how many entries remain to be assigned
			int32 NotYetAssigned = 0;
			for (int32 k = i; k < AllEntries.Num(); ++k)
			{
				if (k != Seed1 && k != Seed2)
				{
					++NotYetAssigned;
				}
			}

			if (Node->LeafEntries.Num() + NotYetAssigned == MinChildren)
			{
				// Must assign all remaining to Node
				for (int32 k = i; k < AllEntries.Num(); ++k)
				{
					if (k != Seed1 && k != Seed2)
					{
						Node->LeafEntries.Add(AllEntries[k]);
					}
				}
				break;
			}
			if (NewNode->LeafEntries.Num() + NotYetAssigned == MinChildren)
			{
				// Must assign all remaining to NewNode
				for (int32 k = i; k < AllEntries.Num(); ++k)
				{
					if (k != Seed1 && k != Seed2)
					{
						NewNode->LeafEntries.Add(AllEntries[k]);
					}
				}
				break;
			}

			// Quadratic pick next: assign to group with least volume increase
			FArcKnowledgeRTreeBounds BoundsA;
			for (const FArcKnowledgeRTreeLeafEntry& E : Node->LeafEntries)
			{
				BoundsA.Expand(E.Position);
			}

			FArcKnowledgeRTreeBounds BoundsB;
			for (const FArcKnowledgeRTreeLeafEntry& E : NewNode->LeafEntries)
			{
				BoundsB.Expand(E.Position);
			}

			FArcKnowledgeRTreeBounds PointBounds;
			PointBounds.Expand(AllEntries[i].Position);

			double IncreaseA = BoundsA.VolumeIncrease(PointBounds);
			double IncreaseB = BoundsB.VolumeIncrease(PointBounds);

			if (IncreaseA <= IncreaseB)
			{
				Node->LeafEntries.Add(AllEntries[i]);
			}
			else
			{
				NewNode->LeafEntries.Add(AllEntries[i]);
			}
		}

		Node->RecomputeBounds();
		Node->RecomputeAggregatedMask();
		NewNode->RecomputeBounds();
		NewNode->RecomputeAggregatedMask();
	}
	else
	{
		// Internal node split — same quadratic approach but on children
		int32 Seed1 = 0;
		int32 Seed2 = 1;
		double WorstWaste = -MAX_dbl;
		const int32 ChildNum = Node->Children.Num();

		for (int32 i = 0; i < ChildNum; ++i)
		{
			for (int32 j = i + 1; j < ChildNum; ++j)
			{
				FArcKnowledgeRTreeBounds Combined;
				Combined.Expand(Node->Children[i]->Bounds);
				Combined.Expand(Node->Children[j]->Bounds);
				double Waste = Combined.Volume() - Node->Children[i]->Bounds.Volume() - Node->Children[j]->Bounds.Volume();
				if (Waste > WorstWaste)
				{
					WorstWaste = Waste;
					Seed1 = i;
					Seed2 = j;
				}
			}
		}

		TArray<FNode*> AllChildren;
		AllChildren.Reserve(Node->Children.Num());
		for (FNode* Child : Node->Children)
		{
			AllChildren.Add(Child);
		}
		Node->Children.Reset();

		Node->Children.Add(AllChildren[Seed1]);
		NewNode->Children.Add(AllChildren[Seed2]);

		for (int32 i = 0; i < AllChildren.Num(); ++i)
		{
			if (i == Seed1 || i == Seed2)
			{
				continue;
			}

			int32 NotYetAssigned = 0;
			for (int32 k = i; k < AllChildren.Num(); ++k)
			{
				if (k != Seed1 && k != Seed2)
				{
					++NotYetAssigned;
				}
			}

			if (Node->Children.Num() + NotYetAssigned == MinChildren)
			{
				for (int32 k = i; k < AllChildren.Num(); ++k)
				{
					if (k != Seed1 && k != Seed2)
					{
						Node->Children.Add(AllChildren[k]);
					}
				}
				break;
			}
			if (NewNode->Children.Num() + NotYetAssigned == MinChildren)
			{
				for (int32 k = i; k < AllChildren.Num(); ++k)
				{
					if (k != Seed1 && k != Seed2)
					{
						NewNode->Children.Add(AllChildren[k]);
					}
				}
				break;
			}

			FArcKnowledgeRTreeBounds BoundsA;
			for (const FNode* Child : Node->Children)
			{
				BoundsA.Expand(Child->Bounds);
			}

			FArcKnowledgeRTreeBounds BoundsB;
			for (const FNode* Child : NewNode->Children)
			{
				BoundsB.Expand(Child->Bounds);
			}

			double IncreaseA = BoundsA.VolumeIncrease(AllChildren[i]->Bounds);
			double IncreaseB = BoundsB.VolumeIncrease(AllChildren[i]->Bounds);

			if (IncreaseA <= IncreaseB)
			{
				Node->Children.Add(AllChildren[i]);
			}
			else
			{
				NewNode->Children.Add(AllChildren[i]);
			}
		}

		Node->RecomputeBounds();
		Node->RecomputeAggregatedMask();
		NewNode->RecomputeBounds();
		NewNode->RecomputeAggregatedMask();
	}

	return NewNode;
}

// ============================================================================
// Remove
// Note: Does not reinsert entries from under-full nodes (no condense-tree).
// This is intentional — the tree is bulk-loaded for mostly-static data and
// individual removes only happen on entity death. Tree quality degrades
// gracefully; BulkLoad rebuilds optimal structure on level streaming.
// ============================================================================

bool FArcKnowledgeRTree::Remove(FMassEntityHandle Entity)
{
	if (Root == nullptr)
	{
		return false;
	}

	// Stack-based search for the entity
	struct FSearchFrame
	{
		FNode* Node;
		FNode* Parent;
		int32 ChildIndexInParent;
	};

	TArray<FSearchFrame> Stack;
	Stack.Add({ Root, nullptr, -1 });

	while (Stack.Num() > 0)
	{
		FSearchFrame Frame = Stack.Pop(EAllowShrinking::No);
		FNode* Node = Frame.Node;

		if (Node->bIsLeaf)
		{
			// Search for entity in leaf entries
			int32 FoundIndex = INDEX_NONE;
			for (int32 i = 0; i < Node->LeafEntries.Num(); ++i)
			{
				if (Node->LeafEntries[i].Entity == Entity)
				{
					FoundIndex = i;
					break;
				}
			}

			if (FoundIndex == INDEX_NONE)
			{
				continue;
			}

			// Remove the entry
			Node->LeafEntries.RemoveAtSwap(FoundIndex);
			--EntryCount;

			// Recompute bounds and mask for this leaf
			Node->RecomputeBounds();
			Node->RecomputeAggregatedMask();

			// Propagate recomputation up — simplest approach: recompute from root
			if (Root != nullptr && !Root->bIsLeaf)
			{
				// Recursive recompute from root
				TArray<FNode*> RecomputeStack;
				RecomputeStack.Add(Root);
				// Post-order traversal
				TArray<FNode*> PostOrder;
				while (RecomputeStack.Num() > 0)
				{
					FNode* Current = RecomputeStack.Pop(EAllowShrinking::No);
					PostOrder.Add(Current);
					if (!Current->bIsLeaf)
					{
						for (FNode* Child : Current->Children)
						{
							RecomputeStack.Add(Child);
						}
					}
				}
				// Recompute in reverse order (children before parents)
				for (int32 i = PostOrder.Num() - 1; i >= 0; --i)
				{
					if (!PostOrder[i]->bIsLeaf)
					{
						// Remove empty leaf children
						for (int32 c = PostOrder[i]->Children.Num() - 1; c >= 0; --c)
						{
							FNode* Child = PostOrder[i]->Children[c];
							bool bChildEmpty = Child->bIsLeaf ? Child->LeafEntries.Num() == 0 : Child->Children.Num() == 0;
							if (bChildEmpty)
							{
								PostOrder[i]->Children.RemoveAtSwap(c);
								delete Child;
								--NodeCount;
							}
						}
						PostOrder[i]->RecomputeBounds();
						PostOrder[i]->RecomputeAggregatedMask();
					}
				}
			}

			// Handle empty/single-child root cases
			if (Root->bIsLeaf && Root->LeafEntries.Num() == 0)
			{
				delete Root;
				Root = nullptr;
				--NodeCount;
			}
			else if (!Root->bIsLeaf && Root->Children.Num() == 0)
			{
				delete Root;
				Root = nullptr;
				--NodeCount;
			}
			else if (!Root->bIsLeaf && Root->Children.Num() == 1)
			{
				FNode* OldRoot = Root;
				Root = Root->Children[0];
				OldRoot->Children.Reset();
				delete OldRoot;
				--NodeCount;
			}

			return true;
		}
		else
		{
			for (int32 i = 0; i < Node->Children.Num(); ++i)
			{
				Stack.Add({ Node->Children[i], Node, i });
			}
		}
	}

	return false;
}

// ============================================================================
// QuerySphere
// ============================================================================

void FArcKnowledgeRTree::QuerySphere(const FVector& Center, double Radius, const FArcKnowledgeTagBitmask& RequiredMask, TArray<FArcKnowledgeRTreeLeafEntry>& OutEntries) const
{
	if (Root == nullptr)
	{
		return;
	}

	const double RadiusSq = Radius * Radius;
	const bool bHasMaskFilter = !RequiredMask.IsEmpty();

	TArray<const FNode*> Stack;
	Stack.Add(Root);

	while (Stack.Num() > 0)
	{
		const FNode* Node = Stack.Pop(EAllowShrinking::No);

		// Prune: AABB doesn't intersect sphere
		if (!Node->Bounds.IntersectsSphere(Center, RadiusSq))
		{
			continue;
		}

		// Prune: aggregated mask doesn't contain all required bits
		if (bHasMaskFilter && !Node->AggregatedMask.HasAllBits(RequiredMask))
		{
			continue;
		}

		if (Node->bIsLeaf)
		{
			for (const FArcKnowledgeRTreeLeafEntry& Entry : Node->LeafEntries)
			{
				if (FVector::DistSquared(Center, Entry.Position) <= RadiusSq)
				{
					if (!bHasMaskFilter || Entry.TagBitmask.HasAllBits(RequiredMask))
					{
						OutEntries.Add(Entry);
					}
				}
			}
		}
		else
		{
			for (const FNode* Child : Node->Children)
			{
				Stack.Add(Child);
			}
		}
	}
}

// ============================================================================
// QueryBox
// ============================================================================

void FArcKnowledgeRTree::QueryBox(const FArcKnowledgeRTreeBounds& Box, const FArcKnowledgeTagBitmask& RequiredMask, TArray<FArcKnowledgeRTreeLeafEntry>& OutEntries) const
{
	if (Root == nullptr)
	{
		return;
	}

	const bool bHasMaskFilter = !RequiredMask.IsEmpty();

	TArray<const FNode*> Stack;
	Stack.Add(Root);

	while (Stack.Num() > 0)
	{
		const FNode* Node = Stack.Pop(EAllowShrinking::No);

		// Prune: AABB doesn't intersect query box
		if (!Node->Bounds.IntersectsBox(Box))
		{
			continue;
		}

		// Prune: aggregated mask doesn't contain all required bits
		if (bHasMaskFilter && !Node->AggregatedMask.HasAllBits(RequiredMask))
		{
			continue;
		}

		if (Node->bIsLeaf)
		{
			for (const FArcKnowledgeRTreeLeafEntry& Entry : Node->LeafEntries)
			{
				// Check point containment in box
				if (Entry.Position.X >= Box.Min.X && Entry.Position.X <= Box.Max.X
					&& Entry.Position.Y >= Box.Min.Y && Entry.Position.Y <= Box.Max.Y
					&& Entry.Position.Z >= Box.Min.Z && Entry.Position.Z <= Box.Max.Z)
				{
					if (!bHasMaskFilter || Entry.TagBitmask.HasAllBits(RequiredMask))
					{
						OutEntries.Add(Entry);
					}
				}
			}
		}
		else
		{
			for (const FNode* Child : Node->Children)
			{
				Stack.Add(Child);
			}
		}
	}
}

// ============================================================================
// BulkLoad — Sort-Tile-Recursive
// ============================================================================

void FArcKnowledgeRTree::BulkLoad(TArray<FArcKnowledgeRTreeLeafEntry>& Entries)
{
	Clear();

	if (Entries.Num() == 0)
	{
		return;
	}

	EntryCount = Entries.Num();
	Root = BuildSTR(Entries, 0, Entries.Num(), 0);
}

FArcKnowledgeRTree::FNode* FArcKnowledgeRTree::BuildSTR(TArray<FArcKnowledgeRTreeLeafEntry>& Entries, int32 Start, int32 Count, int32 Depth)
{
	// Base case: fits in a single leaf
	if (Count <= MaxChildren)
	{
		FNode* Leaf = new FNode();
		Leaf->bIsLeaf = true;
		++NodeCount;

		for (int32 i = Start; i < Start + Count; ++i)
		{
			Leaf->LeafEntries.Add(Entries[i]);
		}
		Leaf->RecomputeBounds();
		Leaf->RecomputeAggregatedMask();
		return Leaf;
	}

	// Sort by cycling axis: X(0), Y(1), Z(2)
	int32 Axis = Depth % 3;
	TArrayView<FArcKnowledgeRTreeLeafEntry> Slice(&Entries[Start], Count);

	switch (Axis)
	{
	case 0:
		Slice.Sort([](const FArcKnowledgeRTreeLeafEntry& A, const FArcKnowledgeRTreeLeafEntry& B) { return A.Position.X < B.Position.X; });
		break;
	case 1:
		Slice.Sort([](const FArcKnowledgeRTreeLeafEntry& A, const FArcKnowledgeRTreeLeafEntry& B) { return A.Position.Y < B.Position.Y; });
		break;
	case 2:
		Slice.Sort([](const FArcKnowledgeRTreeLeafEntry& A, const FArcKnowledgeRTreeLeafEntry& B) { return A.Position.Z < B.Position.Z; });
		break;
	}

	// Partition into slabs of MaxChildren entries
	int32 NumSlabs = (Count + MaxChildren - 1) / MaxChildren;

	// If number of slabs fits in a single internal node, recurse per slab
	if (NumSlabs <= MaxChildren)
	{
		FNode* Internal = new FNode();
		Internal->bIsLeaf = false;
		++NodeCount;

		int32 Offset = Start;
		for (int32 SlabIdx = 0; SlabIdx < NumSlabs; ++SlabIdx)
		{
			int32 SlabSize = FMath::Min(MaxChildren, Start + Count - Offset);
			FNode* Child = BuildSTR(Entries, Offset, SlabSize, Depth + 1);
			Internal->Children.Add(Child);
			Offset += SlabSize;
		}

		Internal->RecomputeBounds();
		Internal->RecomputeAggregatedMask();
		return Internal;
	}

	// Too many slabs — recurse one more level
	// Each super-slab contains MaxChildren slabs worth of entries
	int32 EntriesPerSuperSlab = MaxChildren * MaxChildren;
	int32 NumSuperSlabs = (Count + EntriesPerSuperSlab - 1) / EntriesPerSuperSlab;

	FNode* Internal = new FNode();
	Internal->bIsLeaf = false;
	++NodeCount;

	int32 Offset = Start;
	for (int32 SuperIdx = 0; SuperIdx < NumSuperSlabs; ++SuperIdx)
	{
		int32 SuperSize = FMath::Min(EntriesPerSuperSlab, Start + Count - Offset);
		FNode* Child = BuildSTR(Entries, Offset, SuperSize, Depth + 1);
		Internal->Children.Add(Child);
		Offset += SuperSize;
	}

	Internal->RecomputeBounds();
	Internal->RecomputeAggregatedMask();
	return Internal;
}

// ============================================================================
// Clear / DeleteNode
// ============================================================================

void FArcKnowledgeRTree::Clear()
{
	if (Root != nullptr)
	{
		DeleteNode(Root);
		Root = nullptr;
	}
	EntryCount = 0;
	NodeCount = 0;
}

void FArcKnowledgeRTree::DeleteNode(FNode* Node)
{
	if (!Node->bIsLeaf)
	{
		for (FNode* Child : Node->Children)
		{
			DeleteNode(Child);
		}
	}
	delete Node;
}
