// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreePropertyFunctionBase.h"
#include "GameplayTagContainer.h"

struct FStateTreeExecutionContext;

#include "ArcStateTreeGetAbilityItemMontageDesiredPlayTimePropertyFunction.generated.h"

USTRUCT()
struct ARCABILITYKIT_API FArcStateTreeGetAbilityItemMontageDesiredPlayTimePropertyFunctionInstanceData
{
	GENERATED_BODY()

	// Tag key into FArcItemFragment_AbilityMontages::MontageMap on the
	// active ability's source item.
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag MontageTag;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	float Output = 0.f;
};

USTRUCT(meta = (DisplayName = "Get Ability Item Montage Desired Play Time By Tag"))
struct ARCABILITYKIT_API FArcStateTreeGetAbilityItemMontageDesiredPlayTimePropertyFunction
	: public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcStateTreeGetAbilityItemMontageDesiredPlayTimePropertyFunctionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;
};
