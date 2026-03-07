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

#include "ArcAbilityAction_AddGameplayCue.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "AbilitySystem/ArcItemGameplayAbility.h"
#include "AbilitySystem/Fragments/ArcItemFragment_AbilityGameplayCue.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemsHelpers.h"

void FArcAbilityAction_AddGameplayCue::Execute(FArcAbilityActionContext& Context)
{
	FGameplayTag CueTag = CueTagOverride;

	if (!CueTag.IsValid())
	{
		if (UArcItemGameplayAbility* ItemAbility = Cast<UArcItemGameplayAbility>(Context.Ability))
		{
			const UArcItemDefinition* ItemDef = ItemAbility->NativeGetSourceItemData();
			if (ItemDef)
			{
				if (const auto* Fragment = ItemDef->FindFragment<FArcItemFragment_AbilityGameplayCue>())
				{
					CueTag = Fragment->CueTag;
				}
			}
		}
	}

	if (!CueTag.IsValid()) { return; }

	UAbilitySystemComponent* ASC = Context.Ability->GetAbilitySystemComponentFromActorInfo();
	if (!ASC) { return; }

	ASC->AddGameplayCue(CueTag);
}
