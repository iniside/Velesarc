// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "GameplayTagContainer.h"
#include "ArcImmunityFragment.generated.h"

USTRUCT()
struct ARCMASSABILITIES_API FArcImmunityFragment : public FMassFragment
{
	GENERATED_BODY()

	FGameplayTagContainer ImmunityTags;
};

template<>
struct TMassFragmentTraits<FArcImmunityFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
