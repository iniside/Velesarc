// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/ArcLoreGraphLayout.h"

/**
 * Pure-C++ branch-tree layout for the ArcLore Branch Tree Explorer tab.
 *
 * No Slate, no Lore ABI, no UObject/USTRUCT — only Core containers and math.
 * Consumes the same FArcLoreBranchInput list as the commit-graph layout, but
 * builds a *parent/child branch tree* (one node per branch) rather than a commit
 * DAG. Parent resolution is order-independent: it does not depend on the order
 * branches were enumerated nor on the order entries appear in a branch's Stack.
 * The tidy horizontal layout guarantees no node overlap (see invariant below).
 */

/** One laid-out branch in the tree: identity, lineage link, flags, and assigned position/color. */
struct FArcLoreBranchTreeNode
{
	FString BranchName;
	FString BranchId;
	/** Id-hex of the resolved immediate parent branch (empty ⇒ this node is a root). */
	FString ParentBranchId;
	FString TipHash;
	/** Revision hash this branch forked from its parent at (from the chosen Stack entry). */
	FString ForkRevision;
	bool bIsCurrent = false;
	bool bArchived = false;
	/** Edges from the root of this node's subtree (root depth = 0). */
	int32 Depth = 0;
	FVector2D Pos = FVector2D::ZeroVector;
	/** Indices into FArcLoreBranchTreeLayout::Nodes; sorted by referenced node's BranchName. */
	TArray<int32> ChildIndices;
	FLinearColor Color = FLinearColor::White;

	// --- Local/remote sync fields (mirrored from FArcLoreBranchInput) ---
	/** Id-hex of the remote tip for this branch (empty if no remote). */
	FString RemoteTipHash;
	/** True if a remote tracking branch exists. */
	bool bHasRemote = false;
	/** True if local and remote tips are identical. */
	bool bInSync = false;
	/** Number of local commits not yet on the remote. */
	int32 Ahead = 0;
	/** Number of remote commits not yet in local. */
	int32 Behind = 0;
	/** True when Ahead/Behind have been computed (false = counts unknown/pending). */
	bool bAheadBehindKnown = false;
};

/** The complete branch tree: a flat indexable node array plus the laid-out canvas bounds. */
struct FArcLoreBranchTreeLayout
{
	/** Indexable; ChildIndices reference positions in this array. */
	TArray<FArcLoreBranchTreeNode> Nodes;
	FVector2D Bounds = FVector2D::ZeroVector;
};

/** Entry point: assembles a branch tree layout from the branch input list. */
class FArcLoreBranchTreeBuilder
{
public:
	static FArcLoreBranchTreeLayout BuildTree(const TArray<FArcLoreBranchInput>& Branches);
};
