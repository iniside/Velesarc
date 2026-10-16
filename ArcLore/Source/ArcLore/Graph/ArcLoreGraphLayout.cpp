// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/ArcLoreGraphLayout.h"

namespace ArcLoreGraphLayoutInternal
{
	/**
	 * Lane-cycling color palette (~10 visually distinct hues). Node and segment
	 * colors are Palette[Lane % Num]. When more than Palette.Num() lanes are
	 * concurrently active, colors repeat — acceptable for a read-mostly graph.
	 */
	static const TArray<FLinearColor>& GetLanePalette()
	{
		static const TArray<FLinearColor> Palette = {
			FLinearColor(0.20f, 0.55f, 0.90f), // blue
			FLinearColor(0.90f, 0.45f, 0.20f), // orange
			FLinearColor(0.30f, 0.75f, 0.35f), // green
			FLinearColor(0.80f, 0.30f, 0.55f), // magenta
			FLinearColor(0.85f, 0.75f, 0.20f), // yellow
			FLinearColor(0.45f, 0.40f, 0.85f), // indigo
			FLinearColor(0.25f, 0.75f, 0.75f), // teal
			FLinearColor(0.85f, 0.40f, 0.40f), // red
			FLinearColor(0.55f, 0.70f, 0.25f), // lime
			FLinearColor(0.70f, 0.50f, 0.30f)  // brown
		};
		return Palette;
	}

	static FLinearColor ColorForLane(int32 Lane)
	{
		const TArray<FLinearColor>& Palette = GetLanePalette();
		if (Palette.Num() == 0 || Lane < 0)
		{
			return FLinearColor::White;
		}
		return Palette[Lane % Palette.Num()];
	}

	/** Internal edge between a child commit and one of its parents (linear or merge). */
	struct FInternalEdge
	{
		FString ParentHash;           // resolved to ParentRow/ParentLane after lane assignment
		int32 ChildRow = INDEX_NONE;
		int32 ChildLane = INDEX_NONE;
		int32 ParentRow = INDEX_NONE; // INDEX_NONE when the parent is not in the node map (dangling)
		int32 ParentLane = INDEX_NONE;
		bool bIsMerge = false;
	};

	/**
	 * Ready-set ordering for the Kahn topo-sort: newest TimestampMs first, then
	 * Hash lexical ascending as a stable final tiebreak (equal-ms commits never
	 * jitter between refreshes). Returns true when A should come before B.
	 */
	static bool ReadyLess(const TMap<FString, FArcLoreCommitNode>& NodeMap, const FString& A, const FString& B)
	{
		const FArcLoreCommitNode& NodeA = NodeMap.FindChecked(A);
		const FArcLoreCommitNode& NodeB = NodeMap.FindChecked(B);
		if (NodeA.TimestampMs != NodeB.TimestampMs)
		{
			return NodeA.TimestampMs > NodeB.TimestampMs; // newer first
		}
		return A < B; // lexical tiebreak
	}

	/** First empty slot in ActiveLanes, appending a new lane when none is free. */
	static int32 FirstFreeLane(TArray<FString>& ActiveLanes)
	{
		for (int32 L = 0; L < ActiveLanes.Num(); ++L)
		{
			if (ActiveLanes[L].IsEmpty())
			{
				return L;
			}
		}
		ActiveLanes.Add(FString());
		return ActiveLanes.Num() - 1;
	}
}

