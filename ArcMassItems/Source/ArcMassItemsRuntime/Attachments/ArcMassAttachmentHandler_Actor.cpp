// Copyright Lukasz Baran. All Rights Reserved.

#include "Attachments/ArcMassAttachmentHandler_Actor.h"

#include "MassActorSubsystem.h"
#include "MassEntityManager.h"
#include "Equipment/Fragments/ArcItemFragment_ActorAttachment.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinition.h"
#include "Engine/World.h"

bool FArcMassAttachmentHandler_Actor::HandleItemAddedToSlot(
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
	const FArcItemFragment_ActorAttachment* Fragment = ItemDef
		? ItemDef->FindFragment<FArcItemFragment_ActorAttachment>()
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

void FArcMassAttachmentHandler_Actor::HandleItemAttach(
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

	const FArcItemFragment_ActorAttachment* Fragment = FindAttachmentFragment<FArcItemFragment_ActorAttachment>(ItemAttachment);
	if (!Fragment)
	{
		return;
	}

	AActor* OwnerActor = FindActor(EntityManager, Entity);
	if (!OwnerActor)
	{
		return;
	}

	USceneComponent* ParentComponent = FindAttachmentComponent(EntityManager, Entity, &ItemAttachment);
	if (!ParentComponent)
	{
		return;
	}

	TSubclassOf<AActor> ActorClass = Fragment->ActorClass.LoadSynchronous();
	if (!ActorClass)
	{
		return;
	}

	UWorld* World = OwnerActor->GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	FName AttachSocketName = FindFinalAttachSocket(&ItemAttachment);

	AActor* SpawnedActor = World->SpawnActor<AActor>(
		ActorClass,
		OwnerActor->GetActorLocation(),
		FRotator::ZeroRotator,
		SpawnParams);

	if (!SpawnedActor)
	{
		return;
	}

	SpawnedActor->ForEachComponent(true, [](UActorComponent* Component)
	{
		Component->SetCanEverAffectNavigation(false);
	});

	SpawnedActor->AttachToComponent(
		ParentComponent,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		AttachSocketName);

	State.ObjectsAttachedFromItem.FindOrAdd(ItemId).Objects.Add(SpawnedActor);
}

void FArcMassAttachmentHandler_Actor::HandleItemDetach(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FArcMassItemAttachmentStateFragment& State,
	const FArcMassItemAttachment& Snapshot) const
{
	if (FArcMassItemAttachmentObjectArray* Objs = State.ObjectsAttachedFromItem.Find(Snapshot.ItemId))
	{
		for (const TWeakObjectPtr<UObject>& Obj : Objs->Objects)
		{
			if (AActor* AttachedActor = Cast<AActor>(Obj.Get()))
			{
				AttachedActor->Destroy();
			}
		}
		State.ObjectsAttachedFromItem.Remove(Snapshot.ItemId);
	}
}
