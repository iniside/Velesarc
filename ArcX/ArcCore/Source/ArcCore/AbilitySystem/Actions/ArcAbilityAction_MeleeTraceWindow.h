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
#include "ArcAbilityAction_MeleeTraceWindow.generated.h"

class UArcAT_MeleeTraceWindow;

/**
 * Latent ability action that creates a UArcAT_MeleeTraceWindow.
 * The task listens for ANS gameplay events to start/stop per-frame weapon traces.
 * Each new hit fires a gameplay event (for CachedEventActions) then calls SendTargetingResult.
 * Place this in OnActivateActions on FArcItemFragment_AbilityActions.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Melee Trace Window"))
struct ARCCORE_API FArcAbilityAction_MeleeTraceWindow : public FArcAbilityAction
{
	GENERATED_BODY()

	/** Gameplay event tag fired by UArcANS_MeleeTraceWindow::NotifyBegin. */
	UPROPERTY(EditAnywhere, Category = "Melee")
	FGameplayTag BeginEventTag;

	/** Gameplay event tag fired by UArcANS_MeleeTraceWindow::NotifyEnd. */
	UPROPERTY(EditAnywhere, Category = "Melee")
	FGameplayTag EndEventTag;

	/** Gameplay event tag fired per new hit (for CachedEventActions dispatch). */
	UPROPERTY(EditAnywhere, Category = "Melee")
	FGameplayTag HitEventTag;

	/** Gameplay event tag fired when a trace window ends (carries all hits as TargetData). */
	UPROPERTY(EditAnywhere, Category = "Melee")
	FGameplayTag WindowEndEventTag;

	virtual void Execute(FArcAbilityActionContext& Context) override;
	virtual void CancelLatent(FArcAbilityActionContext& Context) override;

private:
	TWeakObjectPtr<UArcAT_MeleeTraceWindow> ActiveTask;
};
