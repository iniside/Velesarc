// Copyright Lukasz Baran. All Rights Reserved.

#include "Models/QuickBar/ArcQuickBarStructModel.h"

#include "ArcFrontendMVVMModelComponent.h"
#include "Events/ArcMassItemEventSubsystem.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Items/ArcItemData.h"
#include "Display/ArcTagDisplayRegistry.h"
#include "MassAgentComponent.h"
#include "MassAgentSubsystem.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Models/Abilities/ArcAbilityResolver.h"
#include "QuickBar/ArcMassQuickBarConfig.h"
#include "QuickBar/ArcMassQuickBarOperations.h"
#include "QuickBar/ArcMassQuickBarSharedFragment.h"
#include "QuickBar/ArcMassQuickBarTypes.h"
#include "ViewModels/Abilities/ArcAbilityViewModel.h"
#include "ViewModels/ArcItemViewModel.h"
#include "ViewModels/QuickBar/ArcQuickBarSlotViewModel.h"
#include "ViewModels/QuickBar/ArcQuickBarViewModel.h"

namespace ArcUIModelsQuickBarStructModel
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

} // namespace ArcUIModelsQuickBarStructModel

FArcMassQuickBarLocation FArcQuickBarStructModel::FindBarEntry(const FArcMassQuickBarSharedFragment& Shared, FGameplayTag InBarId)
{
	FArcMassQuickBarLocation Found;
	for (const TObjectPtr<UArcMassQuickBarConfig>& Config : Shared.Configs)
	{
		if (Config == nullptr)
		{
			continue;
		}
		ArcMassItems::QuickBar::ForEachBarConfig(*Config,
			[&Found, &Config, InBarId](const FArcMassQuickBarEntry& Bar, const FArcMassQuickBarGroup* Group) -> bool
			{
				if (Bar.BarId == InBarId)
				{
					Found.Config = Config;
					Found.Bar = &Bar;
					Found.Group = Group;
					return false;
				}
				return true;
			});
		if (Found.Bar != nullptr)
		{
			break;
		}
	}
	return Found;
}

FArcQuickBarStructModel::FArcQuickBarStructModel()
{
	bTickForLocalOnly = true;
}

