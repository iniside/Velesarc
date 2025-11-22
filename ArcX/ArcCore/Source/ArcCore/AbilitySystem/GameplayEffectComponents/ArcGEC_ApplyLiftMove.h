// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcLayeredMove_Lift.h"
#include "StateTreeEvents.h"
#include "GameplayEffectComponents/AbilitiesGameplayEffectComponent.h"
#include "ArcGEC_ApplyLiftMove.generated.h"

/**
 * 
 */
UCLASS()
class ARCCORE_API UArcGEC_ApplyLiftMove : public UGameplayEffectComponent
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere)
	FArcLayeredMove_Lift Settings;
	
	/**
  	 * Called when a Gameplay Effect is Added to the ActiveGameplayEffectsContainer.  GE's are added to that container when they have duration (or are predicting locally).
  	 * Note: This also occurs as a result of replication (e.g. when the server replicates a GE to the owning client -- including the 'duplicate' GE after a prediction).
  	 * Return if the effect should remain active, or false to inhibit.  Note: Inhibit does not remove the effect (it remains added but dormant, waiting to uninhibit).
  	 */
	virtual bool OnActiveGameplayEffectAdded(FActiveGameplayEffectsContainer& ActiveGEContainer, FActiveGameplayEffect& ActiveGE) const override;

private:
	/** We must undo all effects when removed */
	void OnActiveGameplayEffectRemoved(const FGameplayEffectRemovalInfo& RemovalInfo) const;
};
