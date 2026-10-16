// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/ArcLoreBranchTreeLayout.h"

// File-scoped layout constants.
namespace ArcLoreBranchTreeLayoutInternal
{
	static const float LevelGapX = 280.f;
	static const float NodeGapY = 80.f;
	static const float RootGapY = 120.f;

	/** ~6 distinct subtree colors; one is assigned per root (mod size) and propagated down. */
	static const FLinearColor RootPalette[] =
	{
		FLinearColor(0.36f, 0.62f, 0.93f), // blue
		FLinearColor(0.42f, 0.78f, 0.45f), // green
		FLinearColor(0.86f, 0.50f, 0.32f), // orange
		FLinearColor(0.70f, 0.48f, 0.86f), // purple
		FLinearColor(0.90f, 0.45f, 0.60f), // pink
		FLinearColor(0.45f, 0.80f, 0.80f), // teal
	};
	static const int32 RootPaletteSize = UE_ARRAY_COUNT(RootPalette);

	/** Bright accent that overrides any subtree color for the current branch. */
	static const FLinearColor CurrentAccent = FLinearColor(1.f, 0.85f, 0.2f);

	/**
	 * Stable comparator ordering two node indices by their referenced node's BranchName.
	 * Used to sort RootIndices and every node's ChildIndices so traversal (and thus the
	 * resulting layout) is independent of input order and stable across refreshes.
	 */
	struct FNodeNameLess
	{
		const TArray<FArcLoreBranchTreeNode>& Nodes;
		explicit FNodeNameLess(const TArray<FArcLoreBranchTreeNode>& InNodes) : Nodes(InNodes) {}

		bool operator()(int32 A, int32 B) const
		{
			return Nodes[A].BranchName < Nodes[B].BranchName;
		}
	};
}

