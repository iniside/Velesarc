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

#include "ArcCore/AbilitySystem/Actors/ArcActorGameplayAbility.h"
#include "UObject/Object.h"

#include "GameplayTagContainer.h"

#include "ArcCore/AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "ArcCore/Items/ArcItemsComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

UArcActorGameplayAbility::UArcActorGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::Type::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::Type::ServerOnly;
}

void UArcActorGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle
											   , const FGameplayAbilityActorInfo* ActorInfo
											   , const FGameplayAbilityActivationInfo ActivationInfo
											   , const FGameplayEventData* TriggerEventData)
{
	
	UArcAbilityActorComponent* Causer = Cast<UArcAbilityActorComponent>(GetCurrentAbilitySpec()->SourceObject);
	SpawnedAbilityActor = Causer->GetOwner();
	
	//Causer->OnDestroyed.AddDynamic(this, &UArcActorGameplayAbility::NativeSpawnedActorDestroyed);
	
	
		RootComponent = Cast<UPrimitiveComponent>(SpawnedAbilityActor->GetRootComponent());
		Causer->OnActorOverlapStartEvent.AddDynamic(this, &UArcActorGameplayAbility::NativeSpawnedActorOverlapBeginEvent);
		Causer->OnActorOverlapEndEvent.AddDynamic(this, &UArcActorGameplayAbility::NativeSpawnedActorOverlapEndEvent);
		Causer->OnActorHitEvent.AddDynamic(this, &UArcActorGameplayAbility::NativeSpawnedActorOverlapHitEvent);
	

	Super::ActivateAbility(Handle
		, ActorInfo
		, ActivationInfo
		, TriggerEventData);
}

void UArcActorGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle
										  , const FGameplayAbilityActorInfo* ActorInfo
										  , const FGameplayAbilityActivationInfo ActivationInfo
										  , bool bReplicateEndAbility
										  , bool bWasCancelled)
{
	Super::EndAbility(Handle
		, ActorInfo
		, ActivationInfo
		, bReplicateEndAbility
		, bWasCancelled);

	ClearDelegates();
	
	if (RootComponent.IsValid())
	{
		AActor* Owner = RootComponent->GetOwner();
		Owner->SetActorHiddenInGame(true);
		Owner->SetActorEnableCollision(false);
		Owner->Destroy();
	}
}

void UArcActorGameplayAbility::NativeSpawnedActorOverlapBeginEvent(UPrimitiveComponent* OverlappedComponent
																   , AActor* OtherActor
																   , UPrimitiveComponent* OtherComp
																   , int32 OtherBodyIndex
																   , bool bFromSweep
																   , const FHitResult& SweepResult)
{
	if (GetAvatarActorFromActorInfo() != OtherActor)
	{
		OnSpawnedActorOverlapBeginEvent(RootComponent->GetOwner()
			, OverlappedComponent
			, OtherActor
			, OtherComp
			, OtherBodyIndex
			, bFromSweep
			, SweepResult);
		if (bRemoveOnCollision)
		{
			ClearDelegates();
		}
	}
	else if (bIgnoreOwnerOnCollision)
	{
		OnSpawnedActorOverlapBeginEvent(RootComponent->GetOwner()
			, OverlappedComponent
			, OtherActor
			, OtherComp
			, OtherBodyIndex
			, bFromSweep
			, SweepResult);
		if (bRemoveOnCollision)
		{
			ClearDelegates();
		}
	}
}

void UArcActorGameplayAbility::NativeSpawnedActorOverlapEndEvent(UPrimitiveComponent* OverlappedComponent
																 , AActor* OtherActor
																 , UPrimitiveComponent* OtherComp
																 , int32 OtherBodyIndex)
{
	if (GetAvatarActorFromActorInfo() != OtherActor)
	{
		OnSpawnedActorOverlapEndEvent(RootComponent->GetOwner()
			, OverlappedComponent
			, OtherActor
			, OtherComp
			, OtherBodyIndex);
		if (bRemoveOnCollision)
		{
			ClearDelegates();
		}
	}
	else if (bIgnoreOwnerOnCollision)
	{
		OnSpawnedActorOverlapEndEvent(RootComponent->GetOwner()
			, OverlappedComponent
			, OtherActor
			, OtherComp
			, OtherBodyIndex);
		if (bRemoveOnCollision)
		{
			ClearDelegates();
		}
	}
}

void UArcActorGameplayAbility::NativeSpawnedActorOverlapHitEvent(UPrimitiveComponent* HitComponent
																 , AActor* OtherActor
																 , UPrimitiveComponent* OtherComp
																 , FVector NormalImpulse
																 , const FHitResult& Hit)
{
	if (GetAvatarActorFromActorInfo() != OtherActor)
	{
		OnSpawnedActorOverlapHitEvent(RootComponent->GetOwner()
			, HitComponent
			, OtherActor
			, OtherComp
			, NormalImpulse
			, Hit);
		if (bRemoveOnCollision)
		{
			ClearDelegates();
		}
	}
	else if (bIgnoreOwnerOnCollision)
	{
		OnSpawnedActorOverlapHitEvent(RootComponent->GetOwner()
			, HitComponent
			, OtherActor
			, OtherComp
			, NormalImpulse
			, Hit);
		if (bRemoveOnCollision)
		{
			ClearDelegates();
		}
	}
}

void UArcActorGameplayAbility::NativeSpawnedActorDestroyed(AActor* DestroyedActor)
{
	OnSpawnedActorDestroyed(DestroyedActor);
	ClearDelegates();
}

void UArcActorGameplayAbility::ClearDelegates()
{
	GetAbilitySystemComponentFromActorInfo()->ClearAbility(GetCurrentAbilitySpecHandle());
	
	if (!RootComponent.IsValid())
	{
		return;
	}
	RootComponent->OnComponentBeginOverlap.RemoveDynamic(this, &UArcActorGameplayAbility::NativeSpawnedActorOverlapBeginEvent);
	RootComponent->OnComponentEndOverlap.RemoveDynamic(this, &UArcActorGameplayAbility::NativeSpawnedActorOverlapEndEvent);
	RootComponent->OnComponentHit.RemoveDynamic(this, &UArcActorGameplayAbility::NativeSpawnedActorOverlapHitEvent);

	AActor* Causer = RootComponent->GetOwner();
	if (Causer)
	{
		Causer->OnDestroyed.RemoveDynamic(this, &UArcActorGameplayAbility::NativeSpawnedActorDestroyed);
	}

	
}

FVector UArcActorGameplayAbility::GetActorLocation() const
{
	return RootComponent->GetComponentLocation();
}

AActor* UArcActorGameplayAbility::GetAbilityActor() const
{
	return SpawnedAbilityActor.Get();
}

UPrimitiveComponent* UArcActorGameplayAbility::GetRootComponent() const
{
	return RootComponent.Get();
}
