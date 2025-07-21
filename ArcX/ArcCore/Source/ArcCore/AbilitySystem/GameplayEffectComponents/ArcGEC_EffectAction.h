/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */



#pragma once

#include "StructUtils/InstancedStruct.h"
#include "GameplayEffectComponent.h"
#include "GameplayEffectTypes.h"
#include "ArcGEC_EffectAction.generated.h"

USTRUCT()
struct ARCCORE_API FArcActiveGameplayEffectAction
{
	GENERATED_BODY()

	FActiveGameplayEffectHandle MyHandle;
public:
	virtual void OnActiveGameplayEffectAdded(FActiveGameplayEffectsContainer& ActiveGEContainer, FActiveGameplayEffect& ActiveGE) { }

	virtual void OnActiveGameplayEffectRemoved(const FGameplayEffectRemovalInfo& RemovalInfo) {}
};
/**
 * 
 */
UCLASS()
class ARCCORE_API UArcGEC_EffectAction : public UGameplayEffectComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Actions", meta = (BaseStruct = "/Script/ArcCore.ArcActiveGameplayEffectAction"))
	TArray<FInstancedStruct> EffectActions;
	
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

USTRUCT()
struct ARCCORE_API FArcGEA_WaitAbilityActivateApplyEffect : public FArcActiveGameplayEffectAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FGameplayTag RequiredTag;

	UPROPERTY(EditAnywhere)
	TSubclassOf<UGameplayEffect> EffectToApply;

	TWeakObjectPtr<UAbilitySystemComponent> OwningASC;
	
	FDelegateHandle OnAbilityActivateDelegateHandle;

	FGameplayEffectContextHandle OriginalContext;
	
public:
	virtual void OnActiveGameplayEffectAdded(FActiveGameplayEffectsContainer& ActiveGEContainer, FActiveGameplayEffect& ActiveGE) override;

	virtual void OnActiveGameplayEffectRemoved(const FGameplayEffectRemovalInfo& RemovalInfo) override;

	void OnAbilityActivate(UGameplayAbility* ActivatedAbility);
};