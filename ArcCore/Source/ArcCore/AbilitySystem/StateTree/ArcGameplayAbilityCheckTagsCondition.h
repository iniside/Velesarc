// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/ArcAbilityStateTreeTypes.h"
#include "ArcGameplayAbilityCheckTagsCondition.generated.h"

class UAbilitySystemComponent;

USTRUCT()
struct FArcGameplayAbilityCheckTagsConditionInstanceData
{
	GENERATED_BODY()

	/** The ability system component whose owned tags to check. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** All of these tags must be present on the ASC for the condition to pass. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer RequiredTags;

	/** If any of these tags are present on the ASC, the condition fails. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer BlockedTags;
};

/**
 * Condition that checks the ability system component's owned gameplay tags.
 * Passes when all RequiredTags are present AND none of the BlockedTags are present.
 */
USTRUCT(meta = (DisplayName = "Check Gameplay Tags", Tooltip = "Condition that checks the ability system component's owned gameplay tags. Passes when all RequiredTags are present AND none of the BlockedTags are present."))
struct FArcGameplayAbilityCheckTagsCondition : public FArcGameplayAbilityConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGameplayAbilityCheckTagsConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
};
