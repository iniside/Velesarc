// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Effects/ArcAttributeCapture.h"
#include "Effects/ArcExtraCapture.h"
#include "ArcEffectDurationCalculation.generated.h"

struct FArcEffectExecutionContext;

/** Computes an effect's final duration from its base duration (request override, else policy duration) and the built spec. */
USTRUCT(meta=(ShowTooltip))
struct ARCMASSABILITIES_API FArcEffectDurationCalculation
{
	GENERATED_BODY()

	virtual ~FArcEffectDurationCalculation() = default;

	virtual void GatherCaptures(TArray<FArcAttributeCaptureDef>& OutCaptures, TArray<FArcExtraCaptureDef>& OutExtras) const {}

	virtual float CalculateDuration(const FArcEffectExecutionContext& Context, float BaseDuration) const
		PURE_VIRTUAL(FArcEffectDurationCalculation::CalculateDuration, return BaseDuration;);
};
