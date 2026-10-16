// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Pure-C++ commit-graph layout for the ArcLore Branch Graph tab.
 *
 * No Slate, no Lore ABI, no UObject/USTRUCT — only Core containers and math.
 * Input structs are filled by the cross-branch fetch op; output structs are
 * consumed by the per-row painting widget. BuildLayout() topo-sorts the commit
 * DAG, assigns rows + lanes purely from DAG structure (independent of branch
 * enumeration order), and precomputes a per-row cell descriptor so each list
 * row can paint its own slice without scroll math.
 */

/** One entry in a branch's fork lineage (an ancestor branch id-hex paired with the revision it forked at). */
struct FArcLoreBranchPoint
{
	/** Ancestor branch id-hex (BranchIdToHex(point.branch)), NOT a display name — the ABI only exposes the opaque branch id here. */
	FString BranchIdHex;
	FString RevisionHash;
};

/** A branch as reported by lore_branch_list: name, tip revision, full fork lineage, flags. */
struct FArcLoreBranchInput
{
	FString Name;
	/** The branch's own id-hex (BranchIdToHex(entry.id)); the key used for order-independent parent resolution against each Stack entry's BranchIdHex. */
	FString Id;
	/** The LOCAL tip revision hash (from the local branch-list entry). Empty for a remote-only branch. Selection (SetSelectedByTipHash) matches on this. */
	FString TipHash;
	TArray<FArcLoreBranchPoint> Stack;
	bool bIsCurrent = false;
	bool bArchived = false;

	/** The REMOTE tip revision hash (from the remote branch-list entry); only meaningful when bHasRemote. */
	FString RemoteTipHash;
	/** True once a remote branch-list entry for this name was merged in. */
	bool bHasRemote = false;
	/** True when there is no remote, or local tip == remote tip (nothing to push/pull at the tip). */
	bool bInSync = false;
	/** Exact commits the local branch is ahead of remote; only valid for the current branch (bAheadBehindKnown). */
	int32 Ahead = 0;
	/** Exact commits the local branch is behind remote; only valid for the current branch (bAheadBehindKnown). */
	int32 Behind = 0;
	/** True only for the current branch, where exact Ahead/Behind came from lore_repository_status. */
	bool bAheadBehindKnown = false;
};

/** A single revision as reported by lore_revision_history (one per branch view; deduped in BuildLayout). */
struct FArcLoreRevisionInput
{
	FString Hash;
	FString BranchName;
	FString ParentHash;
	FString MergeParentHash;
	FString Message;
	FString Author;
	int32 RevisionNumber = 0;
	int64 TimestampMs = 0;
};

/** A laid-out commit node: deduped revision + assigned Row/Lane/Color and any branch refs pointing at it. */
struct FArcLoreCommitNode
{
	FString Hash;
	FString ParentHash;
	FString MergeParentHash;
	FString Message;
	FString Author;
	int64 TimestampMs = 0;
	TArray<FString> BranchRefs;
	int32 Row = INDEX_NONE;
	int32 Lane = INDEX_NONE;
	FLinearColor Color = FLinearColor::White;
	bool bIsCurrentTip = false;
};

/**
 * A single edge slice drawn within ONE row: enters the row's top boundary at
 * InLane and exits the bottom boundary at OutLane. Equal lanes = straight
 * vertical; differing lanes = diagonal (the lane-changing endpoint's row).
 * bDanglingStub marks an edge to an unfetched parent (drawn as a short stub).
 */
struct FArcLoreLaneSegment
{
	int32 InLane = INDEX_NONE;
	int32 OutLane = INDEX_NONE;
	FLinearColor Color = FLinearColor::White;
	bool bIsMerge = false;
	bool bDanglingStub = false;
};

/** Per-row paint descriptor: the node (if any) on this row plus all lane segments passing through it. */
struct FArcLoreGraphRowCell
{
	int32 Row = INDEX_NONE;
	bool bHasNode = false;
	int32 NodeLane = INDEX_NONE;
	FLinearColor NodeColor = FLinearColor::White;
	TArray<FArcLoreLaneSegment> Segments;
};

/** The complete layout: nodes and row cells both indexed/sorted by Row, plus the lane count. */
struct FArcLoreGraphLayout
{
	TArray<FArcLoreCommitNode> Nodes;
	TArray<FArcLoreGraphRowCell> RowCells;
	int32 NumLanes = 0;
};

/** Entry point: assembles a commit-graph layout from branch + revision inputs. */
class FArcLoreGraphLayoutBuilder
{
public:
	static FArcLoreGraphLayout BuildLayout(
		const TArray<FArcLoreBranchInput>& Branches,
		const TArray<FArcLoreRevisionInput>& Revisions);
};
