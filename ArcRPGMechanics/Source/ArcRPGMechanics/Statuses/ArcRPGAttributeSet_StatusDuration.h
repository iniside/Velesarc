// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "Effects/ArcAttributeCapture.h"
#include "ArcRPGAttributeSet_StatusDuration.generated.h"

USTRUCT(BlueprintType)
struct ARCRPGMECHANICS_API FArcRPGAttributeSet_StatusDuration : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Duration", meta = (ToolTip = "Applier's status duration bonus as a fraction (0.2 = +20%), from accents and affixes. The RPG Status Duration multiplies the duration by (1 + this)."))
	FArcAttribute StatusDurationPct;

	UPROPERTY(EditAnywhere, Category = "Duration", meta = (ToolTip = "Duration of conditions applied to this entity as a fraction (-0.2 = 20% shorter). Fraction; no consumer yet."))
	FArcAttribute ConditionDurationReceivedPct;

	UPROPERTY(EditAnywhere, Category = "Duration", meta = (ToolTip = "Duration of control effects applied to this entity as a fraction (-0.2 = 20% shorter). Fraction; no consumer yet."))
	FArcAttribute ControlDurationReceivedPct;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_StatusDuration, StatusDurationPct)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_StatusDuration, ConditionDurationReceivedPct)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcRPGAttributeSet_StatusDuration, ControlDurationReceivedPct)
	ARC_DECLARE_CAPTURE_SOURCE_LIVE(FArcRPGAttributeSet_StatusDuration, StatusDurationPct)
};

template <>
struct TMassFragmentTraits<FArcRPGAttributeSet_StatusDuration>
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
