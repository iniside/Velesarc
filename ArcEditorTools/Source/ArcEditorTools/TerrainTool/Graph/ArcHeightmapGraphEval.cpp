// Copyright Lukasz Baran. All Rights Reserved.

#include "TerrainTool/Graph/ArcHeightmapGraphEval.h"

#include "TerrainTool/Graph/ArcHeightmapGraph.h"
#include "TerrainTool/Graph/ArcHeightmapNodes.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcHeightmapGraphEval, Log, All);

namespace ArcHeightmapGraphEval_Internal
{
	using UE::Geometry::FSampledScalarField2f;

	/**
	 * Build NodeId -> node ptr over the graph's instanced structs, and record which NodeIds are
	 * Output nodes (detected by the instanced struct's concrete script struct type — the ptr alone
	 * does not carry the UScriptStruct). Reports zero-id and duplicate-id errors.
	 */
	bool BuildNodeMap(const UArcHeightmapGraph& Graph,
		TMap<FGuid, const FArcHeightmapNode*>& OutNodes,
		TArray<FGuid>& OutOutputIds,
		TMap<FGuid, FName>& OutOutputLayerNames,
		TMap<FGuid, FString>& OutTypeNames,
		FString& OutError)
	{
		for (const FInstancedStruct& Instance : Graph.Nodes)
		{
			const FArcHeightmapNode* Node = Instance.GetPtr<FArcHeightmapNode>();
			if (Node == nullptr)
			{
				// Null / empty array slot — not a structural error, just skip it.
				continue;
			}

			if (!Node->NodeId.IsValid())
			{
				OutError = TEXT("Heightmap graph contains a node with an unassigned (all-zero) NodeId. ")
					TEXT("Save the asset so PostEditChangeProperty assigns ids, or assign them programmatically.");
				return false;
			}

			if (OutNodes.Contains(Node->NodeId))
			{
				OutError = FString::Printf(
					TEXT("Heightmap graph contains a duplicate NodeId %s."), *Node->NodeId.ToString());
				return false;
			}

			OutNodes.Add(Node->NodeId, Node);

			// Record the concrete struct name for readable per-node timing logs.
			const UScriptStruct* ScriptStruct = Instance.GetScriptStruct();
			OutTypeNames.Add(Node->NodeId, (ScriptStruct != nullptr) ? ScriptStruct->GetName() : TEXT("<unknown>"));

			// Exact-type match on the instanced struct's script struct — the authoritative way to
			// identify the Output marker (a FInstancedStruct carries its concrete UScriptStruct).
			if (Instance.GetScriptStruct() == FArcHeightmapNode_Output::StaticStruct())
			{
				OutOutputIds.Add(Node->NodeId);

				// Type verified above, so the static_cast is safe — read the Output's LayerName so the
				// caller can partition heightfield (empty) vs named weight-map layers.
				const FArcHeightmapNode_Output* OutputNode = static_cast<const FArcHeightmapNode_Output*>(Node);
				OutOutputLayerNames.Add(Node->NodeId, OutputNode->LayerName);
			}
		}

		return true;
	}