FArcLoreGraphLayout FArcLoreGraphLayoutBuilder::BuildLayout(
	const TArray<FArcLoreBranchInput>& Branches,
	const TArray<FArcLoreRevisionInput>& Revisions)
{
	using namespace ArcLoreGraphLayoutInternal;

	FArcLoreGraphLayout Layout;

	// ---- (a) Dedup revisions into a node map by hash -------------------------
	TMap<FString, FArcLoreCommitNode> NodeMap;
	NodeMap.Reserve(Revisions.Num());

	for (const FArcLoreRevisionInput& Rev : Revisions)
	{
		if (Rev.Hash.IsEmpty())
		{
			continue;
		}

		FArcLoreCommitNode* Existing = NodeMap.Find(Rev.Hash);
		if (Existing == nullptr)
		{
			FArcLoreCommitNode Node;
			Node.Hash = Rev.Hash;
			Node.ParentHash = Rev.ParentHash;
			Node.MergeParentHash = Rev.MergeParentHash;
			Node.Message = Rev.Message;
			Node.Author = Rev.Author;
			Node.TimestampMs = Rev.TimestampMs;
			NodeMap.Add(Rev.Hash, MoveTemp(Node));
		}
		else
		{
			// A later branch view may supply data the first one lacked.
			if (Existing->ParentHash.IsEmpty() && !Rev.ParentHash.IsEmpty())
			{
				Existing->ParentHash = Rev.ParentHash;
			}
			if (Existing->MergeParentHash.IsEmpty() && !Rev.MergeParentHash.IsEmpty())
			{
				Existing->MergeParentHash = Rev.MergeParentHash;
			}
			if (Existing->Message.IsEmpty() && !Rev.Message.IsEmpty())
			{
				Existing->Message = Rev.Message;
			}
			if (Existing->Author.IsEmpty() && !Rev.Author.IsEmpty())
			{
				Existing->Author = Rev.Author;
			}
			if (Existing->TimestampMs == 0 && Rev.TimestampMs != 0)
			{
				Existing->TimestampMs = Rev.TimestampMs;
			}
		}
	}

	// Attach branch refs to tip nodes; mark the current branch's tip.
	for (const FArcLoreBranchInput& Branch : Branches)
	{
		if (Branch.TipHash.IsEmpty())
		{
			continue;
		}
		FArcLoreCommitNode* TipNode = NodeMap.Find(Branch.TipHash);
		if (TipNode == nullptr)
		{
			continue;
		}
		TipNode->BranchRefs.Add(Branch.Name);
		if (Branch.bIsCurrent)
		{
			TipNode->bIsCurrentTip = true;
		}
	}

	if (NodeMap.Num() == 0)
	{
		return Layout;
	}

	// ---- (b) Kahn topo-sort, parent -> child, newest-first deterministic -----
	// In-degree counts only parents that exist in the node map; an absent parent
	// (truncated by the fetch cap) does not block the node from being a root.
	TMap<FString, int32> InDegree;
	InDegree.Reserve(NodeMap.Num());
	// Children adjacency: ParentHash -> list of child hashes (so we can decrement).
	TMap<FString, TArray<FString>> Children;
	Children.Reserve(NodeMap.Num());

	for (const TPair<FString, FArcLoreCommitNode>& Pair : NodeMap)
	{
		InDegree.Add(Pair.Key, 0);
	}

	for (const TPair<FString, FArcLoreCommitNode>& Pair : NodeMap)
	{
		const FArcLoreCommitNode& Node = Pair.Value;

		// A parent that exists in the map contributes one to the child's in-degree
		// and registers the child under that parent for later decrement.
		if (!Node.ParentHash.IsEmpty() && NodeMap.Contains(Node.ParentHash))
		{
			InDegree.FindChecked(Node.Hash) += 1;
			Children.FindOrAdd(Node.ParentHash).Add(Node.Hash);
		}
		if (!Node.MergeParentHash.IsEmpty() && NodeMap.Contains(Node.MergeParentHash))
		{
			InDegree.FindChecked(Node.Hash) += 1;
			Children.FindOrAdd(Node.MergeParentHash).Add(Node.Hash);
		}
	}

	TArray<FString> Ready;
	for (const TPair<FString, int32>& Pair : InDegree)
	{
		if (Pair.Value == 0)
		{
			Ready.Add(Pair.Key);
		}
	}
	Ready.Sort([&NodeMap](const FString& A, const FString& B) -> bool
	{
		return ReadyLess(NodeMap, A, B);
	});

	TArray<FString> TopoOrder;
	TopoOrder.Reserve(NodeMap.Num());

	while (Ready.Num() > 0)
	{
		// Pop the front (best per comparator); keeps Ready sorted.
		const FString Current = Ready[0];
		Ready.RemoveAt(0);
		TopoOrder.Add(Current);

		const TArray<FString>* ChildList = Children.Find(Current);
		if (ChildList != nullptr)
		{
			for (const FString& ChildHash : *ChildList)
			{
				int32& Deg = InDegree.FindChecked(ChildHash);
				Deg -= 1;
				if (Deg == 0)
				{
					// Insert keeping Ready sorted by ReadyLess.
					int32 InsertIdx = 0;
					while (InsertIdx < Ready.Num() && ReadyLess(NodeMap, Ready[InsertIdx], ChildHash))
					{
						++InsertIdx;
					}
					Ready.Insert(ChildHash, InsertIdx);
				}
			}
		}
	}

	// Defensive: if a cycle left nodes unvisited (should not happen in a DAG),
	// append them deterministically so commits never vanish.
	if (TopoOrder.Num() < NodeMap.Num())
	{
		TArray<FString> Remaining;
		for (const TPair<FString, FArcLoreCommitNode>& Pair : NodeMap)
		{
			if (!TopoOrder.Contains(Pair.Key))
			{
				Remaining.Add(Pair.Key);
			}
		}
		Remaining.Sort([&NodeMap](const FString& A, const FString& B) -> bool
		{
			return ReadyLess(NodeMap, A, B);
		});
		TopoOrder.Append(Remaining);
	}

	// Assign Row reversed so newest commit = Row 0 (top of display).
	// Kahn's sort emits oldest first (index 0), so we flip: Row = LastIndex - Index.
	// The reversal is a permutation over 0..NumNodes-1, so all downstream
	// RowCells/Layout.Nodes indexing remains in range.
	TMap<FString, int32> RowOf;
	RowOf.Reserve(TopoOrder.Num());
	const int32 LastIndex = TopoOrder.Num() - 1;
	for (int32 Index = 0; Index < TopoOrder.Num(); ++Index)
	{
		const int32 RowValue = LastIndex - Index;
		NodeMap.FindChecked(TopoOrder[Index]).Row = RowValue;
		RowOf.Add(TopoOrder[Index], RowValue);
	}

	const int32 NumRows = TopoOrder.Num();

	// ---- (c) Active-lane assignment over the topo order ----------------------
	// ActiveLanes[L] = the parent hash that lane L is currently "reserved" for
	// (i.e. the next commit expected to occupy lane L). Empty string = free slot.
	TArray<FString> ActiveLanes;
	int32 MaxLane = INDEX_NONE;

	TArray<FInternalEdge> Edges;
	Edges.Reserve(NumRows * 2);

	for (int32 RowIdx = 0; RowIdx < NumRows; ++RowIdx)
	{
		const FString& Hash = TopoOrder[RowIdx];
		FArcLoreCommitNode& Node = NodeMap.FindChecked(Hash);

		// Determine this node's lane: a lane reserved for it (by a prior child),
		// else a fresh free slot.
		int32 NodeLane = INDEX_NONE;
		for (int32 L = 0; L < ActiveLanes.Num(); ++L)
		{
			if (ActiveLanes[L] == Hash)
			{
				NodeLane = L;
				break;
			}
		}
		if (NodeLane == INDEX_NONE)
		{
			NodeLane = FirstFreeLane(ActiveLanes);
		}
		Node.Lane = NodeLane;
		MaxLane = FMath::Max(MaxLane, NodeLane);

		// Free any OTHER lanes still reserved for this node. A fork point (parent
		// with multiple children) has each child reserve a lane for the parent;
		// the parent claims only NodeLane above, so the remaining reservations
		// would leak forever (inflating NumLanes, leaving empty gaps) unless freed.
		// Reset() (not RemoveAt) keeps lane indices stable, so a single scan is safe.
		for (int32 L = 0; L < ActiveLanes.Num(); ++L)
		{
			if (L != NodeLane && ActiveLanes[L] == Hash)
			{
				ActiveLanes[L].Reset();
			}
		}

		// The linear parent inherits this node's lane (continues straight down).
		const bool bHasLinearParent = !Node.ParentHash.IsEmpty();
		if (bHasLinearParent)
		{
			ActiveLanes[NodeLane] = Node.ParentHash;
		}
		else
		{
			// No linear parent: this lane terminates at this node.
			ActiveLanes[NodeLane].Reset();
		}

		// Build the linear-parent edge. ParentLane is resolved AFTER the loop from
		// the parent node's final assigned lane (see the resolve pass below) — the
		// parent may live on a different lane than the child when the child is a
		// fork branch, so the child lane is only a dangling fallback.
		if (bHasLinearParent)
		{
			FInternalEdge Edge;
			Edge.ParentHash = Node.ParentHash;
			Edge.ChildRow = RowIdx;
			Edge.ChildLane = NodeLane;
			Edge.bIsMerge = false;
			Edges.Add(Edge);
		}

		// The merge parent opens a new lane (first free slot, else append).
		if (!Node.MergeParentHash.IsEmpty())
		{
			const int32 MergeLane = FirstFreeLane(ActiveLanes);
			ActiveLanes[MergeLane] = Node.MergeParentHash;
			MaxLane = FMath::Max(MaxLane, MergeLane);

			FInternalEdge Edge;
			Edge.ParentHash = Node.MergeParentHash;
			Edge.ChildRow = RowIdx;
			Edge.ChildLane = NodeLane;
			Edge.bIsMerge = true;
			Edges.Add(Edge);
		}
	}

	// ---- Resolve every edge's parent endpoint from the parent's FINAL lane -----
	// A fork parent shares its lane with only ONE child; every other child's edge
	// must converge into the parent's actual lane (not the child's). Look the
	// parent up by hash; fall back to the child lane only when the parent dangles.
	for (FInternalEdge& Edge : Edges)
	{
		const FArcLoreCommitNode* ParentNode = NodeMap.Find(Edge.ParentHash);
		if (ParentNode != nullptr && ParentNode->Row != INDEX_NONE)
		{
			Edge.ParentRow = ParentNode->Row;
			Edge.ParentLane = ParentNode->Lane;
			MaxLane = FMath::Max(MaxLane, Edge.ParentLane);
		}
		else
		{
			Edge.ParentRow = INDEX_NONE; // dangling (parent not fetched)
			Edge.ParentLane = Edge.ChildLane;
		}
	}

	Layout.NumLanes = (MaxLane == INDEX_NONE) ? 0 : (MaxLane + 1);

	// ---- (d) Build per-row cells from the edge list -------------------------
	Layout.RowCells.SetNum(NumRows);
	for (int32 RowIdx = 0; RowIdx < NumRows; ++RowIdx)
	{
		Layout.RowCells[RowIdx].Row = RowIdx;
	}

	for (const FInternalEdge& Edge : Edges)
	{
		const FLinearColor EdgeColor = ColorForLane(Edge.ChildLane);

		if (Edge.ParentRow == INDEX_NONE)
		{
			// Dangling: a single short stub on the child's row (exits downward
			// with no real parent). Lane does not change.
			if (Layout.RowCells.IsValidIndex(Edge.ChildRow))
			{
				FArcLoreLaneSegment Segment;
				Segment.InLane = Edge.ChildLane;
				Segment.OutLane = Edge.ChildLane;
				Segment.Color = EdgeColor;
				Segment.bIsMerge = Edge.bIsMerge;
				Segment.bDanglingStub = true;
				Layout.RowCells[Edge.ChildRow].Segments.Add(Segment);
			}
			continue;
		}

		// Real edge spanning the inclusive row interval between the child dot and the
		// parent dot. NOTE: in this builder's Kahn order parents are emitted BEFORE
		// their children (in-degree counts parents, children become ready once their
		// parent is popped), so the parent occupies the SMALLER row. The geometry is
		// written orientation-agnostically off Child/ParentRow rather than assuming
		// which is on top, so it stays correct regardless of row direction.
		//
		// The diagonal (lane-changing) segment lives in exactly ONE row, placed by
		// edge kind; every other row in the span is a straight vertical. The segment
		// touching the child dot row must sit at ChildLane; the segment touching the
		// parent dot row must sit at ParentLane. Continuity invariant across the span
		// (top-to-bottom): each row's OutLane equals the next row's InLane.
		const int32 ChildRow = Edge.ChildRow;
		const int32 ParentRow = Edge.ParentRow;
		const int32 StartRow = FMath::Min(ChildRow, ParentRow);
		const int32 EndRow = FMath::Max(ChildRow, ParentRow);
		const bool bLaneChanges = (Edge.ChildLane != Edge.ParentLane);

		// The single row carrying the diagonal:
		// - Lanes equal            -> none (every row a straight vertical).
		// - Merge edge             -> the CHILD row: the line leaves the child toward
		//                            the new/older merge-parent lane immediately.
		// - Non-merge fork branch  -> the row ADJACENT to the parent on the child
		//                            side: the child stays in its own lane until just
		//                            before the parent, converging into ParentLane at
		//                            the parent dot. Adjacent dots collapse this onto
		//                            the child row (clamped into [StartRow, EndRow]).
		int32 DiagonalRow = INDEX_NONE;
		if (bLaneChanges)
		{
			if (Edge.bIsMerge)
			{
				DiagonalRow = ChildRow;
			}
			else
			{
				// One step from the parent toward the child, clamped to the span.
				const int32 StepTowardChild = (ChildRow > ParentRow) ? (ParentRow + 1) : (ParentRow - 1);
				DiagonalRow = FMath::Clamp(StepTowardChild, StartRow, EndRow);
			}
		}

		// Lane a vertical row should sit at, by which side of the diagonal it is on.
		// Rows on the child side of the diagonal ride ChildLane; rows on the parent
		// side ride ParentLane. Distance-from-parent makes this orientation-agnostic.
		for (int32 R = StartRow; R <= EndRow; ++R)
		{
			if (!Layout.RowCells.IsValidIndex(R))
			{
				continue;
			}

			FArcLoreLaneSegment Segment;
			Segment.Color = EdgeColor;
			Segment.bIsMerge = Edge.bIsMerge;
			Segment.bDanglingStub = false;

			if (!bLaneChanges)
			{
				// Straight vertical at the shared lane through the whole span.
				Segment.InLane = Edge.ChildLane;
				Segment.OutLane = Edge.ChildLane;
			}
			else if (R == DiagonalRow)
			{
				// The single diagonal. Orient In/Out so the child-side boundary is
				// ChildLane and the parent-side boundary is ParentLane.
				if (ChildRow < ParentRow)
				{
					// Child is on top: enter (top) at child lane, exit toward parent.
					Segment.InLane = Edge.ChildLane;
					Segment.OutLane = Edge.ParentLane;
				}
				else
				{
					// Parent is on top: enter (top) at parent lane, exit toward child.
					Segment.InLane = Edge.ParentLane;
					Segment.OutLane = Edge.ChildLane;
				}
			}
			else
			{
				// Straight vertical: child-side rows ride ChildLane, parent-side
				// rows ride ParentLane. "Child side" = strictly closer to the child
				// dot than the diagonal row is.
				const bool bChildSide = (ChildRow < ParentRow) ? (R < DiagonalRow) : (R > DiagonalRow);
				const int32 VerticalLane = bChildSide ? Edge.ChildLane : Edge.ParentLane;
				Segment.InLane = VerticalLane;
				Segment.OutLane = VerticalLane;
			}

			Layout.RowCells[R].Segments.Add(Segment);
		}
	}

	// Mark each node's own row with its dot.
	for (const TPair<FString, FArcLoreCommitNode>& Pair : NodeMap)
	{
		const FArcLoreCommitNode& Node = Pair.Value;
		if (Layout.RowCells.IsValidIndex(Node.Row))
		{
			FArcLoreGraphRowCell& Cell = Layout.RowCells[Node.Row];
			Cell.bHasNode = true;
			Cell.NodeLane = Node.Lane;
			Cell.NodeColor = ColorForLane(Node.Lane);
		}
	}

	// ---- (e) Color: assign per-lane node colors, emit Nodes sorted by Row ----
	Layout.Nodes.SetNum(NumRows);
	for (const TPair<FString, FArcLoreCommitNode>& Pair : NodeMap)
	{
		FArcLoreCommitNode Node = Pair.Value;
		Node.Color = ColorForLane(Node.Lane);
		if (Layout.Nodes.IsValidIndex(Node.Row))
		{
			Layout.Nodes[Node.Row] = MoveTemp(Node);
		}
	}

	return Layout;
}