void FArcQuickBarStructModel::Initialize(UArcFrontendMVVMModelComponent& Component)
{
	// Subscribe to entity-association notifications so we can late-bind if the MassAgentComponent
	// hasn't associated its entity yet when Initialize fires. The shared/ability fragments require a
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

bool FArcQuickBarStructModel::TryBindToEntity(UArcFrontendMVVMModelComponent& Component)
{
	// Already bound — nothing to do.
	if (BoundEntity.IsSet())
	{
		return true;
	}

	const FMassEntityHandle Entity = ArcUIModelsQuickBarStructModel::ResolveLocalPlayerEntity(Component);
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

	// Read the slot schema from the entity's quick-bar shared fragment (live query — never cached on
	// a UPROPERTY). Bail without building VMs if the config is missing.
	const FMassEntityView View(Manager, Entity);
	const FArcMassQuickBarSharedFragment* SharedFrag = View.GetConstSharedFragmentDataPtr<FArcMassQuickBarSharedFragment>();
	if (SharedFrag == nullptr)
	{
		return false;
	}
	if (SharedFrag->Configs.IsEmpty())
	{
		return false;
	}

	// Resolve every authored binding to its bar entry BEFORE committing anything: if none resolve,
	// return false without side effects so the entity-association retry semantics stay intact.
	// Misses on individual bindings are permanent (the config is static per shared fragment — a later
	// retry can't change the outcome), so warn and skip those.
	TArray<TPair<const FArcQuickBarBinding*, FArcMassQuickBarLocation>> ResolvedBindings;
	ResolvedBindings.Reserve(BarBindings.Num());
	for (const FArcQuickBarBinding& Binding : BarBindings)
	{
		const FArcMassQuickBarLocation Location = FindBarEntry(*SharedFrag, Binding.BarId);
		if (Location.Bar == nullptr)
		{
			UE_LOGF(LogTemp, Warning,"FArcQuickBarStructModel: no bar '%ls' in any quick-bar config of the entity — binding skipped.",
				*Binding.BarId.ToString());
			continue;
		}
		ResolvedBindings.Emplace(&Binding, Location);
	}
	if (ResolvedBindings.IsEmpty())
	{
		return false;
	}

	UArcMassItemEventSubsystem* EventSub = World->GetSubsystem<UArcMassItemEventSubsystem>();
	if (EventSub == nullptr)
	{
		return false;
	}

	// Commit the bind only now that every prerequisite resolved.
	BoundEntity = Entity;
	BoundEventSubsystem = EventSub;

	// Build one bar VM per resolved binding (Outer = the component, the GC keeper) and one slot VM
	// per authored slot; register each bar VM in the component's ViewModels map keyed by its BarId.
	BarRuntimes.Reserve(ResolvedBindings.Num());
	for (const TPair<const FArcQuickBarBinding*, FArcMassQuickBarLocation>& Resolved : ResolvedBindings)
	{
		const FArcQuickBarBinding& Binding = *Resolved.Key;
		const FArcMassQuickBarEntry& BarEntry = *Resolved.Value.Bar;

		UArcQuickBarViewModel* NewBarVM = NewObject<UArcQuickBarViewModel>(&Component);
		NewBarVM->SetBarId(Binding.BarId);
		NewBarVM->Slots.Reserve(BarEntry.Slots.Num());

		for (const FArcMassQuickBarSlot& SlotDef : BarEntry.Slots)
		{
			UArcQuickBarSlotViewModel* SlotVM = NewObject<UArcQuickBarSlotViewModel>(&Component);
			SlotVM->SetBarId(Binding.BarId);
			SlotVM->SetQuickSlotId(SlotDef.QuickBarSlotId);

			UArcItemViewModel* ItemVM = NewObject<UArcItemViewModel>(&Component);
			SlotVM->SetItem(ItemVM);

			UArcAbilityViewModel* AbilityVM = NewObject<UArcAbilityViewModel>(&Component);
			SlotVM->SetAbility(AbilityVM);

			SlotVM->SetEntity(BoundEntity);
			NewBarVM->Slots.Add(SlotVM);
		}

		Component.RegisterViewModel(NewBarVM, Binding.ContextName, Binding.BarId);

		FArcQuickBarBarRuntime& Runtime = BarRuntimes.AddDefaulted_GetRef();
		Runtime.BarId = Binding.BarId;
		Runtime.ContextName = Binding.ContextName;
		Runtime.BarVM = NewBarVM;
	}

	// Subscribe once per distinct config store to the per-entity item-slot-changed rail (assign / clear /
	// move all fire it) so every bar re-resolves whenever quick-slot occupancy changes (the payload
	// carries no BarId — refresh-all is the correct granularity).
	TArray<const UScriptStruct*> SubscribedStoreTypes;
	TWeakObjectPtr<UArcFrontendMVVMModelComponent> WeakComponent(&Component);
	for (const TObjectPtr<UArcMassQuickBarConfig>& ConfigPtr : SharedFrag->Configs)
	{
		if (ConfigPtr == nullptr || ConfigPtr->StoreType == nullptr)
		{
			continue;
		}
		const UScriptStruct* ConfigStoreType = ConfigPtr->StoreType;
		if (SubscribedStoreTypes.Contains(ConfigStoreType))
		{
			continue;
		}
		SubscribedStoreTypes.Add(ConfigStoreType);
		SlotChangedHandles.Add(EventSub->AddItemSlotChangedForEntity(
			BoundEntity, ConfigStoreType,
			UArcMassItemEventSubsystem::FOnItemSlotChanged::FDelegate::CreateLambda(
				[this, WeakComponent](FMassEntityHandle InEntity, FArcItemId /*ItemId*/, FGameplayTag /*NewSlot*/, const UScriptStruct* /*StoreType*/)
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
					RefreshAllSlots(*Comp);
				})));
	}

	RefreshAllSlots(Component);
	return true;
}

void FArcQuickBarStructModel::RefreshAllSlots(UArcFrontendMVVMModelComponent& Component)
{
	if (!BoundEntity.IsSet())
	{
		return;
	}

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

	for (const FArcQuickBarBarRuntime& Runtime : BarRuntimes)
	{
		UArcQuickBarViewModel* BarVM = Runtime.BarVM.Get();
		if (BarVM == nullptr)
		{
			continue;
		}
		for (const TObjectPtr<UArcQuickBarSlotViewModel>& SlotVM : BarVM->Slots)
		{
			RefreshSlot(SlotVM.Get(), Manager);
		}
	}
}

