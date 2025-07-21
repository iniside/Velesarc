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

#include "ArcCore/AbilitySystem/Actors/ArcAbilityPersistentActor.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "ArcCore/AbilitySystem/ArcCoreGameplayAbility.h"
#include "Components/PrimitiveComponent.h"

// Sets default values
AArcAbilityPersistentActor::AArcAbilityPersistentActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve
	// performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bAllowTickOnDedicatedServer = false;
	bReplicates = true;

	SetReplicateMovement(false);

	AbilitySystem = nullptr;
}

void AArcAbilityPersistentActor::BeginPlay()
{
	Super::BeginPlay();
	OnAbilitySystemCreated.AddDynamic(this
		, &AArcAbilityPersistentActor::NativeOnAbilitySystemCreated);
}

void AArcAbilityPersistentActor::NativeOnAbilitySystemCreated()
{
}

void AArcAbilityPersistentActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AArcAbilityPersistentActor::Destroyed()
{
	Super::Destroyed();
}

UAbilitySystemComponent* AArcAbilityPersistentActor::GetAbilitySystemComponent() const
{
	if (AbilitySystem)
	{
		return AbilitySystem;
	}
	else
	{
		AbilitySystem = NewObject<UAbilitySystemComponent>(const_cast<AArcAbilityPersistentActor*>(this)
			, UAbilitySystemComponent::StaticClass()
			, TEXT("AbilitySystem"));
		AbilitySystem->RegisterComponentWithWorld(GetWorld());
		GetAbilitySystemComponent()->InitAbilityActorInfo(const_cast<AArcAbilityPersistentActor*>(this)
			, const_cast<AArcAbilityPersistentActor*>(this));
		AbilitySystem->RegisterComponentWithWorld(GetWorld());
		AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
		AbilitySystem->SetIsReplicated(true);

		OnAbilitySystemCreated.Broadcast();

		return AbilitySystem;
	}
}