FArcLoreBranchTreeLayout FArcLoreBranchTreeBuilder::BuildTree(const TArray<FArcLoreBranchInput>& Branches)
{
	using namespace ArcLoreBranchTreeLayoutInternal;

	FArcLoreBranchTreeLayout Layout;

	// --- 1. Ingest: one node per input branch, in input order. -----------------------------
	Layout.Nodes.Reserve(Branches.Num());
	TMap<FString, int32> IdToIndex;
	IdToIndex.Reserve(Branches.Num());
	for (int32 InputIndex = 0; InputIndex < Branches.Num(); ++InputIndex)
	{
		const FArcLoreBranchInput& In = Branches[InputIndex];
		FArcLoreBranchTreeNode Node;
		Node.BranchName = In.Name;
		Node.BranchId = In.Id;
		Node.TipHash = In.TipHash;
		Node.bIsCurrent = In.bIsCurrent;
		Node.bArchived = In.bArchived;
		Node.RemoteTipHash = In.RemoteTipHash;
		Node.bHasRemote = In.bHasRemote;
		Node.bInSync = In.bInSync;
		Node.Ahead = In.Ahead;
		Node.Behind = In.Behind;
		Node.bAheadBehindKnown = In.bAheadBehindKnown;
		const int32 NodeIndex = Layout.Nodes.Add(MoveTemp(Node));
		// Last writer wins on duplicate ids — harmless for layout determinism.
		IdToIndex.Add(In.Id, NodeIndex);
	}

	// --- 2. Order-independent parent resolution. -------------------------------------------
	// For branch B, the immediate parent is the resolvable Stack ancestor with the LONGEST
	// own Stack (deepest lineage = closest ancestor); ties broken by lexically-smallest
	// id-hex. This is independent of Stack array order and of branch enumeration order.
	for (int32 NodeIndex = 0; NodeIndex < Layout.Nodes.Num(); ++NodeIndex)
	{
		const FArcLoreBranchInput& In = Branches[NodeIndex];
		FArcLoreBranchTreeNode& Node = Layout.Nodes[NodeIndex];

		int32 BestAncestorNodeIndex = INDEX_NONE;
		int32 BestAncestorStackLen = -1;
		FString BestAncestorIdHex;
		FString BestForkRevision;

		for (const FArcLoreBranchPoint& Point : In.Stack)
		{
			// Resolve against the id map; skip danglers and self-references.
			const int32* FoundIndex = IdToIndex.Find(Point.BranchIdHex);
			if (FoundIndex == nullptr)
			{
				continue;
			}
			if (Point.BranchIdHex == Node.BranchId)
			{
				continue;
			}

			const int32 AncestorStackLen = Branches[*FoundIndex].Stack.Num();
			const bool bDeeper = AncestorStackLen > BestAncestorStackLen;
			const bool bTieSmaller =
				AncestorStackLen == BestAncestorStackLen && Point.BranchIdHex < BestAncestorIdHex;
			if (bDeeper || bTieSmaller)
			{
				BestAncestorNodeIndex = *FoundIndex;
				BestAncestorStackLen = AncestorStackLen;
				BestAncestorIdHex = Point.BranchIdHex;
				BestForkRevision = Point.RevisionHash;
			}
		}

		if (BestAncestorNodeIndex != INDEX_NONE)
		{
			Node.ParentBranchId = BestAncestorIdHex;
			Node.ForkRevision = BestForkRevision;
		}
		// else: ParentBranchId stays empty ⇒ root.
	}

	// --- 3. Build child lists + collect roots (self-parent guard ⇒ root). ------------------
	TArray<int32> RootIndices;
	for (int32 NodeIndex = 0; NodeIndex < Layout.Nodes.Num(); ++NodeIndex)
	{
		const FString& ParentId = Layout.Nodes[NodeIndex].ParentBranchId;
		int32 ParentNodeIndex = INDEX_NONE;
		if (!ParentId.IsEmpty() && ParentId != Layout.Nodes[NodeIndex].BranchId)
		{
			const int32* FoundParent = IdToIndex.Find(ParentId);
			if (FoundParent != nullptr)
			{
				ParentNodeIndex = *FoundParent;
			}
		}

		if (ParentNodeIndex != INDEX_NONE)
		{
			Layout.Nodes[ParentNodeIndex].ChildIndices.Add(NodeIndex);
		}
		else
		{
			RootIndices.Add(NodeIndex);
		}
	}

	// --- 4. Determinism: sort roots and every node's children by referenced BranchName. ----
	const FNodeNameLess NameLess(Layout.Nodes);
	RootIndices.Sort(NameLess);
	for (FArcLoreBranchTreeNode& Node : Layout.Nodes)
	{
		Node.ChildIndices.Sort(NameLess);
	}

	// --- 5. Pass 1 — Depth / X + per-root color, BFS with a global cycle guard. ------------
	int32 MaxDepth = 0;
	TSet<int32> VisitedBFS;
	VisitedBFS.Reserve(Layout.Nodes.Num());
	for (int32 RootOrder = 0; RootOrder < RootIndices.Num(); ++RootOrder)
	{
		const int32 RootIndex = RootIndices[RootOrder];
		if (VisitedBFS.Contains(RootIndex))
		{
			continue; // already absorbed via a cycle through an earlier root
		}

		const FLinearColor SubtreeColor = RootPalette[RootOrder % RootPaletteSize];

		TArray<int32> Frontier;
		Frontier.Add(RootIndex);
		VisitedBFS.Add(RootIndex);
		Layout.Nodes[RootIndex].Depth = 0;

		while (Frontier.Num() > 0)
		{
			const int32 CurrentIndex = Frontier.Pop(EAllowShrinking::No);
			FArcLoreBranchTreeNode& Current = Layout.Nodes[CurrentIndex];
			Current.Pos.X = Current.Depth * LevelGapX;
			MaxDepth = FMath::Max(MaxDepth, Current.Depth);

			// Color: subtree palette, then current-branch accent, then archived alpha fade.
			FLinearColor Resolved = Current.bIsCurrent ? CurrentAccent : SubtreeColor;
			if (Current.bArchived)
			{
				Resolved.A *= 0.45f;
			}
			Current.Color = Resolved;

			const int32 ChildDepth = Current.Depth + 1;
			for (int32 ChildIndex : Current.ChildIndices)
			{
				if (VisitedBFS.Contains(ChildIndex))
				{
					continue; // cycle safety
				}
				VisitedBFS.Add(ChildIndex);
				Layout.Nodes[ChildIndex].Depth = ChildDepth;
				Frontier.Add(ChildIndex);
			}
		}
	}

	// --- 6. Pass 2 — Y (post-order). ------------------------------------------------------
	// No-overlap invariant: a subtree's leaves are assigned strictly increasing, contiguous
	// CurrentY slots (NodeGapY apart) and each internal node sits at the midpoint of its own
	// children's slots. Distinct subtrees therefore occupy disjoint, monotonically advancing
	// Y-bands, so no two nodes ever share a position (the only cost is vertical whitespace).
	float CurrentY = 0.f;
	TSet<int32> VisitedY;
	VisitedY.Reserve(Layout.Nodes.Num());

	// Iterative post-order so a malformed cycle can't infinite-recurse; an explicit Visited
	// guard mirrors the recursive AssignY semantics from the plan.
	for (int32 RootOrder = 0; RootOrder < RootIndices.Num(); ++RootOrder)
	{
		const int32 RootIndex = RootIndices[RootOrder];
		if (VisitedY.Contains(RootIndex))
		{
			continue;
		}

		struct FFrame
		{
			int32 NodeIndex = INDEX_NONE;
			int32 NextChild = 0;
		};
		TArray<FFrame> Stack;
		FFrame RootFrame;
		RootFrame.NodeIndex = RootIndex;
		Stack.Push(RootFrame);
		VisitedY.Add(RootIndex);

		while (Stack.Num() > 0)
		{
			FFrame& Frame = Stack.Top();
			const FArcLoreBranchTreeNode& FrameNode = Layout.Nodes[Frame.NodeIndex];

			// Descend into the next unvisited child first (recurse ALL children before write).
			bool bDescended = false;
			while (Frame.NextChild < FrameNode.ChildIndices.Num())
			{
				const int32 ChildIndex = FrameNode.ChildIndices[Frame.NextChild];
				++Frame.NextChild;
				if (VisitedY.Contains(ChildIndex))
				{
					continue; // cycle safety
				}
				VisitedY.Add(ChildIndex);
				FFrame ChildFrame;
				ChildFrame.NodeIndex = ChildIndex;
				Stack.Push(ChildFrame);
				bDescended = true;
				break;
			}
			if (bDescended)
			{
				continue;
			}

			// Post-order return: all children processed → assign this node's Y now.
			const int32 NodeIndex = Frame.NodeIndex;
			Stack.Pop(EAllowShrinking::No);

			const TArray<int32>& Children = Layout.Nodes[NodeIndex].ChildIndices;
			if (Children.Num() == 0)
			{
				// Leaf: take the next contiguous slot.
				Layout.Nodes[NodeIndex].Pos.Y = CurrentY;
				CurrentY += NodeGapY;
			}
			else
			{
				// Internal: midpoint of first and last child slots (sorted ChildIndices order).
				// All children already have a Y here: well-formed children via this frame's
				// post-order, cycle-skipped children via their original placement. The write
				// happens ONLY on the post-order return — never on descent.
				const int32 FirstChild = Children[0];
				const int32 LastChild = Children[Children.Num() - 1];
				Layout.Nodes[NodeIndex].Pos.Y =
					(Layout.Nodes[FirstChild].Pos.Y + Layout.Nodes[LastChild].Pos.Y) * 0.5f;
			}
		}

		// Separate root subtrees by an extra gap so their Y-bands stay visually distinct.
		CurrentY += RootGapY;
	}

	// --- 8. Bounds. -----------------------------------------------------------------------
	Layout.Bounds.X = (MaxDepth + 1) * LevelGapX;
	Layout.Bounds.Y = CurrentY;

	return Layout;
}
