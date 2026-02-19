// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectComponents/AbilitiesGameplayEffectComponent.h"
#include "ArcGEC_SendAsyncMessageToTarget.generated.h"

/**
 * Send Async Message to the target when the Gameplay Effect is executed or applied.
 * Target is the owner of the Ability System Component the Gameplay Effect is applied to.
 */
UCLASS()
class ARCCORE_API UArcGEC_SendAsyncMessageToTarget : public UGameplayEffectComponent
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere)
	FInstancedStruct ExecutedMessage;
	
	UPROPERTY(EditAnywhere)
	FGameplayTag ExecutedMessageTag;
	
	UPROPERTY(EditAnywhere)
	FInstancedStruct AppliedMessage;
	
	UPROPERTY(EditAnywhere)
	FGameplayTag AppliedMessageTag;
	
	/** 
 	 * Called when a Gameplay Effect is executed.  GE's can only Execute on ROLE_Authority.  GE's only Execute when they're applying an instant effect (otherwise they're added to the ActiveGameplayEffectsContainer).
 	 * Note: Periodic effects Execute every period (and are also added to ActiveGameplayEffectsContainer).  One may think of this as periodically executing an instant effect (and thus can only happen on the server).
 	 */
	virtual void OnGameplayEffectExecuted(FActiveGameplayEffectsContainer& ActiveGEContainer, FGameplayEffectSpec& GESpec, FPredictionKey& PredictionKey) const override;

	/**
	 * Called when a Gameplay Effect is initially applied, or stacked.  GE's are 'applied' in both cases of duration or instant execution.  This call does not happen periodically, nor through replication.
	 * One should favor this function over OnActiveGameplayEffectAdded & OnGameplayEffectExecuted (but all multiple may be used depending on the case).
	 */
	virtual void OnGameplayEffectApplied(FActiveGameplayEffectsContainer& ActiveGEContainer, FGameplayEffectSpec& GESpec, FPredictionKey& PredictionKey) const override;
};
