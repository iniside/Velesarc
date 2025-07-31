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

#include "ArcGunActor.h"

#include "ArcGunStateComponent.h"

// Sets default values
AArcGunActor::AArcGunActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

void AArcGunActor::Initialize(const FArcItemId& InOwningItemId)
{
	UArcGunStateComponent* GunComponent = GetOwner()->FindComponentByClass<UArcGunStateComponent>();

	if (GunComponent == nullptr)
	{
		return;
	}

	if (GunPreFireHandle.IsValid() == false)
	{
		GunPreFireHandle = GunComponent->AddOnPreShoot(FArcOnShootFired::FDelegate::CreateUObject(this, &AArcGunActor::SpawnCosmeticEffectsWhileFire));	
	}
}

// Called when the game starts or when spawned
void AArcGunActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AArcGunActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

USkeletalMeshComponent* AArcGunActor::GetWeaponMeshComponent() const
{
	return FindComponentByClass<USkeletalMeshComponent>();
}

FVector AArcGunActor::GetMuzzleSocketLocation() const
{
	return GetWeaponMeshComponent()->GetSocketLocation(MuzzleSocket);
}

void AArcGunActor::SpawnCosmeticEffectsOnFireStart(
	class UArcGunStateComponent* InWeaponComponent
	, const FArcSelectedGun& EquippedWeapon)
{
	OnSpawnCosmeticEffectsOnFireStart(InWeaponComponent);
}

void AArcGunActor::SpawnCosmeticEffectsWhileFire(
	const FArcGunShotInfo& ShotInfo)
{
	OnSpawnCosmeticEffectsWhileFire(ShotInfo.Targets, ShotInfo.GunStateComponent);
}

void AArcGunActor::SpawnCosmeticEffectsOnFireEnd(
	class UArcGunStateComponent* InWeaponComponent
	, const FArcSelectedGun& EquippedWeapon)
{
	OnSpawnCosmeticEffectsOnFireEnd(InWeaponComponent);
}

