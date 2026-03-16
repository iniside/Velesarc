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

#include "ArcItemFragment_AttributeModifier.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsStoreComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

void FArcItemFragment_AttributeModifier::OnItemAddedToSlot(const FArcItemData* InItem
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

	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(BackingGameplayEffect, 1, ContextHandle);

	for (const auto& [Tag, Magnitude] : Attributes)
	{
		Spec.Data->SetSetByCallerMagnitude(Tag, Magnitude);
	}

	const_cast<FArcItemFragment_AttributeModifier*>(this)->AppliedEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void FArcItemFragment_AttributeModifier::OnItemRemovedFromSlot(const FArcItemData* InItem
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

	ASC->RemoveActiveGameplayEffect(const_cast<FArcItemFragment_AttributeModifier*>(this)->AppliedEffectHandle);
}

#if WITH_EDITOR
EDataValidationResult FArcItemFragment_AttributeModifier::IsDataValid(const UArcItemDefinition* ItemDefinition
	, FDataValidationContext& Context) const
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	if (BackingGameplayEffect == nullptr)
	{
		Context.AddError(FText::FromString(TEXT("AttributeModifier: BackingGameplayEffect must be set.")));
		Result = EDataValidationResult::Invalid;
	}
	else
	{
		const UGameplayEffect* CDO = BackingGameplayEffect->GetDefaultObject<UGameplayEffect>();

		TSet<FGameplayTag> EffectSetByCallerTags;
		for (const FGameplayModifierInfo& Modifier : CDO->Modifiers)
		{
			if (Modifier.ModifierMagnitude.GetMagnitudeCalculationType() == EGameplayEffectMagnitudeCalculation::SetByCaller)
			{
				EffectSetByCallerTags.Add(Modifier.ModifierMagnitude.GetSetByCallerFloat().DataTag);
			}
		}

		for (const auto& [Tag, Magnitude] : Attributes)
		{
			if (!EffectSetByCallerTags.Contains(Tag))
			{
				Context.AddError(FText::FromString(FString::Printf(
					TEXT("AttributeModifier: Attribute tag '%s' has no matching SetByCaller modifier in BackingGameplayEffect '%s'."),
					*Tag.ToString(),
					*BackingGameplayEffect->GetName())));
				Result = EDataValidationResult::Invalid;
			}
		}
	}

	return Result;
}

FArcFragmentDescription FArcItemFragment_AttributeModifier::GetDescription(const UScriptStruct* InStruct) const
{
	FArcFragmentDescription Desc = FArcItemFragment_ItemInstanceBase::GetDescription(InStruct);
	Desc.CommonPairings = {
		FName(TEXT("FArcItemFragment_GrantedGameplayEffects")),
		FName(TEXT("FArcItemFragment_ItemStats"))
	};
	Desc.Prerequisites = { FName(TEXT("AbilitySystemComponent")) };
	Desc.UsageNotes = TEXT(
		"Modifies gameplay attributes via a backing GameplayEffect with SetByCaller magnitudes. "
		"The Attributes TMap maps SetByCaller tags to float magnitudes applied at equip time. "
		"BackingGameplayEffect must have SetByCaller modifiers matching the tags used in Attributes.");
	return Desc;
}
#endif // WITH_EDITOR