	/**
	 * Kahn topological sort. Edges run input -> node (an InputNodeId is a prerequisite of the node).
	 * InputNodeIds that don't resolve to a known node are ignored for ordering (the caller logs them
	 * and treats them as nullptr slots at evaluation). Returns false if not all nodes are ordered
	 * (i.e. a cycle exists).
	 */
	bool TopoSort(const TMap<FGuid, const FArcHeightmapNode*>& Nodes, TArray<FGuid>& OutOrder)
	{
		// In-degree = count of resolvable InputNodeIds. Adjacency: input id -> dependent node ids.
		TMap<FGuid, int32> InDegree;
		TMap<FGuid, TArray<FGuid>> Dependents;
		InDegree.Reserve(Nodes.Num());

		for (const TPair<FGuid, const FArcHeightmapNode*>& Pair : Nodes)
		{
			InDegree.Add(Pair.Key, 0);
		}

		for (const TPair<FGuid, const FArcHeightmapNode*>& Pair : Nodes)
		{
			const FArcHeightmapNode* Node = Pair.Value;
			for (const FGuid& InputId : Node->InputNodeIds)
			{
				if (!Nodes.Contains(InputId))
				{
					// Dangling input — does not constrain ordering.
					continue;
				}

				InDegree[Pair.Key] += 1;
				Dependents.FindOrAdd(InputId).Add(Pair.Key);
			}
		}

		TArray<FGuid> Ready;
		for (const TPair<FGuid, int32>& Pair : InDegree)
		{
			if (Pair.Value == 0)
			{
				Ready.Add(Pair.Key);
			}
		}

		OutOrder.Reset();
		OutOrder.Reserve(Nodes.Num());

		while (Ready.Num() > 0)
		{
			const FGuid Current = Ready.Pop(EAllowShrinking::No);
			OutOrder.Add(Current);

			const TArray<FGuid>* Downstream = Dependents.Find(Current);
			if (Downstream == nullptr)
			{
				continue;
			}

			for (const FGuid& DependentId : *Downstream)
			{
				int32& Degree = InDegree[DependentId];
				Degree -= 1;
				if (Degree == 0)
				{
					Ready.Add(DependentId);
				}
			}
		}

		// If not every node made it into the order, the remainder forms one or more cycles.
		return OutOrder.Num() == Nodes.Num();
	}

	/** Set of NodeIds reachable from OutputId by walking InputNodeIds backward (toward sources). */
	void CollectReachable(const TMap<FGuid, const FArcHeightmapNode*>& Nodes, const FGuid& OutputId, TSet<FGuid>& OutReachable)
	{
		TArray<FGuid> Stack;
		Stack.Add(OutputId);

		while (Stack.Num() > 0)
		{
			const FGuid Current = Stack.Pop(EAllowShrinking::No);
			if (OutReachable.Contains(Current))
			{
				continue;
			}
			OutReachable.Add(Current);

			const FArcHeightmapNode* const* Found = Nodes.Find(Current);
			if (Found == nullptr || *Found == nullptr)
			{
				continue;
			}

			for (const FGuid& InputId : (*Found)->InputNodeIds)
			{
				if (Nodes.Contains(InputId) && !OutReachable.Contains(InputId))
				{
					Stack.Add(InputId);
				}
			}
		}
	}
}

