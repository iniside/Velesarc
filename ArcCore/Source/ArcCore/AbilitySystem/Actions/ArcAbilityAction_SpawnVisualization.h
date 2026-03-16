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
#include "Targeting/ArcAoETypes.h"
#include "Types/TargetingSystemTypes.h"
#include "ArcAbilityAction_SpawnVisualization.generated.h"

class UArcTargetingObject;

/**
 * Latent action that spawns a visualization actor and continuously updates it
 * via async targeting (bRequeueOnCompletion = true). The actor is destroyed
 * and targeting stopped when the action is cancelled or the ability ends.
 *
 * Configuration is read from EditAnywhere properties first, then falls back
 * to item fragments (FArcItemFragment_TargetingObjectPreset).
 *
 * LatentTag must be set for this action to function — it is registered in the
 * ability's latent registry so it can be cancelled by FArcAbilityAction_CancelLatent.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Spawn Visualization"))
struct ARCCORE_API FArcAbilityAction_SpawnVisualization : public FArcAbilityAction
{
	GENERATED_BODY()

	/** Override targeting object. If null, read from item's FArcItemFragment_TargetingObjectPreset. */
	UPROPERTY(EditAnywhere, Category = "Targeting")
	TObjectPtr<UArcTargetingObject> TargetingObjectOverride;

	/** Override visualization actor class. If null, read from item fragment. */
	UPROPERTY(EditAnywhere, Category = "Visualization", meta = (MustImplement = "/Script/ArcCore.ArcAoEVisualizationInterface"))
	TSoftClassPtr<AActor> VisualizationActorClassOverride;

	// --- Runtime state (populated during Execute, lives on cached copy) ---

	TWeakObjectPtr<AActor> SpawnedActor;
	FTargetingRequestHandle AsyncTargetingHandle;
	FArcAoEShapeData CachedShapeData;

	virtual void Execute(FArcAbilityActionContext& Context) override;
	virtual void CancelLatent(FArcAbilityActionContext& Context) override;

	void HandleAsyncTargetingComplete(FTargetingRequestHandle Handle, UArcCoreGameplayAbility* Ability);
};
