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



#include "ArcGunBFL.h"

#include "GameplayEffectExtension.h"
#include "GameplayEffectExecutionCalculation.h"
#include "AbilitySystemComponent.h"

#include "AbilitySystemGlobals.h"
#include "ArcGunStateComponent.h"

#include "AbilitySystem/Actors/ArcActorGameplayAbility.h"

#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsComponent.h"
#include "Items/ArcMapAbilityEffectSpecContainer.h"

#include "AbilitySystem/ArcAbilitiesTypes.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_AbilityEffectsToApply.h"

void UArcGunBFL::BroadcastGunShootEvent(UGameplayAbility* Ability
										, const FGameplayAbilityTargetDataHandle& InHitResults
										, class UArcTargetingObject* InTrace)
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	
	UArcGunStateComponent* WC = UArcGunStateComponent::FindGunStateComponent(ActorInfo->OwnerActor.Get());
	if(WC == nullptr)
	{
		return;
	}
	
	WC->ShootGun(Ability, InHitResults, InTrace);
}

void UArcGunBFL::ShootGunApplyDamageEffectSpec(
	UGameplayAbility* Ability
  , const FGameplayTag& EffectTag
  , const FGameplayAbilityTargetDataHandle& InHitResults
  , class UArcTargetingObject* InTrace)
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	UArcGunStateComponent* WC = UArcGunStateComponent::FindGunStateComponent(ActorInfo->OwnerActor.Get());
	if (WC == nullptr)
	{
		return;
	}
	if (WC->GetOwner()->HasAuthority())
	{
		TArray<const FArcEffectSpecItem*> Effects = WC->GetAmmoGameplayEffectSpecs(EffectTag);
		
		FGameplayAbilityActivationInfo ActivInfo = Ability->GetCurrentActivationInfo();

		FGameplayAbilityTargetDataHandle TargetHandle = InHitResults;

		for (const FArcEffectSpecItem* SpecItem : Effects)
		{
			for (const FGameplayEffectSpecHandle& Effect : SpecItem->Specs)
			{
				FPredictionKey PredKey = Ability->GetCurrentActivationInfo().GetActivationPredictionKey();
				bool IsValid = PredKey.IsValidForMorePrediction();

				if (!IsValid && UAbilitySystemGlobals::Get().ShouldPredictTargetGameplayEffects() == false)
				{
					// Early out to avoid making effect specs that we can't apply
					return;// EffectHandles;
				}

				if (Effect == nullptr)
				{
					//ABILITY_LOG(Error, TEXT("ApplyGameplayEffectToTarget called on ability %s with no GameplayEffect."), *GetName());
				}
				else if (Ability->HasAuthorityOrPredictionKey(ActorInfo, &ActivInfo) || IsValid)
				{
					Effect.Data->GetContext().Get()->SetAbility(Ability);
					//ArcAbility::SetTargetDataHandle(Effect.Data->GetContext().Get(), InHitResults);
					ArcAbility::ApplyEffectSpecTargetHandle(TargetHandle, *Effect.Data.Get(), PredKey);
				}
			}
		}
	}
	WC->ShootGun(Ability, InHitResults, InTrace);
}

void UArcGunBFL::ShootGunApplyDamageEffectSpecFromItem(
      UGameplayAbility* Ability
	, const FArcItemId& InItem
    , const FGameplayTag& EffectTag
    , const FGameplayAbilityTargetDataHandle& InHitResults
    , UArcTargetingObject* InTrace)
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	UArcGunStateComponent* WC = UArcGunStateComponent::FindGunStateComponent(ActorInfo->OwnerActor.Get());

	if (WC == nullptr)
	{
		return;
	}
	
	if (WC->GetOwner()->HasAuthority())
	{
		const FArcItemData* Item = WC->GetItemsComponent()->GetItemPtr(InItem);
		if (Item == nullptr)
		{
			return;
		}

		const FArcItemInstance_EffectToApply* Instance = ArcItems::FindInstance<FArcItemInstance_EffectToApply>(Item);
		if (Instance == nullptr)
		{
			return;
		}
		
		TArray<const FArcEffectSpecItem*> Effects = Instance->GetEffectSpecHandles(EffectTag);

		FGameplayAbilityActivationInfo ActivInfo = Ability->GetCurrentActivationInfo();

		FGameplayAbilityTargetDataHandle TargetHandle = InHitResults;

		for (const FArcEffectSpecItem* SpecItem : Effects)
		{
			for (const FGameplayEffectSpecHandle& Effect : SpecItem->Specs)
			{
				FPredictionKey PredKey = Ability->GetCurrentActivationInfo().GetActivationPredictionKey();
				bool IsValid = PredKey.IsValidForMorePrediction();

				if (!IsValid && UAbilitySystemGlobals::Get().ShouldPredictTargetGameplayEffects() == false)
				{
					// Early out to avoid making effect specs that we can't apply
					return;// EffectHandles;
				}

				if (Effect == nullptr)
				{
					//ABILITY_LOG(Error, TEXT("ApplyGameplayEffectToTarget called on ability %s with no GameplayEffect."), *GetName());
				}
				else if (Ability->HasAuthorityOrPredictionKey(ActorInfo, &ActivInfo) || IsValid)
				{
					Effect.Data->GetContext().Get()->SetAbility(Ability);
					//ArcAbility::SetTargetDataHandle(Effect.Data->GetContext().Get(), InHitResults);
					ArcAbility::ApplyEffectSpecTargetHandle(TargetHandle, *Effect.Data.Get(), PredKey);
				}
			}
		}
	}
	WC->ShootGun(Ability, InHitResults, InTrace);
}

