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

#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"
#include "Items/ArcItemsHelpers.h"

void FArcItemFragment_GrantedAbilities::OnItemAddedToSlot(const FArcItemData* InItem
														  , const FGameplayTag& InSlotId) const
{
	FArcItemInstance_GrantedAbilities* Instance = ArcItems::FindMutableInstance<FArcItemInstance_GrantedAbilities>(InItem);
	Instance->ItemsStore = InItem->GetItemsStoreComponent();
	Instance->ArcASC = Instance->ItemsStore->GetOwner()->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	Instance->Item = InItem;
	Instance->SlotId = InSlotId;
	
	UpdatePendingAbility(InItem);

	if (Instance->ArcASC != nullptr && Instance->PendingAbilities.Num() > 0 && Instance->PendingAbilitiesHandle.IsValid() == false)
	{
		Instance->PendingAbilitiesHandle = Instance->ArcASC->AddOnAbilityGiven(FArcGenericAbilitySpecDelegate::FDelegate::CreateRaw(
			this, &FArcItemFragment_GrantedAbilities::HandleOnAbilityGiven, InItem
		));
	}
	
	if (Instance->ArcASC->GetOwnerRole() < ENetRole::ROLE_Authority)
	{
		return;
	}
	
	for (const FArcAbilityEntry& Ability : Abilities)
	{
		FGameplayAbilitySpec AbilitySpec(Ability.GrantedAbility.GetDefaultObject()
			, 1
			, -1
			, Instance->ItemsStore);
		
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(Instance->SlotId);
		FArcAbilitySpecCustomData CustomData;
		CustomData.SourceItemDef = const_cast<UArcItemDefinition*>(InItem->GetItemDefinition());
		CustomData.SourceItemId = InItem->GetItemId();
		
		AbilitySpec.CustomData = FInstancedStruct::Make<FArcAbilitySpecCustomData>(CustomData);
		if (Ability.bAddInputTag)
		{
			if (Ability.InputTag.IsValid())
			{
				AbilitySpec.GetDynamicSpecSourceTags().AddTag(Ability.InputTag);
			}
			//else if (SlotInstance && SlotInstance->InputTag.IsValid())
			//{
			//	AbilitySpec.DynamicAbilityTags.AddTag(SlotInstance->InputTag);
			//}
		}

		FGameplayAbilitySpecHandle NewHandle = Instance->ArcASC->GiveAbility(AbilitySpec);

		Instance->GrantedAbilities.Add(NewHandle);

		FGameplayAbilitySpec* Spec = Instance->ArcASC->FindAbilitySpecFromHandle(NewHandle);
		if (Spec == nullptr)
		{
			continue;
		}
		
		UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(Spec->GetPrimaryInstance());

		if (ArcAbility == nullptr)
		{
			Instance->PendingAbilities.AddUnique(NewHandle);
			continue;
		}

		ArcAbility->OnAddedToItemSlot(Instance->ArcASC->AbilityActorInfo.Get(), Spec, Instance->SlotId, Instance->Item);
	}
	
}

void FArcItemFragment_GrantedAbilities::OnItemRemovedFromSlot(const FArcItemData* InItem
	, const FGameplayTag& InSlotId) const
{
	if (InItem->GetItemsStoreComponent()->GetOwnerRole() < ROLE_Authority)
	{
		return;
	}
	
	FArcItemInstance_GrantedAbilities* Instance = ArcItems::FindMutableInstance<FArcItemInstance_GrantedAbilities>(InItem);
	for (const FGameplayAbilitySpecHandle& Handle : Instance->GrantedAbilities)
	{
		Instance->ArcASC->ClearAbility(Handle);
	}

	Instance->GrantedAbilities.Empty();
}

void FArcItemFragment_GrantedAbilities::HandleOnAbilityGiven(FGameplayAbilitySpec& AbilitySpec, const FArcItemData* InItem) const
{
	FArcItemInstance_GrantedAbilities* Instance = ArcItems::FindMutableInstance<FArcItemInstance_GrantedAbilities>(InItem);
	if(Instance->PendingAbilities.Contains(AbilitySpec.Handle) == false)
	{
		return;
	}

	UpdatePendingAbility(InItem);
}

void FArcItemFragment_GrantedAbilities::UpdatePendingAbility(const FArcItemData* InItem) const
{
	FArcItemInstance_GrantedAbilities* Instance = ArcItems::FindMutableInstance<FArcItemInstance_GrantedAbilities>(InItem);
	for (const FGameplayAbilitySpecHandle& AbilityHandle : Instance->GrantedAbilities)
	{
		FGameplayAbilitySpec* Spec = Instance->ArcASC->FindAbilitySpecFromHandle(AbilityHandle);
		if (Spec == nullptr)
		{
			Instance->PendingAbilities.AddUnique(AbilityHandle);
			continue;
		}
			
		UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(Spec->GetPrimaryInstance());

		if (ArcAbility == nullptr)
		{
			Instance->PendingAbilities.AddUnique(AbilityHandle);
			continue;
		}

		ArcAbility->OnAddedToItemSlot(Instance->ArcASC->AbilityActorInfo.Get(), Spec, Instance->SlotId, Instance->Item);

		Instance->PendingAbilities.Remove(AbilityHandle);
		if (Instance->PendingAbilities.Num() == 0)
		{
			Instance->ArcASC->RemoveOnAbilityGiven(Instance->PendingAbilitiesHandle);
		}
	}
}