void FArcQuickBarStructModel::RefreshSlot(UArcQuickBarSlotViewModel* SlotVM, FMassEntityManager& Manager)
{
	if (SlotVM == nullptr)
	{
		return;
	}

	UArcItemViewModel* ItemVM = SlotVM->Item;
	UArcAbilityViewModel* AbilityVM = SlotVM->Ability;

	const FArcItemId ItemId = ArcMassItems::QuickBar::GetAssignedItem(Manager, BoundEntity, SlotVM->GetBarId(), SlotVM->QuickSlotId);

	const FArcItemData* ItemData = nullptr;
	if (ItemId.IsValid())
	{
		const FArcMassQuickBarLocation Location = ArcMassItems::QuickBar::FindBar(Manager, BoundEntity, SlotVM->GetBarId());
		if (Location.Config != nullptr && Location.Config->StoreType != nullptr)
		{
			FStructView StoreView = Manager.GetMutableSparseElementDataForEntity(Location.Config->StoreType, BoundEntity);
			FArcMassItemStoreFragment* Store = StoreView.GetPtr<FArcMassItemStoreFragment>();
			if (Store != nullptr)
			{
				FArcMassReplicatedItem* Rep = Store->ReplicatedItems.FindById(ItemId);
				if (Rep != nullptr)
				{
					ItemData = Rep->ToItem();
				}
			}
		}
	}

	if (ItemData != nullptr)
	{
		if (ItemVM != nullptr)
		{
			ItemVM->RefreshFrom(*ItemData, TagDisplayRegistry);
		}
		SlotVM->SetHasItem(true);
		SlotVM->SetActive(ArcMassItems::QuickBar::IsSlotActive(Manager, BoundEntity, SlotVM->GetBarId(), SlotVM->QuickSlotId));

		if (AbilityVM != nullptr)
		{
			const FArcAbilitySnapshot Snapshot = ArcAbilityResolver::ResolveAbilityForItem(Manager, BoundEntity, ItemData);
			AbilityVM->ApplySnapshot(Snapshot);
		}
	}
	else
	{
		// No item on this slot: clear the composed VMs' data, keep the VM objects.
		SlotVM->SetHasItem(false);
		SlotVM->SetActive(false);
		if (AbilityVM != nullptr)
		{
			AbilityVM->ApplySnapshot(FArcAbilitySnapshot{});
		}
	}
}

void FArcQuickBarStructModel::Tick(float DeltaTime, UArcFrontendMVVMModelComponent& Component)
{
	if (!BoundEntity.IsSet() || BarRuntimes.IsEmpty())
	{
		return;
	}

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
	// Mutable manager — ArcAbilityResolver / cooldown check require a non-const FMassEntityManager&.
	FMassEntityManager& Manager = MassSS->GetMutableEntityManager();
	if (!Manager.IsEntityValid(BoundEntity) || !Manager.IsEntityActive(BoundEntity))
	{
		return;
	}

	for (const FArcQuickBarBarRuntime& Runtime : BarRuntimes)
	{
		UArcQuickBarViewModel* BarVM = Runtime.BarVM.Get();
		if (BarVM == nullptr)
		{
			continue;
		}
		for (const TObjectPtr<UArcQuickBarSlotViewModel>& SlotVM : BarVM->Slots)
		{
			RefreshSlot(SlotVM.Get(), Manager);
		}
	}
}

void FArcQuickBarStructModel::Deinitialize(UArcFrontendMVVMModelComponent& Component)
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

	// Then the item-event subscription.
	if (UArcMassItemEventSubsystem* EventSub = BoundEventSubsystem.Get())
	{
		for (const FDelegateHandle& Handle : SlotChangedHandles)
		{
			EventSub->RemoveForEntity(BoundEntity, Handle);
		}
	}
	SlotChangedHandles.Reset();

	// Unregister every bar VM from the component's ViewModels map.
	for (const FArcQuickBarBarRuntime& Runtime : BarRuntimes)
	{
		Component.RemoveViewModel(Runtime.BarId);
	}
	BarRuntimes.Reset();

	BoundEventSubsystem.Reset();
	BoundEntity = FMassEntityHandle();
}
