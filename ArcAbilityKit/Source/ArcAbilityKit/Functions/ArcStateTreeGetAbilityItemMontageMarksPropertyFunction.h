// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreePropertyFunctionBase.h"
#include "GameplayTagContainer.h"
#include "Tasks/ArcMontageMarkEntry.h"

struct FStateTreeExecutionContext;

#include "ArcStateTreeGetAbilityItemMontageMarksPropertyFunction.generated.h"

USTRUCT()
struct ARCABILITYKIT_API FArcStateTreeGetAbilityItemMontageMarksPropertyFunctionInstanceData
{
	GENERATED_BODY()

	// Tag key into FArcItemFragment_AbilityMontages::MontageMap on the
	// active ability's source item.
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag MontageTag;

	// Task play rate. Baked marks are stored unscaled; Output Time is divided by this.
	UPROPERTY(EditAnywhere, Category = "Parameter")
	float PlayRate = 1.0f;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	TArray<FArcMontageMarkEntry> Output;
};

USTRUCT(meta = (DisplayName = "Get Ability Item Montage Marks By Tag"))
struct ARCABILITYKIT_API FArcStateTreeGetAbilityItemMontageMarksPropertyFunction
	: public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcStateTreeGetAbilityItemMontageMarksPropertyFunctionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;
};
