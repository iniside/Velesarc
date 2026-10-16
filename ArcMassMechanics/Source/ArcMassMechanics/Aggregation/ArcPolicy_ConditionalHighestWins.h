// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAggregator.h"
#include "ArcPolicy_ConditionalHighestWins.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Conditional Highest Wins", ToolTip = "Default channel evaluation, except that on each channel listed in ConditionalChannels only the largest bonus per operation counts. Penalties on those channels always count and stack. A bonus is a magnitude that raises the value: positive, above 1 for Multiply Compound, negative for Divide Additive. Override mods are unaffected."))
struct ARCMASSMECHANICS_API FArcPolicy_ConditionalHighestWins : public FArcAggregationPolicy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Policy")
	TArray<uint8> ConditionalChannels;

	virtual float Aggregate(
		const FArcAggregationContext& Context,
		const TArray<FArcAggregatorMod> (&Mods)[static_cast<uint8>(EArcModifierOp::Max)]
	) const override;
};
