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


#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "ArcAbilityActor.generated.h"

class UArcActorGameplayAbility;
class UArcCoreAbilitySystemComponent;
class UArcCoreGameplayAbility;

/*
 * Base actor used with abilities which want to have persence in world.
 * The spawn if backward. We spawn actor frist, and then actor grant ability to owner (ie.
 * player). This ability manages actor events and life time.
 */
UCLASS()
class ARCCORE_API AArcAbilityActor : public AActor
{
	GENERATED_BODY()

private:
	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> PrimitiveRoot;

	/** Handle to ability which manages this actor. */
	FGameplayAbilitySpecHandle GrantedAbility;

	/** Calls of ability which manages this actor and which will be granted to instigator of this actor. */
	UPROPERTY()
	TSubclassOf<UArcActorGameplayAbility> ActorGrantedAbility;

	/** Ability system of original instigator of this actor. */
	UPROPERTY()
	TObjectPtr<UArcCoreAbilitySystemComponent> InstigatorASC;

	/** Ability which instigated creation of this actor. */
	UPROPERTY()
	TObjectPtr<UArcCoreGameplayAbility> InstigatorAbility;

	UPROPERTY(EditAnywhere)
	bool bAllowOwnerHit = false;

	UPROPERTY(EditAnywhere)
	bool bReportCollisionFromClient;
	
public:
	// Sets default values for this actor's properties
	AArcAbilityActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void PostInitializeComponents() override;

	virtual void Destroyed() override;

	UFUNCTION(BlueprintPure, Category = "Arc Core|Ability")
	float GetMass() const;

	void SetInfo(UArcCoreAbilitySystemComponent* InInstigatorASC
				 , UArcCoreGameplayAbility* InInstigatorAbility
				 , TSubclassOf<UArcActorGameplayAbility> InActorGrantedAbility);

	class UArcCoreGameplayAbility* GetInstigatorAbility() const
	{
		return InstigatorAbility;
	}

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Arc Core Ability")
	void OnActorHit(UPrimitiveComponent* HitComponent
					, AActor* OtherActor
					, UPrimitiveComponent* OtherComp
					, FVector NormalImpulse
					, const FHitResult& Hit);

	UFUNCTION(BlueprintImplementableEvent, Category = "Arc Core Ability")
	void OnActorOverlapStart(UPrimitiveComponent* OverlappedComponent
							 , AActor* OtherActor
							 , UPrimitiveComponent* OtherComp
							 , int32 OtherBodyIndex
							 , bool bFromSweep
							 , const FHitResult& SweepResult);

	UFUNCTION(BlueprintImplementableEvent, Category = "Arc Core Ability")
	void OnActorOverlapEnd(UPrimitiveComponent* OverlappedComponent
						   , AActor* OtherActor
						   , UPrimitiveComponent* OtherComp
						   , int32 OtherBodyIndex);

	FArcAbilityActorOverlapStartEvent OnActorOverlapStartEvent;
	FArcAbilityActorOverlapEndEvent OnActorOverlapEndEvent;
	FArcAbilityActorHitEvent OnActorHitEvent;
	
	UFUNCTION()
	virtual void NativeOnActorHit(UPrimitiveComponent* HitComponent
								  , AActor* OtherActor
								  , UPrimitiveComponent* OtherComp
								  , FVector NormalImpulse
								  , const FHitResult& Hit);

	UFUNCTION()
	virtual void NativeOnActorOverlapStart(UPrimitiveComponent* OverlappedComponent
										   , AActor* OtherActor
										   , UPrimitiveComponent* OtherComp
										   , int32 OtherBodyIndex
										   , bool bFromSweep
										   , const FHitResult& SweepResult);

	UFUNCTION()
	virtual void NativeOnActorOverlapEnd(UPrimitiveComponent* OverlappedComponent
										 , AActor* OtherActor
										 , UPrimitiveComponent* OtherComp
										 , int32 OtherBodyIndex);
};
