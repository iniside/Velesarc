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

#include "ArcAbilityAction_SpawnProjectileFromContext.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "AbilitySystem/ArcItemGameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Items/ArcItemDefinition.h"

void FArcAbilityAction_SpawnProjectileFromContext::Execute(FArcAbilityActionContext& Context)
{
	UArcCoreAbilitySystemComponent* ArcASC = Context.Ability->GetArcASC();
	if (!ArcASC) { return; }

	if (!OriginTargetingTag.IsValid()) { return; }

	// Resolve actor class — direct property or item fragment fallback
	TSubclassOf<AActor> ResolvedClass = ActorClass;
	if (!ResolvedClass)
	{
		if (UArcItemGameplayAbility* ItemAbility = Cast<UArcItemGameplayAbility>(Context.Ability))
		{
			if (const UArcItemDefinition* ItemDef = ItemAbility->NativeGetSourceItemData())
			{
				if (const auto* Fragment = ItemDef->FindFragment<FArcItemFragment_ProjectileSettings>())
				{
					ResolvedClass = Fragment->ActorClass;
				}
			}
		}
	}

	if (!ResolvedClass) { return; }

	// Get spawn origin from global targeting
	bool bOriginSuccess = false;
	FHitResult OriginHit = UArcCoreAbilitySystemComponent::GetGlobalHitResult(ArcASC, OriginTargetingTag, bOriginSuccess);
	if (!bOriginSuccess) { return; }

	// Get target location from context target data
	if (!Context.TargetData.IsValid(0)) { return; }

	TOptional<FVector> TargetLocation;
	if (Context.TargetData.Get(0)->HasEndPoint())
	{
		TargetLocation = Context.TargetData.Get(0)->GetEndPoint();
	}
	else if (Context.TargetData.Get(0)->HasHitResult())
	{
		TargetLocation = Context.TargetData.Get(0)->GetHitResult()->ImpactPoint;
	}

	// Build target data from origin hit result for spawn location
	FGameplayAbilityTargetDataHandle SpawnTargetData;
	SpawnTargetData.Add(new FGameplayAbilityTargetData_SingleTargetHit(OriginHit));

	Context.Ability->SpawnAbilityActor(ResolvedClass
		, SpawnTargetData
		, TargetLocation
		, EArcAbilityActorSpawnOrigin::ImpactPoint);
}
