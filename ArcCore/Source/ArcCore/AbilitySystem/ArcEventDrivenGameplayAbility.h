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

#include "ArcItemGameplayAbility.h"
#include "ArcAbilityAction.h"
#include "ArcEventDrivenGameplayAbility.generated.h"

class UArcAT_WaitAbilityStateTree;
struct FArcItemFragment_StateTree;
struct FGameplayEventData;

UCLASS()
class ARCCORE_API UArcEventDrivenGameplayAbility : public UArcItemGameplayAbility
{
	GENERATED_BODY()

public:
	UArcEventDrivenGameplayAbility();

protected:
	virtual void OnAvatarSetSafe(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UFUNCTION()
	void HandleStateTreeEvent(const FGameplayEventData& Payload, FGameplayTag EventTag);

	UFUNCTION()
	void HandleStateTreeStopped(const FGameplayEventData& Payload, FGameplayTag EventTag);

	UFUNCTION()
	void HandleStateTreeSucceeded(const FGameplayEventData& Payload, FGameplayTag EventTag);

	UFUNCTION()
	void HandleStateTreeFailed(const FGameplayEventData& Payload, FGameplayTag EventTag);

	// Cached event-to-actions mappings from item fragment
	TMap<FGameplayTag, FArcAbilityActionList> CachedEventActions;

	UPROPERTY()
	TObjectPtr<UArcAT_WaitAbilityStateTree> StateTreeTask;
};
