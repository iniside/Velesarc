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

#include "AbilitySystem/ArcAbilityAction.h"
#include "ArcAbilityAction_SpawnMassProjectile.generated.h"

class UMassEntityConfigAsset;

/**
 * Spawns a Mass projectile entity using global targeting for origin and target.
 *
 * Origin tag: fetches spawn location from global targeting.
 * Target tag: fetches target location from global targeting — used to compute
 * the initial velocity direction on FArcProjectileFragment.
 *
 * The entity config should include UArcProjectileTrait to configure projectile behavior.
 * Optionally adds the instigator actor to the collision filter ignore list.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Spawn Mass Projectile"))
struct ARCCORE_API FArcAbilityAction_SpawnMassProjectile : public FArcAbilityAction
{
	GENERATED_BODY()

	/** Mass entity config with projectile trait. */
	UPROPERTY(EditAnywhere, Category = "Mass")
	TObjectPtr<UMassEntityConfigAsset> EntityConfig;

	/** Global targeting tag used to fetch the projectile spawn origin. */
	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (Categories = "GlobalTargeting"))
	FGameplayTag OriginTargetingTag;

	/** Global targeting tag used to fetch the target location for direction. */
	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (Categories = "GlobalTargeting"))
	FGameplayTag TargetTargetingTag;

	/** Initial speed of the projectile (cm/s). Applied to the velocity direction. */
	UPROPERTY(EditAnywhere, Category = "Projectile", meta = (ClampMin = "0.0"))
	float InitialSpeed = 5000.f;

	/** Add the instigator actor to the projectile's collision ignore list. */
	UPROPERTY(EditAnywhere, Category = "Projectile")
	bool bIgnoreInstigator = true;

	virtual void Execute(FArcAbilityActionContext& Context) override;
};
