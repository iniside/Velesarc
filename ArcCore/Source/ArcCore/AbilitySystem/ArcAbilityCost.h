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

#include "ArcAbilityCost.generated.h"

class UArcCoreGameplayAbility;
struct FGameplayAbilityActorInfo;
struct FGameplayTagContainer;
struct FGameplayAbilitySpecHandle;
struct FGameplayAbilityActivationInfo;

/**
 *
 */
USTRUCT()
struct ARCCORE_API FArcAbilityCost
{
	GENERATED_BODY()
	/**
	 * Checks if we can afford this cost.
	 *
	 * A failure reason tag can be added to OptionalRelevantTags (if non-null), which can
	 * be queried elsewhere to determine how to provide user feedback (e.g., a clicking
	 * noise if a weapon is out of ammo)
	 *
	 * Ability and ActorInfo are guaranteed to be non-null on entry, but
	 * OptionalRelevantTags can be nullptr.
	 *
	 * @return true if we can pay for the ability, false otherwise.
	 */
	virtual bool CheckCost(const class UArcCoreGameplayAbility* Ability
						   , const FGameplayAbilitySpecHandle& Handle
						   , const FGameplayAbilityActorInfo* ActorInfo
						   , FGameplayTagContainer* OptionalRelevantTags) const
	{
		return true;
	}

	/**
	 * Applies the ability's cost to the target
	 *
	 * Notes:
	 * - Your implementation don't need to check ShouldOnlyApplyCostOnHit(), the caller
	 * does that for you.
	 * - Ability and ActorInfo are guaranteed to be non-null on entry.
	 */
	virtual void ApplyCost(const class UArcCoreGameplayAbility* Ability
						   , const FGameplayAbilitySpecHandle& Handle
						   , const FGameplayAbilityActorInfo* ActorInfo
						   , const FGameplayAbilityActivationInfo& ActivationInfo) const
	{
	}

	virtual ~FArcAbilityCost() = default;
};
