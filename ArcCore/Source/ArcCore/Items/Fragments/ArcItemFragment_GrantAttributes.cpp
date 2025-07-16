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

#include "ArcItemFragment_GrantAttributes.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"
#include "Items/ArcItemsHelpers.h"

void FArcItemFragment_GrantAttributes::OnItemAddedToSlot(const FArcItemData* InItem
														 , const FGameplayTag& InSlotId) const
{
	if (InItem->GetItemsStoreComponent()->GetOwnerRole() < ROLE_Authority)
	{
		return;
	}
	
	if (BackingGameplayEffect == nullptr)
	{
		return;
	}
	
	if (InItem->GetItemsStoreComponent()->GetOwner()->HasAuthority() == false)
	{
		return;
	}
	
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(InItem->GetItemsStoreComponent()->GetOwner());
	if (ASI == nullptr)
	{
		return;
	}

	UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
	if (ASC == nullptr)
	{
		return;
	}

	FArcItemInstance_GrantAttributes* Instance = ArcItems::FindMutableInstance<FArcItemInstance_GrantAttributes>(InItem);
	
	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
 	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(BackingGameplayEffect, 1, ContextHandle);

	for (const FArcGrantAttributesSetByCaller& SetByCaller : StaticAttributes)
	{
		Spec.Data->SetSetByCallerMagnitude(SetByCaller.SetByCallerTag, SetByCaller.Magnitude);
	}

	for (const FArcGrantAttributesSetByCaller& SetByCaller : Instance->DynamicAttributes)
	{
		Spec.Data->SetSetByCallerMagnitude(SetByCaller.SetByCallerTag, SetByCaller.Magnitude);
	}

	Instance->AppliedEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void FArcItemFragment_GrantAttributes::OnItemRemovedFromSlot(const FArcItemData* InItem
	, const FGameplayTag& InSlotId) const
{
	if (InItem->GetItemsStoreComponent()->GetOwnerRole() < ROLE_Authority)
	{
		return;
	}
	
	if (InItem->GetItemsStoreComponent()->GetOwner()->HasAuthority() == false)
	{
		return;
	}

	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(InItem->GetItemsStoreComponent()->GetOwner());
	if (ASI == nullptr)
	{
		return;
	}

	UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
	if (ASC == nullptr)
	{
		return;
	}

	FArcItemInstance_GrantAttributes* Instance = ArcItems::FindMutableInstance<FArcItemInstance_GrantAttributes>(InItem);
	ASC->RemoveActiveGameplayEffect(Instance->AppliedEffectHandle);
}
