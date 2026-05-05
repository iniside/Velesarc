// Copyright Lukasz Baran. All Rights Reserved.

#include "Attachments/ArcMassAttachmentHandler_SkeletalMesh.h"

#include "MassActorSubsystem.h"
#include "MassEntityManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Equipment/Fragments/ArcItemFragment_SkeletalMeshAttachment.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinition.h"

bool FArcMassAttachmentHandler_SkeletalMesh::HandleItemAddedToSlot(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FArcMassItemAttachmentStateFragment& State,
	const FArcItemData* Item,
	const FArcItemData* OwnerItem) const
{
	if (!Item)
	{
		return false;
	}

	const UArcItemDefinition* ItemDef = Item->GetItemDefinition();
	const FArcItemFragment_SkeletalMeshAttachment* Fragment = ItemDef
		? ItemDef->FindFragment<FArcItemFragment_SkeletalMeshAttachment>()
		: nullptr;

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

void FArcMassAttachmentHandler_SkeletalMesh::HandleItemAttach(
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

	const FArcItemFragment_SkeletalMeshAttachment* Fragment = FindAttachmentFragment<FArcItemFragment_SkeletalMeshAttachment>(ItemAttachment);
	if (!Fragment)
	{
		return;
	}

	AActor* OwnerActor = FindActor(EntityManager, Entity);
	if (!OwnerActor)
	{
		return;
	}

	if (USkeletalMesh* Mesh = Fragment->SkeletalMesh.LoadSynchronous())
	{
		USkeletalMeshComponent* SpawnedComponent = SpawnComponent<USkeletalMeshComponent>(OwnerActor, EntityManager, Entity, &ItemAttachment);
		if (!SpawnedComponent)
		{
			return;
		}

		SpawnedComponent->SetSkeletalMesh(Mesh);
		State.ObjectsAttachedFromItem.FindOrAdd(ItemId).Objects.Add(SpawnedComponent);
	}
}

void FArcMassAttachmentHandler_SkeletalMesh::HandleItemDetach(
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
