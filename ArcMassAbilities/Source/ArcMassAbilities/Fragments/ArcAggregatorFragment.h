// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Attributes/ArcAttribute.h"
#include "Attributes/ArcAggregator.h"
#include "Modifiers/ArcDirectModifier.h"
#include "ArcAggregatorFragment.generated.h"

USTRUCT()
struct ARCMASSABILITIES_API FArcAggregatorFragment : public FMassFragment
{
	GENERATED_BODY()

	TMap<FArcAttributeRef, FArcAggregator> Aggregators;

	TMap<FArcModifierHandle, FArcDirectModifier> OwnedModifiers;

	TMap<FArcAttributeRef, TMap<FArcAttributeRef, int32>> LiveDependents;

	TArray<FArcAttributeRef> LiveDependencyOrder;

	FArcAggregator& FindOrAddAggregator(const FArcAttributeRef& Attr)
	{
		return Aggregators.FindOrAdd(Attr);
	}

	FArcAggregator* FindAggregator(const FArcAttributeRef& Attr)
	{
		return Aggregators.Find(Attr);
	}

	const FArcAggregator* FindAggregator(const FArcAttributeRef& Attr) const
	{
		return Aggregators.Find(Attr);
	}

	FArcModifierHandle AddLiveMod(const FArcAttributeRef& Attribute, EArcModifierOp Op,
	                              const FConstSharedStruct& LiveMagnitude, int32 StackCount,
	                              float SnapshotMagnitude, FMassEntityHandle Source,
	                              const FArcGameplayTagRequirements* SourceTagReqs,
	                              const FArcGameplayTagRequirements* TargetTagReqs,
	                              uint8 Channel);

	void RemoveMod(FArcModifierHandle Handle);

	bool HasLiveDependents(const FArcAttributeRef& Source) const;

	void GatherLiveDependents(const FArcAttributeRef& Source, TArray<FArcAttributeRef>& OutOrdered) const;

	void GetEvaluationOrder(TArray<FArcAttributeRef>& OutOrder) const;

private:
	bool CanReach(const FArcAttributeRef& From, const FArcAttributeRef& To) const;
	void AddLiveDependencies(const FArcAttributeRef& Dependent, TConstArrayView<FArcAttributeRef> Sources);
	void RemoveLiveDependencies(const FArcAttributeRef& Dependent, TConstArrayView<FArcAttributeRef> Sources);
	void RebuildLiveDependencyOrder();
};

template<>
struct TMassFragmentTraits<FArcAggregatorFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
