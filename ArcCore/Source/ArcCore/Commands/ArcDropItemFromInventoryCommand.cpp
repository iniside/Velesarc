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

#include "Commands/ArcDropItemFromInventoryCommand.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Interaction/ArcCoreInteractionSubsystem.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsComponent.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_DropActor.h"

bool FArcDropItemFromInventoryCommand::CanSendCommand() const
{
	if (ItemsStore == nullptr || Item.IsValid() == false)
	{
		return false;
	}

	const FArcItemData* E = ItemsStore->GetItemPtr(Item);
	if (E == nullptr)
	{
		return false;
	}

	return true;
}

void FArcDropItemFromInventoryCommand::PreSendCommand()
{
}

bool FArcDropItemFromInventoryCommand::Execute()
{
	const FArcItemData* E = ItemsStore->GetItemPtr(Item);

	const FArcItemFragment_DropActor* DropActor = ArcItems::FindFragment<FArcItemFragment_DropActor>(E);
	if (DropActor == nullptr)
	{
		return false;
	}

	TArray<FArcCoreInteractionCustomData*> CustomData;
	CustomData.Reserve(DropActor->InteractionCustomData.Num());
	
	for (FInstancedStruct& IS : const_cast<FArcItemFragment_DropActor*>(DropActor)->InteractionCustomData)
	{
		void* Allocated = FMemory::Malloc(IS.GetScriptStruct()->GetCppStructOps()->GetSize()
				, IS.GetScriptStruct()->GetCppStructOps()->GetAlignment());
		IS.GetScriptStruct()->GetCppStructOps()->Construct(Allocated);
		IS.GetScriptStruct()->CopyScriptStruct(Allocated, IS.GetMemory());
		FArcCoreInteractionCustomData* Ptr = static_cast<FArcCoreInteractionCustomData*>(Allocated);
		
		CustomData.Add(Ptr);
	}
	
	UWorld* World = ItemsStore->GetWorld();
	APlayerState* PS = Cast<APlayerState>(ItemsStore->GetOwner());

	APawn* P = PS->GetPawn();
	FVector Location = P->GetActorLocation() + (P->GetActorForwardVector() * 150);

	UArcCoreInteractionSubsystem* IS = ItemsStore->GetWorld()->GetSubsystem<UArcCoreInteractionSubsystem>();

	const FGuid IntId = FGuid::NewGuid();
	IS->AddInteraction(IntId, MoveTemp(CustomData));
	
	ItemsStore->DestroyItem(Item);
	
	return true;
}