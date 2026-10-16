// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "Effects/ArcAttributeCapture.h"
#include "ArcAttributeSet_Crit.generated.h"

USTRUCT(BlueprintType, meta = (ToolTip = "Attacker crit values, read by the damage execution on applied hits. An attacker without this set rolls no crit."))
struct ARCMASSMECHANICS_API FArcAttributeSet_Crit : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Crit", meta = (ToolTip = "Crit chance as a 0-1 fraction."))
	FArcAttribute CritChance;

	UPROPERTY(EditAnywhere, Category = "Crit", meta = (ToolTip = "Full damage multiplier on a crit (1.5 is x1.5). Bonuses are modifiers on it."))
	FArcAttribute CritMultiplier;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_Crit, CritChance)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_Crit, CritMultiplier)
	ARC_DECLARE_CAPTURE_SOURCE_LIVE(FArcAttributeSet_Crit, CritChance)
	ARC_DECLARE_CAPTURE_SOURCE_LIVE(FArcAttributeSet_Crit, CritMultiplier)
};

template <>
struct TMassFragmentTraits<FArcAttributeSet_Crit>
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
