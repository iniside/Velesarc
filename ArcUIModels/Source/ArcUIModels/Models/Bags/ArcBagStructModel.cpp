// Copyright Lukasz Baran. All Rights Reserved.

#include "Models/Bags/ArcBagStructModel.h"

#include "ArcFrontendMVVMModelComponent.h"
#include "Bags/ArcBagDefinition.h"
#include "Bags/ArcBagEventSubsystem.h"
#include "Bags/ArcBagOperations.h"
#include "Bags/ArcBagStoreFragment.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_Icon.h"
#include "Items/Fragments/ArcItemFragment_UIDescription.h"
#include "MassAgentComponent.h"
#include "MassAgentSubsystem.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "StructUtils/StructView.h"
#include "ViewModels/Bags/ArcBagContainerViewModel.h"
#include "ViewModels/Bags/ArcBagEntries.h"

namespace ArcUIModelsBagStructModel
{
	FMassEntityHandle ResolveLocalPlayerEntity(UArcFrontendMVVMModelComponent& Component)
	{
		AActor* Owner = Component.GetOwner();
		APlayerState* PS = Cast<APlayerState>(Owner);
		if (PS == nullptr)
		{
			return FMassEntityHandle();
		}
		APawn* Pawn = PS->GetPawn();
		if (Pawn == nullptr)
		{
			return FMassEntityHandle();
		}
		UMassAgentComponent* Agent = Pawn->FindComponentByClass<UMassAgentComponent>();
		if (Agent == nullptr)
		{
			return FMassEntityHandle();
		}
		return Agent->GetEntityHandle();
	}

	FMassEntityManager* GetEntityManager(UWorld* World, FMassEntityHandle Entity)
	{
		if (World == nullptr || !Entity.IsSet())
		{
			return nullptr;
		}
		UMassEntitySubsystem* MassSS = World->GetSubsystem<UMassEntitySubsystem>();
		if (MassSS == nullptr)
		{
			return nullptr;
		}
		FMassEntityManager& Manager = MassSS->GetMutableEntityManager();
		return Manager.IsEntityValid(Entity) ? &Manager : nullptr;
	}

	FArcBagId GetDisplayedBagId(const FArcRmlStructList& Bags, int32 Index)
	{
		const TArray<FInstancedStruct>& Rows = Bags.GetRows();
		if (!Rows.IsValidIndex(Index))
		{
			return FArcBagId();
		}
		const FArcBagTabEntry* Row = Rows[Index].GetPtr<FArcBagTabEntry>();
		return Row != nullptr ? Row->BagId : FArcBagId();
	}

	FArcBagSlotEntry MakeSlotEntry(int32 SlotIndex, const FArcItemData* Item, bool bSelected)
	{
		FArcBagSlotEntry Entry;
		Entry.SlotIndex = SlotIndex;
		Entry.bSelected = bSelected;
		if (Item == nullptr)
		{
			return Entry;
		}
		Entry.bHasItem = true;
		Entry.ItemId = Item->ItemId;
		Entry.Stacks = static_cast<int32>(Item->Stacks);
		if (const FArcItemFragment_Icon* IconFrag = ArcItemsHelper::FindFragment<FArcItemFragment_Icon>(Item))
		{
			Entry.Icon = IconFrag->ItemIcon;
		}
		if (const FArcItemFragment_UIDescription* UIDescription = ArcItemsHelper::FindFragment<FArcItemFragment_UIDescription>(Item))
		{
			Entry.ItemName = UIDescription->ItemName;
			Entry.ItemDescription = UIDescription->ItemDescription;
		}
		return Entry;
	}

	bool ApplyOrder(FArcRmlStructList& List, const TArray<FString>& Order)
	{
		const TSet<FString> Kept(Order);
		TArray<FString> Stale;
		for (const FString& Key : List.GetKeys())
		{
			if (!Kept.Contains(Key))
			{
				Stale.Add(Key);
			}
		}

		bool bChanged = false;
		for (const FString& Key : Stale)
		{
			bChanged |= List.Remove(Key);
		}

		const uint64 StructureVersion = List.GetStructureVersion();
		List.SetOrder(Order);
		bChanged |= StructureVersion != List.GetStructureVersion();
		return bChanged;
	}

} // namespace ArcUIModelsBagStructModel

