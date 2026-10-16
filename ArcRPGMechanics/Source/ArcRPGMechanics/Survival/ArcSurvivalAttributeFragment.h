// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"

#include "ArcSurvivalAttributeFragment.generated.h"

/**
 * Survival needs hosted as proper FArcAttributes.
 *
 * Currently hosts only Fatigue — promoted from FArcFatigueNeedFragment::CurrentValue
 * so consumers (FArcRPGAttributeHandler_MaxStamina, FArcRPGAttributeHandler_MaxEnergy) can
 * declare Fatigue as an observed input via FArcAttributeHandler::GatherInputs
 * and react to changes through the normal attribute pipeline instead of via the
 * regen processor's bucket-poll + const_cast workaround.
 *
 * The smooth-accumulator copy continues to live on FArcFatigueNeedFragment.
 * UArcSurvivalAttributeBridgeProcessor mirrors it onto this fragment at 1s cadence.
 */
USTRUCT()
struct ARCRPGMECHANICS_API FArcSurvivalAttributeFragment : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute Fatigue;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcSurvivalAttributeFragment, Fatigue)
};

template <>
struct TMassFragmentTraits<FArcSurvivalAttributeFragment>
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
