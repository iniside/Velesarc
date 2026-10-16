// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Abilities/ArcAbilityHandle.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Attributes/ArcAttribute.h"
#include "Mass/EntityHandle.h"
#include "ArcMassMechanics/Tiers/ArcTierTypes.h"
#include "ArcAbilityTask_EvalThresholdTier.generated.h"

class UArcTierEffectTable;
struct FMassEntityManager;

USTRUCT()
struct ARCMASSMECHANICS_API FArcAbilityTask_EvalThresholdTier_InstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FArcAttributeRef ValueAttribute;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ToolTip = "Optional. When set, its current value is subtracted from the value, so the bands measure the overshoot past it."))
	FArcAttributeRef ThresholdAttribute;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FArcTierBands Bands;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UArcTierEffectTable> Table = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag Key;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	float Magnitude = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	int32 Tier = 0;
};

USTRUCT(meta = (DisplayName = "Evaluate Threshold Tier", ToolTip = "Reads ValueAttribute, minus ThresholdAttribute when set, maps it through Bands to a tier and applies the table effect for (Key, Tier) to the owner with the ability's source data. The effect is applied once and no handle is kept, so Instant effects are allowed. Always succeeds; a missing attribute, table or entry applies nothing."))
struct ARCMASSMECHANICS_API FArcAbilityTask_EvalThresholdTier : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAbilityTask_EvalThresholdTier_InstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
