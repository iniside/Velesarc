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



#include "ArcAttachmentHandler.h"

#include "ArcItemAttachmentComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/ArcCoreAssetManager.h"
#include "Fragments/ArcItemFragment_ItemVisualAttachment.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"

USceneComponent* FArcAttachmentHandlerCommon::FindAttachmentComponent(UArcItemAttachmentComponent* InAttachmentComponent, const FArcItemAttachment* ItemAttachment) const
{
	AActor* OwnerCharacter = InAttachmentComponent->FindCharacter();

	USceneComponent* ParentComponent = nullptr;

	if (ComponentTag.IsValid() && !ComponentTag.IsNone())
	{
		ParentComponent = OwnerCharacter->FindComponentByTag<USceneComponent>(ComponentTag);
	}
	if (ParentComponent == nullptr)
	{
		return nullptr;
	}

	if (!ItemAttachment->OwnerItemDefinition)
	{
		return ParentComponent;
	}

	const FArcItemAttachment* OwnerItemAttachment = InAttachmentComponent->GetAttachment(ItemAttachment->OwnerId);
	if (OwnerItemAttachment == nullptr)
	{
		return ParentComponent;
	}
	UObject* AttachmentTarget = InAttachmentComponent->FindFirstAttachedObject(ItemAttachment->OwnerItemDefinition);
	if (USceneComponent* TargetSceneComponent = Cast<USceneComponent>(AttachmentTarget))
	{
		return TargetSceneComponent;
	}
	
	return ParentComponent;
}

FName FArcAttachmentHandlerCommon::FindSocketName(UArcItemAttachmentComponent* InAttachmentComponent
	, const FArcItemData* InItem
	, const FArcItemData* InOwnerItem) const
{
	if (TransformFinder.IsValid())
	{
		return TransformFinder.GetPtr<FArcAttachmentTransformFinder>()->FindSocketName(InAttachmentComponent, InItem, InOwnerItem);
	}

	if (AttachmentData.Num() > 0)
	{
		return AttachmentData[0].SocketName;
	}

	return NAME_None;
}

FTransform FArcAttachmentHandlerCommon::FindRelativeTransform(UArcItemAttachmentComponent* InAttachmentComponent
	, const FArcItemData* InItem
	, const FArcItemData* InOwnerItem) const
{
	if (TransformFinder.IsValid())
	{
		return TransformFinder.GetPtr<FArcAttachmentTransformFinder>()->FindRelativeTransform(InAttachmentComponent, InItem, InOwnerItem);
	}

	return FTransform::Identity;
}

FArcItemAttachment FArcAttachmentHandlerCommon::MakeItemAttachment(UArcItemAttachmentComponent* InAttachmentComponent, const FArcItemData* InItem
																   , const FArcItemData* InOwnerItem) const
{
	FName AttachSocket = FindSocketName(InAttachmentComponent, InItem, InOwnerItem);

	const FArcAttachmentHandlerCommon* Handler = FindAttachmentHandlerOnOwnerItem(InItem, InOwnerItem, SupportedItemFragment());
	if (Handler)
	{
		AttachSocket = Handler->FindSocketName(InAttachmentComponent, InItem, InOwnerItem);
	}
	
	FArcItemAttachment Attached;
	Attached.SlotId = InItem->GetSlotId();
	Attached.OwnerSlotId = InOwnerItem != nullptr ? InOwnerItem->GetSlotId() : FGameplayTag::EmptyTag;
	Attached.ItemId = InItem->GetItemId();
	Attached.OwnerId = InOwnerItem != nullptr ? InOwnerItem->GetItemId() : FArcItemId::InvalidId;
	Attached.SocketName = AttachSocket;
	Attached.SocketComponentTag = ComponentTag;
	Attached.ItemDefinition = InItem->GetItemDefinition();
	Attached.OwnerItemDefinition = InOwnerItem != nullptr ? InOwnerItem->GetItemDefinition() : nullptr;
	Attached.RelativeTransform = FindRelativeTransform(InAttachmentComponent, InItem, InOwnerItem);
	Attached.SocketComponentTag = ComponentTag;
	Attached.VisualItemDefinition = GetVisualItem(InItem);
	return Attached;
}

FName FArcAttachmentHandlerCommon::FindFinalAttachSocket(const FArcItemAttachment* ItemAttachment) const
{
	const FName NewSocket = ItemAttachment->ChangedSocket != NAME_None ? ItemAttachment->ChangedSocket : ItemAttachment->SocketName;

	return NewSocket;
}

UArcItemDefinition* FArcAttachmentHandlerCommon::GetVisualItem(const FArcItemData* InItem) const
{
	const FArcItemFragment_ItemVisualAttachment* VisualItemData = ArcItemsHelper::GetFragment<FArcItemFragment_ItemVisualAttachment>(InItem);

	const FArcItemInstance_ItemVisualAttachment* VisualInstance = ArcItemsHelper::FindInstance<FArcItemInstance_ItemVisualAttachment>(InItem);
	UArcItemDefinition* VisualItemDefinition = nullptr;
	
	if (VisualInstance != nullptr)
	{
		if (VisualInstance->VisualItem.IsValid() == true)
		{
			VisualItemDefinition = UArcCoreAssetManager::GetAsset<UArcItemDefinition>(VisualInstance->VisualItem);
		}
		else if (VisualItemData != nullptr)
		{
			VisualItemDefinition = UArcCoreAssetManager::GetAsset<UArcItemDefinition>(VisualItemData->DefaultVisualItem);
		}
	}
	else if (VisualItemData != nullptr)
	{
		VisualItemDefinition = UArcCoreAssetManager::GetAsset<UArcItemDefinition>(VisualItemData->DefaultVisualItem);
	}

	return VisualItemDefinition;
}

const FArcAttachmentHandlerCommon* FArcAttachmentHandlerCommon::FindAttachmentHandlerOnOwnerItem(const FArcItemData* InItem, const FArcItemData* InOwnerItem, UScriptStruct* InType) const
{
	if (InOwnerItem == nullptr)
	{
		return nullptr;
	}

	const FArcItemFragment_ItemAttachmentSlots* Fragment = ArcItemsHelper::FindFragment<FArcItemFragment_ItemAttachmentSlots>(InOwnerItem);

	if (Fragment == nullptr)
	{
		return nullptr;
	}

	const FArcItemAttachmentSlot* SlotEntry = Fragment->FindSlotEntry(InItem->GetSlotId());

	for (const FInstancedStruct& IS : SlotEntry->Handlers)
	{
		if (IS.GetScriptStruct() != FArcAttachmentHandlerCommon::StaticStruct())
		{
			continue;
		}
		
		if (IS.GetPtr<FArcAttachmentHandlerCommon>()->SupportedItemFragment() == InType)
		{
			return IS.GetPtr<FArcAttachmentHandlerCommon>();
		}
	}

	return nullptr;
}
