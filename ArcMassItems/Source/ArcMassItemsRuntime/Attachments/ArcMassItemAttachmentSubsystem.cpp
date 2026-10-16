// Copyright Lukasz Baran. All Rights Reserved.

#include "Attachments/ArcMassItemAttachmentSubsystem.h"

#include "Attachments/ArcMassItemAttachmentFragments.h"
#include "Attachments/ArcMassItemAttachmentConfig.h"
#include "Attachments/ArcMassItemAttachmentSlots.h"
#include "Attachments/ArcMassAttachmentHandler.h"
#include "Events/ArcMassItemEventSubsystem.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinitionData.h"

#include "MassActorSubsystem.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"

// ---------------------------------------------------------------------------
// File-scoped helpers (unique namespace — mandatory per CLAUDE.md)
// ---------------------------------------------------------------------------

namespace ArcMassItemsAttachmentSubsystem_Internal
{
	static bool HandlerSupportsItem(const FArcMassAttachmentHandler* Handler, const FArcItemData* ItemData)
	{
		if (!Handler || !ItemData) return false;
		UScriptStruct* Required = Handler->SupportedItemFragment();
		if (!ensureMsgf(Required, TEXT("Attachment handler has no SupportedItemFragment override"))) return false;
		return ItemData->FindFragment(Required) != nullptr;
	}

	static bool HandlerSupportsDefinition(const FArcMassAttachmentHandler* Handler, const FPrimaryAssetId& DefinitionId)
	{
		const FArcItemDefinitionData* Def = FArcItemDefinitionData::Find(DefinitionId);
		if (!Handler || !Def) return false;
		UScriptStruct* Required = Handler->SupportedItemFragment();
		if (!ensureMsgf(Required, TEXT("Attachment handler has no SupportedItemFragment override"))) return false;
		return Def->GetFragment(Required) != nullptr;
	}
}

// ---------------------------------------------------------------------------
// Initialize / Deinitialize
// ---------------------------------------------------------------------------

void UArcMassItemAttachmentSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UMassEntitySubsystem>();
	Collection.InitializeDependency<UArcMassItemEventSubsystem>();

	EventSubsystem = GetWorld()->GetSubsystem<UArcMassItemEventSubsystem>();
	if (!EventSubsystem)
	{
		return;
	}

	// One subscription to the generic slot rail covers assign, clear, and move —
	// the drain fires OnItemSlotChanged for all three, so ReconcileEntity runs once.
	HandleSlotChanged = EventSubsystem->OnItemSlotChanged.AddUObject(
		this, &UArcMassItemAttachmentSubsystem::OnItemSlotChanged);
	HandleItemAttached = EventSubsystem->OnItemAttached.AddUObject(
		this, &UArcMassItemAttachmentSubsystem::OnItemAttached);
	HandleItemDetached = EventSubsystem->OnItemDetached.AddUObject(
		this, &UArcMassItemAttachmentSubsystem::OnItemDetached);
	HandleAttachmentChanged = EventSubsystem->OnItemAttachmentChanged.AddUObject(
		this, &UArcMassItemAttachmentSubsystem::OnItemAttachmentChanged);
	HandleItemRemoved = EventSubsystem->OnItemRemoved.AddUObject(
		this, &UArcMassItemAttachmentSubsystem::OnItemRemoved);
}

void UArcMassItemAttachmentSubsystem::Deinitialize()
{
	if (EventSubsystem)
	{
		EventSubsystem->OnItemSlotChanged.Remove(HandleSlotChanged);
		EventSubsystem->OnItemAttached.Remove(HandleItemAttached);
		EventSubsystem->OnItemDetached.Remove(HandleItemDetached);
		EventSubsystem->OnItemAttachmentChanged.Remove(HandleAttachmentChanged);
		EventSubsystem->OnItemRemoved.Remove(HandleItemRemoved);
		EventSubsystem = nullptr;
	}

	EntityManager = nullptr;
}

// ---------------------------------------------------------------------------
// Lazy cache
// ---------------------------------------------------------------------------

bool UArcMassItemAttachmentSubsystem::EnsureCached()
{
	if (!EntityManager)
	{
		UMassEntitySubsystem* MassSub = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
		if (MassSub)
		{
			EntityManager = &MassSub->GetMutableEntityManager();
		}
	}
	return EntityManager != nullptr;
}

// ---------------------------------------------------------------------------
// TeardownEntity
// ---------------------------------------------------------------------------

