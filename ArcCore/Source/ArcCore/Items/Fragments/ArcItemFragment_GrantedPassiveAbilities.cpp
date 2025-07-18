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

#include "Items/Fragments/ArcItemFragment_GrantedPassiveAbilities.h"


#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "Items/ArcItemsHelpers.h"
#if 0
void FArcItemInstance_GrantedPassiveAbilities::OnItemAddedToSlot(const FArcItemData* InItem
																 , const FGameplayTag& InSlotId)
{
	const FArcItemFragment_GrantedPassiveAbilities* PassiveAbilities = ArcItems::GetFragment<FArcItemFragment_GrantedPassiveAbilities>(InItem);
	ItemsStore = InItem->GetItemsStoreComponent();
	ArcASC = ItemsStore->GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	Item = InItem;
	SlotId = InSlotId;
	
	UpdatePendingAbility();

	if (ArcASC != nullptr && PendingAbilities.Num() > 0)
	{
		PendingAbilitiesHandle = ArcASC->AddOnAbilityGiven(FArcGenericAbilitySpecDelegate::FDelegate::CreateRaw(
			this, &FArcItemInstance_GrantedPassiveAbilities::HandleOnAbilityGiven
		));
	}
	
	if (ArcASC->GetOwnerRole() < ENetRole::ROLE_Authority)
	{
		return;
	}
	
	if (PassiveAbilities)
	{
		for (const TSubclassOf<UGameplayAbility>& GA : PassiveAbilities->Abilities)
		{
			FGameplayAbilitySpec AbilitySpec(GA
				, 1
				, -1
				, InItem->GetItemsStoreComponent());

			FGameplayAbilitySpecHandle AbilityHandle = ArcASC->GiveAbility(AbilitySpec);

			GrantedPassiveAbilities.Add(AbilityHandle);

			FGameplayAbilitySpec* Spec = ArcASC->FindAbilitySpecFromHandle(AbilityHandle);
			if (Spec == nullptr)
			{
				continue;
			}
		
			UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(Spec->GetPrimaryInstance());

			ArcAbility->OnAddedToItemSlot(ArcASC->AbilityActorInfo.Get(), Spec, InSlotId, InItem);

			if (ArcAbility == nullptr)
			{
				PendingAbilities.AddUnique(AbilityHandle);
				continue;
			}
		}
	}
	
	for (const TSoftClassPtr<UGameplayAbility>& GA : InItem->GetGeneratedGrantedPassiveAbilities())
	{
		FGameplayAbilitySpec AbilitySpec(GA.LoadSynchronous()->GetDefaultObject<UGameplayAbility>()
			, 1
			, -1
			, ItemsStore);

		FGameplayAbilitySpecHandle AbilityHandle = ArcASC->GiveAbility(AbilitySpec);

		FGameplayAbilitySpec* Spec = ArcASC->FindAbilitySpecFromHandle(AbilityHandle);
		if (Spec == nullptr)
		{
			continue;
		}
			
		UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(Spec->GetPrimaryInstance());

		if (ArcAbility == nullptr)
		{
			PendingAbilities.AddUnique(AbilityHandle);
			continue;
		}

		ArcAbility->OnAddedToItemSlot(ArcASC->AbilityActorInfo.Get(), Spec, InSlotId, InItem);
	}
}

void FArcItemInstance_GrantedPassiveAbilities::OnItemRemovedFromSlot(const FArcItemData* InItem
	, const FGameplayTag& InSlotId)
{
	if (ArcASC->GetOwnerRole() < ENetRole::ROLE_Authority)
	{
		return;
	}

	for (const FGameplayAbilitySpecHandle& Handle : GrantedPassiveAbilities)
	{
		ArcASC->ClearAbility(Handle);
	}

	GrantedPassiveAbilities.Empty();
}

void FArcItemInstance_GrantedPassiveAbilities::HandleOnAbilityGiven(FGameplayAbilitySpec& AbilitySpec)
{
	if(PendingAbilities.Contains(AbilitySpec.Handle) == false)
	{
		return;
	}

	UpdatePendingAbility();
}

void FArcItemInstance_GrantedPassiveAbilities::UpdatePendingAbility()
{
	for (const FGameplayAbilitySpecHandle& AbilityHandle : GrantedPassiveAbilities)
	{
		FGameplayAbilitySpec* Spec = ArcASC->FindAbilitySpecFromHandle(AbilityHandle);
		if (Spec == nullptr)
		{
			PendingAbilities.AddUnique(AbilityHandle);
			continue;
		}
			
		UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(Spec->GetPrimaryInstance());

		if (ArcAbility == nullptr)
		{
			PendingAbilities.AddUnique(AbilityHandle);
			continue;
		}

		ArcAbility->OnAddedToItemSlot(ArcASC->AbilityActorInfo.Get(), Spec, SlotId, Item);

		PendingAbilities.Remove(AbilityHandle);
		if (PendingAbilities.Num() == 0)
		{
			ArcASC->RemoveOnAbilityGiven(PendingAbilitiesHandle);
		}
	}
}
#endif

