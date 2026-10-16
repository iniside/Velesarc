// Copyright Lukasz Baran. All Rights Reserved.

#include "Models/ArcBuilderStructModel.h"

#include "ArcFrontendMVVMModelComponent.h"
#include "MassAgentComponent.h"
#include "MassAgentSubsystem.h"
#include "MassEntitySubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#include "ArcBuilderSubsystem.h"
#include "ArcBuildingDefinition.h"

#include "Events/ArcMassItemEventSubsystem.h"
#include "Fragments/ArcMassItemStoreFragment.h"

#include "ViewModels/ArcBuilderViewModel.h"
#include "ViewModels/ArcBuildingViewModel.h"

namespace ArcBuilderStructModelPrivate
{
	FMassEntityHandle ResolvePlayerEntity(UArcFrontendMVVMModelComponent& Component)
	{
		AActor* Owner = Component.GetOwner();
		const APlayerState* PS = Cast<APlayerState>(Owner);
		if (PS == nullptr)
		{
			return FMassEntityHandle();
		}
		const APawn* Pawn = PS->GetPawn();
		if (Pawn == nullptr)
		{
			return FMassEntityHandle();
		}
		const UMassAgentComponent* Agent = Pawn->FindComponentByClass<UMassAgentComponent>();
		if (Agent == nullptr)
		{
			return FMassEntityHandle();
		}
		return Agent->GetEntityHandle();
	}

	// PlayerState -> owning PlayerController -> LocalPlayer -> per-player builder subsystem.
	// Every hop is nullptr for remote players' PlayerStates on a client, and the first two
	// can be nullptr for the local player early in join — callers bail silently and retry.
	UArcBuilderSubsystem* ResolveBuilderSubsystem(UArcFrontendMVVMModelComponent& Component)
	{
		AActor* Owner = Component.GetOwner();
		const APlayerState* PS = Cast<APlayerState>(Owner);
		if (PS == nullptr)
		{
			return nullptr;
		}
		const APlayerController* PC = PS->GetPlayerController();
		if (PC == nullptr)
		{
			return nullptr;
		}
		const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
		if (LocalPlayer == nullptr)
		{
			return nullptr;
		}
		return LocalPlayer->GetSubsystem<UArcBuilderSubsystem>();
	}

	// The item store is a sparse ELEMENT (keyed by struct type), NOT a sparse fragment —
	// read via GetSparseElementDataForEntity, matching the crafting/bag path. Returns
	// nullptr when the entity is unbound/invalid or has no store.
	const FArcMassItemStoreFragment* ResolvePlayerInventory(
		UArcFrontendMVVMModelComponent& Component,
		FMassEntityHandle PlayerEntity)
	{
		UWorld* World = Component.GetWorld();
		UMassEntitySubsystem* MassSS = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
		if (MassSS == nullptr)
		{
			return nullptr;
		}
		FMassEntityManager& Manager = MassSS->GetMutableEntityManager();
		if (!Manager.IsEntityValid(PlayerEntity))
		{
			return nullptr;
		}
		const FConstStructView StoreView = Manager.GetSparseElementDataForEntity(
			FArcMassItemStoreFragment::StaticStruct(), PlayerEntity);
		return StoreView.GetPtr<FArcMassItemStoreFragment>();
	}
}

// ---------------------------------------------------------------------------
// CreateViewModels
// ---------------------------------------------------------------------------

void FArcBuilderStructModel::CreateViewModels(UArcFrontendMVVMModelComponent& Owner)
{
	bTickForLocalOnly = true;  // FORCE — overrides any serialized override

	FArcViewModelSpec Spec;
	Spec.ContextName = ContextName;
	Spec.ViewModelClass = UArcBuilderViewModel::StaticClass();
	Spec.RegistrationTag = RegistrationTag;
	Owner.CreateAndRegisterViewModels({ Spec });

	BuilderVM = Cast<UArcBuilderViewModel>(Owner.FindViewModelByContext(ContextName, UArcBuilderViewModel::StaticClass()));
	OwnerComp = &Owner;
}

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------

