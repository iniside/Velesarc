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

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArcGunTypes.h"

#include "ArcGunActor.generated.h"

UCLASS()
class ARCGUN_API AArcGunActor : public AActor
{
	GENERATED_BODY()

	UPROPERTY()
	FArcItemId OwningItemId;

	FDelegateHandle GunPreFireHandle;
	
public:	
	// Sets default values for this actor's properties
	AArcGunActor();

	void Initialize(const FArcItemId& InOwningItemId);
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	virtual class USkeletalMeshComponent* GetWeaponMeshComponent() const { return nullptr; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category="Arc Core|Weapon")
	void OnSpawnCosmeticEffectsOnFireStart(class UArcGunStateComponent* InWeaponComponent);
	
	UFUNCTION(BlueprintImplementableEvent, Category="Arc Core|Weapon")
	void OnSpawnCosmeticEffectsWhileFire(const TArray<FHitResult>& Hits, class UArcGunStateComponent* InWeaponComponent);

	UFUNCTION(BlueprintImplementableEvent, Category="Arc Core|Weapon")
	void OnSpawnCosmeticEffectsOnFireEnd(class UArcGunStateComponent* InWeaponComponent);

	virtual void SpawnCosmeticEffectsOnFireStart(
		class UArcGunStateComponent* InWeaponComponent
		, const FArcSelectedGun& EquippedWeapon);
	
	virtual void SpawnCosmeticEffectsWhileFire(
		const FArcGunShotInfo& ShotInfo);
	
	virtual void SpawnCosmeticEffectsOnFireEnd(
		class UArcGunStateComponent* InWeaponComponent
		, const FArcSelectedGun& EquippedWeapon);
};
