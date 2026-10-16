// Copyright Lukasz Baran. All Rights Reserved.

#include "Models/ArcEquipmentStructModel.h"

#include "ArcFrontendMVVMModelComponent.h"
#include "Equipment/ArcMassEquipmentFragment.h"
#include "Equipment/ArcMassEquipmentSlotPreset.h"
#include "Equipment/ArcMassEquipmentTypes.h"
#include "Equipment/ArcMassEquipmentUtils.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Events/ArcGameEventTags.h"
#include "Events/ArcMassItemEventSubsystem.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "MassAgentComponent.h"
#include "Items/ArcItemData.h"
#include "MassAgentSubsystem.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "ViewModels/ArcEquipmentSlotViewModel.h"
#include "ViewModels/ArcItemViewModel.h"
#include "Display/ArcTagDisplayRegistry.h"

namespace ArcUIModelsEquipmentStructModel
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

	const FArcMassEquipmentFragment* GetEquipmentFragment(UWorld* World, FMassEntityHandle Entity)
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
		const FMassEntityManager& Manager = MassSS->GetEntityManager();
		if (!Manager.IsEntityValid(Entity))
		{
			return nullptr;
		}
		const FMassEntityView View(Manager, Entity);
		return View.GetSparseFragmentDataPtr<FArcMassEquipmentFragment>();
	}

	FArcMassItemStoreFragment* GetStoreFragment(UWorld* World, FMassEntityHandle Entity, const UScriptStruct* FragmentType)
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

	const UArcMassEquipmentSlotPreset* GetEquipmentPreset(UWorld* World, FMassEntityHandle Entity)
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
		const FMassEntityManager& Manager = MassSS->GetEntityManager();
		if (!Manager.IsEntityValid(Entity))
		{
			return nullptr;
		}
		const FMassEntityView View(Manager, Entity);
		const FArcMassEquipmentSharedFragment* SharedFrag = View.GetConstSharedFragmentDataPtr<FArcMassEquipmentSharedFragment>();
		if (SharedFrag == nullptr)
		{
			return nullptr;
		}
		return SharedFrag->Preset;
	}

	const FArcMassEquipmentSlot* FindSlotDef(const UArcMassEquipmentSlotPreset* Preset, const FGameplayTag& SlotId)
	{
		if (Preset == nullptr)
		{
			return nullptr;
		}
		for (const FArcMassEquipmentSlot& Slot : Preset->EquipmentSlots)
		{
			if (Slot.SlotId == SlotId)
			{
				return &Slot;
			}
		}
		return nullptr;
	}

} // namespace ArcUIModelsEquipmentStructModel

