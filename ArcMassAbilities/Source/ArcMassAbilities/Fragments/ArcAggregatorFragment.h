// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
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

	FArcAggregator& FindOrAddAggregator(const FArcAttributeRef& Attr)
	{
		return Aggregators.FindOrAdd(Attr);
	}

	FArcAggregator* FindAggregator(const FArcAttributeRef& Attr)
	{
		return Aggregators.Find(Attr);
	}

};

template<>
struct TMassFragmentTraits<FArcAggregatorFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
