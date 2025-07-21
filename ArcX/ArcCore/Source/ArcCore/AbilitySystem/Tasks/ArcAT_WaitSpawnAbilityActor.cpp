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

#include "ArcCore/AbilitySystem/Tasks/ArcAT_WaitSpawnAbilityActor.h"

#include "AbilitySystemComponent.h"
#include "ArcCore/AbilitySystem/Actors/ArcAbilityActor.h"
#include "ArcCore/AbilitySystem/ArcCoreGameplayAbility.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UArcAT_WaitSpawnAbilityActor* UArcAT_WaitSpawnAbilityActor::WaitSpawnAbilityActor(UGameplayAbility* OwningAbility
																				  , TSubclassOf<UArcActorGameplayAbility> InActorGrantedAbility
																				  , FGameplayAbilityTargetDataHandle TargetData
																				  , TSubclassOf<AActor> Class
																				  , EArcActorSpawnMode InSpawnMode
																				  , bool InbAllowOnNonAuthority
																				  , bool bSpawnUnderTargetActor)
{
	UArcAT_WaitSpawnAbilityActor* MyObj = NewAbilityTask<UArcAT_WaitSpawnAbilityActor>(OwningAbility);
	MyObj->CachedTargetDataHandle = MoveTemp(TargetData);
	MyObj->ActorGrantedAbility = InActorGrantedAbility;
	MyObj->bAllowOnNonAuthority = InbAllowOnNonAuthority;
	MyObj->SpawnUnderTargetActor = bSpawnUnderTargetActor;
	MyObj->SpawnMode = InSpawnMode;
	
	return MyObj;
}

// ---------------------------------------------------------------------------------------

bool UArcAT_WaitSpawnAbilityActor::BeginSpawningActor(UGameplayAbility* OwningAbility
													  , TSubclassOf<UArcActorGameplayAbility> InAbilityEffect
													  , FGameplayAbilityTargetDataHandle TargetData
													  , TSubclassOf<AActor> Class
													  , EArcActorSpawnMode InSpawnMode
													  , bool InbAllowOnNonAuthority
													  , bool bSpawnUnderTargetActor
													  , AActor*& SpawnedActor)
{
	if (Ability && (Ability->GetCurrentActorInfo()->IsNetAuthority() || bAllowOnNonAuthority == true) &&
		ShouldBroadcastAbilityTaskDelegates())
	{
		UWorld* const World = AbilitySystemComponent->GetWorld();

		// don't like it. Real instigatgo might not be pawn, but it is so hardcoded in
		// engine...
		APawn* Instigator = Cast<APawn>(GetAvatarActor());
		if (World)
		{
			SpawnedActor = World->SpawnActorDeferred<AActor>(Class
				, FTransform::Identity
				, GetOwnerActor()
				, Instigator
				, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		}
	}

	if (SpawnedActor == nullptr)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			DidNotSpawn.Broadcast(nullptr);
		}
		return false;
	}
	
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);
	UArcCoreGameplayAbility* ArcGA = Cast<UArcCoreGameplayAbility>(OwningAbility);
	
	return true;
}

void UArcAT_WaitSpawnAbilityActor::FinishSpawningActor(UGameplayAbility* OwningAbility
													   , TSubclassOf<UArcActorGameplayAbility> InAbilityEffect
													   , FGameplayAbilityTargetDataHandle TargetData
													   , AActor* SpawnedActor)
{
	if (SpawnedActor)
	{
		bool bTransformSet = false;
		FTransform SpawnTransform;
		if (FGameplayAbilityTargetData* LocationData = CachedTargetDataHandle.Get(0))
		// Hardcode to use data 0. It's OK if data isn't useful/valid.
		{
			if (SpawnUnderTargetActor)
			{
				if (LocationData->GetActors().Num() > 0)
				{
					TWeakObjectPtr<AActor> Actor = LocationData->GetActors()[0];
					if (ACharacter* Character = Cast<ACharacter>(Actor.Get()))
					{
						SpawnTransform.SetLocation(Character->GetCharacterMovement()->CurrentFloor.HitResult.ImpactPoint);
						bTransformSet = true;
					}

					if (bTransformSet == false)
					{
						if (UCapsuleComponent* CC = Cast<UCapsuleComponent>(Actor->GetRootComponent()))
						{
							FVector Loc = Actor->GetActorLocation();
							Loc.Z -= CC->GetScaledCapsuleHalfHeight();

							SpawnTransform.SetLocation(Loc);
							bTransformSet = true;
						}
					}
				}
			}
			else
			{
				// Set location. Rotation is unaffected.
				if (LocationData->HasHitResult())
				{
					SpawnTransform.SetLocation(LocationData->GetHitResult()->Location);
					bTransformSet = true;
				}
				else if (LocationData->HasEndPoint())
				{
					SpawnTransform = LocationData->GetEndPointTransform();
					bTransformSet = true;
				}
			}
		}

		SpawnedActor->FinishSpawning(SpawnTransform);

		if (ShouldBroadcastAbilityTaskDelegates())
		{
			Success.Broadcast(SpawnedActor);
		}
	}

	EndTask();
}
