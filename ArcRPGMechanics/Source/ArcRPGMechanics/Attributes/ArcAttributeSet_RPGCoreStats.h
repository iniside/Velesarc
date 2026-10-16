// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"

#include "ArcAttributeSet_RPGCoreStats.generated.h"

USTRUCT()
struct ARCRPGMECHANICS_API FArcAttributeSet_RPGCoreStats : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute Strength;

	UPROPERTY(EditAnywhere)
	FArcAttribute Dexterity;

	UPROPERTY(EditAnywhere)
	FArcAttribute Constitution;

	UPROPERTY(EditAnywhere)
	FArcAttribute Agility;

	UPROPERTY(EditAnywhere)
	FArcAttribute Perception;

	UPROPERTY(EditAnywhere)
	FArcAttribute Magic;

	UPROPERTY(EditAnywhere)
	FArcAttribute Intelligence;

	UPROPERTY(EditAnywhere)
	FArcAttribute Willpower;

	UPROPERTY(EditAnywhere)
	FArcAttribute Wisdom;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGCoreStats, Strength)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGCoreStats, Dexterity)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGCoreStats, Constitution)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGCoreStats, Agility)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGCoreStats, Perception)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGCoreStats, Magic)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGCoreStats, Intelligence)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGCoreStats, Willpower)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_RPGCoreStats, Wisdom)
};

template <>
struct TMassFragmentTraits<FArcAttributeSet_RPGCoreStats>
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
