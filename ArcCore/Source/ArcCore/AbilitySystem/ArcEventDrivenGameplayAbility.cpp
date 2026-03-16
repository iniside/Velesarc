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

#include "ArcEventDrivenGameplayAbility.h"
#include "ArcAbilityStateTreeTypes.h"
#include "Fragments/ArcItemFragment_AbilityActions.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemsHelpers.h"

UArcEventDrivenGameplayAbility::UArcEventDrivenGameplayAbility()
{
}

void UArcEventDrivenGameplayAbility::OnAvatarSetSafe(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSetSafe(ActorInfo, Spec);

	// Cache event actions from item fragment
	const FArcItemData* ItemData = GetSourceItemEntryPtr();
	if (ItemData)
	{
		const FArcItemFragment_AbilityActions* ActionsFragment = ArcItemsHelper::FindFragment<FArcItemFragment_AbilityActions>(ItemData);
		if (ActionsFragment)
		{
			CachedEventActions = ActionsFragment->GetEventActions();
		}
	}
}

void UArcEventDrivenGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Read StateTree from item fragment and start it
	const UArcItemDefinition* ItemDef = NativeGetSourceItemData();
	if (ItemDef)
	{
		if (const FArcItemFragment_StateTree* STFragment = ItemDef->FindFragment<FArcItemFragment_StateTree>())
		{
			// Get event tags from cached event actions
			FGameplayTagContainer EventTags;
			for (const auto& [Tag, EA] : CachedEventActions)
			{
				if (Tag.IsValid())
				{
					EventTags.AddTag(Tag);
				}
			}

			StateTreeTask = UArcAT_WaitAbilityStateTree::WaitAbilityStateTree(
				this,
				STFragment->StateTreeRef,
				STFragment->InputPressedTag,    // InputEventTag — none by default
				STFragment->InputReleasedTag,    // InputReleasedEventTag — none by default
				EventTags);

			if (StateTreeTask)
			{
				StateTreeTask->OnGameplayEvent.AddDynamic(this, &UArcEventDrivenGameplayAbility::HandleStateTreeEvent);
				StateTreeTask->OnStateTreeStopped.AddDynamic(this, &UArcEventDrivenGameplayAbility::HandleStateTreeStopped);
				StateTreeTask->OnStateTreeSucceeded.AddDynamic(this, &UArcEventDrivenGameplayAbility::HandleStateTreeSucceeded);
				StateTreeTask->OnStateTreeFailed.AddDynamic(this, &UArcEventDrivenGameplayAbility::HandleStateTreeFailed);
				StateTreeTask->ReadyForActivation();
			}
		}
	}
}

void UArcEventDrivenGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StateTreeTask = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UArcEventDrivenGameplayAbility::HandleStateTreeEvent(const FGameplayEventData& Payload, FGameplayTag EventTag)
{
	// Look up matching event actions
	if (FArcAbilityActionList* EA = CachedEventActions.Find(EventTag))
	{
		FArcAbilityActionContext ActionContext;
		ActionContext.Ability = this;
		ActionContext.Handle = GetCurrentAbilitySpecHandle();
		ActionContext.ActorInfo = GetCurrentActorInfo();
		ActionContext.ActivationInfo = GetCurrentActivationInfo();
		ActionContext.EventTag = EventTag;
		ExecuteActionList(EA->Actions, ActionContext);
	}
}

void UArcEventDrivenGameplayAbility::HandleStateTreeStopped(const FGameplayEventData& Payload, FGameplayTag EventTag)
{
	K2_EndAbility();
}

void UArcEventDrivenGameplayAbility::HandleStateTreeSucceeded(const FGameplayEventData& Payload, FGameplayTag EventTag)
{
	K2_EndAbility();
}

void UArcEventDrivenGameplayAbility::HandleStateTreeFailed(const FGameplayEventData& Payload, FGameplayTag EventTag)
{
	K2_EndAbility();
}
