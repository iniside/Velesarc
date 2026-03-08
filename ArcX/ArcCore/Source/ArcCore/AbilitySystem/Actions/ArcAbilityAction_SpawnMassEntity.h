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
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "ArcAbilityAction_SpawnMassEntity.generated.h"

class UMassEntityConfigAsset;

/**
 * Spawns a Mass entity from a MassEntityConfig asset using the context's TargetData
 * to determine spawn location. Sets FTransformFragment on the spawned entity if present.
 *
 * Spawn location is derived from the first hit result's ImpactPoint in TargetData,
 * or from the target data endpoint if no hit result is available.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Spawn Mass Entity"))
struct ARCCORE_API FArcAbilityAction_SpawnMassEntity : public FArcAbilityAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Mass")
	TObjectPtr<UMassEntityConfigAsset> EntityConfig;

	UPROPERTY(EditAnywhere, Category = "Mass")
	EArcAbilityActorSpawnOrigin SpawnOrigin = EArcAbilityActorSpawnOrigin::ImpactPoint;

	virtual void Execute(FArcAbilityActionContext& Context) override;
};
