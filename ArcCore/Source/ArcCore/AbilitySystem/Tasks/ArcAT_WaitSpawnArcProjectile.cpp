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

#include "ArcCore/AbilitySystem/Tasks/ArcAT_WaitSpawnArcProjectile.h"

#include "AbilitySystem/Actors/ArcActorGameplayAbility.h"
#include "ArcCore/AbilitySystem/Actors/ArcAbilityProjectile.h"
#include "ArcCore/AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "ArcCore/AbilitySystem/ArcCoreGameplayAbility.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

UArcAT_WaitSpawnArcProjectile* UArcAT_WaitSpawnArcProjectile::SpawnArcProjectile(FName TaskName
																				 , UGameplayAbility* OwningAbility
																				 , FVector InStartVelocity
																				 , float InGravityScale
																				 , FTransform InSpawnTransform
																				 , TSubclassOf<UArcActorGameplayAbility> InActorAbility
																				 , TSubclassOf<AArcAbilityProjectile> InClass)
{
	UArcAT_WaitSpawnArcProjectile* MyObj = NewAbilityTask<UArcAT_WaitSpawnArcProjectile>(OwningAbility, TaskName);
	MyObj->StartVelocity = MoveTemp(InStartVelocity);
	MyObj->GravityScale = MoveTemp(InGravityScale);
	MyObj->SpawnTransform = MoveTemp(InSpawnTransform);
	MyObj->ActorAbility = InActorAbility;
	return MyObj;
}

UArcAT_WaitSpawnArcProjectile::UArcAT_WaitSpawnArcProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UArcAT_WaitSpawnArcProjectile::Activate()
{
	ENetMode NM = GetOwnerActor()->GetNetMode();
	if (NM == ENetMode::NM_DedicatedServer)
	{
		//	AbilitySystemComponent->ForceReplication();
	}
}

void UArcAT_WaitSpawnArcProjectile::OnDestroy(bool AbilityEnded)
{
	Super::OnDestroy(AbilityEnded);
}

// ---------------------------------------------------------------------------------------

bool UArcAT_WaitSpawnArcProjectile::BeginSpawningActor(UGameplayAbility* OwningAbility
													   , FGameplayAbilityTargetDataHandle TargetData
													   , TSubclassOf<UArcActorGameplayAbility> InActorAbility
													   , TSubclassOf<AArcAbilityProjectile> InActorClass
													   , AArcAbilityProjectile*& SpawnedActor)
{
	ENetMode NM = GetOwnerActor()->GetNetMode();
	if (GetOwnerActor()->GetLocalRole() < ENetRole::ROLE_Authority)
	{
		return false;
	}

	if (!Ability->GetCurrentActorInfo()->IsNetAuthority())
	{
		return false;
	}
	if (Ability && ShouldBroadcastAbilityTaskDelegates())
	{
		UWorld* const World = GEngine->GetWorldFromContextObject(OwningAbility
			, EGetWorldErrorMode::LogAndReturnNull);
		if (World)
		{
			FGameplayAbilityActorInfo ActorInfo = Ability->GetActorInfo();
			APawn* Instigator = Cast<APawn>(ActorInfo.AvatarActor.Get());

			SpawnedActor = World->SpawnActorDeferred<AArcAbilityProjectile>(InActorClass
				, SpawnTransform
				, ActorInfo.OwnerActor.Get()
				, Instigator
				, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

			if (SpawnedActor)
			{
				if (NM == ENetMode::NM_Client)
				{
					//	SpawnedActor->SetActorEnableCollision(false);
				}
				UArcCoreAbilitySystemComponent* NxASC = Cast<UArcCoreAbilitySystemComponent>(ActorInfo.AbilitySystemComponent.Get());
				UArcCoreGameplayAbility* AuGA = Cast<UArcCoreGameplayAbility>(OwningAbility);
				SpawnedActor->SetInfo(NxASC, AuGA, ActorAbility);
			}
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

	return true;
}

void UArcAT_WaitSpawnArcProjectile::FinishSpawningActor(UGameplayAbility* OwningAbility
														, FGameplayAbilityTargetDataHandle TargetData
														, TSubclassOf<UArcActorGameplayAbility> InActorAbility
														, AArcAbilityProjectile* SpawnedActor)
{
	if (GetOwnerActor()->GetLocalRole() < ENetRole::ROLE_Authority)
	{
		return;
	}
	ENetMode NM = GetOwnerActor()->GetNetMode();

	UArcCoreAbilitySystemComponent* NxASC = Cast<UArcCoreAbilitySystemComponent>(AbilitySystemComponent);

	if (SpawnedActor)
	{
		SpawnedActor->SetVelocity(StartVelocity);
		SpawnedActor->SetGravityScale(GravityScale);
		SpawnedActor->FinishSpawning(SpawnTransform);

		if (ShouldBroadcastAbilityTaskDelegates())
		{
			if (NM == ENetMode::NM_Client)
			{
			}
			if (NM == ENetMode::NM_DedicatedServer || NM == ENetMode::NM_Standalone)
			{
				ServerSpawned.Broadcast(SpawnedActor);
			}
		}
	}

	// EndTask();
}

// ---------------------------------------------------------------------------------------