void FArcEquipmentStructModel::Initialize(UArcFrontendMVVMModelComponent& Component)
{
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

bool FArcEquipmentStructModel::TryBindToEntity(UArcFrontendMVVMModelComponent& Component)
{
	// Already bound — nothing to do.
	if (BoundEntity.IsSet())
	{
		return true;
	}

	BoundEntity = ArcUIModelsEquipmentStructModel::ResolveLocalPlayerEntity(Component);
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

	// Game event subsystem is optional — availability owner-tag updates just won't fire
	// if this subsystem is absent (e.g. test worlds without ArcMassAbilities).
	BoundGameEventSubsystem = World->GetSubsystem<UArcGameEventSubsystem>();

	TWeakObjectPtr<UArcFrontendMVVMModelComponent> WeakComponent(&Component);
	const FMassEntityHandle Entity = BoundEntity;

	const UScriptStruct* EffectiveStoreType = StoreFragmentType.Get() != nullptr
		? StoreFragmentType.Get()
		: FArcMassItemStoreFragment::StaticStruct();

	StacksChangedHandle = EventSub->AddItemStacksChangedForEntity(
		Entity, EffectiveStoreType,
		UArcMassItemEventSubsystem::FOnItemStacksChanged::FDelegate::CreateLambda(
			[this, WeakComponent](FMassEntityHandle InEntity, FArcItemId InItemId, int32 /*StackDelta*/, const UScriptStruct* /*StoreType*/)
			{
				HandleStacksChanged(InEntity, InItemId, WeakComponent);
			}));

	if (UArcGameEventSubsystem* GameEvents = BoundGameEventSubsystem.Get())
	{
		TFunction<FArcGameEventDelegate()> MakeTagDelegate = [this, WeakComponent]() -> FArcGameEventDelegate
		{
			return FArcGameEventDelegate::CreateLambda(
				[this, WeakComponent](const FArcGameEvent& Event)
				{
					if (Event.Target != BoundEntity)
					{
						return;
					}
					UArcFrontendMVVMModelComponent* C = WeakComponent.Get();
					if (C == nullptr)
					{
						return;
					}
					RefreshAvailability(*C);
				});
		};
		TagAddedHandle = GameEvents->Subscribe(ArcGameEventTags::TagAdded, Entity, EArcEventRole::Target, MakeTagDelegate());
		TagRemovedHandle = GameEvents->Subscribe(ArcGameEventTags::TagRemoved, Entity, EArcEventRole::Target, MakeTagDelegate());
	}

	// Build one UArcEquipmentSlotViewModel per binding, registering each under
	// ContextName + binding.RegistrationTag (the tag the slot resolver reads). A binding
	// whose SlotDef is missing is skipped without aborting the rest.
	const UArcMassEquipmentSlotPreset* Preset = ArcUIModelsEquipmentStructModel::GetEquipmentPreset(World, BoundEntity);
	SlotRuntimes.Reserve(SlotBindings.Num());
	for (const FArcEquipmentSlotBinding& Binding : SlotBindings)
	{
		const FArcMassEquipmentSlot* SlotDef = ArcUIModelsEquipmentStructModel::FindSlotDef(Preset, Binding.SlotId);
		if (SlotDef == nullptr)
		{
			continue;
		}

		UArcEquipmentSlotViewModel* NewSlotVM = NewObject<UArcEquipmentSlotViewModel>(&Component);
		NewSlotVM->InitFromSlot(*SlotDef);
		NewSlotVM->SetEntity(BoundEntity);
		Component.RegisterViewModel(NewSlotVM, ContextName, Binding.RegistrationTag);

		// One per-slot SlotChanged subscription keyed on this binding's SlotId: fires when
		// an item enters or leaves this slot (in / out / swap), no in-lambda tag filter needed.
		const FDelegateHandle SlotChangedHandle = EventSub->AddItemSlotChangedForEntityAndSlot(
			Entity, EffectiveStoreType, Binding.SlotId,
			UArcMassItemEventSubsystem::FOnItemSlotChanged::FDelegate::CreateLambda(
				[this, WeakComponent](FMassEntityHandle InEntity, FArcItemId /*ItemId*/, FGameplayTag /*NewSlot*/, const UScriptStruct* /*StoreType*/)
				{
					HandleSignal(FName(), MakeArrayView(&InEntity, 1), WeakComponent);
				}));

		FArcEquipmentSlotRuntime& Runtime = SlotRuntimes.AddDefaulted_GetRef();
		Runtime.SlotId = Binding.SlotId;
		Runtime.RegistrationTag = Binding.RegistrationTag;
		Runtime.SlotVM = NewSlotVM;
		Runtime.SlotChangedHandle = SlotChangedHandle;
	}

	RefreshOccupants(Component);
	RefreshAvailability(Component);

	return true;
}

void FArcEquipmentStructModel::Deinitialize(UArcFrontendMVVMModelComponent& Component)
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

	UArcMassItemEventSubsystem* EventSub = BoundEventSubsystem.Get();
	if (EventSub != nullptr)
	{
		EventSub->RemoveForEntity(BoundEntity, StacksChangedHandle);
	}
	StacksChangedHandle.Reset();

	if (UArcGameEventSubsystem* GameEvents = BoundGameEventSubsystem.Get())
	{
		GameEvents->Unsubscribe(TagAddedHandle);
		GameEvents->Unsubscribe(TagRemovedHandle);
	}
	TagAddedHandle.Reset();
	TagRemovedHandle.Reset();

	// Tear down each per-slot subscription + registered VM, then drop all runtime state.
	for (FArcEquipmentSlotRuntime& Runtime : SlotRuntimes)
	{
		if (EventSub != nullptr)
		{
			EventSub->RemoveForEntity(BoundEntity, Runtime.SlotChangedHandle);
		}
		Component.RemoveViewModel(Runtime.RegistrationTag);
	}
	SlotRuntimes.Empty();

	BoundEventSubsystem.Reset();
	BoundGameEventSubsystem.Reset();
	BoundEntity = FMassEntityHandle();
}

void FArcEquipmentStructModel::HandleSignal(FName SignalName, TConstArrayView<FMassEntityHandle> Entities, TWeakObjectPtr<UArcFrontendMVVMModelComponent> WeakComponent)
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
	RefreshOccupants(*Component);
	RefreshAvailability(*Component);
}

