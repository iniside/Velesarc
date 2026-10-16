// Copyright Lukasz Baran. All Rights Reserved.

#include "Models/ArcItemStoreStructModel.h"

#include "ArcFrontendMVVMModelComponent.h"
#include "Equipment/ArcMassEquipmentFragment.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemListEntry.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_Icon.h"
#include "Items/Fragments/ArcItemFragment_UIDescription.h"
#include "MassAgentComponent.h"
#include "MassAgentSubsystem.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Events/ArcMassItemEventSubsystem.h"
#include "ViewModels/ArcItemStoreViewModel.h"

namespace ArcUIModelsItemStoreStructModel
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

	const FArcMassItemStoreFragment* GetStoreFragment(UWorld* World, FMassEntityHandle Entity, const UScriptStruct* FragmentType)
	{
		if (World == nullptr || !Entity.IsSet() || FragmentType == nullptr)
		{
			return nullptr;
		}
		if (!FragmentType->IsChildOf(FArcMassItemStoreFragment::StaticStruct()))
		{
			return nullptr;
		}
		UMassEntitySubsystem* MassSS = World->GetSubsystem<UMassEntitySubsystem>();
		if (MassSS == nullptr)
		{
			return nullptr;
		}
		FMassEntityManager& Manager = MassSS->GetMutableEntityManager();
		if (!Manager.IsEntityValid(Entity))
		{
			return nullptr;
		}
		const FStructView StructView = Manager.GetMutableSparseElementDataForEntity(FragmentType, Entity);
		return StructView.GetPtr<FArcMassItemStoreFragment>();
	}

	const UScriptStruct* ResolveStoreType(const UScriptStruct* Picked)
	{
		return Picked != nullptr ? Picked : FArcMassItemStoreFragment::StaticStruct();
	}

	TSet<FArcItemId> CollectEquippedIds(UWorld* World, FMassEntityHandle Entity)
	{
		TSet<FArcItemId> EquippedIds;
		if (World == nullptr || !Entity.IsSet())
		{
			return EquippedIds;
		}
		UMassEntitySubsystem* MassSS = World->GetSubsystem<UMassEntitySubsystem>();
		if (MassSS == nullptr)
		{
			return EquippedIds;
		}
		const FMassEntityManager& Manager = MassSS->GetEntityManager();
		if (!Manager.IsEntityValid(Entity))
		{
			return EquippedIds;
		}
		const FMassEntityView View(Manager, Entity);
		const FArcMassEquipmentFragment* EquipFrag = View.GetSparseFragmentDataPtr<FArcMassEquipmentFragment>();
		if (EquipFrag == nullptr)
		{
			return EquippedIds;
		}
		for (const TPair<FGameplayTag, FArcItemId>& Pair : EquipFrag->EquippedItems)
		{
			if (Pair.Value.IsValid())
			{
				EquippedIds.Add(Pair.Value);
			}
		}
		return EquippedIds;
	}

	FArcItemListEntry MakeEntry(const FArcItemData& Item, bool bIsEquipped)
	{
		FArcItemListEntry Entry;
		Entry.ItemId = Item.ItemId;
		Entry.Stacks = static_cast<int32>(Item.Stacks);
		Entry.Level = static_cast<int32>(Item.Level);
		Entry.CurrentItemStoreSlot = Item.Slot;
		Entry.Tags = Item.ItemAggregatedTags;
		Entry.bIsEquipped = bIsEquipped;
		if (const FArcItemFragment_UIDescription* UI = ArcItemsHelper::FindFragment<FArcItemFragment_UIDescription>(&Item))
		{
			Entry.DisplayName = UI->ItemName;
			Entry.Description = UI->ItemDescription;
		}
		if (const FArcItemFragment_Icon* IconFrag = ArcItemsHelper::FindFragment<FArcItemFragment_Icon>(&Item))
		{
			Entry.Icon = IconFrag->ItemIcon;
		}
		return Entry;
	}

}

void FArcItemStoreStructModel::CreateViewModels(UArcFrontendMVVMModelComponent& Component)
{
	FArcViewModelSpec Spec;
	Spec.ContextName = ContextName;
	Spec.ViewModelClass = UArcItemStoreViewModel::StaticClass();
	Spec.RegistrationTag = RegistrationTag;
	Component.CreateAndRegisterViewModels({ Spec });
}

