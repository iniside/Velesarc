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

#include "Abilities/Tasks/AbilityTask.h"
#include "ArcCore/AbilitySystem/Tasks/ArcAbilityTask.h"


#include "ArcAT_WaitSpawnAbilityActor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitSpawnAbilityActorDelegate
	, AActor*
	, SpawnedActor);

/**
 * Ability task that spawns an actor using deferred spawning.
 * After BeginSpawningActor, the caller can configure the actor before FinishSpawningActor completes it.
 */
UCLASS()
class ARCCORE_API UArcAT_WaitSpawnAbilityActor : public UArcAbilityTask
{
	GENERATED_BODY()

private:
	bool bAllowOnNonAuthority = false;

	FGameplayAbilityTargetDataHandle CachedTargetDataHandle;

	UPROPERTY(BlueprintAssignable)
	FWaitSpawnAbilityActorDelegate Success;

	UPROPERTY(BlueprintAssignable)
	FWaitSpawnAbilityActorDelegate DidNotSpawn;

	bool SpawnUnderTargetActor = false;

public:
	UFUNCTION(BlueprintCallable
		, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true")
		, Category = "Arc Core|Ability|Tasks")
	static UArcAT_WaitSpawnAbilityActor* WaitSpawnAbilityActor(UGameplayAbility* OwningAbility
															   , FGameplayAbilityTargetDataHandle TargetData
															   , TSubclassOf<AActor> Class
															   , bool InbAllowOnNonAuthority = false
															   , bool bSpawnUnderTargetActor = true);

	UFUNCTION(BlueprintCallable
		, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true")
		, Category = "Abilities")
	bool BeginSpawningActor(UGameplayAbility* OwningAbility
							, FGameplayAbilityTargetDataHandle TargetData
							, TSubclassOf<AActor> Class
							, bool InbAllowOnNonAuthority
							, bool bSpawnUnderTargetActor
							, AActor*& SpawnedActor);

	UFUNCTION(BlueprintCallable
		, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true")
		, Category = "Abilities")
	void FinishSpawningActor(UGameplayAbility* OwningAbility
							 , FGameplayAbilityTargetDataHandle TargetData
							 , AActor* SpawnedActor);
};
