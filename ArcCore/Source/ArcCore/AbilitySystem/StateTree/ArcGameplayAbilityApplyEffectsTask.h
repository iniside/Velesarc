// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystem/ArcAbilityStateTreeTypes.h"
#include "ArcGameplayAbilityApplyEffectsTask.generated.h"

class UGameplayAbility;

USTRUCT()
struct FArcGameplayAbilityApplyEffectsTaskInstanceData
{
	GENERATED_BODY()

	/** The item gameplay ability to apply effects from. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UGameplayAbility> Ability;

	/** Tag filter for which effects to apply. If empty, applies ALL effects from the item. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag EffectTag;

	/** Target data specifying who receives the gameplay effects. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayAbilityTargetDataHandle TargetData;

	/** Handles of all gameplay effects that were successfully applied. */
	UPROPERTY(EditAnywhere, Category = Output)
	TArray<FActiveGameplayEffectHandle> AppliedEffects;
};

/**
 * Applies gameplay effects from the item definition to the specified targets.
 * When EffectTag is set, only effects matching that tag are applied; otherwise all item effects are applied.
 * Requires a UArcItemGameplayAbility. Completes immediately.
 */
USTRUCT(meta = (DisplayName = "Apply Effects From Item", Tooltip = "Applies gameplay effects from the item definition to the specified targets. When EffectTag is set, only effects matching that tag are applied; otherwise all item effects are applied. Requires a UArcItemGameplayAbility. Completes immediately."))
struct FArcGameplayAbilityApplyEffectsTask : public FArcGameplayAbilityTaskBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FArcGameplayAbilityApplyEffectsTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
};
