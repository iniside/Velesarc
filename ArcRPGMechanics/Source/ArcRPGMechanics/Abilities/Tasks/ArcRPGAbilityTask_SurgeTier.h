// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Attributes/ArcAttribute.h"
#include "ArcMassMechanics/Tiers/ArcTierTypes.h"
#include "ArcRPGAbilityTask_SurgeTier.generated.h"

class UArcTierEffectTable;

USTRUCT()
struct ARCRPGMECHANICS_API FArcRPGAbilityTask_SurgeTier_InstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FArcAttributeRef ValueAttribute;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ToolTip = "Optional. When set, its current value is subtracted from the predicted value, so the bands measure the overshoot past it."))
	FArcAttributeRef ThresholdAttribute;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FArcTierBands Bands;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UArcTierEffectTable> Table = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	float Magnitude = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	int32 Tier = 0;
};

USTRUCT(DisplayName = "Surge: Evaluate Tier", Category = "Arc|Ability|RPG", meta = (ToolTip = "Predicts ValueAttribute plus the amount of the ability's Entropy Cost (RPG), found directly or inside a Cost Set, minus ThresholdAttribute when set, maps it through Bands to a tier and applies the table effect for (spell domain tag, Tier) to the owner with the ability's source data. The spell domain tag (ArcSpell.Element.*) comes from the source item's Item Tags. No entropy cost, or a cost that does not resolve, adds 0; a missing item has no key. The effect is applied once and no handle is kept, so Instant effects are allowed. Always succeeds; a missing attribute, table or entry applies nothing."))
struct ARCRPGMECHANICS_API FArcRPGAbilityTask_SurgeTier : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcRPGAbilityTask_SurgeTier_InstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
