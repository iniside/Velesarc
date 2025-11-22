// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectComponent.h"
#include "StateTreeEvents.h"
#include "ArcGEC_MassStateTreeEvent.generated.h"

/**
 * 
 */
UCLASS()
class ARCAI_API UArcGEC_MassStateTreeEvent : public UGameplayEffectComponent
{
	GENERATED_BODY()

	/**
	 * Only send when effect have duration.
	 */
	UPROPERTY(EditAnywhere)
	FStateTreeEvent OnActiveEvent;

	/**
	 * Send on each execution of the effect. This can include initial application and subsequent period ticks.
	 */
	UPROPERTY(EditAnywhere)
	FStateTreeEvent OnExecutedEvent;
	
	/**
	 * Send when the effect is initially applied, or stacked.  This does not occur periodically, nor through replication.
	 */
	UPROPERTY(EditAnywhere)
	FStateTreeEvent OnAppliedEvent;
	
	/**
 	 * Called when a Gameplay Effect is Added to the ActiveGameplayEffectsContainer.  GE's are added to that container when they have duration (or are predicting locally).
 	 * Note: This also occurs as a result of replication (e.g. when the server replicates a GE to the owning client -- including the 'duplicate' GE after a prediction).
 	 * Return if the effect should remain active, or false to inhibit.  Note: Inhibit does not remove the effect (it remains added but dormant, waiting to uninhibit).
 	 */
	virtual bool OnActiveGameplayEffectAdded(FActiveGameplayEffectsContainer& ActiveGEContainer, FActiveGameplayEffect& ActiveGE) const override;

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
