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



#include "ArcStartInteractionCommand.h"

#include "ArcCoreInteractionSubsystem.h"
#include "Engine/World.h"
#include "ArcInteractionLevelPlacedComponent.h"
#include "ArcInteractionReceiverComponent.h"
#include "GameFramework/Character.h"
#include "Items/Factory/ArcLootSubsystem.h"

#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Items/ArcItemsStoreComponent.h"

bool FArcStartInteractionCommand::Execute()
{
	if (ReceiverComponent == nullptr)
	{
		return false;
	}
	
	UArcCoreInteractionSubsystem* Subsystem = ReceiverComponent->GetWorld()->GetSubsystem<UArcCoreInteractionSubsystem>();
	if (Subsystem)
	{
		
	}
	
	return true;
}

bool FArcInteractionChangeBoolStateCommand::Execute()
{
	if (ReceiverComponent == nullptr)
	{
		return false;
	}
	
	UArcCoreInteractionSubsystem* Subsystem = ReceiverComponent->GetWorld()->GetSubsystem<UArcCoreInteractionSubsystem>();
	if (Subsystem == nullptr)
	{
		return false;
	}

	FArcInteractionData_BoolState* BoolState = Subsystem->FindInteractionDataMutable<FArcInteractionData_BoolState>(InteractionId);
	if (BoolState == nullptr)
	{
		return false;
	}

	BoolState->bBoolState = bNewState;

	Subsystem->MarkInteractionChanged(InteractionId);
	
	return true;
}

bool FArcOpenRandomLootObjectInteractionCommand::Execute()
{
	if (ReceiverComponent == nullptr)
	{
		return false;
	}
	
	UArcCoreInteractionSubsystem* Subsystem = ReceiverComponent->GetWorld()->GetSubsystem<UArcCoreInteractionSubsystem>();
	if (Subsystem == nullptr)
	{
		return false;
	}

	
	UArcLootSubsystem* LootSubsystem = ReceiverComponent->GetWorld()->GetSubsystem<UArcLootSubsystem>();
	if (LootSubsystem == nullptr)
	{
		return false;
	}
	
	const FArcInteractionData_BoolState* ConstState = Subsystem->GetInteractionData<FArcInteractionData_BoolState>(InteractionId);
	if (ConstState == nullptr)
	{
		return false;
	}
	if (ConstState->bBoolState == true)
	{
		return false;
	}
	
	FArcInteractionData_LootTable* LootTable = Subsystem->FindInteractionDataMutable<FArcInteractionData_LootTable>(InteractionId);
	if (LootTable == nullptr)
	{
		return false;
	}

	FArcInteractionData_ItemSpecs* ItemSpecs = Subsystem->FindInteractionDataMutable<FArcInteractionData_ItemSpecs>(InteractionId);
	if (ItemSpecs == nullptr)
	{
		return false;
	}

	FArcInteractionData_BoolState* State = Subsystem->FindInteractionDataMutable<FArcInteractionData_BoolState>(InteractionId);
	State->bBoolState = true;

	UArcItemListTable* SpawnTable = LootTable->LootTable.LoadSynchronous();
	if (SpawnTable == nullptr)
	{
		return false;
	}

	ACharacter* Owner = ReceiverComponent->GetOwner<ACharacter>();
	if (Owner == nullptr)
	{
		return false;
	}
	
	APlayerController* PC = Owner->GetController<APlayerController>();
	if (PC == nullptr)
	{
		return false;
	}

	FArcSelectedItemSpecContainer Items = LootSubsystem->GenerateItemsFor(SpawnTable
		, From
		, PC);

	for (FArcItemSpec& Spec : Items.SelectedItems)
	{
		ItemSpecs->AddItem(MoveTemp(Spec));
	}
	
	Subsystem->MarkInteractionChanged(InteractionId);
	
	return true;
}

bool FArcPickItemFromInteractionCommand::Execute()
{
	UArcCoreInteractionSubsystem* InteractionSystem = ToComponent->GetWorld()->GetSubsystem<UArcCoreInteractionSubsystem>();

	const FArcInteractionData_ItemSpecs* ItemSpecsInteraction = InteractionSystem->GetInteractionData<FArcInteractionData_ItemSpecs>(InteractionId);
	if (ItemSpecsInteraction)
	{
		for (const FArcItemSpecEntry& Item : ItemSpecsInteraction->ItemSpec)
		{
			if (Item.Id == Id)
			{
				FArcItemId ItemId = ToComponent->AddItem(Item.ItemSpec, FArcItemId());
			}
		}
	}
	
	return true;
}

bool FArcPickAllItemsFromInteractionCommand::Execute()
{
	UArcCoreInteractionSubsystem* InteractionSystem = ToComponent->GetWorld()->GetSubsystem<UArcCoreInteractionSubsystem>();

	const FArcInteractionData_ItemSpecs* ItemSpecsInteraction = InteractionSystem->GetInteractionData<FArcInteractionData_ItemSpecs>(InteractionId);
	if (ItemSpecsInteraction)
	{
		for (const FArcItemSpecEntry& Item : ItemSpecsInteraction->ItemSpec)
		{
			FArcItemId ItemId = ToComponent->AddItem(Item.ItemSpec, FArcItemId());
			return true;
		}
	}
	
	return true;
}

bool FArcPickAllItemsAndRemoveInteractionCommand::Execute()
{
	UArcCoreInteractionSubsystem* InteractionSystem = ToComponent->GetWorld()->GetSubsystem<UArcCoreInteractionSubsystem>();

	const FArcInteractionData_ItemSpecs* ItemSpecsInteraction = InteractionSystem->GetInteractionData<FArcInteractionData_ItemSpecs>(InteractionId);
	if (ItemSpecsInteraction)
	{
		for (const FArcItemSpecEntry& Item : ItemSpecsInteraction->ItemSpec)
		{
			FArcItemId ItemId = ToComponent->AddItem(Item.ItemSpec, FArcItemId());
		}
	}

	InteractionSystem->RemoveInteraction(InteractionId, true);
	return true;
}
