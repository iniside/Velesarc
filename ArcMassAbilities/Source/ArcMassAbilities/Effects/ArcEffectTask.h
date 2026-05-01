// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "Mass/EntityHandle.h"
#include "Attributes/ArcAttribute.h"
#include "ArcEffectTask.generated.h"

struct FArcActiveEffect;
class UArcEffectDefinition;
class UArcAbilityDefinition;

USTRUCT()
struct ARCMASSABILITIES_API FArcEffectTask
{
	GENERATED_BODY()

	virtual ~FArcEffectTask() = default;

	virtual void OnApplication(FArcActiveEffect& OwningEffect) {}
	virtual void OnRemoved(FArcActiveEffect& OwningEffect) {}
	virtual void OnExecute(FArcActiveEffect& OwningEffect) {}
};

USTRUCT(BlueprintType, meta=(DisplayName="On Attribute Changed"))
struct ARCMASSABILITIES_API FArcEffectTask_OnAttributeChanged : public FArcEffectTask
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttributeRef Attribute;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UArcEffectDefinition> EffectToApply = nullptr;

	FDelegateHandle DelegateHandle;

	virtual void OnApplication(FArcActiveEffect& OwningEffect) override;
	virtual void OnRemoved(FArcActiveEffect& OwningEffect) override;
};

USTRUCT(BlueprintType, meta=(DisplayName="On Ability Activated"))
struct ARCMASSABILITIES_API FArcEffectTask_OnAbilityActivated : public FArcEffectTask
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TObjectPtr<UArcAbilityDefinition> AbilityFilter = nullptr;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UArcEffectDefinition> EffectToApply = nullptr;

	FDelegateHandle DelegateHandle;

	virtual void OnApplication(FArcActiveEffect& OwningEffect) override;
	virtual void OnRemoved(FArcActiveEffect& OwningEffect) override;
};

USTRUCT(BlueprintType, meta=(DisplayName="On Effect Applied"))
struct ARCMASSABILITIES_API FArcEffectTask_OnEffectApplied : public FArcEffectTask
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FGameplayTagRequirements EffectTagFilter;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UArcEffectDefinition> EffectToApply = nullptr;

	FDelegateHandle DelegateHandle;

	virtual void OnApplication(FArcActiveEffect& OwningEffect) override;
	virtual void OnRemoved(FArcActiveEffect& OwningEffect) override;
};

USTRUCT(BlueprintType, meta=(DisplayName="On Effect Executed"))
struct ARCMASSABILITIES_API FArcEffectTask_OnEffectExecuted : public FArcEffectTask
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FGameplayTagRequirements EffectTagFilter;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UArcEffectDefinition> EffectToApply = nullptr;

	FDelegateHandle DelegateHandle;

	virtual void OnApplication(FArcActiveEffect& OwningEffect) override;
	virtual void OnRemoved(FArcActiveEffect& OwningEffect) override;
};
