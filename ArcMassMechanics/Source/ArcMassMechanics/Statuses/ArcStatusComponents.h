// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Tags/ArcGameplayTagRequirements.h"
#include "Effects/ArcEffectComponent.h"
#include "ArcStatusComponents.generated.h"

struct FArcActiveAbility;
struct FArcEffectSpec;

USTRUCT(BlueprintType, meta = (DisplayName = "Interrupt Abilities", ToolTip = "On application, cancels the target's running abilities with reason Interrupted and the effect source as instigator. An ability is interrupted when its definition AbilityTags satisfy Abilities (all Must Have Tags, none of Must Not Have Tags); empty Abilities match every active ability, and an ability without a definition is skipped. The ability that applied this effect to its own owner is never interrupted."))
struct ARCMASSMECHANICS_API FArcEffectComp_InterruptAbilities : public FArcEffectComponent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Interrupt")
	FArcGameplayTagRequirements Abilities;

	virtual void OnApplied(const FArcEffectContext& Context) const override;
};
