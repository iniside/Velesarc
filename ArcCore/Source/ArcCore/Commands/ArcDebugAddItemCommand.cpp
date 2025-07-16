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

#include "ArcDebugAddItemCommand.h"

#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "GameFramework/PlayerState.h"
#include "Items/ArcItemsComponent.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemsStoreComponent.h"

bool FArcDebugAddItemCommand::CanSendCommand() const
{
	return true;
}

void FArcDebugAddItemCommand::PreSendCommand()
{
	APlayerState* PS = Character->GetPlayerState();
	UArcItemsStoreComponent* ItemsComponent = PS->FindComponentByClass<UArcItemsStoreComponent>();
}

bool FArcDebugAddItemCommand::Execute()
{
	if (Character)
	{
		APlayerState* PS = Character->GetPlayerState();
		UArcItemsStoreComponent* ItemsComponent = PS->FindComponentByClass<UArcItemsStoreComponent>();

		FArcItemSpec Spec;
		//Spec.SetItemId(FArcItemId::Generate()).
		//	 SetItemData(FSoftObjectPath(UArcItemDefinition::StaticClass()->GetDefaultObject<UArcItemDefinition>())).
		//	 SetItemLevel(1).AddItemStat({UArcTestAttributeSet::GetHealthAttribute(), 20.f}).
		//	 AddItemStat({UArcTestAttributeSet::GetMaxHealthAttribute(), 50.f}).
		//	 AddItemStat({UArcTestAttributeSet::GetDamageAttribute(), 10.f}).
		//	 AddGeneratedEffect(UGameplayEffect::StaticClass()).AddGeneratedEffect(UGameplayEffect::StaticClass()).
		//	 AddGeneratedEffect(UGameplayEffect::StaticClass()).AddGeneratedEffect(UGameplayEffect::StaticClass()).
		//	 AddGeneratedEffect(UGameplayEffect::StaticClass()).AddGeneratedEffect(UGameplayEffect::StaticClass()).
		//	 AddGeneratedEffect(UGameplayEffect::StaticClass()).AddGeneratedEffect(UGameplayEffect::StaticClass()).
		//	 AddGeneratedEffect(UGameplayEffect::StaticClass()).AddGeneratedEffect(UGameplayEffect::StaticClass()).
		//	 AddGeneratedPassiveAbilities(UGameplayAbility::StaticClass()).
		//	 AddGeneratedPassiveAbilities(UGameplayAbility::StaticClass()).
		//	 AddGeneratedPassiveAbilities(UGameplayAbility::StaticClass()).
		//	 AddGeneratedPassiveAbilities(UGameplayAbility::StaticClass()).
		//	 AddGeneratedPassiveAbilities(UGameplayAbility::StaticClass()).
		//	 AddGeneratedPassiveAbilities(UGameplayAbility::StaticClass()).
		//	 AddGeneratedPassiveAbilities(UGameplayAbility::StaticClass()).
		//	 AddGeneratedPassiveAbilities(UGameplayAbility::StaticClass()).
		//	 AddGeneratedPassiveAbilities(UGameplayAbility::StaticClass()).
		//	 AddGeneratedPassiveAbilities(UGameplayAbility::StaticClass()).AddGeneratedPassiveAbilities(
		//		 UGameplayAbility::StaticClass());
//
		//ItemsComponent->AddNewItem(Spec);
	}
	
	return true;
}

bool FArcDebugModifyItemCommand::CanSendCommand() const
{
	return true;
}

void FArcDebugModifyItemCommand::PreSendCommand()
{
	APlayerState* PS = Character->GetPlayerState();
	UArcItemsStoreComponent* ItemsComponent = PS->FindComponentByClass<UArcItemsStoreComponent>();
}

bool FArcDebugModifyItemCommand::Execute()
{
	APlayerState* PS = Character->GetPlayerState();
	UArcItemsStoreComponent* ItemsComponent = PS->FindComponentByClass<UArcItemsStoreComponent>();
	//ItemsComponent->GetItemsStore()->GetItemsArray().Edit().Edit(0).Rep++;
	//ItemsComponent->GetItemsStore()->GetItemsArray().Edit().Edit(0).Item.GetItem()->GetReplicatedItemStats()[0].
	//		FinalValue += 20;
	
	return true;
}
