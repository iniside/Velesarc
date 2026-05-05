// Copyright Lukasz Baran. All Rights Reserved.

#include "Attachments/ArcMassAttachmentHandler_VisualItem.h"

#include "MassActorSubsystem.h"
#include "MassEntityManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Equipment/Fragments/ArcItemFragment_ItemVisualAttachment.h"
#include "Equipment/Fragments/ArcItemFragment_StaticMeshAttachment.h"
#include "Equipment/Fragments/ArcItemFragment_SkeletalMeshAttachment.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemsHelpers.h"
#include "Core/ArcCoreAssetManager.h"

bool FArcMassAttachmentHandler_VisualItem::HandleItemAddedToSlot(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FArcMassItemAttachmentStateFragment& State,
	const FArcItemData* Item,
	const FArcItemData* OwnerItem) const
{
	if (!Item || !Item->GetItemDefinition())
	{
		return false;
	}

	const FArcItemFragment_ItemVisualAttachment* Fragment = Item->GetItemDefinition()->FindFragment<FArcItemFragment_ItemVisualAttachment>();
	if (!Fragment)
	{
		return false;
	}

	AActor* Actor = FindActor(EntityManager, Entity);
	if (!Actor)
	{
		return false;
	}

	FArcMassItemAttachment Attachment = MakeItemAttachment(Item, OwnerItem, State);
	State.ActiveAttachments.Add(Attachment);
	State.TakenSockets.FindOrAdd(Attachment.SlotId).Names.Add(Attachment.SocketName);

	return true;
}

void FArcMassAttachmentHandler_VisualItem::HandleItemAttach(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FArcMassItemAttachmentStateFragment& State,
	const FArcItemId ItemId,
	const FArcItemId OwnerItem) const
{
	const int32 Index = State.ActiveAttachments.IndexOfByPredicate(
		[&ItemId](const FArcMassItemAttachment& A) { return A.ItemId == ItemId; });
	if (Index == INDEX_NONE)
	{
		return;
	}

	FArcMassItemAttachment& ItemAttachment = State.ActiveAttachments[Index];

	if (State.ObjectsAttachedFromItem.Contains(ItemId))
	{
		return;
	}

	const UArcItemDefinition* VisualItemDef = ItemAttachment.VisualItemDefinition.Get();
	if (!VisualItemDef)
	{
		return;
	}

	AActor* OwnerActor = FindActor(EntityManager, Entity);
	if (!OwnerActor)
	{
		return;
	}

	if (const FArcItemFragment_StaticMeshAttachment* SMFrag = VisualItemDef->FindFragment<FArcItemFragment_StaticMeshAttachment>())
	{
		if (UStaticMesh* Mesh = SMFrag->StaticMeshAttachClass.LoadSynchronous())
		{
			UStaticMeshComponent* SpawnedComponent = SpawnComponent<UStaticMeshComponent>(OwnerActor, EntityManager, Entity, &ItemAttachment);
			if (SpawnedComponent)
			{
				SpawnedComponent->SetStaticMesh(Mesh);
				State.ObjectsAttachedFromItem.FindOrAdd(ItemId).Objects.Add(SpawnedComponent);
			}
		}
	}

	if (const FArcItemFragment_SkeletalMeshAttachment* SKFrag = VisualItemDef->FindFragment<FArcItemFragment_SkeletalMeshAttachment>())
	{
		if (USkeletalMesh* Mesh = SKFrag->SkeletalMesh.LoadSynchronous())
		{
			USkeletalMeshComponent* SpawnedComponent = SpawnComponent<USkeletalMeshComponent>(OwnerActor, EntityManager, Entity, &ItemAttachment);
			if (SpawnedComponent)
			{
				SpawnedComponent->SetSkeletalMesh(Mesh);
				if (SKFrag->SkeletalMeshAnimInstance.IsValid())
				{
					SpawnedComponent->SetAnimInstanceClass(SKFrag->SkeletalMeshAnimInstance.LoadSynchronous());
				}
				State.ObjectsAttachedFromItem.FindOrAdd(ItemId).Objects.Add(SpawnedComponent);
			}
		}
	}
}

void FArcMassAttachmentHandler_VisualItem::HandleItemDetach(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FArcMassItemAttachmentStateFragment& State,
	const FArcMassItemAttachment& Snapshot) const
{
	if (FArcMassItemAttachmentObjectArray* Objs = State.ObjectsAttachedFromItem.Find(Snapshot.ItemId))
	{
		for (const TWeakObjectPtr<UObject>& Obj : Objs->Objects)
		{
			if (UActorComponent* Comp = Cast<UActorComponent>(Obj.Get()))
			{
				Comp->DestroyComponent();
			}
		}
		State.ObjectsAttachedFromItem.Remove(Snapshot.ItemId);
	}
}
