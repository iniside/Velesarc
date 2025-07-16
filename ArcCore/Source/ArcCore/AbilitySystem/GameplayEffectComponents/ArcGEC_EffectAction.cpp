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



#include "ArcGEC_EffectAction.h"

#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"

bool UArcGEC_EffectAction::OnActiveGameplayEffectAdded(FActiveGameplayEffectsContainer& ActiveGEContainer, FActiveGameplayEffect& ActiveGE) const
{
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(ActiveGEContainer.Owner);
	if (!ArcASC)
	{
		return false;
	}

	if (ActiveGEContainer.IsNetAuthority())
	{
		ActiveGE.EventSet.OnEffectRemoved.AddUObject(this, &UArcGEC_EffectAction::OnActiveGameplayEffectRemoved);
	}
	
	ArcASC->AddActiveGameplayEffectAction(ActiveGE.Handle, EffectActions);

	TArray<FInstancedStruct>& Actions = ArcASC->GetActiveGameplayEffectActions(ActiveGE.Handle);

	for (FInstancedStruct& Action : Actions)
	{
		FArcActiveGameplayEffectAction* EffectAction = Action.GetMutablePtr<FArcActiveGameplayEffectAction>();
		EffectAction->MyHandle = ActiveGE.Handle;
		EffectAction->OnActiveGameplayEffectAdded(ActiveGEContainer, ActiveGE);
	}

	return true;
}

void UArcGEC_EffectAction::OnActiveGameplayEffectRemoved(const FGameplayEffectRemovalInfo& RemovalInfo) const
{
	UArcCoreAbilitySystemComponent* ArcASC = Cast<UArcCoreAbilitySystemComponent>(RemovalInfo.ActiveEffect->Handle.GetOwningAbilitySystemComponent());
	if (!ArcASC)
	{
		return;
	}
	
	TArray<FInstancedStruct>& Actions = ArcASC->GetActiveGameplayEffectActions(RemovalInfo.ActiveEffect->Handle);

	for (FInstancedStruct& Action : Actions)
	{
		Action.GetMutablePtr<FArcActiveGameplayEffectAction>()->OnActiveGameplayEffectRemoved(RemovalInfo);
	}

	ArcASC->RemoveActiveGameplayEffectActions(RemovalInfo.ActiveEffect->Handle);
}

void FArcGEA_WaitAbilityActivateApplyEffect::OnActiveGameplayEffectAdded(FActiveGameplayEffectsContainer& ActiveGEContainer
	, FActiveGameplayEffect& ActiveGE)
{
	if (ActiveGEContainer.Owner)
	{
		OwningASC = ActiveGEContainer.Owner;
		OnAbilityActivateDelegateHandle = ActiveGEContainer.Owner->AbilityActivatedCallbacks.AddRaw(this, &FArcGEA_WaitAbilityActivateApplyEffect::OnAbilityActivate);

		OriginalContext = ActiveGE.Spec.GetContext();
	}
}

void FArcGEA_WaitAbilityActivateApplyEffect::OnActiveGameplayEffectRemoved(const FGameplayEffectRemovalInfo& RemovalInfo)
{
	if (RemovalInfo.ActiveEffect->Handle.GetOwningAbilitySystemComponent())
	{
		RemovalInfo.ActiveEffect->Handle.GetOwningAbilitySystemComponent()->AbilityActivatedCallbacks.Remove(OnAbilityActivateDelegateHandle);
	}
}

void FArcGEA_WaitAbilityActivateApplyEffect::OnAbilityActivate(UGameplayAbility* ActivatedAbility)
{
	const FGameplayTagContainer& AbilityTags = ActivatedAbility->GetAssetTags();
	
	if (RequiredTag.IsValid() && !AbilityTags.HasTag(RequiredTag))// ||(WithoutTag.IsValid() && AbilityTags.HasTag(WithoutTag)))
	{
		// Failed tag check
		return;
	}

	FGameplayEffectSpecHandle Spec = OriginalContext.GetAbilityInstance_NotReplicated()->MakeOutgoingGameplayEffectSpec(EffectToApply);
	OriginalContext.GetInstigatorAbilitySystemComponent()->ApplyGameplayEffectSpecToTarget(*Spec.Data, OwningASC.Get());
}
