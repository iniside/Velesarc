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

#include "Items/Fragments/ArcItemFragment_GrantedGameplayEffects.h"

#include "ArcGameplayEffectContext.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Items/ArcItemsHelpers.h"

void FArcItemFragment_GrantedGameplayEffects::OnItemAddedToSlot(const FArcItemData* InItem, const FGameplayTag& InSlotId) const
{
	FArcItemInstance_GrantedGameplayEffects* GrantedGameplayEffects = ArcItemsHelper::FindMutableInstance<FArcItemInstance_GrantedGameplayEffects>(InItem);
	UArcCoreAbilitySystemComponent* ArcASC = InItem->GetItemsStoreComponent()->GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();

	FGameplayEffectContextHandle ContextHandle = ArcASC->MakeEffectContext();
	check(ContextHandle.Get()->GetScriptStruct() == FArcGameplayEffectContext::StaticStruct());
	FArcGameplayEffectContext* ArcContext = static_cast<FArcGameplayEffectContext*>(ContextHandle.Get());

	ArcContext->SetSourceItemDef(InItem->GetItemDefinition());
	ArcContext->SetItemStoreComponent(InItem->GetItemsStoreComponent());
	ArcContext->SetSourceItemId(InItem->GetItemId());

	for (TSubclassOf<UGameplayEffect> Effect : Effects)
	{
		FGameplayEffectSpecHandle Spec = ArcASC->MakeOutgoingSpec(Effect, 1, ContextHandle);
		if ( Spec.IsValid())
		{
			FActiveGameplayEffectHandle ActiveHandle = ArcASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), ArcASC);
			GrantedGameplayEffects->GrantedEffects.Add(ActiveHandle);	
		}
	}
}

void FArcItemFragment_GrantedGameplayEffects::OnItemRemovedFromSlot(const FArcItemData* InItem, const FGameplayTag& InSlotId) const
{
	FArcItemInstance_GrantedGameplayEffects* GrantedGameplayEffects = ArcItemsHelper::FindMutableInstance<FArcItemInstance_GrantedGameplayEffects>(InItem);
	UArcCoreAbilitySystemComponent* ArcASC = InItem->GetItemsStoreComponent()->GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();

	for (FActiveGameplayEffectHandle Handle : GrantedGameplayEffects->GrantedEffects)
	{
		ArcASC->RemoveActiveGameplayEffect(Handle);
	}
}

#if WITH_EDITOR
FArcFragmentDescription FArcItemFragment_GrantedGameplayEffects::GetDescription(const UScriptStruct* InStruct) const
{
	FArcFragmentDescription Desc = FArcItemFragment_ItemInstanceBase::GetDescription(InStruct);
	Desc.CommonPairings = {
		FName(TEXT("FArcItemFragment_GrantedAbilities")),
		FName(TEXT("FArcItemFragment_GrantAttributes"))
	};
	Desc.Prerequisites = { FName(TEXT("AbilitySystemComponent")) };
	Desc.UsageNotes = TEXT(
		"Applies persistent gameplay effects while the item is equipped. "
		"Effects are applied on slot entry and removed on slot exit. "
		"Use for passive stat buffs, resistances, or any effect tied to equipping an item. "
		"For on-hit/on-use effects, use FArcItemFragment_AbilityEffectsToApply instead.");
	return Desc;
}
#endif // WITH_EDITOR