void FArcEquipmentStructModel::RefreshOccupants(UArcFrontendMVVMModelComponent& Component)
{
	// Resolve fragment + store once, then refresh every slot runtime against them.
	const FArcMassEquipmentFragment* EquipFrag =
		ArcUIModelsEquipmentStructModel::GetEquipmentFragment(Component.GetWorld(), BoundEntity);
	const UScriptStruct* EffectiveStoreType = StoreFragmentType.Get() != nullptr
		? StoreFragmentType.Get()
		: FArcMassItemStoreFragment::StaticStruct();
	FArcMassItemStoreFragment* StoreFrag =
		ArcUIModelsEquipmentStructModel::GetStoreFragment(Component.GetWorld(), BoundEntity, EffectiveStoreType);

	for (FArcEquipmentSlotRuntime& Runtime : SlotRuntimes)
	{
		RefreshOccupantsForRuntime(Runtime, EquipFrag, StoreFrag);
	}
}

void FArcEquipmentStructModel::RefreshOccupantsForRuntime(FArcEquipmentSlotRuntime& Runtime, const FArcMassEquipmentFragment* EquipFrag, FArcMassItemStoreFragment* StoreFrag)
{
	UArcEquipmentSlotViewModel* Slot = Runtime.SlotVM.Get();
	if (Slot == nullptr)
	{
		return;
	}

	// Resolve the item currently occupying this slot (invalid id if empty / no fragment).
	FArcItemId NewItemId;
	if (EquipFrag != nullptr)
	{
		const FArcItemId* FoundId = EquipFrag->EquippedItems.Find(Runtime.SlotId);
		if (FoundId != nullptr && FoundId->IsValid())
		{
			NewItemId = *FoundId;
		}
	}

	const FArcItemData* ItemData = nullptr;
	if (NewItemId.IsValid() && StoreFrag != nullptr)
	{
		if (const FArcMassReplicatedItem* Replicated = StoreFrag->ReplicatedItems.FindById(NewItemId))
		{
			ItemData = Replicated->ToItem();
		}
	}

	if (ItemData == nullptr)
	{
		Slot->SetCurrentItem(NewItemId, nullptr);
		return;
	}

	UArcItemViewModel* ItemVM = Slot->GetCurrentItem();
	if (ItemVM == nullptr)
	{
		ItemVM = NewObject<UArcItemViewModel>(Slot);
	}
	ItemVM->RefreshFrom(*ItemData, TagDisplayRegistry);
	Slot->SetCurrentItem(NewItemId, ItemVM);
}

void FArcEquipmentStructModel::RefreshAvailability(UArcFrontendMVVMModelComponent& Component)
{
	UWorld* World = Component.GetWorld();
	if (World == nullptr)
	{
		return;
	}
	UMassEntitySubsystem* MassSS = World->GetSubsystem<UMassEntitySubsystem>();
	if (MassSS == nullptr)
	{
		return;
	}
	FMassEntityManager& Manager = MassSS->GetMutableEntityManager();
	if (!Manager.IsEntityValid(BoundEntity))
	{
		return;
	}

	// Resolve the preset once, then re-eval availability for every slot runtime.
	const UArcMassEquipmentSlotPreset* Preset = ArcUIModelsEquipmentStructModel::GetEquipmentPreset(World, BoundEntity);
	for (FArcEquipmentSlotRuntime& Runtime : SlotRuntimes)
	{
		RefreshAvailabilityForRuntime(Runtime, Manager, Preset);
	}
}

void FArcEquipmentStructModel::RefreshAvailabilityForRuntime(FArcEquipmentSlotRuntime& Runtime, FMassEntityManager& Manager, const UArcMassEquipmentSlotPreset* Preset)
{
	UArcEquipmentSlotViewModel* Slot = Runtime.SlotVM.Get();
	if (Slot == nullptr)
	{
		return;
	}

	const FArcMassEquipmentSlot* SlotDef = ArcUIModelsEquipmentStructModel::FindSlotDef(Preset, Runtime.SlotId);
	if (SlotDef == nullptr)
	{
		return;
	}

	const bool bAvail = Arc::MassEquipment::IsSlotAvailable(Manager, BoundEntity, *SlotDef);
	Slot->SetAvailable(bAvail);
}

void FArcEquipmentStructModel::HandleStacksChanged(FMassEntityHandle InEntity, FArcItemId ItemId, TWeakObjectPtr<UArcFrontendMVVMModelComponent> WeakComponent)
{
	if (!BoundEntity.IsSet() || InEntity != BoundEntity)
	{
		return;
	}
	UArcFrontendMVVMModelComponent* Component = WeakComponent.Get();
	if (Component == nullptr)
	{
		return;
	}
	for (const FArcEquipmentSlotRuntime& Runtime : SlotRuntimes)
	{
		const UArcEquipmentSlotViewModel* Slot = Runtime.SlotVM.Get();
		if (Slot != nullptr && Slot->GetCurrentItemId() == ItemId)
		{
			RefreshOccupants(*Component);
			return;
		}
	}
}
