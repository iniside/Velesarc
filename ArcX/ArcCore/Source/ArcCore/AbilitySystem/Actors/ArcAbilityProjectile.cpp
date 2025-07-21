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

#include "ArcCore/AbilitySystem/Actors/ArcAbilityProjectile.h"
#include "ArcCore/AbilitySystem/Actors/ArcAbilityPMC.h"
#include "ArcCore/AbilitySystem/ArcCoreGameplayAbility.h"
#include "Components/PrimitiveComponent.h"

const FName AArcAbilityProjectile::BasePMCName = FName("BasePMCName");
// Sets default values
AArcAbilityProjectile::AArcAbilityProjectile()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve
	// performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = true;
	bReplicates = true;

	SetReplicateMovement(true);
	BasePMC2 = CreateDefaultSubobject<UArcAbilityPMC>(AArcAbilityProjectile::BasePMCName);
	BasePMC2->PrimaryComponentTick.bAllowTickOnDedicatedServer = true;
}

// Called when the game starts or when spawned
void AArcAbilityProjectile::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AArcAbilityProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AArcAbilityProjectile::SetLocalVelocity(const FVector& Start
											 , const FVector& End
											 , float Speed)
{
	const FVector Dir = End - Start;
	const FVector NorDir = Dir.GetSafeNormal();

	BasePMC2->Velocity = NorDir * Speed;
}

void AArcAbilityProjectile::SetVelocity(const FVector& InVelocity)
{
	BasePMC2->Velocity = InVelocity.GetSafeNormal();
	BasePMC2->InitialSpeed = InVelocity.Length();
}

void AArcAbilityProjectile::SetGravityScale(float InGravityScale)
{
	BasePMC2->ProjectileGravityScale = InGravityScale;
}