void FArcBagStructModel::Initialize(UArcFrontendMVVMModelComponent& Component)
{
	// Subscribe to entity-association notifications so we can late-bind if the MassAgentComponent
	// hasn't associated its entity yet when Initialize fires. The bag fragment requires a
	// fully-composed entity, so the immediate bind below frequently misses on first frame.
	UWorld* World = Component.GetWorld();
	if (World != nullptr)
	{
		UMassAgentSubsystem* AgentSS = World->GetSubsystem<UMassAgentSubsystem>();
		if (AgentSS != nullptr)
		{
			TWeakObjectPtr<UArcFrontendMVVMModelComponent> WeakComponent(&Component);
			EntityAssociatedHandle = AgentSS->GetOnMassAgentComponentEntityAssociated().AddLambda(
				[this, WeakComponent](const UMassAgentComponent& AgentComponent)
				{
					// Early-out: already bound.
					if (BoundEntity.IsSet())
					{
						return;
					}

					// Filter: only react when the firing agent belongs to our player's pawn.
					UArcFrontendMVVMModelComponent* Comp = WeakComponent.Get();
					if (Comp == nullptr)
					{
						return;
					}
					AActor* Owner = Comp->GetOwner();
					APlayerState* PS = Cast<APlayerState>(Owner);
					if (PS == nullptr)
					{
						return;
					}
					APawn* Pawn = PS->GetPawn();
					if (Pawn == nullptr || AgentComponent.GetOwner() != Pawn)
					{
						return;
					}

					TryBindToEntity(*Comp);
				});
		}
	}

	// Attempt immediate bind; if the entity isn't ready yet the delegate above will retry.
	TryBindToEntity(Component);
}

bool FArcBagStructModel::TryBindToEntity(UArcFrontendMVVMModelComponent& Component)
{
	// Already bound — nothing to do.
	if (BoundEntity.IsSet())
	{
		return true;
	}

	const FMassEntityHandle Entity = ArcUIModelsBagStructModel::ResolveLocalPlayerEntity(Component);
	if (!Entity.IsSet())
	{
		return false;
	}

	UWorld* World = Component.GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	UMassEntitySubsystem* MassSS = World->GetSubsystem<UMassEntitySubsystem>();
	if (MassSS == nullptr)
	{
		return false;
	}
	FMassEntityManager& Manager = MassSS->GetMutableEntityManager();
	if (!Manager.IsEntityValid(Entity))
	{
		return false;
	}

	// Confirm the entity carries a bag overlay before building anything.
	const FMassEntityView View(Manager, Entity);
	const FArcMassBagStoreFragment* BagFrag = View.GetSparseFragmentDataPtr<FArcMassBagStoreFragment>();
	if (BagFrag == nullptr)
	{
		return false;
	}

	UArcMassBagEventSubsystem* BagEvents = World->GetSubsystem<UArcMassBagEventSubsystem>();
	if (BagEvents == nullptr)
	{
		return false;
	}

	// Commit the bind only now that every prerequisite resolved.
	BoundEntity = Entity;
	BoundBagEvents = BagEvents;

	UArcBagContainerViewModel* NewContainer = NewObject<UArcBagContainerViewModel>(&Component);
	NewContainer->SetEntity(BoundEntity);
	Component.RegisterViewModel(NewContainer, ContextName, RegistrationTag);
	ContainerVM = NewContainer;

	TWeakObjectPtr<UArcFrontendMVVMModelComponent> WeakComponent(&Component);
	ActiveBagChangedHandle = NewContainer->OnActiveBagChanged.AddLambda(
		[this, WeakComponent]()
		{
			if (UArcFrontendMVVMModelComponent* Comp = WeakComponent.Get())
			{
				RebuildAll(*Comp);
			}
		});

	BagListChangedHandle = BagEvents->AddBagListChangedForEntity(
		BoundEntity,
		UArcMassBagEventSubsystem::FOnBagListChanged::FDelegate::CreateLambda(
			[this, WeakComponent](FMassEntityHandle InEntity)
			{
				if (!BoundEntity.IsSet() || InEntity != BoundEntity)
				{
					return;
				}
				UArcFrontendMVVMModelComponent* Comp = WeakComponent.Get();
				if (Comp == nullptr)
				{
					return;
				}
				RebuildAll(*Comp);
			}));

	BagSlotsChangedHandle = BagEvents->AddBagSlotsChangedForEntity(
		BoundEntity,
		UArcMassBagEventSubsystem::FOnBagSlotsChanged::FDelegate::CreateLambda(
			[this, WeakComponent](FMassEntityHandle InEntity, FArcBagId BagId)
			{
				if (!BoundEntity.IsSet() || InEntity != BoundEntity)
				{
					return;
				}
				UArcFrontendMVVMModelComponent* Comp = WeakComponent.Get();
				if (Comp == nullptr)
				{
					return;
				}
				RefreshBag(*Comp, BagId);
			}));

	RebuildAll(Component);
	return true;
}

