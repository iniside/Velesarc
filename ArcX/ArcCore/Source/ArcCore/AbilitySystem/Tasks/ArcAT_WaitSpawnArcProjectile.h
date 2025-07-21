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

#include "ArcCore/AbilitySystem/Tasks/ArcAbilityTask.h"
#include "ArcAT_WaitSpawnArcProjectile.generated.h"

class ArcCoreAbilitySystemComponent;
class AArcAbilityProjectile;
class UArcActorGameplayAbility;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitSpawnAuProjectileDelegate
	, AArcAbilityProjectile*
	, SpawnedActor);

/**
 *
 */
UCLASS()
class ARCCORE_API UArcAT_WaitSpawnArcProjectile : public UArcAbilityTask
{
	GENERATED_BODY()

protected:
	TSubclassOf<UArcActorGameplayAbility> ActorAbility;
	
	UPROPERTY(Transient)
	FTransform SpawnTransform;

	FVector StartVelocity;

	float GravityScale;

	UPROPERTY()
	TObjectPtr<AArcAbilityProjectile> AbilityProjectile;


	FDelegateHandle DelegateHandle;

public:
	/** Called when we can't spawn: on clients or potentially on server if they fail to
	 * spawn (rare) */
	UPROPERTY(BlueprintAssignable)
	FWaitSpawnAuProjectileDelegate DidNotSpawn;

	/*
	 * Called only on server or in standalone game.
	 */
	UPROPERTY(BlueprintAssignable)
	FWaitSpawnAuProjectileDelegate ServerSpawned;


	/**
	 * @brief Task which spawns actor projectile along with ability granted to instigator which will manage that projectile
	 * functionality.
	 * Projectile is only spawned on server.
	 * @param TaskName Name of task. Optional
	 * @param OwningAbility Hidden
	 * @param InStartVelocity Initial velocity for projectile actor
	 * @param InSpawnTransform Where projectile will be spawned
	 * @param InActorAbility 
	 * @param InActorClass Actor to spawn 
	 * @return 
	 */
	UFUNCTION(BlueprintCallable
		, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true")
		, Category = "Arc Core|Ability|Tasks")
	static UArcAT_WaitSpawnArcProjectile* SpawnArcProjectile(FName TaskName
															 , UGameplayAbility* OwningAbility
															 , FVector InStartVelocity
															 , float InGravityScale
															 , FTransform InSpawnTransform
															 , TSubclassOf<UArcActorGameplayAbility> InActorAbility
															 , TSubclassOf<AArcAbilityProjectile> InActorClass);

	UArcAT_WaitSpawnArcProjectile(const FObjectInitializer& ObjectInitializer);

	virtual void Activate() override;

	virtual void OnDestroy(bool AbilityEnded) override;

protected:
	UFUNCTION(BlueprintCallable
		, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true")
		, Category = "Abilities")
	bool BeginSpawningActor(UGameplayAbility* OwningAbility
							, FGameplayAbilityTargetDataHandle TargetData
							, TSubclassOf<UArcActorGameplayAbility> InActorAbility
							, TSubclassOf<AArcAbilityProjectile> InActorClass
							, AArcAbilityProjectile*& SpawnedActor);

	UFUNCTION(BlueprintCallable
		, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true")
		, Category = "Abilities")
	void FinishSpawningActor(UGameplayAbility* OwningAbility
							 , FGameplayAbilityTargetDataHandle TargetData
							 , TSubclassOf<UArcActorGameplayAbility> InAbilityEffect
							 , AArcAbilityProjectile* SpawnedActor);

};