bool EvaluateGraph(const UArcHeightmapGraph& Graph,
	const FArcHeightmapEvalContext& Ctx,
	FArcHeightmapEvalResult& OutResult,
	FString& OutError)
{
	using namespace ArcHeightmapGraphEval_Internal;
	using UE::Geometry::FSampledScalarField2f;

	OutError.Reset();

	if (Graph.Nodes.Num() == 0)
	{
		OutError = TEXT("Heightmap graph is empty (no nodes).");
		return false;
	}

	// 1. Index nodes by id and find Output markers (errors on zero / duplicate ids).
	TMap<FGuid, const FArcHeightmapNode*> NodeMap;
	TArray<FGuid> OutputIds;
	TMap<FGuid, FName> OutputLayerNames;
	TMap<FGuid, FString> NodeTypeNames;
	if (!BuildNodeMap(Graph, NodeMap, OutputIds, OutputLayerNames, NodeTypeNames, OutError))
	{
		return false;
	}

	if (NodeMap.Num() == 0)
	{
		OutError = TEXT("Heightmap graph has no valid nodes (all entries were empty).");
		return false;
	}

	// 2. Partition Output nodes into the single heightfield (empty LayerName) and named weight layers.
	//    Exactly one heightfield is required; named layer names must be unique.
	FGuid HeightfieldOutputId;
	int32 HeightfieldOutputCount = 0;
	TArray<FGuid> NamedLayerOutputIds;
	TMap<FName, FGuid> SeenLayerNames;
	for (const FGuid& OutputId : OutputIds)
	{
		const FName LayerName = OutputLayerNames.FindChecked(OutputId);
		if (LayerName.IsNone())
		{
			++HeightfieldOutputCount;
			HeightfieldOutputId = OutputId;
			continue;
		}

		if (SeenLayerNames.Contains(LayerName))
		{
			OutError = FString::Printf(
				TEXT("Heightmap graph has more than one Output node with layer name '%s'; layer names must be unique."),
				*LayerName.ToString());
			return false;
		}
		SeenLayerNames.Add(LayerName, OutputId);
		NamedLayerOutputIds.Add(OutputId);
	}

	if (HeightfieldOutputCount == 0)
	{
		OutError = TEXT("Heightmap graph has no heightfield Output node; exactly one Output with an empty LayerName is required.");
		return false;
	}
	if (HeightfieldOutputCount > 1)
	{
		OutError = FString::Printf(
			TEXT("Heightmap graph has %d heightfield Output nodes (empty LayerName); exactly one is required."),
			HeightfieldOutputCount);
		return false;
	}

	// 3. Warn about dangling InputNodeIds (slots that resolve to no known node).
	for (const TPair<FGuid, const FArcHeightmapNode*>& Pair : NodeMap)
	{
		for (const FGuid& InputId : Pair.Value->InputNodeIds)
		{
			if (InputId.IsValid() && !NodeMap.Contains(InputId))
			{
				UE_LOG(LogArcHeightmapGraphEval, Warning,
					TEXT("Node %s references missing input %s; that slot will resolve to a null field."),
					*Pair.Key.ToString(), *InputId.ToString());
			}
		}
	}

	// 4. Topological sort; failure means a cycle.
	TArray<FGuid> TopoOrder;
	if (!TopoSort(NodeMap, TopoOrder))
	{
		OutError = FString::Printf(
			TEXT("Heightmap graph contains a cycle (only %d of %d nodes could be ordered)."),
			TopoOrder.Num(), NodeMap.Num());
		return false;
	}

	// 5. Disconnected-node warning — anything unreachable from ANY Output (heightfield or a named
	//    weight layer) is dead / mis-wired. Seed the reachable set from every output id.
	TSet<FGuid> Reachable;
	for (const FGuid& OutputId : OutputIds)
	{
		CollectReachable(NodeMap, OutputId, Reachable);
	}
	for (const TPair<FGuid, const FArcHeightmapNode*>& Pair : NodeMap)
	{
		if (!Reachable.Contains(Pair.Key))
		{
			UE_LOG(LogArcHeightmapGraphEval, Warning,
				TEXT("Node %s is not reachable from any Output node (disconnected / dead node)."),
				*Pair.Key.ToString());
		}
	}

	// 6. Evaluate in dependency order, owning each node's result field.
	TMap<FGuid, TSharedPtr<FSampledScalarField2f>> OwnedResults;
	OwnedResults.Reserve(TopoOrder.Num());

	const double EvalStartSeconds = FPlatformTime::Seconds();
	for (const FGuid& NodeId : TopoOrder)
	{
		const FArcHeightmapNode* Node = NodeMap.FindChecked(NodeId);

		// Assemble inputs in the SAME order as InputNodeIds; unresolved / missing -> nullptr.
		TArray<const FSampledScalarField2f*> Inputs;
		Inputs.Reserve(Node->InputNodeIds.Num());
		for (const FGuid& InputId : Node->InputNodeIds)
		{
			const TSharedPtr<FSampledScalarField2f>* Resolved = OwnedResults.Find(InputId);
			Inputs.Add((Resolved != nullptr) ? Resolved->Get() : nullptr);
		}

		TSharedPtr<FSampledScalarField2f> Result = MakeShared<FSampledScalarField2f>();

		// Per-node timing trace: measures the wall-clock spent inside each node's Evaluate, so a
		// slow node (typically Erosion) is obvious in the log. Logged at Display so it shows by default.
		const double NodeStartSeconds = FPlatformTime::Seconds();
		Node->Evaluate(Inputs, Ctx, *Result);
		const double NodeMs = (FPlatformTime::Seconds() - NodeStartSeconds) * 1000.0;

		const FString* TypeName = NodeTypeNames.Find(NodeId);
		const TCHAR* TypeStr = TypeName != nullptr ? **TypeName : TEXT("<unknown>");

		// Scan the node's output for non-finite cells and value range, so the FIRST node that
		// introduces NaN/Inf (the source of the downstream erosion hang) is named in the log
		// rather than chased blind. Min/Max are over finite cells only.
		const TArray64<float>& ResultVals = Result->GridValues.GridValues();
		int32 NonFiniteCount = 0;
		float MinV = TNumericLimits<float>::Max();
		float MaxV = TNumericLimits<float>::Lowest();
		for (int64 Idx = 0; Idx < ResultVals.Num(); ++Idx)
		{
			const float V = ResultVals[Idx];
			if (!FMath::IsFinite(V))
			{
				++NonFiniteCount;
				continue;
			}
			MinV = FMath::Min(MinV, V);
			MaxV = FMath::Max(MaxV, V);
		}

		if (NonFiniteCount > 0)
		{
			UE_LOG(LogArcHeightmapGraphEval, Warning,
				TEXT("Node %s (%s) emitted %d non-finite (NaN/Inf) cell(s) of %lld in %.2f ms ")
				TEXT("[finite range %.3f..%.3f] — this node is the NaN source."),
				TypeStr, *NodeId.ToString(), NonFiniteCount,
				static_cast<int64>(ResultVals.Num()), NodeMs, MinV, MaxV);
		}
		else
		{
			UE_LOG(LogArcHeightmapGraphEval, Display,
				TEXT("Evaluated node %s (%s) in %.2f ms [range %.3f..%.3f]."),
				TypeStr, *NodeId.ToString(), NodeMs,
				ResultVals.Num() > 0 ? MinV : 0.0f, ResultVals.Num() > 0 ? MaxV : 0.0f);
		}

		OwnedResults.Add(NodeId, Result);
	}
	const double TotalMs = (FPlatformTime::Seconds() - EvalStartSeconds) * 1000.0;
	UE_LOG(LogArcHeightmapGraphEval, Display,
		TEXT("Evaluated %d node(s) in %.2f ms total."), TopoOrder.Num(), TotalMs);

	// 7. Copy the heightfield Output's result into OutResult.Height.
	const TSharedPtr<FSampledScalarField2f>* HeightfieldResult = OwnedResults.Find(HeightfieldOutputId);
	if (HeightfieldResult == nullptr || !HeightfieldResult->IsValid())
	{
		OutError = TEXT("Internal error: Output node produced no result.");
		return false;
	}
	OutResult.Height = **HeightfieldResult;

	// 8. Copy each named weight-map layer Output's result into OutResult.Layers, keyed by LayerName.
	for (const FGuid& LayerOutputId : NamedLayerOutputIds)
	{
		const TSharedPtr<FSampledScalarField2f>* LayerResult = OwnedResults.Find(LayerOutputId);
		if (LayerResult == nullptr || !LayerResult->IsValid())
		{
			OutError = TEXT("Internal error: Output node produced no result.");
			return false;
		}
		const FName LayerName = OutputLayerNames.FindChecked(LayerOutputId);
		OutResult.Layers.Add(LayerName, **LayerResult);
	}

	return true;
}

bool EvaluateGraph(const UArcHeightmapGraph& Graph,
	const FArcHeightmapEvalContext& Ctx,
	UE::Geometry::FSampledScalarField2f& OutField,
	FString& OutError)
{
	FArcHeightmapEvalResult Result;
	if (!EvaluateGraph(Graph, Ctx, Result, OutError))
	{
		return false;
	}
	OutField = MoveTemp(Result.Height);
	return true;
}
