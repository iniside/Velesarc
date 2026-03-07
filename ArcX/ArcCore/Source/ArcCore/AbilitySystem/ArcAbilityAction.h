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

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "GameplayAbilitySpec.h"
#include "AbilitySystemComponent.h"
#include "Types/TargetingSystemTypes.h"
#include "ArcAbilityAction.generated.h"

class UArcCoreGameplayAbility;

USTRUCT(BlueprintType)
struct ARCCORE_API FArcAbilityActionContext
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UArcCoreGameplayAbility> Ability;

	UPROPERTY()
	FGameplayAbilitySpecHandle Handle;

	UPROPERTY()
	FGameplayAbilityActivationInfo ActivationInfo;

	/** Optional, populated during targeting callbacks. */
	UPROPERTY()
	FGameplayAbilityTargetDataHandle TargetData;

	/** Optional, populated during local target result. */
	UPROPERTY()
	TArray<FHitResult> HitResults;

	/** The StateTree event tag. */
	UPROPERTY()
	FGameplayTag EventTag;

	/** Raw pointer to non-UObject struct, not a UPROPERTY. */
	const FGameplayAbilityActorInfo* ActorInfo = nullptr;

	/** Optional. */
	UPROPERTY()
	FTargetingRequestHandle TargetingRequestHandle;

	/** True when ability is ending due to cancellation. */
	bool bWasCancelled = false;
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcAbilityAction
{
	GENERATED_BODY()

	virtual ~FArcAbilityAction() = default;

	// Execute the action. For latent actions (LatentTag is valid), this is also
	// the activation — set up runtime state (spawn actors, start targeting, etc.)
	// that CancelLatent() will tear down. The action is registered in the ability's
	// latent registry after Execute() returns.
	virtual void Execute(FArcAbilityActionContext& Context) {}

	// Called on the cached copy when cancelled by CancelLatent action or ability end.
	virtual void CancelLatent(FArcAbilityActionContext& Context) {}

	// If valid, this action is registered as latent under this tag after Execute().
	UPROPERTY(EditAnywhere, Category = "Latent", meta = (Categories = "Ability.Latent"))
	FGameplayTag LatentTag;

	bool IsLatent() const { return LatentTag.IsValid(); }
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcAbilityActionList
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/ArcCore.ArcAbilityAction", ExcludeBaseStruct))
	TArray<FInstancedStruct> Actions;
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcAbilityEventActions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta = (Categories = "Ability.Event"))
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/ArcCore.ArcAbilityAction", ExcludeBaseStruct))
	TArray<FInstancedStruct> Actions;
};