void UArcGunBFL::ShootGunApplyDamageEffectSpecFromSlot(UGameplayAbility* Ability
  , const FGameplayTag& InSlot
  , const FGameplayTag& EffectTag
  , const FGameplayAbilityTargetDataHandle& InHitResults
  , UArcTargetingObject* InTrace)
{
	if (InSlot.IsValid() == false)
	{
		return;
	}
	
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	UArcCoreGameplayAbility* ArcGA = Cast<UArcCoreGameplayAbility>(Ability);
	
	UArcGunStateComponent* WC = UArcGunStateComponent::FindGunStateComponent(ActorInfo->OwnerActor.Get());
	UArcItemsStoreComponent* ItemsStore = ArcGA->GetSourceItemStore();
	
	if (WC == nullptr)
	{
		return;
	}
	
	if (WC->GetOwner()->HasAuthority())
	{
		const FArcItemData* Item = ArcGA->GetSourceItemEntryPtr();
		const FArcItemData* SlotItem = ItemsStore->GetItemFromSlot(InSlot);
		
		const FArcItemInstance_EffectToApply* Instance = ArcItems::FindInstance<FArcItemInstance_EffectToApply>(SlotItem);

		if (Instance)
		{
			TArray<const FArcEffectSpecItem*> Effects = Instance->GetEffectSpecHandles(EffectTag);

			FGameplayAbilityActivationInfo ActivInfo = Ability->GetCurrentActivationInfo();

			FGameplayAbilityTargetDataHandle TargetHandle = InHitResults;
			
			for (const FArcEffectSpecItem* SpecItem : Effects)
			{
				for (const FGameplayEffectSpecHandle& Effect : SpecItem->Specs)
				{
					FPredictionKey PredKey = Ability->GetCurrentActivationInfo().GetActivationPredictionKey();
					bool IsValid = PredKey.IsValidForMorePrediction();

				
					if (!IsValid && UAbilitySystemGlobals::Get().ShouldPredictTargetGameplayEffects() == false)
					{
						// Early out to avoid making effect specs that we can't apply
						return;// EffectHandles;
					}

					if (Effect == nullptr)
					{
						//ABILITY_LOG(Error, TEXT("ApplyGameplayEffectToTarget called on ability %s with no GameplayEffect."), *GetName());
					}
					else if (Ability->HasAuthorityOrPredictionKey(ActorInfo, &ActivInfo) || IsValid)
					{
						ArcAbility::SetSourceItem(Effect.Data->GetContext().Get(), Item->GetItemId(), ItemsStore);
						Effect.Data->GetContext().Get()->SetAbility(Ability);
						//ArcAbility::SetTargetDataHandle(Effect.Data->GetContext().Get(), InHitResults);
						ArcAbility::ApplyEffectSpecTargetHandle(TargetHandle, *Effect.Data.Get(), PredKey);
					}
				}
			}
		}
	}
	WC->ShootGun(Ability, InHitResults, InTrace);
}

void UArcGunBFL::GunApplyDamageEffectSpec(
	UGameplayAbility* Ability
  , UGameplayAbility* ActualHitAbility
  , const FGameplayTag& EffectTag
  , const FGameplayAbilityTargetDataHandle& InHitResults
  , class UArcTargetingObject* InTrace)
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	
	//probabaly better pass as param and require it.
	UArcCoreGameplayAbility* AuAB = Cast<UArcCoreGameplayAbility>(Ability);

	TArray<FGameplayEffectSpecHandle> Effects = AuAB->GetEffectSpecFromItem(EffectTag);
	
	FGameplayAbilityActivationInfo ActivInfo = Ability->GetCurrentActivationInfo();

	FGameplayAbilityTargetDataHandle TargetHandle = InHitResults;

	for (FGameplayEffectSpecHandle& Effect : Effects)
	{
		TArray<FActiveGameplayEffectHandle> EffectHandles;
		FPredictionKey PredKey = Ability->GetCurrentActivationInfo().GetActivationPredictionKey();
		bool IsValid = PredKey.IsValidForMorePrediction();

		if (!IsValid && UAbilitySystemGlobals::Get().ShouldPredictTargetGameplayEffects() == false)
		{
			// Early out to avoid making effect specs that we can't apply
			return;// EffectHandles;
		}

		if (Effect == nullptr)
		{
			//ABILITY_LOG(Error, TEXT("ApplyGameplayEffectToTarget called on ability %s with no GameplayEffect."), *GetName());
		}
		else if (Ability->HasAuthorityOrPredictionKey(ActorInfo, &ActivInfo) || IsValid)
		{
			Effect.Data->GetContext().Get()->SetAbility(ActualHitAbility);
			//ArcAbility::SetTargetDataHandle(Effect.Data->GetContext().Get(), InHitResults);
			EffectHandles = ArcAbility::ApplyEffectSpecTargetHandle(TargetHandle, *Effect.Data.Get(), PredKey);
		}
	}
}