void FArcBagStructModel::RebuildAll(UArcFrontendMVVMModelComponent& Component)
{
	UArcBagContainerViewModel* Container = ContainerVM.Get();
	if (Container == nullptr)
	{
		return;
	}

	FMassEntityManager* Manager = ArcUIModelsBagStructModel::GetEntityManager(Component.GetWorld(), BoundEntity);
	if (Manager == nullptr)
	{
		return;
	}

	FMassEntityView View(*Manager, BoundEntity);
	FArcMassBagStoreFragment* Frag = View.GetSparseFragmentDataPtr<FArcMassBagStoreFragment>();
	if (Frag == nullptr)
	{
		return;
	}

	TArray<FArcBagInstance>& Instances = Frag->Bags.Items;
	const FArcBagId PreviousActiveId = ArcUIModelsBagStructModel::GetDisplayedBagId(Container->GetBags(), Container->GetActiveBagIndex());
	const int32 ActiveIndex = Instances.Num() > 0 ? FMath::Clamp(Container->GetActiveBagIndex(), 0, Instances.Num() - 1) : 0;
	FArcBagInstance* ActiveBag = Instances.IsValidIndex(ActiveIndex) ? &Instances[ActiveIndex] : nullptr;
	if (ActiveBag == nullptr || ActiveBag->BagId != PreviousActiveId)
	{
		Container->ClearSelectedSlot();
	}

	FArcRmlStructList& List = Container->GetMutableBags();
	bool bChanged = false;
	TArray<FString> Order;
	Order.Reserve(Instances.Num());
	for (int32 Index = 0; Index < Instances.Num(); ++Index)
	{
		FArcBagInstance& Bag = Instances[Index];
		const UArcBagDefinition* Def = ArcMassItems::Bags::ResolveBagDefinition(Bag);

		FArcBagTabEntry Entry;
		Entry.BagId = Bag.BagId;
		Entry.DisplayName = Def != nullptr ? Def->DisplayName : FText();
		Entry.SlotCount = Bag.Slots.Num();
		Entry.bActive = Index == ActiveIndex;

		FString Key = Bag.BagId.ToString();
		bChanged |= List.Set(Key, Entry);
		Order.Add(MoveTemp(Key));
	}
	bChanged |= ArcUIModelsBagStructModel::ApplyOrder(List, Order);

	Container->SetActiveBagIndex(ActiveIndex);
	if (bChanged)
	{
		Container->BroadcastBagsChanged();
	}

	RebuildSlots(*Container, ActiveBag, *Frag, *Manager);
}

void FArcBagStructModel::RefreshBag(UArcFrontendMVVMModelComponent& Component, FArcBagId BagId)
{
	UArcBagContainerViewModel* Container = ContainerVM.Get();
	if (Container == nullptr)
	{
		return;
	}

	if (Container->GetBags().Find(BagId.ToString()) == nullptr)
	{
		RebuildAll(Component);
		return;
	}

	if (BagId != ArcUIModelsBagStructModel::GetDisplayedBagId(Container->GetBags(), Container->GetActiveBagIndex()))
	{
		return;
	}

	FMassEntityManager* Manager = ArcUIModelsBagStructModel::GetEntityManager(Component.GetWorld(), BoundEntity);
	if (Manager == nullptr)
	{
		return;
	}

	FMassEntityView View(*Manager, BoundEntity);
	FArcMassBagStoreFragment* Frag = View.GetSparseFragmentDataPtr<FArcMassBagStoreFragment>();
	if (Frag == nullptr)
	{
		return;
	}

	FArcBagInstance* Bag = ArcMassItems::Bags::FindBag(*Frag, BagId);
	if (Bag == nullptr)
	{
		RebuildAll(Component);
		return;
	}

	RebuildSlots(*Container, Bag, *Frag, *Manager);
}

