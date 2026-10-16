// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Tags/ArcGameplayTagRequirements.h"
#include "Mass/EntityHandle.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcEffectTypes.h"
#include "ArcEffectTask.generated.h"

struct FArcActiveEffect;
class UArcEffectDefinition;
class UArcAbilityDefinition;

USTRUCT(meta=(ShowTooltip))
struct ARCMASSABILITIES_API FArcEffectTask
{
	GENERATED_BODY()

	virtual ~FArcEffectTask() = default;

	virtual void OnApplication(FArcActiveEffect& OwningEffect) {}
	virtual void OnRemoved(FArcActiveEffect& OwningEffect, EArcEffectRemovalReason Reason) {}
	virtual void OnExecute(FArcActiveEffect& OwningEffect) {}
	virtual void OnInhibitionChanged(FArcActiveEffect& OwningEffect, bool bInhibited) {}
};

/** Applies EffectToApply to the owner entity whenever the watched Attribute changes while this effect is active. */
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
	virtual void OnRemoved(FArcActiveEffect& OwningEffect, EArcEffectRemovalReason Reason) override;
};

/** Applies EffectToApply to the owner entity whenever an ability matching AbilityFilter activates while this effect is active. */
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
	virtual void OnRemoved(FArcActiveEffect& OwningEffect, EArcEffectRemovalReason Reason) override;
};

UENUM()
enum class EArcTagChangeTrigger : uint8
{
	Added,
	Removed,
	Both
};

UENUM()
enum class EArcTagChangeParty : uint8
{
	Target,
	Source
};

/** Applies EffectToApply to the owner entity whenever RequiredTag is gained or lost on the watched party while this effect is active. */
USTRUCT(BlueprintType, meta=(DisplayName="On Tag Changed"))
struct ARCMASSABILITIES_API FArcEffectTask_OnTagChanged : public FArcEffectTask
{
	GENERATED_BODY()

	/** Target watches the entity carrying this effect; Source watches the entity that applied it. With Source and no source entity nothing is watched. */
	UPROPERTY(EditAnywhere)
	EArcTagChangeParty Party = EArcTagChangeParty::Target;

	/** The owned tag whose gain or loss fires the task. */
	UPROPERTY(EditAnywhere)
	FGameplayTag RequiredTag;

	UPROPERTY(EditAnywhere)
	EArcTagChangeTrigger When = EArcTagChangeTrigger::Added;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UArcEffectDefinition> EffectToApply = nullptr;

	FDelegateHandle AddedHandle;
	FDelegateHandle RemovedHandle;

	virtual void OnApplication(FArcActiveEffect& OwningEffect) override;
	virtual void OnRemoved(FArcActiveEffect& OwningEffect, EArcEffectRemovalReason Reason) override;
};