void FArcBuilderStructModel::Initialize(UArcFrontendMVVMModelComponent& Owner)
{
	UArcBuilderViewModel* VM = BuilderVM.Get();
	if (!VM)
	{
		return;
	}

	// Subscribe to entity-association notifications so we can late-bind when the pawn's
	// MassAgentComponent hasn't associated its entity yet at Initialize time (frequently
	// misses on first frame — same race the crafting model handles). Without the retry,
	// PlayerEntity stays invalid forever and affordability never tracks the inventory.
	UWorld* World = Owner.GetWorld();
	if (World != nullptr)
	{
		UMassAgentSubsystem* AgentSS = World->GetSubsystem<UMassAgentSubsystem>();
		if (AgentSS != nullptr)
		{
			BoundAgentSubsystem = AgentSS;
			TWeakObjectPtr<UArcFrontendMVVMModelComponent> WeakOwner(&Owner);
			EntityAssociatedHandle = AgentSS->GetOnMassAgentComponentEntityAssociated().AddLambda(
				[this, WeakOwner](const UMassAgentComponent& AgentComponent)
				{
					// Early-out: already bound.
					if (PlayerEntity.IsSet())
					{
						return;
					}

					// Filter: only react when the firing agent belongs to our player's pawn.
					UArcFrontendMVVMModelComponent* Comp = WeakOwner.Get();
					if (Comp == nullptr)
					{
						return;
					}
					const APlayerState* PS = Cast<APlayerState>(Comp->GetOwner());
					if (PS == nullptr)
					{
						return;
					}
					const APawn* Pawn = PS->GetPawn();
					if (Pawn == nullptr || AgentComponent.GetOwner() != Pawn)
					{
						return;
					}

					TryBindToPlayerEntity(*Comp);
				});
		}
	}

	// Attempt immediate binds; failed rails retry via the association delegate and Tick.
	TryBindToPlayerEntity(Owner);
	TryBindToSubsystem(Owner);
}

bool FArcBuilderStructModel::TryBindToSubsystem(UArcFrontendMVVMModelComponent& Owner)
{
	// Already bound — nothing to do.
	if (BoundBuilderSubsystem.IsValid())
	{
		return true;
	}

	UArcBuilderViewModel* VM = BuilderVM.Get();
	if (!VM)
	{
		return false;
	}

	UArcBuilderSubsystem* Subsystem = ArcBuilderStructModelPrivate::ResolveBuilderSubsystem(Owner);
	if (Subsystem == nullptr)
	{
		return false;
	}

	BoundBuilderSubsystem = Subsystem;
	BuilderStateChangedHandle = Subsystem->OnBuilderStateChanged.AddRaw(
		this, &FArcBuilderStructModel::OnBuilderStateChanged);

	// Push the subsystem into the VM so the placement actions can forward to it.
	VM->SetBuilderSubsystem(Subsystem);

	// Initial refresh — bLastToolRegistered starts false, so an already-registered tool
	// counts as an unregistered->registered transition and populates the buildings list.
	OnBuilderStateChanged();

	return true;
}

bool FArcBuilderStructModel::TryBindToPlayerEntity(UArcFrontendMVVMModelComponent& Owner)
{
	// Already bound — nothing to do.
	if (PlayerEntity.IsSet())
	{
		return true;
	}

	UArcBuilderViewModel* VM = BuilderVM.Get();
	if (!VM)
	{
		return false;
	}

	const FMassEntityHandle Entity = ArcBuilderStructModelPrivate::ResolvePlayerEntity(Owner);
	if (!Entity.IsSet())
	{
		return false;
	}

	PlayerEntity = Entity;

	// Subscribe to player inventory events so building affordability refreshes on any
	// add / remove / stack-change in the player's item store.
	UWorld* World = Owner.GetWorld();
	if (World == nullptr)
	{
		return true;
	}

	UArcMassItemEventSubsystem* ItemEvents = World->GetSubsystem<UArcMassItemEventSubsystem>();
	if (ItemEvents)
	{
		BoundItemEventSubsystem = ItemEvents;

		const UScriptStruct* BaseStoreType = FArcMassItemStoreFragment::StaticStruct();
		TWeakObjectPtr<UArcFrontendMVVMModelComponent> WeakOwner(&Owner);

		ItemAddedHandle = ItemEvents->AddItemAddedForEntity(
			PlayerEntity,
			BaseStoreType,
			UArcMassItemEventSubsystem::FOnItemAdded::FDelegate::CreateLambda(
				[this, WeakOwner](FMassEntityHandle /*Entity*/, FArcItemId /*ItemId*/, const UScriptStruct* /*StoreType*/)
				{
					if (WeakOwner.IsValid())
					{
						RefreshAffordability();
					}
				}));

		ItemRemovedHandle = ItemEvents->AddItemRemovedForEntity(
			PlayerEntity,
			BaseStoreType,
			UArcMassItemEventSubsystem::FOnItemRemoved::FDelegate::CreateLambda(
				[this, WeakOwner](FMassEntityHandle /*Entity*/, FArcItemId /*ItemId*/, const UScriptStruct* /*StoreType*/)
				{
					if (WeakOwner.IsValid())
					{
						RefreshAffordability();
					}
				}));

		ItemStacksChangedHandle = ItemEvents->AddItemStacksChangedForEntity(
			PlayerEntity,
			BaseStoreType,
			UArcMassItemEventSubsystem::FOnItemStacksChanged::FDelegate::CreateLambda(
				[this, WeakOwner](FMassEntityHandle /*Entity*/, FArcItemId /*ItemId*/, int32 /*Delta*/, const UScriptStruct* /*StoreType*/)
				{
					if (WeakOwner.IsValid())
					{
						RefreshAffordability();
					}
				}));
	}

	// Kick the initial affordability pass — building VMs built before the entity bound
	// were counted against an empty store.
	RefreshAffordability();

	return true;
}