void FArcItemFragment_GrantedPassiveAbilities::OnItemAddedToSlot(const FArcItemData* InItem
	, const FGameplayTag& InSlotId) const
{
	FArcItemInstance_GrantedPassiveAbilities* PassiveAbilities = ArcItems::FindMutableInstance<FArcItemInstance_GrantedPassiveAbilities>(InItem);
	PassiveAbilities->ItemsStore = InItem->GetItemsStoreComponent();
	PassiveAbilities->ArcASC = PassiveAbilities->ItemsStore->GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	PassiveAbilities->Item = InItem;
	PassiveAbilities->SlotId = InSlotId;
	
	UpdatePendingAbility(InItem);

	if (PassiveAbilities->ArcASC != nullptr && PassiveAbilities->PendingAbilities.Num() > 0 && PassiveAbilities->PendingAbilitiesHandle.IsValid() == false)
	{
		PassiveAbilities->PendingAbilitiesHandle = PassiveAbilities->ArcASC->AddOnAbilityGiven(FArcGenericAbilitySpecDelegate::FDelegate::CreateRaw(
			this, &FArcItemFragment_GrantedPassiveAbilities::HandleOnAbilityGiven, InItem
		));
	}
	
	if (PassiveAbilities->ArcASC->GetOwnerRole() < ENetRole::ROLE_Authority)
	{
		return;
	}
	
	if (PassiveAbilities)
	{
		for (const TSubclassOf<UGameplayAbility>& GA : Abilities)
		{
			FGameplayAbilitySpec AbilitySpec(GA
				, 1
				, -1
				, InItem->GetItemsStoreComponent());

			FGameplayAbilitySpecHandle AbilityHandle = PassiveAbilities->ArcASC->GiveAbility(AbilitySpec);

			PassiveAbilities->GrantedPassiveAbilities.Add(AbilityHandle);

			FGameplayAbilitySpec* Spec = PassiveAbilities->ArcASC->FindAbilitySpecFromHandle(AbilityHandle);
			if (Spec == nullptr)
			{
				continue;
			}
		
			UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(Spec->GetPrimaryInstance());

			ArcAbility->OnAddedToItemSlot(PassiveAbilities->ArcASC->AbilityActorInfo.Get(), Spec, InSlotId, InItem);

			if (ArcAbility == nullptr)
			{
				PassiveAbilities->PendingAbilities.AddUnique(AbilityHandle);
				continue;
			}
		}
	}
}

void FArcItemFragment_GrantedPassiveAbilities::OnItemRemovedFromSlot(const FArcItemData* InItem
	, const FGameplayTag& InSlotId) const
{
	if (InItem->GetItemsStoreComponent()->GetOwnerRole() < ROLE_Authority)
	{
		return;
	}
	
	FArcItemInstance_GrantedPassiveAbilities* PassiveAbilities = ArcItems::FindMutableInstance<FArcItemInstance_GrantedPassiveAbilities>(InItem);
	if (PassiveAbilities->ArcASC->GetOwnerRole() < ENetRole::ROLE_Authority)
	{
		return;
	}

	for (const FGameplayAbilitySpecHandle& Handle : PassiveAbilities->GrantedPassiveAbilities)
	{
		PassiveAbilities->ArcASC->ClearAbility(Handle);
	}

	PassiveAbilities->GrantedPassiveAbilities.Empty();

}

void FArcItemFragment_GrantedPassiveAbilities::HandleOnAbilityGiven(FGameplayAbilitySpec& AbilitySpec
	, const FArcItemData* InItem) const
{
	FArcItemInstance_GrantedPassiveAbilities* PassiveAbilities = ArcItems::FindMutableInstance<FArcItemInstance_GrantedPassiveAbilities>(InItem);
	if(PassiveAbilities->PendingAbilities.Contains(AbilitySpec.Handle) == false)
	{
		return;
	}

	UpdatePendingAbility(InItem);
}

void FArcItemFragment_GrantedPassiveAbilities::UpdatePendingAbility(const FArcItemData* InItem) const
{
	FArcItemInstance_GrantedPassiveAbilities* PassiveAbilities = ArcItems::FindMutableInstance<FArcItemInstance_GrantedPassiveAbilities>(InItem);
	for (const FGameplayAbilitySpecHandle& AbilityHandle : PassiveAbilities->GrantedPassiveAbilities)
	{
		FGameplayAbilitySpec* Spec = PassiveAbilities->ArcASC->FindAbilitySpecFromHandle(AbilityHandle);
		if (Spec == nullptr)
		{
			PassiveAbilities->PendingAbilities.AddUnique(AbilityHandle);
			continue;
		}
			
		UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(Spec->GetPrimaryInstance());

		if (ArcAbility == nullptr)
		{
			PassiveAbilities->PendingAbilities.AddUnique(AbilityHandle);
			continue;
		}

		ArcAbility->OnAddedToItemSlot(PassiveAbilities->ArcASC->AbilityActorInfo.Get(), Spec, PassiveAbilities->SlotId, PassiveAbilities->Item);

		PassiveAbilities->PendingAbilities.Remove(AbilityHandle);
		if (PassiveAbilities->PendingAbilities.Num() == 0)
		{
			PassiveAbilities->ArcASC->RemoveOnAbilityGiven(PassiveAbilities->PendingAbilitiesHandle);
		}
	}
}