bool FArcItemStoreStructModel::TryBindToEntity(UArcFrontendMVVMModelComponent& Component)
{
	// Already bound — nothing to do.
	if (BoundEntity.IsSet())
	{
		return true;
	}

	BoundEntity = ArcUIModelsItemStoreStructModel::ResolveLocalPlayerEntity(Component);
	if (!BoundEntity.IsSet())
	{
		return false;
	}

	UWorld* World = Component.GetWorld();
	if (World == nullptr)
	{
		BoundEntity = FMassEntityHandle();
		return false;
	}

	UArcMassItemEventSubsystem* EventSub = World->GetSubsystem<UArcMassItemEventSubsystem>();
	if (EventSub == nullptr)
	{
		BoundEntity = FMassEntityHandle();
		return false;
	}
	BoundEventSubsystem = EventSub;
	WeakBoundComponent = &Component;

	TWeakObjectPtr<UArcFrontendMVVMModelComponent> WeakComponent(&Component);
	const FMassEntityHandle Entity = BoundEntity;
	const UScriptStruct* EffectiveStoreType =
		ArcUIModelsItemStoreStructModel::ResolveStoreType(StoreFragmentType.Get());

	AddedHandle = EventSub->AddItemAddedForEntity(
		Entity, EffectiveStoreType,
		UArcMassItemEventSubsystem::FOnItemAdded::FDelegate::CreateLambda(
			[this, WeakComponent](FMassEntityHandle InEntity, FArcItemId /*ItemId*/, const UScriptStruct* /*StoreType*/)
			{
				HandleSignal(FName(), MakeArrayView(&InEntity, 1), WeakComponent);
			}));

	RemovedHandle = EventSub->AddItemRemovedForEntity(
		Entity, EffectiveStoreType,
		UArcMassItemEventSubsystem::FOnItemRemoved::FDelegate::CreateLambda(
			[this, WeakComponent](FMassEntityHandle InEntity, FArcItemId /*ItemId*/, const UScriptStruct* /*StoreType*/)
			{
				HandleSignal(FName(), MakeArrayView(&InEntity, 1), WeakComponent);
			}));

	SlotChangedHandle = EventSub->AddItemSlotChangedForEntity(
		Entity, EffectiveStoreType,
		UArcMassItemEventSubsystem::FOnItemSlotChanged::FDelegate::CreateLambda(
			[this, WeakComponent](FMassEntityHandle InEntity, FArcItemId /*ItemId*/, FGameplayTag /*NewSlot*/, const UScriptStruct* /*StoreType*/)
			{
				HandleSignal(FName(), MakeArrayView(&InEntity, 1), WeakComponent);
			}));

	StacksChangedHandle = EventSub->AddItemStacksChangedForEntity(
		Entity, EffectiveStoreType,
		UArcMassItemEventSubsystem::FOnItemStacksChanged::FDelegate::CreateLambda(
			[this, WeakComponent](FMassEntityHandle InEntity, FArcItemId /*ItemId*/, int32 /*StackDelta*/, const UScriptStruct* /*StoreType*/)
			{
				HandleSignal(FName(), MakeArrayView(&InEntity, 1), WeakComponent);
			}));

	RebuildFromFragment(Component);

	return true;
}

