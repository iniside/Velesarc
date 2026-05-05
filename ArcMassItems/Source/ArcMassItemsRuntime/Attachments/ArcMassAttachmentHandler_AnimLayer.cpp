// Copyright Lukasz Baran. All Rights Reserved.

#include "Attachments/ArcMassAttachmentHandler_AnimLayer.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "MassActorSubsystem.h"
#include "MassEntityManager.h"
#include "Equipment/Fragments/ArcItemFragment_AnimLayer.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinition.h"
#include "Equipment/ArcItemAttachmentComponent.h"

bool FArcMassAttachmentHandler_AnimLayer::HandleItemAddedToSlot(
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

	const FArcItemFragment_AnimLayer* Fragment = Item->GetItemDefinition()->FindFragment<FArcItemFragment_AnimLayer>();
	if (!Fragment)
	{
		return false;
	}

	FArcMassItemAttachment Attachment = MakeItemAttachment(Item, OwnerItem, State);
	State.ActiveAttachments.Add(Attachment);
	return true;
}

void FArcMassAttachmentHandler_AnimLayer::HandleItemAttach(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FArcMassItemAttachmentStateFragment& State,
	const FArcItemId ItemId,
	const FArcItemId OwnerItem) const
{
	AActor* Actor = FindActor(EntityManager, Entity);
	if (!Actor)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(Actor);
	if (!Character || !Character->GetMesh())
	{
		return;
	}

	const int32 Index = State.ActiveAttachments.IndexOfByPredicate(
		[&ItemId](const FArcMassItemAttachment& A) { return A.ItemId == ItemId; });
	if (Index == INDEX_NONE)
	{
		return;
	}

	const FArcMassItemAttachment& ItemAttachment = State.ActiveAttachments[Index];
	const UArcItemDefinition* ItemDef = ItemAttachment.ItemDefinition.Get();
	if (!ItemDef)
	{
		return;
	}

	const FArcItemFragment_AnimLayer* Fragment = ItemDef->FindFragment<FArcItemFragment_AnimLayer>();
	if (!Fragment)
	{
		return;
	}

	UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	for (const TSoftClassPtr<UAnimInstance>& AnimLayer : Fragment->AnimLayerToLink)
	{
		UClass* LayerClass = AnimLayer.LoadSynchronous();
		if (LayerClass)
		{
			AnimInstance->LinkAnimClassLayers(LayerClass);

			UAnimInstance* LinkedInstance = AnimInstance->GetLinkedAnimLayerInstanceByClass(LayerClass);
			if (IArcAnimLayerItemInterface* LinkedLayer = Cast<IArcAnimLayerItemInterface>(LinkedInstance))
			{
				LinkedLayer->SetSourceItemDefinition(ItemDef);
			}
		}
	}
}

void FArcMassAttachmentHandler_AnimLayer::HandleItemDetach(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FArcMassItemAttachmentStateFragment& State,
	const FArcMassItemAttachment& Snapshot) const
{
	AActor* Actor = FindActor(EntityManager, Entity);
	if (!Actor)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(Actor);
	if (!Character || !Character->GetMesh())
	{
		return;
	}

	const UArcItemDefinition* Definition = Snapshot.ItemDefinition.Get();
	if (!Definition)
	{
		return;
	}

	const FArcItemFragment_AnimLayer* Fragment = Definition->FindFragment<FArcItemFragment_AnimLayer>();
	if (!Fragment)
	{
		return;
	}

	UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	for (const TSoftClassPtr<UAnimInstance>& AnimLayer : Fragment->AnimLayerToLink)
	{
		if (UClass* LayerClass = AnimLayer.LoadSynchronous())
		{
			AnimInstance->UnlinkAnimClassLayers(LayerClass);
		}
	}
}

void FArcMassAttachmentHandler_AnimLayer::HandleItemRemovedFromSlot(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Entity,
	FArcMassItemAttachmentStateFragment& State,
	const FArcMassItemAttachment& Snapshot) const
{
	// No state-side cleanup specific to AnimLayer; visual unlink runs in Phase 2.
}
