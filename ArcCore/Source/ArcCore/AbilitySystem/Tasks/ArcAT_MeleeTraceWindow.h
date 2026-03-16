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

#include "Abilities/Tasks/AbilityTask.h"
#include "ArcCore/AbilitySystem/Tasks/ArcAbilityTask.h"
#include "GameplayTagContainer.h"
#include "ArcAT_MeleeTraceWindow.generated.h"

struct FArcItemFragment_MeleeSockets;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArcMeleeHitDynamic, const FHitResult&, HitResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArcMeleeTraceWindowEndDynamic, const TArray<FHitResult>&, AllHits);

/**
 * Ability task that performs per-frame weapon socket traces during an animation-driven window.
 * Listens for ASC gameplay events (from UArcANS_MeleeTraceWindow) to start/stop tracing.
 * Deduplicates hits per window. Per new hit: fires a gameplay event (for CachedEventActions),
 * then calls SendTargetingResult (for ProcessAbilityTargetActions + replication).
 * Fires a gameplay event on window end. Stays alive across multiple windows (combo-friendly).
 */
UCLASS()
class ARCCORE_API UArcAT_MeleeTraceWindow : public UArcAbilityTask
{
	GENERATED_BODY()

public:
	UArcAT_MeleeTraceWindow();

	UFUNCTION(BlueprintCallable, Category = "Arc Core|Ability|Tasks",
		meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UArcAT_MeleeTraceWindow* MeleeTraceWindow(
		UGameplayAbility* OwningAbility,
		FName TaskInstanceName,
		FGameplayTag InBeginEventTag,
		FGameplayTag InEndEventTag,
		FGameplayTag InHitEventTag,
		FGameplayTag InWindowEndEventTag);

	/** Fires for each new unique hit during the trace window. */
	UPROPERTY(BlueprintAssignable)
	FArcMeleeHitDynamic OnMeleeHit;

	/** Fires when the trace window ends, with all hits accumulated during the window. */
	UPROPERTY(BlueprintAssignable)
	FArcMeleeTraceWindowEndDynamic OnTraceWindowEnd;

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	void OnGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload);

	void BeginTraceWindow();
	void EndTraceWindow();
	void PerformTraces();

	void DispatchHit(const FHitResult& Hit);

	/** Resolves the mesh component to read sockets from, based on the item fragment config. */
	UMeshComponent* ResolveMeshComponent() const;

	FGameplayTag BeginEventTag;
	FGameplayTag EndEventTag;
	FGameplayTag HitEventTag;
	FGameplayTag WindowEndEventTag;

	bool bIsTracing = false;
	bool bHasPreviousFrame = false;

	FVector PrevTipLocation;
	FVector PrevBaseLocation;

	// Cached from FArcItemFragment_MeleeSockets at window begin
	FName CachedTipSocket;
	FName CachedBaseSocket;
	float CachedTraceRadius = 0.f;
	TEnumAsByte<ECollisionChannel> CachedTraceChannel = ECC_Visibility;

	TWeakObjectPtr<UMeshComponent> CachedMeshComponent;

	TSet<TWeakObjectPtr<AActor>> HitActors;
	TArray<FHitResult> AllWindowHits;

	FGameplayTagContainer CachedEventTags;
	FDelegateHandle EventDelegateHandle;
};
