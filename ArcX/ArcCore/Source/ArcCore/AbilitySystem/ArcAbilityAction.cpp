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

#include "ArcAbilityAction.h"
#include "ArcCoreGameplayAbility.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemsHelpers.h"
#include "Targeting/ArcTargetingObject.h"

// ---------------------------------------------------------------------------
// FArcAbilityAction_ApplyCooldown
// ---------------------------------------------------------------------------

void FArcAbilityAction_ApplyCooldown::Execute(const FArcAbilityActionContext& Context) const
{
	Context.Ability->ApplyCooldown(Context.Handle, Context.ActorInfo, Context.ActivationInfo);
}

// ---------------------------------------------------------------------------
// FArcAbilityAction_ApplyCost
// ---------------------------------------------------------------------------

void FArcAbilityAction_ApplyCost::Execute(const FArcAbilityActionContext& Context) const
{
	// Requires public wrapper added in Phase 2
	Context.Ability->ApplyCost(Context.Handle, Context.ActorInfo, Context.ActivationInfo);
}

// ---------------------------------------------------------------------------
// FArcAbilityAction_ExecuteTargeting
// ---------------------------------------------------------------------------

void FArcAbilityAction_ExecuteTargeting::Execute(const FArcAbilityActionContext& Context) const
{
	UArcTargetingObject* TargetingObj = TargetingObjectOverride;

	if (!TargetingObj)
	{
		const UArcItemDefinition* ItemDef = Context.Ability->NativeGetSourceItemData();
		if (ItemDef)
		{
			if (const auto* Fragment = ItemDef->FindFragment<FArcItemFragment_TargetingObjectPreset>())
			{
				TargetingObj = Fragment->TargetingObject;
			}
		}
	}

	Context.Ability->ExecuteLocalTargeting(TargetingObj);
}

// ---------------------------------------------------------------------------
// FArcAbilityAction_SendTargetingResult
// ---------------------------------------------------------------------------

void FArcAbilityAction_SendTargetingResult::Execute(const FArcAbilityActionContext& Context) const
{
	UArcTargetingObject* TargetingObj = nullptr;

	const UArcItemDefinition* ItemDef = Context.Ability->NativeGetSourceItemData();
	if (ItemDef)
	{
		if (const auto* Fragment = ItemDef->FindFragment<FArcItemFragment_TargetingObjectPreset>())
		{
			TargetingObj = Fragment->TargetingObject;
		}
	}

	Context.Ability->SendTargetingResult(Context.TargetData, TargetingObj);
}

// ---------------------------------------------------------------------------
// FArcAbilityAction_ApplyEffectsFromItem
// ---------------------------------------------------------------------------

void FArcAbilityAction_ApplyEffectsFromItem::Execute(const FArcAbilityActionContext& Context) const
{
	// Requires public wrapper added in Phase 2
	if (EffectTag.IsValid())
	{
		Context.Ability->ApplyEffectsFromItem(EffectTag, Context.TargetData);
	}
	else
	{
		Context.Ability->ApplyAllEffectsFromItem(Context.TargetData);
	}
}

// ---------------------------------------------------------------------------
// FArcAbilityAction_EndAbility
// ---------------------------------------------------------------------------

void FArcAbilityAction_EndAbility::Execute(const FArcAbilityActionContext& Context) const
{
	Context.Ability->EndAbility(Context.Handle, Context.ActorInfo, Context.Ability->GetCurrentActivationInfo(), true, false);
}
