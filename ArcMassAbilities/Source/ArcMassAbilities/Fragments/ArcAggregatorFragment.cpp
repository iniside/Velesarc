// Copyright Lukasz Baran. All Rights Reserved.

#include "Fragments/ArcAggregatorFragment.h"

#include "ArcMassAbilitiesLogs.h"
#include "Effects/ArcMagnitudeCalculation.h"

namespace ArcAggregatorFragmentPrivate
{
	void CollectReachable(const TMap<FArcAttributeRef, TMap<FArcAttributeRef, int32>>& Graph,
	                      const FArcAttributeRef& From, TSet<FArcAttributeRef>& Reached)
	{
		const TMap<FArcAttributeRef, int32>* Edges = Graph.Find(From);
		if (!Edges)
		{
			return;
		}

		for (const TPair<FArcAttributeRef, int32>& Edge : *Edges)
		{
			bool bAlreadyReached = false;
			Reached.Add(Edge.Key, &bAlreadyReached);
			if (!bAlreadyReached)
			{
				CollectReachable(Graph, Edge.Key, Reached);
			}
		}
	}

	void VisitPostOrder(const TMap<FArcAttributeRef, TMap<FArcAttributeRef, int32>>& Graph,
	                    const FArcAttributeRef& Node, TSet<FArcAttributeRef>& Visited,
	                    TArray<FArcAttributeRef>& PostOrder)
	{
		bool bAlreadyVisited = false;
		Visited.Add(Node, &bAlreadyVisited);
		if (bAlreadyVisited)
		{
			return;
		}

		const TMap<FArcAttributeRef, int32>* Edges = Graph.Find(Node);
		if (Edges)
		{
			for (const TPair<FArcAttributeRef, int32>& Edge : *Edges)
			{
				VisitPostOrder(Graph, Edge.Key, Visited, PostOrder);
			}
		}

		PostOrder.Add(Node);
	}

	FString DescribeAttribute(const FArcAttributeRef& Attribute)
	{
		return FString::Printf(TEXT("%s::%s"),
			Attribute.FragmentType ? *Attribute.FragmentType->GetName() : TEXT("<null>"),
			*Attribute.PropertyName.ToString());
	}
}

FArcModifierHandle FArcAggregatorFragment::AddLiveMod(const FArcAttributeRef& Attribute, EArcModifierOp Op,
                                                      const FConstSharedStruct& LiveMagnitude, int32 StackCount,
                                                      float SnapshotMagnitude, FMassEntityHandle Source,
                                                      const FArcGameplayTagRequirements* SourceTagReqs,
                                                      const FArcGameplayTagRequirements* TargetTagReqs,
                                                      uint8 Channel)
{
	FArcAggregator& Aggregator = FindOrAddAggregator(Attribute);

	const FArcLiveMagnitude* Live = LiveMagnitude.GetPtr<FArcLiveMagnitude>();
	TArray<FArcAttributeRef> Sources;
	if (Live)
	{
		Live->GetSourceAttributes(Sources);
	}

	const FArcAttributeRef* CycleSource = Sources.FindByPredicate(
		[this, &Attribute](const FArcAttributeRef& SourceAttribute) -> bool
		{
			return SourceAttribute == Attribute || CanReach(Attribute, SourceAttribute);
		});

	if (!Live || CycleSource)
	{
		if (CycleSource)
		{
			UE_LOGF(LogArcMassAbilities, Warning,
				"Live magnitude on %ls reading %ls would form a dependency cycle; using snapshot magnitude %.4f",
				*ArcAggregatorFragmentPrivate::DescribeAttribute(Attribute),
				*ArcAggregatorFragmentPrivate::DescribeAttribute(*CycleSource),
				SnapshotMagnitude);
		}
		return Aggregator.AddMod(Op, SnapshotMagnitude, Source, SourceTagReqs, TargetTagReqs, Channel);
	}

	AddLiveDependencies(Attribute, Sources);
	return Aggregator.AddLiveMod(Op, LiveMagnitude, StackCount, SnapshotMagnitude, Source,
	                             SourceTagReqs, TargetTagReqs, Channel);
}

void FArcAggregatorFragment::RemoveMod(FArcModifierHandle Handle)
{
	for (TPair<FArcAttributeRef, FArcAggregator>& Pair : Aggregators)
	{
		FConstSharedStruct RemovedLive;
		if (!Pair.Value.RemoveMod(Handle, &RemovedLive))
		{
			continue;
		}

		const FArcLiveMagnitude* Live = RemovedLive.GetPtr<FArcLiveMagnitude>();
		if (Live)
		{
			TArray<FArcAttributeRef> Sources;
			Live->GetSourceAttributes(Sources);
			RemoveLiveDependencies(Pair.Key, Sources);
		}
		return;
	}
}