void UArcMassItemAttachmentSubsystem::TeardownEntity(FMassEntityHandle Entity)
{
	if (!EnsureCached()) return;
	if (!EntityManager->IsEntityValid(Entity)) return;

	FStructView SV = EntityManager->GetMutableSparseElementDataForEntity(
		FArcMassItemAttachmentStateFragment::StaticStruct(), Entity);
	FArcMassItemAttachmentStateFragment* State = SV.GetPtr<FArcMassItemAttachmentStateFragment>();
	if (!State) return;

	for (TPair<FArcItemId, FArcMassItemAttachmentObjectArray>& Pair : State->ObjectsAttachedFromItem)
	{
		for (TWeakObjectPtr<UObject>& ObjPtr : Pair.Value.Objects)
		{
			if (!ObjPtr.IsValid()) continue;
			UObject* Obj = ObjPtr.Get();
			if (UActorComponent* Comp = Cast<UActorComponent>(Obj))
			{
				Comp->DestroyComponent();
			}
			else if (AActor* Actor = Cast<AActor>(Obj))
			{
				Actor->Destroy();
			}
		}
	}

	State->ObjectsAttachedFromItem.Empty();
	State->ActiveAttachments.Empty();
	State->TakenSockets.Empty();
}

// ---------------------------------------------------------------------------
// DispatchRemovedHandlers
// ---------------------------------------------------------------------------

void UArcMassItemAttachmentSubsystem::DispatchRemovedHandlers(
	const FArcMassItemAttachmentConfigFragment& Config,
	FMassEntityHandle Entity,
	FArcMassItemAttachmentStateFragment& State,
	const FArcMassItemAttachment& Snapshot)
{
	const FArcMassItemAttachmentSlot* Slot = Config.Config->FindSlot(Snapshot.SlotId);
	if (!Slot) return;

	// Remove socket from TakenSockets.
	FArcMassItemAttachmentNameArray* Sockets = State.TakenSockets.Find(Snapshot.SlotId);
	if (Sockets)
	{
		Sockets->Names.Remove(Snapshot.SocketName);
	}

	for (const FInstancedStruct& HandlerInst : Slot->Handlers)
	{
		const FArcMassAttachmentHandler* Handler = HandlerInst.GetPtr<FArcMassAttachmentHandler>();
		if (!ArcMassItemsAttachmentSubsystem_Internal::HandlerSupportsDefinition(Handler, Snapshot.ItemDefinition)) continue;
		Handler->HandleItemRemovedFromSlot(*EntityManager, Entity, State, Snapshot);
	}
}

// ---------------------------------------------------------------------------
// EvictActiveItem
// ---------------------------------------------------------------------------

bool UArcMassItemAttachmentSubsystem::EvictActiveItem(
	FMassEntityHandle Entity,
	FArcItemId ItemId,
	const FArcMassItemAttachmentConfigFragment& Config,
	FArcMassItemAttachmentStateFragment& State)
{
	const int32 Index = State.ActiveAttachments.IndexOfByPredicate(
		[ItemId](const FArcMassItemAttachment& A) { return A.ItemId == ItemId; });
	if (Index == INDEX_NONE) return false;

	FArcMassItemAttachment Snapshot = State.ActiveAttachments[Index];
	State.ActiveAttachments.RemoveAt(Index);

	DispatchRemovedHandlers(Config, Entity, State, Snapshot);

	const FArcMassItemAttachmentSlot* Slot = Config.Config->FindSlot(Snapshot.SlotId);
	if (Slot)
	{
		for (const FInstancedStruct& HandlerInst : Slot->Handlers)
		{
			const FArcMassAttachmentHandler* Handler = HandlerInst.GetPtr<FArcMassAttachmentHandler>();
			if (!ArcMassItemsAttachmentSubsystem_Internal::HandlerSupportsDefinition(Handler, Snapshot.ItemDefinition)) continue;
			Handler->HandleItemDetach(*EntityManager, Entity, State, Snapshot);
		}
	}

	return true;
}

// ---------------------------------------------------------------------------
// ReconcileEntity — ports ProcessSlotChangedForEntity
// ---------------------------------------------------------------------------

