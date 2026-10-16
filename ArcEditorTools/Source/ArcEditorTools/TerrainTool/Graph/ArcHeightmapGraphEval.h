// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Spatial/SampledScalarField2.h"

class UArcHeightmapGraph;
struct FArcHeightmapEvalContext;

/** Multi-output result of a heightmap graph evaluation: the single heightfield plus zero or more
 *  named weight-map layers (keyed by the producing Output node's LayerName). */
struct FArcHeightmapEvalResult
{
	UE::Geometry::FSampledScalarField2f Height;
	TMap<FName, UE::Geometry::FSampledScalarField2f> Layers;
};

/**
 * Topological CPU evaluator for a UArcHeightmapGraph (Step 3).
 *
 * Resolves the node DAG with a Kahn topological sort keyed by FGuid NodeId (edges = InputNodeIds),
 * evaluates every node in dependency order into owned per-node result fields, and copies each Output
 * node's result into OutResult. Cycle-safe and degenerate-graph-safe: never hangs, never crashes.
 *
 * Multi-output contract: exactly one Output node must have an empty LayerName — its result becomes
 * OutResult.Height (the heightfield). Any number of Output nodes may have non-empty LayerNames, which
 * must be UNIQUE; each becomes an entry in OutResult.Layers (a weight-map layer). Zero or more-than-one
 * empty-LayerName Output, or a duplicate non-empty LayerName, are errors.
 *
 * Returns true on success (OutResult populated). On any structural error returns false and sets
 * OutError to a human-readable reason. Error conditions: empty graph, a node with an all-zero NodeId,
 * a duplicate NodeId, a cycle, zero / more-than-one heightfield (empty-LayerName) Output, or a
 * duplicated layer name. A dangling InputNodeId (one that resolves to no known node) is NOT an error —
 * the slot resolves to nullptr and a Warning is logged. Disconnected nodes (unreachable from any Output
 * node) are likewise a Warning only.
 */
bool EvaluateGraph(const UArcHeightmapGraph& Graph,
	const FArcHeightmapEvalContext& Ctx,
	FArcHeightmapEvalResult& OutResult,
	FString& OutError);

/** Single-field convenience overload: evaluates the graph and returns only the heightfield. */
bool EvaluateGraph(const UArcHeightmapGraph& Graph,
	const FArcHeightmapEvalContext& Ctx,
	UE::Geometry::FSampledScalarField2f& OutField,
	FString& OutError);
