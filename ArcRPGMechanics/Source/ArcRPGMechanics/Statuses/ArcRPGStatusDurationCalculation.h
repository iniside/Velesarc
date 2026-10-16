// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Effects/ArcEffectDurationCalculation.h"
#include "ArcRPGStatusDurationCalculation.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "RPG Status Duration", ToolTip = "D-75 / D-81 status duration: Base * (1 + Scale * skill power) x (1 + Wisdom K * WIS modifier) x (1 + source Status Duration Pct), floored at 0. Base is the request's duration override (a trigger's Duration), else the policy Duration. Skill power comes from the Skill Scaling fragment on the source item (the item that granted the ability), captured when the spec is built; without the item, the fragment or its Skill it is 0. With no source entity the WIS and Status Duration Pct terms are 1. An effect whose tags have any World Time Effect Types has its Base in world time units: it is also multiplied by Game Hours Per World Unit and the real seconds per game hour."))
struct ARCRPGMECHANICS_API FArcRPGStatusDurationCalculation : public FArcEffectDurationCalculation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Skill", meta = (ToolTip = "D-75 Scale of this effect's duration. 0 keeps the duration fixed."))
	float Scale = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Attribute", meta = (ToolTip = "k of the WIS multiplier (combat.yaml parameter_k.duration)."))
	float WisdomK = 0.1f;

	UPROPERTY(EditAnywhere, Category = "World Time", meta = (Categories = "ArcEffect.Type", ToolTip = "Effect types whose base duration is in world time units (statuses.yaml families with scale: world)."))
	FGameplayTagContainer WorldTimeEffectTypes;

	UPROPERTY(EditAnywhere, Category = "World Time", meta = (ClampMin = "0", ToolTip = "Game hours in one world time unit of the base duration."))
	float GameHoursPerWorldUnit = 1.f;

	virtual void GatherCaptures(TArray<FArcAttributeCaptureDef>& OutCaptures, TArray<FArcExtraCaptureDef>& OutExtras) const override;

	virtual float CalculateDuration(const FArcEffectExecutionContext& Context, float BaseDuration) const override;
};