void UArcMassItemAttachmentSubsystem::ReconcileEntity(
	FMassEntityHandle Entity,
	const FArcMassItemAttachmentConfigFragment& Config,
	FArcMassItemAttachmentStateFragment& State)
{
	if (!Config.Config) return;

	// Stale sweep runs ONCE across ALL interested stores — ActiveAttachments is not
	// partitioned by store, so a per-store sweep would evict the other stores' records.
	{
		// First pass: collect each live item's effective slot across every interested store.
		TMap<FArcItemId, FGameplayTag> LiveEffectiveSlots;
		for (const TObjectPtr<UScriptStruct>& StoreTypePtr : Config.InterestedStoreTypes)
		{
			const UScriptStruct* StoreType = StoreTypePtr.Get();
			if (!StoreType) continue;

			FConstStructView StoreView = EntityManager->GetSparseElementDataForEntity(StoreType, Entity);
			const FArcMassItemStoreFragment* Store = StoreView.GetPtr<FArcMassItemStoreFragment>();
			if (!Store) continue;

			for (const FArcMassReplicatedItem& RepItem : Store->ReplicatedItems.Items)
			{
				const FArcItemData* ItemData = RepItem.ToItem();
				if (!ItemData) continue;

				const FGameplayTag CurrentTop = ItemData->GetSlotId();
				const FGameplayTag CurrentEffective = CurrentTop.IsValid() ? CurrentTop : ItemData->GetAttachSlot();
				LiveEffectiveSlots.Add(ItemData->GetItemId(), CurrentEffective);
			}
		}

		// Second pass: mark stale (item gone from every store, or effective slot mismatch).
		TArray<int32> StaleIndices;
		for (int32 i = 0; i < State.ActiveAttachments.Num(); ++i)
		{
			const FArcMassItemAttachment& Active = State.ActiveAttachments[i];
			const FGameplayTag* EffectiveSlot = LiveEffectiveSlots.Find(Active.ItemId);
			if (EffectiveSlot == nullptr || *EffectiveSlot != Active.SlotId)
			{
				StaleIndices.Add(i);
			}
		}

		// Third pass: remove from highest index down, then dispatch.
		for (int32 k = StaleIndices.Num() - 1; k >= 0; --k)
		{
			const int32 Idx = StaleIndices[k];
			FArcMassItemAttachment Snapshot = State.ActiveAttachments[Idx];
			State.ActiveAttachments.RemoveAt(Idx);

			DispatchRemovedHandlers(Config, Entity, State, Snapshot);

			const FArcMassItemAttachmentSlot* Slot = Config.Config->FindSlot(Snapshot.SlotId);
			if (Slot)
			{
				for (const FInstancedStruct& HandlerInst : Slot->Handlers)
				{
					const FArcMassAttachmentHandler* Handler = HandlerInst.GetPtr<FArcMassAttachmentHandler>();
					if (!ArcMassItemsAttachmentSubsystem_Internal::HandlerSupportsDefinition(
							Handler, Snapshot.ItemDefinition)) continue;
					Handler->HandleItemDetach(*EntityManager, Entity, State, Snapshot);
				}
			}
		}
	}

	for (const TObjectPtr<UScriptStruct>& StoreTypePtr : Config.InterestedStoreTypes)
	{
		const UScriptStruct* StoreType = StoreTypePtr.Get();
		if (!StoreType) continue;

		FConstStructView StoreView = EntityManager->GetSparseElementDataForEntity(StoreType, Entity);
		const FArcMassItemStoreFragment* Store = StoreView.GetPtr<FArcMassItemStoreFragment>();
		if (!Store) continue;

		// Add path: items in valid slots that aren't yet in ActiveAttachments.
		for (const FArcMassReplicatedItem& RepItem : Store->ReplicatedItems.Items)
		{
			const FArcItemData* ItemData = RepItem.ToItem();
			if (!ItemData) continue;

			const FGameplayTag SlotId = ItemData->GetSlotId();
			if (!SlotId.IsValid()) continue;

			if (State.ActiveAttachments.ContainsByPredicate(
					[ItemData](const FArcMassItemAttachment& A) { return A.ItemId == ItemData->GetItemId(); }))
			{
				continue;
			}

			const FArcMassItemAttachmentSlot* Slot = Config.Config->FindSlot(SlotId);
			if (!Slot) continue;

			const FArcItemData* OwnerData = nullptr;
			if (ItemData->GetOwnerId().IsValid())
			{
				const FArcMassReplicatedItem* OwnerRep =
					const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemData->GetOwnerId());
				OwnerData = OwnerRep ? OwnerRep->ToItem() : nullptr;
			}

			for (const FInstancedStruct& HandlerInst : Slot->Handlers)
			{
				const FArcMassAttachmentHandler* Handler = HandlerInst.GetPtr<FArcMassAttachmentHandler>();
				if (!ArcMassItemsAttachmentSubsystem_Internal::HandlerSupportsItem(Handler, ItemData)) continue;

				if (Handler->HandleItemAddedToSlot(*EntityManager, Entity, State, ItemData, OwnerData))
				{
					// Inline attach (Phase 1+2 collapsed — handler bails when actor absent,
					// same late-actor behavior as the processor).
					const int32 NewIdx = State.ActiveAttachments.IndexOfByPredicate(
						[ItemData](const FArcMassItemAttachment& A) { return A.ItemId == ItemData->GetItemId(); });
					if (NewIdx != INDEX_NONE)
					{
						Handler->HandleItemAttach(
							*EntityManager, Entity, State,
							ItemData->GetItemId(),
							State.ActiveAttachments[NewIdx].OwnerId);
					}
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Delegate callbacks
// ---------------------------------------------------------------------------

void UArcMassItemAttachmentSubsystem::OnItemSlotChanged(
	FMassEntityHandle Entity, FArcItemId /*ItemId*/, FGameplayTag /*NewSlot*/, const UScriptStruct* /*StoreType*/)
{
	if (!EnsureCached()) return;
	if (!EntityManager->IsEntityValid(Entity)) return;

	FMassEntityView View(*EntityManager, Entity);
	const FArcMassItemAttachmentConfigFragment* Config =
		View.GetConstSharedFragmentDataPtr<FArcMassItemAttachmentConfigFragment>();
	if (!Config || !Config->Config) return;

	FStructView SV = EntityManager->GetMutableSparseElementDataForEntity(
		FArcMassItemAttachmentStateFragment::StaticStruct(), Entity);
	FArcMassItemAttachmentStateFragment* State = SV.GetPtr<FArcMassItemAttachmentStateFragment>();
	if (!State) return;

	ReconcileEntity(Entity, *Config, *State);
}

void UArcMassItemAttachmentSubsystem::OnItemAttached(
	FMassEntityHandle Entity, FArcItemId AttachmentId, FGameplayTag /*AttachSlot*/, const UScriptStruct* /*StoreType*/)
{
	if (!EnsureCached()) return;
	if (!EntityManager->IsEntityValid(Entity)) return;

	FMassEntityView View(*EntityManager, Entity);
	const FArcMassItemAttachmentConfigFragment* Config =
		View.GetConstSharedFragmentDataPtr<FArcMassItemAttachmentConfigFragment>();
	if (!Config || !Config->Config) return;

	FStructView SV = EntityManager->GetMutableSparseElementDataForEntity(
		FArcMassItemAttachmentStateFragment::StaticStruct(), Entity);
	FArcMassItemAttachmentStateFragment* State = SV.GetPtr<FArcMassItemAttachmentStateFragment>();
	if (!State) return;

	// Port of ProcessAttachedForEntity: locate the item by AttachmentId across InterestedStoreTypes.
	for (const TObjectPtr<UScriptStruct>& StoreTypePtr : Config->InterestedStoreTypes)
	{
		const UScriptStruct* StoreType = StoreTypePtr.Get();
		if (!StoreType) continue;

		FConstStructView StoreView = EntityManager->GetSparseElementDataForEntity(StoreType, Entity);
		const FArcMassItemStoreFragment* Store = StoreView.GetPtr<FArcMassItemStoreFragment>();
		if (!Store) continue;

		const FArcMassReplicatedItem* RepItem =
			const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(AttachmentId);
		const FArcItemData* ItemData = RepItem ? RepItem->ToItem() : nullptr;
		if (!ItemData) continue;

		const FGameplayTag AttachSlotId = ItemData->GetAttachSlot();
		if (!AttachSlotId.IsValid()) continue;

		if (State->ActiveAttachments.ContainsByPredicate(
				[AttachmentId](const FArcMassItemAttachment& A) { return A.ItemId == AttachmentId; }))
		{
			continue;
		}

		const FArcMassItemAttachmentSlot* Slot = Config->Config->FindSlot(AttachSlotId);
		if (!Slot) continue;

		const FArcItemData* OwnerData = nullptr;
		if (ItemData->GetOwnerId().IsValid())
		{
			const FArcMassReplicatedItem* OwnerRep =
				const_cast<FArcMassReplicatedItemArray&>(Store->ReplicatedItems).FindById(ItemData->GetOwnerId());
			OwnerData = OwnerRep ? OwnerRep->ToItem() : nullptr;
		}

		for (const FInstancedStruct& HandlerInst : Slot->Handlers)
		{
			const FArcMassAttachmentHandler* Handler = HandlerInst.GetPtr<FArcMassAttachmentHandler>();
			if (!ArcMassItemsAttachmentSubsystem_Internal::HandlerSupportsItem(Handler, ItemData)) continue;

			if (Handler->HandleItemAddedToSlot(*EntityManager, Entity, *State, ItemData, OwnerData))
			{
				const int32 NewIdx = State->ActiveAttachments.IndexOfByPredicate(
					[AttachmentId](const FArcMassItemAttachment& A) { return A.ItemId == AttachmentId; });
				if (NewIdx != INDEX_NONE)
				{
					Handler->HandleItemAttach(
						*EntityManager, Entity, *State,
						AttachmentId,
						State->ActiveAttachments[NewIdx].OwnerId);
				}
			}
		}
	}
}

void UArcMassItemAttachmentSubsystem::OnItemDetached(
	FMassEntityHandle Entity, FArcItemId AttachmentId, const UScriptStruct* /*StoreType*/)
{
	if (!EnsureCached()) return;
	if (!EntityManager->IsEntityValid(Entity)) return;

	FMassEntityView View(*EntityManager, Entity);
	const FArcMassItemAttachmentConfigFragment* Config =
		View.GetConstSharedFragmentDataPtr<FArcMassItemAttachmentConfigFragment>();
	if (!Config || !Config->Config) return;

	FStructView SV = EntityManager->GetMutableSparseElementDataForEntity(
		FArcMassItemAttachmentStateFragment::StaticStruct(), Entity);
	FArcMassItemAttachmentStateFragment* State = SV.GetPtr<FArcMassItemAttachmentStateFragment>();
	if (!State) return;

	EvictActiveItem(Entity, AttachmentId, *Config, *State);
}

void UArcMassItemAttachmentSubsystem::OnItemRemoved(
	FMassEntityHandle Entity, FArcItemId ItemId, const UScriptStruct* /*StoreType*/)
{
	if (!EnsureCached()) return;
	if (!EntityManager->IsEntityValid(Entity)) return;

	FMassEntityView View(*EntityManager, Entity);
	const FArcMassItemAttachmentConfigFragment* Config =
		View.GetConstSharedFragmentDataPtr<FArcMassItemAttachmentConfigFragment>();
	if (!Config || !Config->Config) return;

	FStructView SV = EntityManager->GetMutableSparseElementDataForEntity(
		FArcMassItemAttachmentStateFragment::StaticStruct(), Entity);
	FArcMassItemAttachmentStateFragment* State = SV.GetPtr<FArcMassItemAttachmentStateFragment>();
	if (!State) return;

	EvictActiveItem(Entity, ItemId, *Config, *State);
}

void UArcMassItemAttachmentSubsystem::OnItemAttachmentChanged(
	FMassEntityHandle Entity, FArcItemId ItemId, const UScriptStruct* /*StoreType*/)
{
	if (!EnsureCached()) return;
	if (!EntityManager->IsEntityValid(Entity)) return;

	FMassEntityView View(*EntityManager, Entity);
	const FArcMassItemAttachmentConfigFragment* Config =
		View.GetConstSharedFragmentDataPtr<FArcMassItemAttachmentConfigFragment>();
	if (!Config || !Config->Config) return;

	FStructView SV = EntityManager->GetMutableSparseElementDataForEntity(
		FArcMassItemAttachmentStateFragment::StaticStruct(), Entity);
	FArcMassItemAttachmentStateFragment* State = SV.GetPtr<FArcMassItemAttachmentStateFragment>();
	if (!State) return;

	// Port of ProcessAttachmentChangedForEntity — keyed on ItemId from delegate.
	const int32 Index = State->ActiveAttachments.IndexOfByPredicate(
		[ItemId](const FArcMassItemAttachment& A) { return A.ItemId == ItemId; });
	if (Index == INDEX_NONE) return;

	const FArcMassItemAttachment& Attachment = State->ActiveAttachments[Index];
	const FArcMassItemAttachmentSlot* Slot = Config->Config->FindSlot(Attachment.SlotId);
	if (!Slot) return;

	for (const FInstancedStruct& HandlerInst : Slot->Handlers)
	{
		const FArcMassAttachmentHandler* Handler = HandlerInst.GetPtr<FArcMassAttachmentHandler>();
		if (!ArcMassItemsAttachmentSubsystem_Internal::HandlerSupportsDefinition(
				Handler, Attachment.ItemDefinition)) continue;
		Handler->HandleItemAttachmentChanged(*EntityManager, Entity, *State, Attachment);
	}
}
