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



#include "ArcItemFragment_StaticMeshComponentAttachment.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "Equipment/ArcItemAttachmentComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"

#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"

bool FArcAttachmentHandler_ComponentStaticMesh::HandleItemAddedToSlot(UArcItemAttachmentComponent* InAttachmentComponent
															 , const FArcItemData* InItem
															 , const FArcItemData* InOwnerItem) const
{
	const FArcItemFragment_StaticMeshComponentAttachment* StaticFragment = ArcItems::GetFragment<FArcItemFragment_StaticMeshComponentAttachment>(InItem);
	if (StaticFragment == nullptr)
	{
		return false;
	}
	
	APlayerState* PS = Cast<APlayerState>(InAttachmentComponent->GetOwner());
	ACharacter* C = PS->GetPawn<ACharacter>();
	if (C == nullptr)
	{
		return false;
	}

	FArcItemAttachment Attached = MakeItemAttachment(InAttachmentComponent, InItem, InOwnerItem);
	
	InAttachmentComponent->AddAttachedItem(Attached);
	
	return true;
}

void FArcAttachmentHandler_ComponentStaticMesh::HandleItemAttach(UArcItemAttachmentComponent* InAttachmentComponent
	, const FArcItemId InItemId
	, const FArcItemId InOwnerItem) const
{
	const FArcItemAttachment* ItemAttachment = InAttachmentComponent->GetAttachment(InItemId);

	UObject* AttachedObject = InAttachmentComponent->FindFirstAttachedObject(ItemAttachment->ItemDefinition);
	if (AttachedObject)
	{
		return;
	}
	
	const FArcItemFragment_StaticMeshComponentAttachment* Fragment = FindAttachmentFragment<FArcItemFragment_StaticMeshComponentAttachment>(*ItemAttachment);
	
	if (TSubclassOf<UStaticMeshComponent> SceneComponentClass = Fragment->StaticMeshAttachClass.LoadSynchronous())
	{
		FName AttachSocket = ItemAttachment->SocketName;

		AActor* OwnerCharacter = InAttachmentComponent->FindCharacter();
		
		UStaticMeshComponent* SpawnedComponent = SpawnComponent<UStaticMeshComponent>(OwnerCharacter, InAttachmentComponent, ItemAttachment, SceneComponentClass);
		if (SpawnedComponent == nullptr)
		{
			return;
		}
		
		InAttachmentComponent->AddAttachmentForItem(ItemAttachment->ItemDefinition, SpawnedComponent);
		InAttachmentComponent->CallbackItemAttached(InItemId);
	}
}