// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UArcHeightmapGraph;
class UArcHeightmapEdGraph;

/**
 * Bidirectional round-trip between the editor UEdGraph mirror (UArcHeightmapEdGraph)
 * and the data asset (UArcHeightmapGraph). All correspondence is keyed by
 * FArcHeightmapNode::NodeId — never by array position — so adding or deleting a node
 * never scrambles the surviving nodes' data.
 */
namespace ArcHeightmapGraphBridge
{
	/**
	 * Populate EdGraph from Asset. Mints NodeIds for any id-less node (dirtying the
	 * asset only when a mint actually happened), spawns one mirror node per
	 * FInstancedStruct entry, then wires input pins from InputNodeIds. Dangling
	 * input GUIDs are skipped silently (matching the evaluator's warning semantics).
	 */
	void ARCEDITORTOOLS_API BuildEdGraphFromAsset(UArcHeightmapGraph& Asset, UArcHeightmapEdGraph& EdGraph);

	/**
	 * Write EdGraph back into Asset (the UpdateAsset analog). Rebuilds Asset.Nodes by
	 * NodeId — preserving each surviving node's authored struct data — and rewrites
	 * InputNodeIds from the live pin links. Unlinked input slots are stored as an
	 * invalid FGuid (intentionally pruning a dangling ref). The CALLER owns any
	 * FScopedTransaction; this only calls Modify() + MarkPackageDirty().
	 */
	void ARCEDITORTOOLS_API WriteAssetFromEdGraph(UArcHeightmapEdGraph& EdGraph, UArcHeightmapGraph& Asset);
}