// ---------------------------------------------------------------------------
// Deinitialize
// ---------------------------------------------------------------------------

void FArcBuilderStructModel::Deinitialize(UArcFrontendMVVMModelComponent& /*Owner*/)
{
	// Tear down the builder-state subscription.
	if (UArcBuilderSubsystem* Subsystem = BoundBuilderSubsystem.Get())
	{
		if (BuilderStateChangedHandle.IsValid())
		{
			Subsystem->OnBuilderStateChanged.Remove(BuilderStateChangedHandle);
		}
	}
	BuilderStateChangedHandle.Reset();
	BoundBuilderSubsystem.Reset();

	// Tear down the entity-association late-bind subscription.
	if (UMassAgentSubsystem* AgentSS = BoundAgentSubsystem.Get())
	{
		if (EntityAssociatedHandle.IsValid())
		{
			AgentSS->GetOnMassAgentComponentEntityAssociated().Remove(EntityAssociatedHandle);
		}
	}
	EntityAssociatedHandle.Reset();
	BoundAgentSubsystem.Reset();

	// Tear down player item-event subscriptions.
	if (UArcMassItemEventSubsystem* ItemEvents = BoundItemEventSubsystem.Get())
	{
		ItemEvents->RemoveForEntity(PlayerEntity, ItemAddedHandle);
		ItemEvents->RemoveForEntity(PlayerEntity, ItemRemovedHandle);
		ItemEvents->RemoveForEntity(PlayerEntity, ItemStacksChangedHandle);
	}
	ItemAddedHandle.Reset();
	ItemRemovedHandle.Reset();
	ItemStacksChangedHandle.Reset();
	BoundItemEventSubsystem.Reset();

	BuilderVM.Reset();
	OwnerComp.Reset();
	PlayerEntity = FMassEntityHandle();
	bLastToolRegistered = false;
}

// ---------------------------------------------------------------------------
// Tick — late-bind retry net for both rails
// ---------------------------------------------------------------------------

void FArcBuilderStructModel::Tick(
	float /*DeltaTime*/,
	UArcFrontendMVVMModelComponent& Component)
{
	// Late-bind retries: cover orderings the association delegate can't (subsystem rail
	// has no event to retry on at all — PlayerController/LocalPlayer link up silently).
	// No-ops once bound (cheap validity checks).
	if (!BoundBuilderSubsystem.IsValid())
	{
		TryBindToSubsystem(Component);
	}
	if (!PlayerEntity.IsSet())
	{
		TryBindToPlayerEntity(Component);
	}
}

// ---------------------------------------------------------------------------
// OnBuilderStateChanged — self-discriminating coarse-rail handler
// ---------------------------------------------------------------------------

void FArcBuilderStructModel::OnBuilderStateChanged()
{
	UArcBuilderViewModel* VM = BuilderVM.Get();
	if (!VM)
	{
		return;
	}

	UArcBuilderSubsystem* Subsystem = BoundBuilderSubsystem.Get();
	if (Subsystem == nullptr)
	{
		return;
	}

	// The delegate also fires on requirement flips every ghost-drag update — but the
	// buildings list only changes on tool register/unregister (resolution happened once
	// at registration). Rebuild the list only on registration transitions.
	const bool bToolRegistered = Subsystem->IsToolRegistered();
	if (bToolRegistered && !bLastToolRegistered)
	{
		RebuildBuildingsList();
	}
	else if (!bToolRegistered && bLastToolRegistered)
	{
		// Tool unregistered: clear selection first (resets per-entry bIsSelected while the
		// entries still exist), then push the empty list — the VM stays registered so
		// widget bindings remain stable.
		VM->SelectBuilding(nullptr);
		VM->SetBuildings(TArray<TObjectPtr<UArcBuildingViewModel>>());
	}
	bLastToolRegistered = bToolRegistered;

	// Always push the cheap state bools — UE_MVVM_SET_PROPERTY_VALUE no-ops when unchanged.
	VM->RefreshState(bToolRegistered, Subsystem->IsPlacing(), Subsystem->DidMeetPlacementRequirements());
}

