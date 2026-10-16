// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "Effects/ArcAttributeCapture.h"
#include "ArcAttributeSet_Mitigation.generated.h"

USTRUCT(BlueprintType, meta = (ToolTip = "Damage-type values resolved by tags. The damage execution reads each with the hit's damage-type tag in the source tags, so a modifier qualifies per type through its source tag requirements; a parent tag such as ArcEffect.Damage.Magical covers every child type. Resistance and DamageReduction are the defender's 0-1 fractions, 1 or more is immunity. Armor is the defender's global armor, added to the struck piece's armor; it applies on periodic ticks too."))
struct ARCMASSMECHANICS_API FArcAttributeSet_Mitigation : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Mitigation")
	FArcAttribute Armor;

	UPROPERTY(EditAnywhere, Category = "Mitigation")
	FArcAttribute Resistance;

	UPROPERTY(EditAnywhere, Category = "Mitigation")
	FArcAttribute DamageReduction;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_Mitigation, Armor)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_Mitigation, Resistance)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcAttributeSet_Mitigation, DamageReduction)
	ARC_DECLARE_CAPTURE_TARGET_LIVE(FArcAttributeSet_Mitigation, Armor)
	ARC_DECLARE_CAPTURE_TARGET_LIVE(FArcAttributeSet_Mitigation, Resistance)
	ARC_DECLARE_CAPTURE_TARGET_LIVE(FArcAttributeSet_Mitigation, DamageReduction)
};

template <>
struct TMassFragmentTraits<FArcAttributeSet_Mitigation>
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
