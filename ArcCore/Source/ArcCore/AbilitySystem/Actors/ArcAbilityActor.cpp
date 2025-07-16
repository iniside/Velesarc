/**
 * This file is part of ArcX.
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

#include "ArcCore/AbilitySystem/Actors/ArcAbilityActor.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "ArcCore/AbilitySystem/Actors/ArcActorGameplayAbility.h"
#include "ArcCore/AbilitySystem/ArcCoreGameplayAbility.h"
#include "Components/PrimitiveComponent.h"

// Sets default values
AArcAbilityActor::AArcAbilityActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve
	// performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bAllowTickOnDedicatedServer = false;
	bReplicates = true;

	SetReplicateMovement(false);
}

// Called when the game starts or when spawned
void AArcAbilityActor::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AArcAbilityActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AArcAbilityActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
}

void AArcAbilityActor::Destroyed()
{
	Super::Destroyed();
}

float AArcAbilityActor::GetMass() const
{
	return PrimitiveRoot->GetMass();
}

void AArcAbilityActor::SetInfo(UArcCoreAbilitySystemComponent* InInstigatorASC
							   , UArcCoreGameplayAbility* InInstigatorAbility
							   , TSubclassOf<UArcActorGameplayAbility> InActorGrantedAbility)
{
	InstigatorAbility = InInstigatorAbility;
	InstigatorASC = InInstigatorASC;
	ActorGrantedAbility = InActorGrantedAbility;
}

void AArcAbilityActor::NativeOnActorHit(UPrimitiveComponent* HitComponent
										, AActor* OtherActor
										, UPrimitiveComponent* OtherComp
										, FVector NormalImpulse
										, const FHitResult& Hit)
{
	if (!bAllowOwnerHit && OtherActor->GetInstigator() == GetInstigator())
	{
		return;
	}

	OnActorHitEvent.Broadcast(HitComponent, OtherActor, OtherComp, NormalImpulse, Hit);
	
	OnActorHit(HitComponent
		, OtherActor
		, OtherComp
		, NormalImpulse
		, Hit);
}

void AArcAbilityActor::NativeOnActorOverlapStart(UPrimitiveComponent* OverlappedComponent
												 , AActor* OtherActor
												 , UPrimitiveComponent* OtherComp
												 , int32 OtherBodyIndex
												 , bool bFromSweep
												 , const FHitResult& SweepResult)
{
	if (!bAllowOwnerHit && OtherActor->GetInstigator() == GetInstigator())
	{
		return;
	}

	OnActorOverlapStartEvent.Broadcast(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
	
	OnActorOverlapStart(OverlappedComponent
		, OtherActor
		, OtherComp
		, OtherBodyIndex
		, bFromSweep
		, SweepResult);
}

void AArcAbilityActor::NativeOnActorOverlapEnd(UPrimitiveComponent* OverlappedComponent
											   , AActor* OtherActor
											   , UPrimitiveComponent* OtherComp
											   , int32 OtherBodyIndex)
{
	if (!bAllowOwnerHit && OtherActor->GetInstigator() == GetInstigator())
	{
		return;
	}

	OnActorOverlapEndEvent.Broadcast(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex);
	
	OnActorOverlapEnd(OverlappedComponent
		, OtherActor
		, OtherComp
		, OtherBodyIndex);
}
