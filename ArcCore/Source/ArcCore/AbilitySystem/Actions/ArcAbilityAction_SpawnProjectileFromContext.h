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
#include "ArcAbilityAction_SpawnProjectileFromContext.generated.h"

/**
 * Spawns a projectile actor using global targeting for origin and ability
 * context target data for the target location.
 *
 * Origin tag: fetches HitResult.ImpactPoint from global targeting as spawn location.
 * Target: uses Context.TargetData endpoint as the target location passed to
 * UArcAbilityActorComponent.
 *
 * ActorClass can be set directly or falls back to FArcItemFragment_ProjectileSettings.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Spawn Projectile (Context Target)"))
struct ARCCORE_API FArcAbilityAction_SpawnProjectileFromContext : public FArcAbilityAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Actor")
	TSubclassOf<AActor> ActorClass;

	/** Global targeting tag used to fetch the projectile spawn origin (ImpactPoint). */
	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (Categories = "GlobalTargeting"))
	FGameplayTag OriginTargetingTag;

	virtual void Execute(FArcAbilityActionContext& Context) override;
};