void FArcBagStructModel::RebuildSlots(UArcBagContainerViewModel& Container, FArcBagInstance* Bag, FArcMassBagStoreFragment& Frag, FMassEntityManager& Manager)
{
	FArcRmlStructList& List = Container.GetMutableSlots();
	if (Bag == nullptr)
	{
		Container.ClearSelectedSlot();
		if (List.Num() > 0)
		{
			List.Reset();
			Container.BroadcastSlotsChanged();
		}
		Container.SetCapacity(FString());
		return;
	}

	FArcMassItemStoreFragment* Store = nullptr;
	if (Frag.TargetStoreType != nullptr)
	{
		FStructView StoreView = Manager.GetMutableSparseElementDataForEntity(Frag.TargetStoreType, BoundEntity);
		Store = StoreView.GetPtr<FArcMassItemStoreFragment>();
	}

	const int32 SlotNum = Bag->Slots.Num();
	if (Container.GetSelectedSlotIndex() >= SlotNum)
	{
		Container.ClearSelectedSlot();
	}
	const int32 SelectedIndex = Container.GetSelectedSlotIndex();

	bool bChanged = false;
	int32 Filled = 0;
	TArray<FString> Order;
	Order.Reserve(SlotNum);
	for (int32 Index = 0; Index < SlotNum; ++Index)
	{
		const FArcItemId Id = Bag->Slots[Index];
		const FArcItemData* Data = nullptr;
		if (Id.IsValid() && Store != nullptr)
		{
			FArcMassReplicatedItem* Rep = Store->ReplicatedItems.FindById(Id);
			if (Rep != nullptr)
			{
				Data = Rep->ToItem();
			}
		}

		const FArcBagSlotEntry Entry = ArcUIModelsBagStructModel::MakeSlotEntry(Index, Data, Index == SelectedIndex);
		if (Entry.bHasItem)
		{
			++Filled;
		}

		FString Key = FString::FromInt(Index);
		bChanged |= List.Set(Key, Entry);
		Order.Add(MoveTemp(Key));
	}
	bChanged |= ArcUIModelsBagStructModel::ApplyOrder(List, Order);

	if (bChanged)
	{
		Container.BroadcastSlotsChanged();
	}
	Container.SetCapacity(FString::Printf(TEXT("%d / %d"), Filled, SlotNum));
}

void FArcBagStructModel::Deinitialize(UArcFrontendMVVMModelComponent& Component)
{
	// Remove the agent-association delegate first (it captures `this`).
	UWorld* World = Component.GetWorld();
	if (World != nullptr && EntityAssociatedHandle.IsValid())
	{
		UMassAgentSubsystem* AgentSS = World->GetSubsystem<UMassAgentSubsystem>();
		if (AgentSS != nullptr)
		{
			AgentSS->GetOnMassAgentComponentEntityAssociated().Remove(EntityAssociatedHandle);
		}
	}
	EntityAssociatedHandle.Reset();

	// Then the bag-event subscriptions.
	if (UArcMassBagEventSubsystem* BagEvents = BoundBagEvents.Get())
	{
		BagEvents->RemoveForEntity(BoundEntity, BagListChangedHandle);
		BagEvents->RemoveForEntity(BoundEntity, BagSlotsChangedHandle);
	}
	BagListChangedHandle.Reset();
	BagSlotsChangedHandle.Reset();

	if (UArcBagContainerViewModel* Container = ContainerVM.Get())
	{
		Container->OnActiveBagChanged.Remove(ActiveBagChangedHandle);
	}
	ActiveBagChangedHandle.Reset();

	// Unregister the container VM from the component's ViewModels map.
	Component.RemoveViewModel(RegistrationTag);

	BoundBagEvents.Reset();
	ContainerVM.Reset();
	BoundEntity = FMassEntityHandle();
}
