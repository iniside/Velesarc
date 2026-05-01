// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "MassEntityTypes.h"

#include "ArcMassHealthStatsFragment.generated.h"

USTRUCT()
struct ARCMASSDAMAGESYSTEM_API FArcMassHealthStatsFragment : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute Health;

	UPROPERTY(EditAnywhere)
	FArcAttribute MaxHealth;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMassHealthStatsFragment, Health)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMassHealthStatsFragment, MaxHealth)
};

template <>
struct TMassFragmentTraits<FArcMassHealthStatsFragment>
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
