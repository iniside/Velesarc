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



#include "ArcItemFragment_ActorAttachment.h"

#include "ArcItemFragment_ItemVisualAttachment.h"
#include "Engine/World.h"

#include "Components/SkeletalMeshComponent.h"
#include "Core/ArcCoreAssetManager.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Equipment/ArcEquipmentUtils.h"
#include "Equipment/ArcItemAttachmentComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"

#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"

bool FArcAttachmentHandler_Actor::HandleItemAddedToSlot(UArcItemAttachmentComponent* InAttachmentComponent
														, const FArcItemData* InItem
														, const FArcItemData* InOwnerItem) const
{
	ACharacter* Character = InAttachmentComponent->FindCharacter();

	if (Character == nullptr)
	{
		return false;
	}

	UArcItemDefinition* VisualItemDefinition = nullptr;
	
	// Items can be attached and/or grant attachment sockets.
	const FArcItemFragment_ActorAttachment* ActorAttachment = ArcEquipmentUtils::GetAttachmentFragment<FArcItemFragment_ActorAttachment>(InItem, VisualItemDefinition);

	if (!ActorAttachment)
	{
		return false;
	}
	
	FArcItemAttachment Attached = MakeItemAttachment(InAttachmentComponent, InItem, InOwnerItem);
	InAttachmentComponent->AddAttachedItem(Attached);
	
	return true;
}

void FArcAttachmentHandler_Actor::HandleItemRemovedFromSlot(UArcItemAttachmentComponent* InAttachmentComponent
															, const FArcItemData* InItem
															, const FGameplayTag& SlotId
															, const FArcItemData* InOwnerItem) const
{
	if (InAttachmentComponent->GetNetMode() != ENetMode::NM_DedicatedServer)
	{
		const FArcItemAttachment* Attachment = InAttachmentComponent->GetAttachment(InItem->GetItemId());
		if (Attachment == nullptr)
		{
			return;
		}
		UObject* AttachedObject = InAttachmentComponent->FindFirstAttachedObject(Attachment->ItemDefinition);
		AActor* AttachedActor = Cast<AActor>(AttachedObject);
		
		if (AttachedActor == nullptr)
		{
			return;
		}

		AttachedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		AttachedActor->SetActorHiddenInGame(true);
		AttachedActor->SetActorEnableCollision(false);
		AttachedActor->Destroy();
	}
	InAttachmentComponent->RemoveAttachment(InItem->GetItemId());
}

void FArcAttachmentHandler_Actor::HandleItemAttach(UArcItemAttachmentComponent* InAttachmentComponent
	, const FArcItemId InItemId
	, const FArcItemId InOwnerItem) const
{
	const FArcItemAttachment* ItemAttachment = InAttachmentComponent->GetAttachment(InItemId);
	if (!ItemAttachment)
	{
		return;
	}
	
	UObject* AttachedObject = InAttachmentComponent->FindFirstAttachedObject(ItemAttachment->ItemDefinition);
	if (AttachedObject)
	{
		return;
	}
	
	const FArcItemFragment_ActorAttachment* ActorAttachment = FindAttachmentFragment<FArcItemFragment_ActorAttachment>(*ItemAttachment);
	if (!ActorAttachment)
	{
		return;
	}
	
	TSubclassOf<AActor> ActorClass = nullptr;
	ActorClass = ActorAttachment->ActorClass.LoadSynchronous();
	
	if (ActorClass)
	{	
		ACharacter* OwnerCharacter = InAttachmentComponent->FindCharacter();

		USceneComponent* ParentComponent = FindAttachmentComponent(InAttachmentComponent, ItemAttachment);
		if (!ParentComponent)
		{
			return;
		}
		
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = OwnerCharacter->GetPlayerState();
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			
		AActor* SpawnedActor = InAttachmentComponent->GetWorld()->SpawnActor<AActor>(ActorClass
			, OwnerCharacter->GetActorLocation()
			, FRotator::ZeroRotator
			, SpawnParams);

		SpawnedActor->ForEachComponent(true, [](UActorComponent* Component)
		{
			Component->SetCanEverAffectNavigation(false);
		});
		
		SpawnedActor->AttachToComponent(ParentComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, ItemAttachment->SocketName);
		
		InAttachmentComponent->AddAttachmentForItem(ItemAttachment->ItemDefinition, SpawnedActor);
	}
}

void FArcAttachmentHandler_Actor::HandleItemAttachmentChanged(UArcItemAttachmentComponent* InAttachmentComponent
	, const FArcItemAttachment& ItemAttachment) const
{

	if (ItemAttachment.OldVisualItemDefinition == nullptr)
	{
		return;
	}

	if (ItemAttachment.OldVisualItemDefinition == ItemAttachment.VisualItemDefinition)
	{
		return;
	}
	
	UObject* AttachedObject = InAttachmentComponent->FindFirstAttachedObject(ItemAttachment.ItemDefinition);
	AActor* AttachedActor = Cast<AActor>(AttachedObject);
		
	if (AttachedActor)
	{
		AttachedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		AttachedActor->SetActorHiddenInGame(true);
		AttachedActor->SetActorEnableCollision(false);
		AttachedActor->Destroy();
	}

	const FArcItemFragment_ActorAttachment* ActorAttachment = FindAttachmentFragment<FArcItemFragment_ActorAttachment>(ItemAttachment);
	
	if (!ActorAttachment)
	{
		return;
	}

	TSubclassOf<AActor> ActorClass = nullptr;
	ActorClass = ActorAttachment->ActorClass.LoadSynchronous();
	if (ActorClass)
	{	
		ACharacter* OwnerCharacter = InAttachmentComponent->FindCharacter();
			
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = OwnerCharacter->GetPlayerState();
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			
		AActor* SpawnedActor = InAttachmentComponent->GetWorld()->SpawnActor<AActor>(ActorClass
			, OwnerCharacter->GetActorLocation()
			, FRotator::ZeroRotator
			, SpawnParams);
			
		USceneComponent* AttachTo = OwnerCharacter->GetMesh();
		SpawnedActor->AttachToComponent(AttachTo, FAttachmentTransformRules::SnapToTargetNotIncludingScale, ItemAttachment.SocketName);
		
		InAttachmentComponent->AddAttachmentForItem(ItemAttachment.ItemDefinition, SpawnedActor);
	}
}

void FArcAttachmentHandler_Actor::HandleOnItemLoaded(UArcItemAttachmentComponent* InAttachmentComponent, const FArcItemId InItemId) const
{

}
