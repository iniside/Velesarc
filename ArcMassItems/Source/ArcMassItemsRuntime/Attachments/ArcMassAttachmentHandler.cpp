// Copyright Lukasz Baran. All Rights Reserved.

#include "Attachments/ArcMassAttachmentHandler.h"

#include "MassActorSubsystem.h"
#include "MassEntityManager.h"
#include "Mass/EntityFragments.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_SocketSlots.h"
#include "Equipment/Fragments/ArcItemFragment_ItemAttachmentSlots.h"
#include "Equipment/Fragments/ArcItemFragment_ItemVisualAttachment.h"
#include "Equipment/ArcAttachmentHandler.h"
#include "Core/ArcCoreAssetManager.h"
#include "GameFramework/Character.h"

AActor* FArcMassAttachmentHandlerCommon::FindActor(FMassEntityManager& EntityManager, FMassEntityHandle Entity) const
{
	FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
	if (!ActorFragment)
	{
		return nullptr;
	}
	return ActorFragment->GetMutable();
}

USceneComponent* FArcMassAttachmentHandlerCommon::FindAttachmentComponent(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	const FArcMassItemAttachment* ItemAttachment) const
{
	if (!ItemAttachment)
	{
		return nullptr;
	}

	AActor* Actor = FindActor(EntityManager, Entity);
	if (!Actor)
	{
		return nullptr;
	}

	if (!ComponentTag.IsNone())
	{
		return Actor->FindComponentByTag<USceneComponent>(ComponentTag);
	}

	if (ACharacter* Character = Cast<ACharacter>(Actor))
	{
		return Character->GetMesh();
	}

	return Actor->GetRootComponent();
}

FName FArcMassAttachmentHandlerCommon::FindSocketName(
	const FArcItemData* Item,
	const FArcItemData* OwnerItem,
	const FArcMassItemAttachmentStateFragment& State) const
{
	if (TransformFinder.IsValid())
	{
		const FArcAttachmentTransformFinder* Finder = TransformFinder.GetPtr<FArcAttachmentTransformFinder>();
		if (Finder)
		{
			return Finder->FindSocketName(nullptr, Item, OwnerItem);
		}
	}

	const FGameplayTag SlotId = Item ? Item->GetSlotId() : FGameplayTag::EmptyTag;
	const FArcMassItemAttachmentNameArray* Taken = State.TakenSockets.Find(SlotId);

	for (const FArcSocketArray& SocketArray : AttachmentData)
	{
		if (SocketArray.RequiredTags.Num() == 0)
		{
			if (!Taken || !Taken->Names.Contains(SocketArray.SocketName))
			{
				return SocketArray.SocketName;
			}
		}
	}

	return NAME_None;
}

FTransform FArcMassAttachmentHandlerCommon::FindRelativeTransform(const FArcItemData* Item, const FArcItemData* OwnerItem) const
{
	if (TransformFinder.IsValid())
	{
		const FArcAttachmentTransformFinder* Finder = TransformFinder.GetPtr<FArcAttachmentTransformFinder>();
		if (Finder)
		{
			return Finder->FindRelativeTransform(nullptr, Item, OwnerItem);
		}
	}

	return FTransform::Identity;
}

FArcMassItemAttachment FArcMassAttachmentHandlerCommon::MakeItemAttachment(
	const FArcItemData* Item,
	const FArcItemData* OwnerItem,
	const FArcMassItemAttachmentStateFragment& State) const
{
	FName AttachSocket = FindSocketName(Item, OwnerItem, State);

	// Nested attachment: owner-side slot config can override the socket.
	const FArcMassAttachmentHandlerCommon* OwnerHandler =
		FindAttachmentHandlerOnOwnerItem(Item, OwnerItem, SupportedItemFragment());
	if (OwnerHandler)
	{
		AttachSocket = OwnerHandler->FindSocketName(Item, OwnerItem, State);
	}

	FArcMassItemAttachment Attachment;
	Attachment.SlotId = Item ? Item->GetSlotId() : FGameplayTag::EmptyTag;
	Attachment.OwnerSlotId = OwnerItem ? OwnerItem->GetSlotId() : FGameplayTag::EmptyTag;
	Attachment.ItemId = Item ? Item->GetItemId() : FArcItemId();
	Attachment.OwnerId = OwnerItem ? OwnerItem->GetItemId() : FArcItemId();
	Attachment.ItemDefinition = Item ? Item->GetItemDefinition() : nullptr;
	Attachment.OwnerItemDefinition = OwnerItem ? OwnerItem->GetItemDefinition() : nullptr;
	Attachment.SocketName = AttachSocket;
	Attachment.RelativeTransform = FindRelativeTransform(Item, OwnerItem);
	Attachment.SocketComponentTag = ComponentTag;
	Attachment.VisualItemDefinition = GetVisualItem(Item);
	return Attachment;
}

FName FArcMassAttachmentHandlerCommon::FindFinalAttachSocket(const FArcMassItemAttachment* ItemAttachment) const
{
	if (!ItemAttachment)
	{
		return NAME_None;
	}

	return ItemAttachment->SocketName.IsNone() ? NAME_None : ItemAttachment->SocketName;
}

UArcItemDefinition* FArcMassAttachmentHandlerCommon::GetVisualItem(const FArcItemData* Item) const
{
	if (!Item)
	{
		return nullptr;
	}

	const FArcItemFragment_ItemVisualAttachment* VisualFrag =
		ArcItemsHelper::GetFragment<FArcItemFragment_ItemVisualAttachment>(Item);
	const FArcItemInstance_ItemVisualAttachment* VisualInst =
		ArcItemsHelper::FindInstance<FArcItemInstance_ItemVisualAttachment>(Item);

	if (VisualInst && VisualInst->VisualItem.IsValid())
	{
		return UArcCoreAssetManager::GetAsset<UArcItemDefinition>(VisualInst->VisualItem);
	}
	if (VisualFrag && VisualFrag->DefaultVisualItem.IsValid())
	{
		return UArcCoreAssetManager::GetAsset<UArcItemDefinition>(VisualFrag->DefaultVisualItem);
	}
	return nullptr;
}

const FArcMassAttachmentHandlerCommon* FArcMassAttachmentHandlerCommon::FindAttachmentHandlerOnOwnerItem(
	const FArcItemData* Item,
	const FArcItemData* OwnerItem,
	UScriptStruct* Type) const
{
	if (!OwnerItem || !Item)
	{
		return nullptr;
	}

	const UArcItemDefinition* OwnerDef = OwnerItem->GetItemDefinition();
	const FArcItemFragment_ItemAttachmentSlots* Slots = OwnerDef
		? OwnerDef->FindFragment<FArcItemFragment_ItemAttachmentSlots>()
		: nullptr;

	if (!Slots)
	{
		return nullptr;
	}

	for (const FArcItemAttachmentSlot& Slot : Slots->AttachmentSlots)
	{
		if (Slot.SlotId == Item->GetSlotId())
		{
			for (const FInstancedStruct& Handler : Slot.Handlers)
			{
				if (const FArcMassAttachmentHandlerCommon* CommonHandler = Handler.GetPtr<FArcMassAttachmentHandlerCommon>())
				{
					if (CommonHandler->SupportedItemFragment() == Type)
					{
						return CommonHandler;
					}
				}
			}
		}
	}

	return nullptr;
}
