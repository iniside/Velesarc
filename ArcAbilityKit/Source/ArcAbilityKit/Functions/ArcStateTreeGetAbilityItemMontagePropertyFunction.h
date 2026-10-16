// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreePropertyFunctionBase.h"
#include "GameplayTagContainer.h"

class UAnimMontage;
struct FStateTreeExecutionContext;

#include "ArcStateTreeGetAbilityItemMontagePropertyFunction.generated.h"

USTRUCT()
struct ARCABILITYKIT_API FArcStateTreeGetAbilityItemMontagePropertyFunctionInstanceData
{
	GENERATED_BODY()

	// Tag key into FArcItemFragment_AbilityMontages::MontageMap on the
	// active ability's source item.
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag MontageTag;

	// Resolved montage (sync-loaded). Null if the item, fragment, or tag is missing.
	UPROPERTY(VisibleAnywhere, Category = "Output")
	TObjectPtr<UAnimMontage> Output = nullptr;
};

USTRUCT(meta = (DisplayName = "Get Ability Item Montage By Tag"))
struct ARCABILITYKIT_API FArcStateTreeGetAbilityItemMontagePropertyFunction
	: public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcStateTreeGetAbilityItemMontagePropertyFunctionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;
};
