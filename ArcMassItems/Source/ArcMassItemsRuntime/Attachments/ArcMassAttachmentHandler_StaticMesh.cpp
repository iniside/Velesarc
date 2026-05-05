// Copyright Lukasz Baran. All Rights Reserved.

#include "Attachments/ArcMassAttachmentHandler_StaticMesh.h"

#include "MassActorSubsystem.h"
#include "MassEntityManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Equipment/Fragments/ArcItemFragment_StaticMeshAttachment.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinition.h"

bool FArcMassAttachmentHandler_StaticMesh::HandleItemAddedToSlot(
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
	const FArcItemFragment_StaticMeshAttachment* Fragment = ItemDef
		? ItemDef->FindFragment<FArcItemFragment_StaticMeshAttachment>()
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

void FArcMassAttachmentHandler_StaticMesh::HandleItemAttach(
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

	const FArcItemFragment_StaticMeshAttachment* Fragment = FindAttachmentFragment<FArcItemFragment_StaticMeshAttachment>(ItemAttachment);
	if (!Fragment)
	{
		return;
	}

	AActor* OwnerActor = FindActor(EntityManager, Entity);
	if (!OwnerActor)
	{
		return;
	}

	if (UStaticMesh* Mesh = Fragment->StaticMeshAttachClass.LoadSynchronous())
	{
		UStaticMeshComponent* SpawnedComponent = SpawnComponent<UStaticMeshComponent>(OwnerActor, EntityManager, Entity, &ItemAttachment);
		if (!SpawnedComponent)
		{
			return;
		}

		SpawnedComponent->SetStaticMesh(Mesh);
		State.ObjectsAttachedFromItem.FindOrAdd(ItemId).Objects.Add(SpawnedComponent);
	}
}

void FArcMassAttachmentHandler_StaticMesh::HandleItemDetach(
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
