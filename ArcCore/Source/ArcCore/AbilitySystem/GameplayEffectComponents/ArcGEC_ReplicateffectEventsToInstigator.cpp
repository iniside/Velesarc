/**
 * This file is part of ArcX.
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



#include "ArcGEC_ReplicateffectEventsToInstigator.h"

#include "ArcWorldDelegates.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"

bool UArcGEC_ReplicateffectEventsToInstigator::OnActiveGameplayEffectAdded(FActiveGameplayEffectsContainer& ActiveGEContainer
																		   , FActiveGameplayEffect& ActiveGE) const
{
	UArcCoreAbilitySystemComponent* ASC = Cast<UArcCoreAbilitySystemComponent>(ActiveGEContainer.Owner);

	//UArcWorldDelegates::Get(ASC)->OnAnyGameplayEffectAddedToSelf.Broadcast(ASC, ActiveGE);

	if (!ActiveGE.Spec.GetContext().GetInstigator())
	{
		return false;
	}

	ENetMode NM = ActiveGE.Spec.GetContext().GetInstigator()->GetNetMode();
	if (NM == ENetMode::NM_DedicatedServer)
	{
		UArcCoreAbilitySystemComponent* InstigatorASC = Cast<UArcCoreAbilitySystemComponent>(ActiveGE.Spec.GetContext().GetInstigatorAbilitySystemComponent());
		FArcActiveGameplayEffectForRPC ForRPC(ActiveGE);
		InstigatorASC->ClientNotifyTargetEffectApplied(ActiveGEContainer.Owner->GetOwner(), ForRPC);
	}
	else if (NM == ENetMode::NM_Standalone)
	{
		FArcActiveGameplayEffectForRPC ForRPC(ActiveGE);
		UArcWorldDelegates::Get(ASC)->OnAnyGameplayEffectAddedToTarget.Broadcast(ActiveGEContainer.Owner->GetOwner(), ForRPC);
	}
	ActiveGE.EventSet.OnEffectRemoved.AddUObject(this, &UArcGEC_ReplicateffectEventsToInstigator::OnActiveGameplayEffectRemoved, &ActiveGEContainer);
	return true;
}

void UArcGEC_ReplicateffectEventsToInstigator::OnActiveGameplayEffectRemoved(const FGameplayEffectRemovalInfo& RemovalInfo
	, FActiveGameplayEffectsContainer* ActiveGEContainer) const
{
	UArcCoreAbilitySystemComponent* ASC = Cast<UArcCoreAbilitySystemComponent>(ActiveGEContainer->Owner);

	if (!RemovalInfo.ActiveEffect->Spec.GetContext().GetInstigator())
	{
		return;
	}

	ENetMode NM = RemovalInfo.ActiveEffect->Spec.GetContext().GetInstigator()->GetNetMode();
	if (NM == ENetMode::NM_DedicatedServer)
	{
		UArcCoreAbilitySystemComponent* InstigatorASC = Cast<UArcCoreAbilitySystemComponent>(RemovalInfo.ActiveEffect->Spec.GetContext().GetInstigatorAbilitySystemComponent());
		FArcActiveGameplayEffectForRPC ForRPC(*RemovalInfo.ActiveEffect);
		InstigatorASC->ClientNotifyTargetEffectRemoved(ActiveGEContainer->Owner->GetOwner(), ForRPC);
	}
	else if (NM == ENetMode::NM_Standalone)
	{
		FArcActiveGameplayEffectForRPC ForRPC(*RemovalInfo.ActiveEffect);
		UArcWorldDelegates::Get(ASC)->OnAnyGameplayEffectRemovedFromTarget.Broadcast(ActiveGEContainer->Owner->GetOwner(), ForRPC);
	}
}