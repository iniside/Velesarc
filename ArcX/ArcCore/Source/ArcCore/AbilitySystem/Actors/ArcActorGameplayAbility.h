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

#include "ArcCore/AbilitySystem/ArcPassiveGameplayAbility.h"
#include "ArcActorGameplayAbility.generated.h"

class UPrimitiveComponent;
class AActor;

/**
 * This ability expects SourceObject to be actor.
 * Once activated it tries to bind various collision events from Source Actor, and exposes them as events back to blueprints.
 *
 * This ability should live on Instigator of actor, not actor itself.
 */
UCLASS()
class ARCCORE_API UArcActorGameplayAbility : public UArcCoreGameplayAbility
{
	GENERATED_BODY()

private:
	/** Cached pointer to RootComponent of Source Actor. */
	TWeakObjectPtr<UPrimitiveComponent> RootComponent = nullptr;

	/** Cached pointer to actor, which this ability is using as Source Object. */
	TWeakObjectPtr<AActor> SpawnedAbilityActor = nullptr;

	/** If true will ignore any collision with instigator of this ability. */
	UPROPERTY(EditDefaultsOnly, Category = "Actor")
	uint8 bIgnoreOwnerOnCollision : 1;

	/** If true ability will self remove on any collision or overlap. */
	UPROPERTY(EditDefaultsOnly, Category = "Actor")
	uint8 bRemoveOnCollision : 1;

public:
	UArcActorGameplayAbility();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle
								 , const FGameplayAbilityActorInfo* ActorInfo
								 , const FGameplayAbilityActivationInfo ActivationInfo
								 , const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle
							, const FGameplayAbilityActorInfo* ActorInfo
							, const FGameplayAbilityActivationInfo ActivationInfo
							, bool bReplicateEndAbility
							, bool bWasCancelled) override;

	UFUNCTION()
	virtual void NativeSpawnedActorOverlapBeginEvent(UPrimitiveComponent* OverlappedComponent
													 , AActor* OtherActor
													 , UPrimitiveComponent* OtherComp
													 , int32 OtherBodyIndex
													 , bool bFromSweep
													 , const FHitResult& SweepResult);

	UFUNCTION()
	virtual void NativeSpawnedActorOverlapEndEvent(UPrimitiveComponent* OverlappedComponent
												   , AActor* OtherActor
												   , UPrimitiveComponent* OtherComp
												   , int32 OtherBodyIndex);

	UFUNCTION()
	virtual void NativeSpawnedActorOverlapHitEvent(UPrimitiveComponent* HitComponent
												   , AActor* OtherActor
												   , UPrimitiveComponent* OtherComp
												   , FVector NormalImpulse
												   , const FHitResult& Hit);

	UFUNCTION()
	virtual void NativeSpawnedActorDestroyed(AActor* DestroyedActor);

	UFUNCTION(BlueprintImplementableEvent, Category = "Arc Core")
	void OnSpawnedActorDestroyed(AActor* DestroyedActor);

	UFUNCTION(BlueprintImplementableEvent, Category = "Arc Core")
	void OnSpawnedActorOverlapBeginEvent(AActor* Owner
										 , UPrimitiveComponent* OverlappedComponent
										 , AActor* OtherActor
										 , UPrimitiveComponent* OtherComp
										 , int32 OtherBodyIndex
										 , bool bFromSweep
										 , const FHitResult& SweepResult);

	UFUNCTION(BlueprintImplementableEvent, Category = "Arc Core")
	void OnSpawnedActorOverlapEndEvent(AActor* Owner
									   , UPrimitiveComponent* OverlappedComponent
									   , AActor* OtherActor
									   , UPrimitiveComponent* OtherComp
									   , int32 OtherBodyIndex);

	UFUNCTION(BlueprintImplementableEvent, Category = "Arc Core")
	void OnSpawnedActorOverlapHitEvent(AActor* Owner
									   , UPrimitiveComponent* HitComponent
									   , AActor* OtherActor
									   , UPrimitiveComponent* OtherComp
									   , FVector NormalImpulse
									   , const FHitResult& Hit);

	UFUNCTION(BlueprintPure, Category = "Arc Core|Ability")
	FVector GetActorLocation() const;

	UFUNCTION(BlueprintPure, Category = "Arc Core|Ability")
	AActor* GetAbilityActor() const;

	UFUNCTION(BlueprintPure, Category = "Arc Core|Ability")
	UPrimitiveComponent* GetRootComponent() const;
private:
	void ClearDelegates();
};