// ---------------------------------------------------------------------------
// RebuildBuildingsList
// ---------------------------------------------------------------------------

void FArcBuilderStructModel::RebuildBuildingsList()
{
	UArcBuilderViewModel* VM = BuilderVM.Get();
	if (!VM)
	{
		return;
	}

	UArcFrontendMVVMModelComponent* Owner = OwnerComp.Get();
	if (!Owner)
	{
		return;
	}

	UArcBuilderSubsystem* Subsystem = BoundBuilderSubsystem.Get();
	if (Subsystem == nullptr)
	{
		return;
	}

	// Cached at tool registration — no sync loads here.
	const TArray<TObjectPtr<UArcBuildingDefinition>>& Definitions = Subsystem->GetAvailableBuildings();

	// Existing VMs keyed by primary asset id are reused to preserve widget bindings.
	// Definitions with an unset BuildingId return an INVALID primary asset id, which
	// would collapse map keys — fall back to pointer identity for those.
	const TArray<TObjectPtr<UArcBuildingViewModel>>& ExistingBuildings = VM->GetBuildings();
	TMap<FPrimaryAssetId, TObjectPtr<UArcBuildingViewModel>> ExistingById;
	TMap<const UArcBuildingDefinition*, TObjectPtr<UArcBuildingViewModel>> ExistingByDef;
	ExistingById.Reserve(ExistingBuildings.Num());
	for (TObjectPtr<UArcBuildingViewModel> ExistingVM : ExistingBuildings)
	{
		if (!ExistingVM)
		{
			continue;
		}
		if (ExistingVM->GetBuildingAssetId().IsValid())
		{
			ExistingById.Add(ExistingVM->GetBuildingAssetId(), ExistingVM);
		}
		else if (ExistingVM->GetDefinition() != nullptr)
		{
			ExistingByDef.Add(ExistingVM->GetDefinition(), ExistingVM);
		}
	}

	// Player inventory snapshot for the affordability rollup (empty store until the
	// player entity binds — 0 counts is the correct interim state).
	static const FArcMassItemStoreFragment EmptyStore;
	const FArcMassItemStoreFragment* PlayerInventory =
		ArcBuilderStructModelPrivate::ResolvePlayerInventory(*Owner, PlayerEntity);
	const FArcMassItemStoreFragment& InventoryRef = PlayerInventory ? *PlayerInventory : EmptyStore;

	TArray<TObjectPtr<UArcBuildingViewModel>> NewBuildings;
	NewBuildings.Reserve(Definitions.Num());

	for (UArcBuildingDefinition* Def : Definitions)
	{
		if (Def == nullptr)
		{
			continue;
		}

		const FPrimaryAssetId AssetId = Def->GetPrimaryAssetId();
		TObjectPtr<UArcBuildingViewModel>* Found = AssetId.IsValid()
			? ExistingById.Find(AssetId)
			: ExistingByDef.Find(Def);
		UArcBuildingViewModel* BuildingVM = Found ? Found->Get() : nullptr;
		if (!BuildingVM)
		{
			BuildingVM = NewObject<UArcBuildingViewModel>(Owner);
		}

		BuildingVM->RefreshFrom(Def, InventoryRef);
		NewBuildings.Add(BuildingVM);
	}

	VM->SetBuildings(MoveTemp(NewBuildings));
}

// ---------------------------------------------------------------------------
// RefreshAffordability — player item-event rail (no list rebuild)
// ---------------------------------------------------------------------------

void FArcBuilderStructModel::RefreshAffordability()
{
	UArcBuilderViewModel* VM = BuilderVM.Get();
	if (!VM)
	{
		return;
	}

	UArcFrontendMVVMModelComponent* Owner = OwnerComp.Get();
	if (!Owner)
	{
		return;
	}

	// No early-out on a missing store: an empty store yields 0 counts / bCanAfford=false,
	// which is the correct state until the player actually has items.
	static const FArcMassItemStoreFragment EmptyStore;
	const FArcMassItemStoreFragment* PlayerInventory =
		ArcBuilderStructModelPrivate::ResolvePlayerInventory(*Owner, PlayerEntity);
	const FArcMassItemStoreFragment& InventoryRef = PlayerInventory ? *PlayerInventory : EmptyStore;

	const TArray<TObjectPtr<UArcBuildingViewModel>>& Buildings = VM->GetBuildings();
	for (TObjectPtr<UArcBuildingViewModel> BuildingVM : Buildings)
	{
		if (!BuildingVM)
		{
			continue;
		}
		UArcBuildingDefinition* Def = BuildingVM->GetDefinition();
		if (Def != nullptr)
		{
			BuildingVM->RefreshFrom(Def, InventoryRef);
		}
	}
}