bool FArcAggregatorFragment::HasLiveDependents(const FArcAttributeRef& Source) const
{
	return LiveDependents.Contains(Source);
}

void FArcAggregatorFragment::GatherLiveDependents(const FArcAttributeRef& Source, TArray<FArcAttributeRef>& OutOrdered) const
{
	TSet<FArcAttributeRef> Reached;
	ArcAggregatorFragmentPrivate::CollectReachable(LiveDependents, Source, Reached);
	if (Reached.IsEmpty())
	{
		return;
	}

	for (const FArcAttributeRef& Dependent : LiveDependencyOrder)
	{
		if (Reached.Contains(Dependent))
		{
			OutOrdered.Add(Dependent);
		}
	}
}

void FArcAggregatorFragment::GetEvaluationOrder(TArray<FArcAttributeRef>& OutOrder) const
{
	OutOrder.Reserve(OutOrder.Num() + Aggregators.Num());
	for (const TPair<FArcAttributeRef, FArcAggregator>& Pair : Aggregators)
	{
		if (!LiveDependencyOrder.Contains(Pair.Key))
		{
			OutOrder.Add(Pair.Key);
		}
	}

	for (const FArcAttributeRef& Dependent : LiveDependencyOrder)
	{
		if (Aggregators.Contains(Dependent))
		{
			OutOrder.Add(Dependent);
		}
	}
}

bool FArcAggregatorFragment::CanReach(const FArcAttributeRef& From, const FArcAttributeRef& To) const
{
	TSet<FArcAttributeRef> Reached;
	ArcAggregatorFragmentPrivate::CollectReachable(LiveDependents, From, Reached);
	return Reached.Contains(To);
}

void FArcAggregatorFragment::AddLiveDependencies(const FArcAttributeRef& Dependent, TConstArrayView<FArcAttributeRef> Sources)
{
	bool bStructureChanged = false;
	for (const FArcAttributeRef& SourceAttribute : Sources)
	{
		int32& Count = LiveDependents.FindOrAdd(SourceAttribute).FindOrAdd(Dependent);
		bStructureChanged |= (Count == 0);
		++Count;
	}

	if (bStructureChanged)
	{
		RebuildLiveDependencyOrder();
	}
}

void FArcAggregatorFragment::RemoveLiveDependencies(const FArcAttributeRef& Dependent, TConstArrayView<FArcAttributeRef> Sources)
{
	bool bStructureChanged = false;
	for (const FArcAttributeRef& SourceAttribute : Sources)
	{
		TMap<FArcAttributeRef, int32>* Edges = LiveDependents.Find(SourceAttribute);
		if (!Edges)
		{
			continue;
		}

		int32* Count = Edges->Find(Dependent);
		if (!Count)
		{
			continue;
		}

		if (--(*Count) > 0)
		{
			continue;
		}

		Edges->Remove(Dependent);
		if (Edges->IsEmpty())
		{
			LiveDependents.Remove(SourceAttribute);
		}
		bStructureChanged = true;
	}

	if (bStructureChanged)
	{
		RebuildLiveDependencyOrder();
	}
}

void FArcAggregatorFragment::RebuildLiveDependencyOrder()
{
	LiveDependencyOrder.Reset();

	TSet<FArcAttributeRef> Dependents;
	for (const TPair<FArcAttributeRef, TMap<FArcAttributeRef, int32>>& Pair : LiveDependents)
	{
		for (const TPair<FArcAttributeRef, int32>& Edge : Pair.Value)
		{
			Dependents.Add(Edge.Key);
		}
	}

	TSet<FArcAttributeRef> Visited;
	TArray<FArcAttributeRef> PostOrder;
	for (const TPair<FArcAttributeRef, TMap<FArcAttributeRef, int32>>& Pair : LiveDependents)
	{
		ArcAggregatorFragmentPrivate::VisitPostOrder(LiveDependents, Pair.Key, Visited, PostOrder);
	}

	for (int32 Index = PostOrder.Num() - 1; Index >= 0; --Index)
	{
		if (Dependents.Contains(PostOrder[Index]))
		{
			LiveDependencyOrder.Add(PostOrder[Index]);
		}
	}
}
