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


#include "UObject/Object.h"
#include "ArcGunBFL.generated.h"

struct FArcItemId;
struct FGameplayTag;
class UGameplayAbility;
struct FGameplayAbilityTargetDataHandle;
class UArcTargetingObject;
struct FGameplayAbilitySpecHandle;

/**
 * @class UArcGunBFL
 * @brief This class provides blueprint function library for Arc Gun actions.
 *
 * This class contains functions related to shooting guns and applying damage effects.
 */
UCLASS()
class ARCGUN_API UArcGunBFL : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Weapon", meta = (HidePin = "Ability", DefaultToSelf = "Ability"))
	static void BroadcastGunShootEvent(
		UGameplayAbility* Ability
		, const FGameplayAbilityTargetDataHandle& InHitResults
		, class UArcTargetingObject* InTrace
	);

	/*
	 * This function will apply damage effects and call FireShot() function on weapon component broadcasting events.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Weapon", meta = (HidePin = "Ability", DefaultToSelf = "Ability"))
	static void ShootGunApplyDamageEffectSpec(
		UGameplayAbility* Ability
	  , const FGameplayTag& EffectTag
	  , const FGameplayAbilityTargetDataHandle& InHitResults
	  , class UArcTargetingObject* InTrace
	);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Weapon", meta = (HidePin = "Ability", DefaultToSelf = "Ability"))
	static void ShootGunApplyDamageEffectSpecFromItem(
		UGameplayAbility* Ability
		, const FArcItemId& InItem
		, const FGameplayTag& EffectTag
		, const FGameplayAbilityTargetDataHandle& InHitResults
		, class UArcTargetingObject* InTrace
	);

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Weapon", meta = (HidePin = "Ability", DefaultToSelf = "Ability"))
	static void ShootGunApplyDamageEffectSpecFromSlot(
		UGameplayAbility* Ability
		, const FGameplayTag& InSlot
		, const FGameplayTag& EffectTag
		, const FGameplayAbilityTargetDataHandle& InHitResults
		, class UArcTargetingObject* InTrace
	);

	 /**
	  * @brief Function will apply damage effects for weapon but will not call @link UArcGunStateComponent::ShootGun
	  * Only use that if you called BroadcastGunShootEvent somewhere else are now responding for ShootGun event.
	  * @param Ability Ability which contains this function
	  * @param ActualHitAbility Ability from which event came
	  * @param EffectTag TAg of effects in item to apply
	  * @param InHitResults Hits to which apply effects
	  * @param TargetingHandle Targeting handle to make local traces on remotes
	  * @param CustomData custom targeting data.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Weapon", meta = (HidePin = "Ability", DefaultToSelf = "Ability"))
	static void GunApplyDamageEffectSpec(
		UGameplayAbility* Ability
		, UGameplayAbility* ActualHitAbility
		, const FGameplayTag& EffectTag
		, const FGameplayAbilityTargetDataHandle& InHitResults
		, class UArcTargetingObject* InTrace
	);
};
