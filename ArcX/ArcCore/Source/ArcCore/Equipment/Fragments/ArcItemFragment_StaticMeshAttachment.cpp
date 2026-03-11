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



#include "ArcItemFragment_StaticMeshAttachment.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "Equipment/ArcEquipmentUtils.h"
#include "Equipment/ArcItemAttachmentComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"

#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"

bool FArcAttachmentHandler_StaticMesh::HandleItemAddedToSlot(UArcItemAttachmentComponent* InAttachmentComponent
															 , const FArcItemData* InItem
															 , const FArcItemData* InOwnerItem) const
{
	UArcItemDefinition* VisualItemDefinition = nullptr;
	
	const FArcItemFragment_StaticMeshAttachment* Fragment = ArcEquipmentUtils::GetAttachmentFragment<FArcItemFragment_StaticMeshAttachment>(InItem, VisualItemDefinition);
	if (Fragment == nullptr && VisualItemDefinition == nullptr)
	{
		return false;
	}
	
	APlayerState* PS = Cast<APlayerState>(InAttachmentComponent->GetOwner());
	AActor* C = InAttachmentComponent->FindCharacter();
	if (C == nullptr)
	{
		return false;
	}

	FArcItemAttachment Attached = MakeItemAttachment(InAttachmentComponent, InItem, InOwnerItem);
	
	InAttachmentComponent->AddAttachedItem(Attached);
	
	return true;
}

void FArcAttachmentHandler_StaticMesh::HandleItemAttach(UArcItemAttachmentComponent* InAttachmentComponent
	, const FArcItemId InItemId
	, const FArcItemId InOwnerItem) const
{
	const FArcItemAttachment* ItemAttachment = InAttachmentComponent->GetAttachment(InItemId);

	UObject* AlreadyAttachedObject = InAttachmentComponent->FindFirstAttachedObject(ItemAttachment->ItemDefinition);
	if (AlreadyAttachedObject)
	{
		return;
	}

	const FArcItemFragment_StaticMeshAttachment* Fragment = FindAttachmentFragment<FArcItemFragment_StaticMeshAttachment>(*ItemAttachment);
	if (!Fragment)
	{
		return;
	}
	
	if (UStaticMesh* AttachedObject = Fragment->StaticMeshAttachClass.LoadSynchronous())
	{	
		AActor* OwnerCharacter = InAttachmentComponent->FindCharacter();

		UStaticMeshComponent* SpawnedComponent = SpawnComponent<UStaticMeshComponent>(OwnerCharacter, InAttachmentComponent, ItemAttachment);
		if (SpawnedComponent == nullptr)
		{
			return;
		}
		
		SpawnedComponent->SetStaticMesh(AttachedObject);

		InAttachmentComponent->AddAttachmentForItem(ItemAttachment->ItemDefinition, SpawnedComponent);
		
		InAttachmentComponent->CallbackItemAttached(InItemId);
	}
}

#if WITH_EDITOR
FArcFragmentDescription FArcItemFragment_StaticMeshAttachment::GetDescription(const UScriptStruct* InStruct) const
{
	FArcFragmentDescription Desc = FArcItemFragment::GetDescription(InStruct);
	Desc.CommonPairings = {
		FName(TEXT("FArcItemFragment_SocketSlots")),
		FName(TEXT("FArcItemFragment_ItemAttachmentSlots"))
	};
	Desc.Prerequisites = { FName(TEXT("UArcItemAttachmentComponent")) };
	Desc.UsageNotes = TEXT(
		"Attaches a static mesh to the character when equipped. "
		"AttachTags are matched against socket tag requirements to determine which socket to use. "
		"Uses FArcAttachmentHandler_StaticMesh for async loading and attachment. "
		"Simplest visual attachment — use for weapons, shields, or items with a single static mesh.");
	return Desc;
}
#endif // WITH_EDITOR