void FArcItemStoreStructModel::Initialize(UArcFrontendMVVMModelComponent& Component)
{
	StoreVM = Cast<UArcItemStoreViewModel>(
		Component.FindViewModelByContext(ContextName, UArcItemStoreViewModel::StaticClass()));
	if (UArcItemStoreViewModel* VM = StoreVM.Get())
	{
		VM->SetFilter(InitialFilter);

		TWeakObjectPtr<UArcFrontendMVVMModelComponent> WeakComponent(&Component);
		FilterChangedHandle = VM->OnFilterChanged.AddLambda(
			[this, WeakComponent]()
			{
				if (!BoundEntity.IsSet())
				{
					return;
				}
				if (UArcFrontendMVVMModelComponent* Comp = WeakComponent.Get())
				{
					RebuildFromFragment(*Comp);
				}
			});
	}

	// Subscribe to entity-association notifications so we can late-bind if
	// the MassAgentComponent hasn't run BeginPlay yet when Initialize fires.
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

void FArcItemStoreStructModel::Deinitialize(UArcFrontendMVVMModelComponent& Component)
{
	// Unsubscribe from the agent-association delegate first.
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

	if (UArcMassItemEventSubsystem* EventSub = BoundEventSubsystem.Get())
	{
		EventSub->RemoveForEntity(BoundEntity, AddedHandle);
		EventSub->RemoveForEntity(BoundEntity, RemovedHandle);
		EventSub->RemoveForEntity(BoundEntity, SlotChangedHandle);
		EventSub->RemoveForEntity(BoundEntity, StacksChangedHandle);
	}
	AddedHandle.Reset();
	RemovedHandle.Reset();
	SlotChangedHandle.Reset();
	StacksChangedHandle.Reset();

	if (UArcItemStoreViewModel* VM = StoreVM.Get())
	{
		VM->OnFilterChanged.Remove(FilterChangedHandle);
		VM->Reset();
	}
	FilterChangedHandle.Reset();
	StoreVM.Reset();

	BoundEventSubsystem.Reset();
	WeakBoundComponent.Reset();
	BoundEntity = FMassEntityHandle();
}

void FArcItemStoreStructModel::HandleSignal(FName SignalName, TConstArrayView<FMassEntityHandle> Entities, TWeakObjectPtr<UArcFrontendMVVMModelComponent> WeakComponent)
{
	if (!BoundEntity.IsSet())
	{
		return;
	}
	bool bMatches = false;
	for (const FMassEntityHandle& Entity : Entities)
	{
		if (Entity == BoundEntity)
		{
			bMatches = true;
			break;
		}
	}
	if (!bMatches)
	{
		return;
	}
	UArcFrontendMVVMModelComponent* Component = WeakComponent.Get();
	if (Component == nullptr)
	{
		return;
	}
	RebuildFromFragment(*Component);
}

void FArcItemStoreStructModel::RebuildFromFragment(UArcFrontendMVVMModelComponent& Component)
{
	UArcItemStoreViewModel* VM = StoreVM.Get();
	if (VM == nullptr)
	{
		return;
	}

	UWorld* World = Component.GetWorld();
	const UScriptStruct* EffectiveStoreType =
		ArcUIModelsItemStoreStructModel::ResolveStoreType(StoreFragmentType.Get());
	const FArcMassItemStoreFragment* Frag =
		ArcUIModelsItemStoreStructModel::GetStoreFragment(World, BoundEntity, EffectiveStoreType);
	const TSet<FArcItemId> EquippedIds = ArcUIModelsItemStoreStructModel::CollectEquippedIds(World, BoundEntity);
	const FArcItemViewFilter& Filter = VM->GetFilter();
	FArcRmlStructList& List = VM->GetMutableItems();

	bool bChanged = false;
	TArray<FString> Order;
	TSet<FString> Kept;
	if (Frag != nullptr)
	{
		Order.Reserve(Frag->ReplicatedItems.Items.Num());
		Kept.Reserve(Frag->ReplicatedItems.Items.Num());
		for (const FArcMassReplicatedItem& Replicated : Frag->ReplicatedItems.Items)
		{
			const FArcItemData* Data = Replicated.ToItem();
			if (Data == nullptr || !Data->ItemId.IsValid())
			{
				continue;
			}
			const FArcItemListEntry Entry = ArcUIModelsItemStoreStructModel::MakeEntry(*Data, EquippedIds.Contains(Data->ItemId));
			if (!Filter.Matches(Entry))
			{
				continue;
			}
			FString Key = Data->ItemId.ToString();
			bChanged |= List.Set(Key, Entry);
			Kept.Add(Key);
			Order.Add(MoveTemp(Key));
		}
	}

	TArray<FString> Stale;
	for (const FString& Key : List.GetKeys())
	{
		if (!Kept.Contains(Key))
		{
			Stale.Add(Key);
		}
	}
	for (const FString& Key : Stale)
	{
		bChanged |= List.Remove(Key);
	}

	const uint64 StructureVersion = List.GetStructureVersion();
	List.SetOrder(Order);
	bChanged |= StructureVersion != List.GetStructureVersion();

	if (bChanged)
	{
		VM->BroadcastItemsChanged();
	}
}
