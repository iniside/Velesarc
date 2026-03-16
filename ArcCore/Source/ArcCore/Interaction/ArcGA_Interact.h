// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "ArcGA_Interact.generated.h"

class UArcCoreInteractionSourceComponent;

UCLASS()
class ARCCORE_API UArcGA_Interact : public UArcCoreGameplayAbility
{
	GENERATED_BODY()

public:
	UArcGA_Interact();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/**
	 * Override in children to customize interaction behavior.
	 * Default implementation calls BeginInteraction on the current target.
	 */
	virtual void PerformInteraction(UArcCoreInteractionSourceComponent* InteractionSource);
};
