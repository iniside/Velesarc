// Copyright Lukasz Baran. All Rights Reserved.

#include "Models/ArcCraftingStructModel.h"

#include "ArcFrontendMVVMModelComponent.h"
#include "MassAgentComponent.h"
#include "MassAgentSubsystem.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "MassSignalSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Types/MVVMViewModelCollection.h"
#include "Types/MVVMViewModelContext.h"

#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Mass/ArcCraftDeliveryConfigFragment.h"
#include "ArcCraft/Mass/ArcCraftQueueChangedSignal.h"
#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Shared/ArcCraftRecords.h"
#include "ArcCraft/Station/ArcCraftItemSource.h"
#include "ArcCraft/Station/ArcCraftOutputDelivery_EntityItemStore.h"
#include "ArcCraft/Station/ArcRecipeLookup.h"

#include "Events/ArcMassItemEventSubsystem.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemData.h"

#include "ViewModels/ArcCraftingViewModel.h"
#include "ViewModels/ArcRecipeViewModel.h"
#include "ViewModels/ArcCraftQueueEntryViewModel.h"

namespace ArcCraftingStructModelPrivate
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
}

// ---------------------------------------------------------------------------
// CreateViewModels
// ---------------------------------------------------------------------------

void FArcCraftingStructModel::CreateViewModels(UArcFrontendMVVMModelComponent& Owner)
{
	bTickForLocalOnly = true;  // FORCE — overrides any serialized override

	FArcViewModelSpec Spec;
	Spec.ContextName = ContextName;
	Spec.ViewModelClass = UArcCraftingViewModel::StaticClass();
	Spec.RegistrationTag = RegistrationTag;
	Owner.CreateAndRegisterViewModels({ Spec });

	CraftingVM = Cast<UArcCraftingViewModel>(Owner.FindViewModelByContext(ContextName, UArcCraftingViewModel::StaticClass()));
	OwnerComp = &Owner;
}

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------

void FArcCraftingStructModel::Initialize(UArcFrontendMVVMModelComponent& Owner)
{
	UArcCraftingViewModel* VM = CraftingVM.Get();
	if (!VM)
	{
		return;
	}

	// Bind to station-change delegate up front — station binds can arrive from interaction
	// code independently of the player-entity bind below.
	VM->OnBoundStationChangedDelegate.AddRaw(this, &FArcCraftingStructModel::OnBoundStationChanged);

	// Recount ingredient availability whenever the player selects a different recipe, so the
	// detail panel always reflects the live inventory + station input at selection time.
	VM->OnSelectedRecipeChangedDelegate.AddRaw(this, &FArcCraftingStructModel::OnSelectedRecipeChanged);

	VM->OnInputPreviewChangedDelegate.AddRaw(this, &FArcCraftingStructModel::OnInputPreviewChanged);

	// Subscribe to entity-association notifications so we can late-bind when the pawn's
	// MassAgentComponent hasn't associated its entity yet at Initialize time (frequently
	// misses on first frame — same race FArcBagStructModel handles). Without the retry,
	// PlayerEntity stays invalid forever and ingredient availability counts only
	// station-deposited items (player inventory resolves to an empty store).
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

	// Attempt immediate bind; if the entity isn't associated yet the delegate above retries.
	TryBindToPlayerEntity(Owner);
}

bool FArcCraftingStructModel::TryBindToPlayerEntity(UArcFrontendMVVMModelComponent& Owner)
{
	// Already bound — nothing to do.
	if (PlayerEntity.IsSet())
	{
		return true;
	}

	UArcCraftingViewModel* VM = CraftingVM.Get();
	if (!VM)
	{
		return false;
	}

	const FMassEntityHandle Entity = ArcCraftingStructModelPrivate::ResolvePlayerEntity(Owner);
	if (!Entity.IsSet())
	{
		return false;
	}

	PlayerEntity = Entity;

	// Push context fields into the VM.
	VM->SetInstigatorEntity(PlayerEntity);

	// Subscribe to player inventory events so recipe ingredient availability refreshes on
	// any add / remove / stack-change in the player's item store.
	UWorld* World = Owner.GetWorld();
	if (World == nullptr)
	{
		return true;
	}

	UArcMassItemEventSubsystem* ItemEvents = World->GetSubsystem<UArcMassItemEventSubsystem>();
	if (ItemEvents)
	{
		BoundItemEventSubsystem = ItemEvents;

		TWeakObjectPtr<UArcFrontendMVVMModelComponent> WeakOwner(&Owner);

		for (const TObjectPtr<UScriptStruct>& StoreType : InventoryStoreTypes)
		{
			if (StoreType == nullptr)
			{
				continue;
			}

			ItemEventHandles.Add(ItemEvents->AddItemAddedForEntity(
				PlayerEntity,
				StoreType,
				UArcMassItemEventSubsystem::FOnItemAdded::FDelegate::CreateLambda(
					[this, WeakOwner](FMassEntityHandle /*Entity*/, FArcItemId /*ItemId*/, const UScriptStruct* /*StoreType*/)
					{
						if (WeakOwner.IsValid())
						{
							RefreshAllRecipeAvailability();
						}
					})));

			ItemEventHandles.Add(ItemEvents->AddItemRemovedForEntity(
				PlayerEntity,
				StoreType,
				UArcMassItemEventSubsystem::FOnItemRemoved::FDelegate::CreateLambda(
					[this, WeakOwner](FMassEntityHandle /*Entity*/, FArcItemId /*ItemId*/, const UScriptStruct* /*StoreType*/)
					{
						if (WeakOwner.IsValid())
						{
							RefreshAllRecipeAvailability();
						}
					})));

			ItemEventHandles.Add(ItemEvents->AddItemStacksChangedForEntity(
				PlayerEntity,
				StoreType,
				UArcMassItemEventSubsystem::FOnItemStacksChanged::FDelegate::CreateLambda(
					[this, WeakOwner](FMassEntityHandle /*Entity*/, FArcItemId /*ItemId*/, int32 /*Delta*/, const UScriptStruct* /*StoreType*/)
					{
						if (WeakOwner.IsValid())
						{
							RefreshAllRecipeAvailability();
						}
					})));

			// Slotting an item makes it non-loose, so it leaves the inventory rows.
			ItemEventHandles.Add(ItemEvents->AddItemSlotChangedForEntity(
				PlayerEntity,
				StoreType,
				UArcMassItemEventSubsystem::FOnItemSlotChanged::FDelegate::CreateLambda(
					[this, WeakOwner](FMassEntityHandle /*Entity*/, FArcItemId /*ItemId*/, FGameplayTag /*NewSlot*/, const UScriptStruct* /*StoreType*/)
					{
						if (WeakOwner.IsValid())
						{
							RefreshAllRecipeAvailability();
						}
					})));
		}
	}

	// Kick the initial population. On the immediate-bind path no station is bound yet, so
	// default to hand-crafting on the player entity — SetBoundStation fires the
	// station-change delegate (Old=invalid, New=PlayerEntity). On the late-bind path
	// interaction code may already have bound a station whose recipes were built against
	// an empty inventory — recount availability against the now-resolved store instead.
	if (VM->GetBoundStation().IsSet())
	{
		RefreshAllRecipeAvailability();
	}
	else
	{
		VM->SetBoundStation(PlayerEntity);
	}

	return true;
}

// ---------------------------------------------------------------------------
// Deinitialize
// ---------------------------------------------------------------------------

void FArcCraftingStructModel::Deinitialize(UArcFrontendMVVMModelComponent& /*Owner*/)
{
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
		for (const FDelegateHandle& Handle : ItemEventHandles)
		{
			ItemEvents->RemoveForEntity(PlayerEntity, Handle);
		}
	}
	ItemEventHandles.Reset();
	BoundItemEventSubsystem.Reset();

	// Tear down queue-signal and input-signal subscriptions.
	if (UMassSignalSubsystem* Signals = BoundSignalSubsystem.Get())
	{
		if (QueueSignalHandle.IsValid())
		{
			Signals->GetSignalDelegateByName(ArcCraftSignals::QueueChanged).Remove(QueueSignalHandle);
		}
		if (InputSignalHandle.IsValid())
		{
			Signals->GetSignalDelegateByName(ArcCraftSignals::InputChanged).Remove(InputSignalHandle);
		}
		if (OutputSignalHandle.IsValid())
		{
			Signals->GetSignalDelegateByName(ArcCraftSignals::OutputChanged).Remove(OutputSignalHandle);
		}
	}
	QueueSignalHandle.Reset();
	InputSignalHandle.Reset();
	OutputSignalHandle.Reset();
	SubscribedStationEntity = FMassEntityHandle();
	BoundSignalSubsystem.Reset();

	// Tear down station-change and recipe-selection delegates.
	if (UArcCraftingViewModel* VM = CraftingVM.Get())
	{
		VM->OnBoundStationChangedDelegate.RemoveAll(this);
		VM->OnSelectedRecipeChangedDelegate.RemoveAll(this);
		VM->OnInputPreviewChangedDelegate.RemoveAll(this);
	}

	CraftingVM.Reset();
	OwnerComp.Reset();
	PlayerEntity = FMassEntityHandle();
}

// ---------------------------------------------------------------------------
// OnBoundStationChanged — subscription lifecycle hub
// ---------------------------------------------------------------------------

void FArcCraftingStructModel::OnBoundStationChanged(FMassEntityHandle Old, FMassEntityHandle New)
{
	UArcFrontendMVVMModelComponent* Owner = OwnerComp.Get();
	if (!Owner)
	{
		return;
	}

	UWorld* World = Owner->GetWorld();
	if (!World)
	{
		return;
	}

	// --- Unsubscribe from the old station's queue and input signals ---
	if (SubscribedStationEntity.IsSet() && SubscribedStationEntity == Old)
	{
		if (UMassSignalSubsystem* Signals = BoundSignalSubsystem.Get())
		{
			if (QueueSignalHandle.IsValid())
			{
				Signals->GetSignalDelegateByName(ArcCraftSignals::QueueChanged).Remove(QueueSignalHandle);
			}
			if (InputSignalHandle.IsValid())
			{
				Signals->GetSignalDelegateByName(ArcCraftSignals::InputChanged).Remove(InputSignalHandle);
			}
			if (OutputSignalHandle.IsValid())
			{
				Signals->GetSignalDelegateByName(ArcCraftSignals::OutputChanged).Remove(OutputSignalHandle);
			}
		}
		QueueSignalHandle.Reset();
		InputSignalHandle.Reset();
		OutputSignalHandle.Reset();
		SubscribedStationEntity = FMassEntityHandle();
		BoundSignalSubsystem.Reset();
	}

	if (!New.IsSet())
	{
		if (UArcCraftingViewModel* VM = CraftingVM.Get())
		{
			VM->SetbSupportsManualDeposit(false);
			// Drop stale station surfaces so they don't persist across station
			// switch / panel close (reviewer N1).
			VM->ClearStationInput();
			VM->ClearStationOutput();
		}
		return;
	}

	UArcCraftingViewModel* VM = CraftingVM.Get();
	if (!VM)
	{
		return;
	}

	// --- Determine display name ---
	const FText DisplayName = (New == PlayerEntity)
		? FText::FromString(TEXT("Hand Crafting"))
		: FText::FromString(TEXT("Workbench"));
	VM->SetStationDisplayName(DisplayName);

	// --- Determine bHasQueue from delivery config ---
	UMassEntitySubsystem* MassSS = World->GetSubsystem<UMassEntitySubsystem>();
	if (MassSS)
	{
		FMassEntityManager& Manager = MassSS->GetMutableEntityManager();
		if (Manager.IsEntityValid(New))
		{
			const FMassEntityView EntityView(Manager, New);
			const FArcCraftDeliveryConfigFragment* DeliveryCfg =
				EntityView.GetConstSharedFragmentDataPtr<FArcCraftDeliveryConfigFragment>();
			if (DeliveryCfg)
			{
				const bool bIsEntityItemStore =
					DeliveryCfg->OutputDelivery.GetScriptStruct() ==
					FArcCraftOutputDelivery_EntityItemStore::StaticStruct();
				VM->SetbHasQueue(!bIsEntityItemStore);

				const FArcCraftItemSource* Source = DeliveryCfg->IngredientSource.GetPtr<FArcCraftItemSource>();
				VM->SetbSupportsManualDeposit(Source != nullptr && Source->AcceptsDeposits());
			}
			else
			{
				// No config fragment — default to showing a queue; no deposit support.
				VM->SetbHasQueue(true);
				VM->SetbSupportsManualDeposit(false);
			}
		}
	}

	// --- Clear any stale recipe selection before the Recipes array is replaced ---
	VM->SelectRecipe(nullptr);

	// --- Rebuild recipes and queue ---
	RebuildRecipesForStation(New);
	RebuildQueueForStation(New);

	// --- Subscribe to queue-changed and input-changed signals on the new station ---
	UMassSignalSubsystem* SignalSS = World->GetSubsystem<UMassSignalSubsystem>();
	if (SignalSS)
	{
		BoundSignalSubsystem = SignalSS;
		SubscribedStationEntity = New;

		QueueSignalHandle = SignalSS->GetSignalDelegateByName(ArcCraftSignals::QueueChanged).AddRaw(
			this, &FArcCraftingStructModel::OnQueueChanged);

		InputSignalHandle = SignalSS->GetSignalDelegateByName(ArcCraftSignals::InputChanged).AddRaw(
			this, &FArcCraftingStructModel::OnInputChanged);

		OutputSignalHandle = SignalSS->GetSignalDelegateByName(ArcCraftSignals::OutputChanged).AddRaw(
			this, &FArcCraftingStructModel::OnOutputChanged);
	}

	// Initial populate of the shared station input/output surfaces for the new station.
	RefreshStationInput();
	RefreshStationOutput();
}

// ---------------------------------------------------------------------------
// OnSelectedRecipeChanged — recount availability for the freshly selected recipe
// ---------------------------------------------------------------------------

void FArcCraftingStructModel::OnSelectedRecipeChanged(UArcRecipeViewModel* NewRecipe)
{
	if (NewRecipe == nullptr)
	{
		return;
	}

	// The player entity may still be unbound (association race) — retry before counting
	// so the recount below sees the real inventory as soon as it exists.
	if (!PlayerEntity.IsSet())
	{
		if (UArcFrontendMVVMModelComponent* Owner = OwnerComp.Get())
		{
			TryBindToPlayerEntity(*Owner);
		}
	}

	// Only the freshly selected recipe — the rest recount on their own rails.
	RefreshRecipeAvailability(NewRecipe);
}

void FArcCraftingStructModel::OnInputPreviewChanged()
{
	RefreshAllRecipeAvailability();
}

// ---------------------------------------------------------------------------
// OnQueueChanged — signal delegate callback
// ---------------------------------------------------------------------------

void FArcCraftingStructModel::OnQueueChanged(FName /*SignalName*/, TConstArrayView<FMassEntityHandle> Entities)
{
	for (const FMassEntityHandle& Entity : Entities)
	{
		if (Entity == SubscribedStationEntity)
		{
			RebuildQueueForStation(Entity);
			return;
		}
	}
}

// ---------------------------------------------------------------------------
// OnInputChanged — signal delegate callback
// ---------------------------------------------------------------------------

void FArcCraftingStructModel::OnInputChanged(FName /*SignalName*/, TConstArrayView<FMassEntityHandle> Entities)
{
	for (const FMassEntityHandle& Entity : Entities)
	{
		if (Entity == SubscribedStationEntity)
		{
			RefreshStationInput();
			// Station deposit/withdraw/craft-consumption changes ingredient availability
			// (HaveFromStation) for every recipe — refresh all recipe VMs too.
			RefreshAllRecipeAvailability();
			return;
		}
	}
}

// ---------------------------------------------------------------------------
// OnOutputChanged — signal delegate callback
// ---------------------------------------------------------------------------

void FArcCraftingStructModel::OnOutputChanged(FName /*SignalName*/, TConstArrayView<FMassEntityHandle> Entities)
{
	for (const FMassEntityHandle& Entity : Entities)
	{
		if (Entity == SubscribedStationEntity)
		{
			RefreshStationOutput();
			return;
		}
	}
}

// ---------------------------------------------------------------------------
// Tick — simulate progress on the active queue entry between signals
// ---------------------------------------------------------------------------

void FArcCraftingStructModel::Tick(
	float /*DeltaTime*/,
	UArcFrontendMVVMModelComponent& Component)
{
	UArcCraftingViewModel* VM = CraftingVM.Get();
	if (VM == nullptr)
	{
		return;
	}

	// Late-bind retry: covers orderings the association delegate can't — e.g. the pawn's
	// MassAgentComponent associated before Initialize ran, or PlayerState.GetPawn() linked
	// up after the association event fired. No-op once bound (cheap IsSet check).
	if (!PlayerEntity.IsSet())
	{
		TryBindToPlayerEntity(Component);
	}
	if (!SubscribedStationEntity.IsSet())
	{
		return;
	}

	const TArray<TObjectPtr<UArcCraftQueueEntryViewModel>>& QueueVMs = VM->GetQueue();
	if (QueueVMs.IsEmpty())
	{
		return;
	}

	const double LocalNowSeconds   = FPlatformTime::Seconds();
	const int64  UtcNowUnixSeconds = FDateTime::UtcNow().ToUnixTimestamp();

	for (const TObjectPtr<UArcCraftQueueEntryViewModel>& EntryVM : QueueVMs)
	{
		if (EntryVM && EntryVM->GetbIsActive())
		{
			EntryVM->UpdateSimulatedProgress(LocalNowSeconds, UtcNowUnixSeconds);
			break;  // queue invariant: at most one active entry (front of queue)
		}
	}
}

// ---------------------------------------------------------------------------
// RebuildRecipesForStation
// ---------------------------------------------------------------------------

void FArcCraftingStructModel::RebuildRecipesForStation(FMassEntityHandle Station)
{
	UArcCraftingViewModel* VM = CraftingVM.Get();
	if (!VM)
	{
		CachedStationRecipeIds.Reset();
		return;
	}

	UArcFrontendMVVMModelComponent* Owner = OwnerComp.Get();
	if (!Owner)
	{
		CachedStationRecipeIds.Reset();
		VM->SetRecipes(TArray<TObjectPtr<UArcRecipeViewModel>>());
		return;
	}

	UWorld* World = Owner->GetWorld();
	UMassEntitySubsystem* MassSS = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!MassSS)
	{
		CachedStationRecipeIds.Reset();
		VM->SetRecipes(TArray<TObjectPtr<UArcRecipeViewModel>>());
		return;
	}

	FMassEntityManager& Manager = MassSS->GetMutableEntityManager();
	if (!Manager.IsEntityValid(Station))
	{
		CachedStationRecipeIds.Reset();
		VM->SetRecipes(TArray<TObjectPtr<UArcRecipeViewModel>>());
		return;
	}

	const FMassEntityView EntityView(Manager, Station);

	// Two-axis recipe discovery. The station config fragment carries both the station *type*
	// tags (StationTags) and the tier/category tags this station offers (OfferedRecipeTags).
	// A recipe is offered iff its RequiredStationTags ⊆ StationTags AND its RecipeTags overlap
	// OfferedRecipeTags. A station with empty OfferedRecipeTags offers nothing via discovery.
	// (The player/hand-craft entity has this fragment too — authored on MC_Player.)
	FGameplayTagContainer StationTags;
	FGameplayTagContainer OfferedRecipeTags;
	const FArcCraftStationConfigFragment* StationCfg =
		EntityView.GetConstSharedFragmentDataPtr<FArcCraftStationConfigFragment>();
	if (StationCfg)
	{
		StationTags = StationCfg->StationTags;
		OfferedRecipeTags = StationCfg->OfferedRecipeTags;
	}

	CachedStationRecipeIds = FArcRecipeLookup::FindRecipesForStation(StationTags, OfferedRecipeTags);

	// Build or recycle UArcRecipeViewModel objects.
	// Existing VMs keyed by RecipeId are reused to preserve widget bindings.
	const TArray<TObjectPtr<UArcRecipeViewModel>>& ExistingRecipes = VM->GetRecipes();
	TMap<FPrimaryAssetId, TObjectPtr<UArcRecipeViewModel>> ExistingMap;
	ExistingMap.Reserve(ExistingRecipes.Num());
	for (TObjectPtr<UArcRecipeViewModel> ExistingVM : ExistingRecipes)
	{
		if (ExistingVM)
		{
			ExistingMap.Add(ExistingVM->GetRecipeId().AssetId, ExistingVM);
		}
	}

	VM->SetInventoryRows(GatherInventoryRows(Manager));
	const TArray<FArcCraftStoreItemRow> InventoryAfterPreview = VM->GetInventoryRowsAfterPreview();

	const FArcCraftInputFragment* StationInputFrag = EntityView.GetFragmentDataPtr<FArcCraftInputFragment>();
	TConstArrayView<FArcItemSpec> StationInputView = StationInputFrag
		? TConstArrayView<FArcItemSpec>(StationInputFrag->InputItems)
		: TConstArrayView<FArcItemSpec>();

	TArray<TObjectPtr<UArcRecipeViewModel>> NewRecipes;
	NewRecipes.Reserve(CachedStationRecipeIds.Num());

	for (const FPrimaryAssetId& RecipeAssetId : CachedStationRecipeIds)
	{
		const FArcRecipeDefinitionData* RecipeDef = ArcCraftRecords::Find<FArcRecipeDefinitionData>(RecipeAssetId);
		if (!RecipeDef)
		{
			continue;
		}

		TObjectPtr<UArcRecipeViewModel>* Found = ExistingMap.Find(RecipeAssetId);
		UArcRecipeViewModel* RecipeVM = Found ? Found->Get() : nullptr;
		if (!RecipeVM)
		{
			RecipeVM = NewObject<UArcRecipeViewModel>(Owner);
		}

		RecipeVM->RefreshFrom(
			RecipeAssetId,
			*RecipeDef,
			InventoryAfterPreview,
			StationInputView,
			VM->GetPreviewRows(),
			TagDisplayRegistry,
			StatDisplayRegistry,
			MaxCraftableCount);

		NewRecipes.Add(RecipeVM);
	}

	VM->SetRecipes(MoveTemp(NewRecipes));
}

// ---------------------------------------------------------------------------
// RebuildQueueForStation
// Rebuilds the Queue FieldNotify array from the server-committed queue fragment
// only (positional VM recycling).
// ---------------------------------------------------------------------------

void FArcCraftingStructModel::RebuildQueueForStation(FMassEntityHandle Station)
{
	UArcCraftingViewModel* VM = CraftingVM.Get();
	if (!VM)
	{
		return;
	}

	UArcFrontendMVVMModelComponent* Owner = OwnerComp.Get();
	if (!Owner)
	{
		return;
	}

	// Committed entries from the server-side queue fragment.
	TArray<FArcCraftQueueEntry> CommittedEntries;

	UWorld* World = Owner->GetWorld();
	UMassEntitySubsystem* MassSS = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (MassSS)
	{
		FMassEntityManager& Manager = MassSS->GetMutableEntityManager();
		if (Manager.IsEntityValid(Station))
		{
			const FMassEntityView EntityView(Manager, Station);
			const FArcCraftQueueFragment* QueueFrag = EntityView.GetFragmentDataPtr<FArcCraftQueueFragment>();
			if (QueueFrag)
			{
				CommittedEntries = QueueFrag->Entries;
			}
		}
	}

	// Build the committed Queue — positional recycle against the existing committed VMs.
	const TArray<TObjectPtr<UArcCraftQueueEntryViewModel>>& ExistingQueue = VM->GetQueue();
	const int32 CommittedCount = CommittedEntries.Num();

	TArray<TObjectPtr<UArcCraftQueueEntryViewModel>> NewQueue;
	NewQueue.Reserve(CommittedCount);

	for (int32 EntryIdx = 0; EntryIdx < CommittedCount; ++EntryIdx)
	{
		const FArcCraftQueueEntry& Entry = CommittedEntries[EntryIdx];
		const bool bIsActive = (Entry.State == EArcCraftQueueEntryState::Active);

		UArcCraftQueueEntryViewModel* EntryVM = nullptr;
		if (EntryIdx < ExistingQueue.Num() && ExistingQueue[EntryIdx])
		{
			EntryVM = ExistingQueue[EntryIdx].Get();
		}
		if (!EntryVM)
		{
			EntryVM = NewObject<UArcCraftQueueEntryViewModel>(Owner);
		}

		EntryVM->RefreshFrom(Entry, ArcCraftRecords::Find<FArcRecipeDefinitionData>(Entry.RecipeId), bIsActive);
		NewQueue.Add(EntryVM);
	}

	VM->SetQueue(MoveTemp(NewQueue));
}

// ---------------------------------------------------------------------------
// RefreshRecipeAvailability — re-run RefreshFrom for one recipe VM (OnlyRecipe)
// or every existing recipe VM (nullptr) against the inventory rows, the preview and the server input.
// ---------------------------------------------------------------------------

void FArcCraftingStructModel::RefreshRecipeAvailability(UArcRecipeViewModel* OnlyRecipe)
{
	UArcCraftingViewModel* VM = CraftingVM.Get();
	if (!VM)
	{
		return;
	}

	UArcFrontendMVVMModelComponent* Owner = OwnerComp.Get();
	if (!Owner)
	{
		return;
	}

	UWorld* World = Owner->GetWorld();
	UMassEntitySubsystem* MassSS = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!MassSS)
	{
		return;
	}

	FMassEntityManager& Manager = MassSS->GetMutableEntityManager();

	// An unbound PlayerEntity must not skip the refresh: station-input counts still update.
	VM->SetInventoryRows(GatherInventoryRows(Manager));
	const TArray<FArcCraftStoreItemRow> InventoryAfterPreview = VM->GetInventoryRowsAfterPreview();

	TConstArrayView<FArcItemSpec> StationInputView;
	const FMassEntityHandle StationEntity = VM->GetBoundStation();
	if (Manager.IsEntityValid(StationEntity))
	{
		const FMassEntityView StationView(Manager, StationEntity);
		const FArcCraftInputFragment* StationInputFrag = StationView.GetFragmentDataPtr<FArcCraftInputFragment>();
		if (StationInputFrag)
		{
			StationInputView = TConstArrayView<FArcItemSpec>(StationInputFrag->InputItems);
		}
	}

	const TArray<TObjectPtr<UArcRecipeViewModel>>& Recipes = VM->GetRecipes();
	for (TObjectPtr<UArcRecipeViewModel> RecipeVM : Recipes)
	{
		if (!RecipeVM)
		{
			continue;
		}
		if (OnlyRecipe != nullptr && RecipeVM != OnlyRecipe)
		{
			continue;
		}

		const FPrimaryAssetId& RecipeId = RecipeVM->GetRecipeId().AssetId;
		if (!CachedStationRecipeIds.Contains(RecipeId))
		{
			continue;
		}
		const FArcRecipeDefinitionData* RecipeDef = ArcCraftRecords::Find<FArcRecipeDefinitionData>(RecipeId);
		if (RecipeDef)
		{
			RecipeVM->RefreshFrom(
				RecipeId,
				*RecipeDef,
				InventoryAfterPreview,
				StationInputView,
				VM->GetPreviewRows(),
				TagDisplayRegistry,
				StatDisplayRegistry,
				MaxCraftableCount);
		}
	}
}

// ---------------------------------------------------------------------------
// RefreshStationInput — lightweight push of the station's input items to the VM.
// ---------------------------------------------------------------------------

void FArcCraftingStructModel::RefreshStationInput()
{
	UArcCraftingViewModel* VM = CraftingVM.Get();
	if (!VM)
	{
		return;
	}

	UArcFrontendMVVMModelComponent* Owner = OwnerComp.Get();
	if (!Owner)
	{
		return;
	}

	UWorld* World = Owner->GetWorld();
	UMassEntitySubsystem* MassSS = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!MassSS)
	{
		return;
	}

	FMassEntityManager& Manager = MassSS->GetMutableEntityManager();
	const FMassEntityHandle StationEntity = VM->GetBoundStation();
	if (!Manager.IsEntityValid(StationEntity))
	{
		return;
	}

	const FMassEntityView StationView(Manager, StationEntity);
	const FArcCraftInputFragment* InputFrag = StationView.GetFragmentDataPtr<FArcCraftInputFragment>();
	if (InputFrag)
	{
		VM->SetServerInput(InputFrag->InputItems, TagDisplayRegistry);
	}
	else
	{
		VM->SetServerInput(TConstArrayView<FArcItemSpec>(), TagDisplayRegistry);
	}
}

// ---------------------------------------------------------------------------
// GatherInventoryRows — loose items in the player's InventoryStoreTypes stores.
// ---------------------------------------------------------------------------

TArray<FArcCraftStoreItemRow> FArcCraftingStructModel::GatherInventoryRows(FMassEntityManager& Manager) const
{
	TArray<FArcCraftStoreItemRow> Rows;
	if (!Manager.IsEntityValid(PlayerEntity))
	{
		return Rows;
	}

	for (const TObjectPtr<UScriptStruct>& StoreType : InventoryStoreTypes)
	{
		if (StoreType == nullptr || !StoreType->IsChildOf(FArcMassItemStoreFragment::StaticStruct()))
		{
			continue;
		}

		const FConstStructView StoreView = Manager.GetSparseElementDataForEntity(StoreType, PlayerEntity);
		const FArcMassItemStoreFragment* Store = StoreView.GetPtr<FArcMassItemStoreFragment>();
		if (Store == nullptr)
		{
			continue;
		}

		for (const FArcMassReplicatedItem& RepItem : Store->ReplicatedItems.Items)
		{
			const FArcItemData* ItemData = RepItem.ToItem();
			if (ItemData == nullptr || ItemData->GetStacks() == 0)
			{
				continue;
			}
			// The craft command rejects slotted and attached items.
			if (ItemData->GetSlotId().IsValid() || ItemData->GetOwnerId().IsValid() || !ItemData->GetAttachedItems().IsEmpty())
			{
				continue;
			}

			FArcCraftStoreItemRow& Row = Rows.AddDefaulted_GetRef();
			Row.OwnerEntity = PlayerEntity;
			Row.StoreType = StoreType;
			Row.Spec = ItemData->Spec;
			Row.Spec.SetItemId(ItemData->GetItemId());
			Row.Spec.Amount = ItemData->GetStacks();
			Row.Stacks = static_cast<int32>(ItemData->GetStacks());
		}
	}
	return Rows;
}

// ---------------------------------------------------------------------------
// RefreshStationOutput — lightweight push of the station's output items to the VM.
// ---------------------------------------------------------------------------

void FArcCraftingStructModel::RefreshStationOutput()
{
	UArcCraftingViewModel* VM = CraftingVM.Get();
	if (!VM)
	{
		return;
	}

	UArcFrontendMVVMModelComponent* Owner = OwnerComp.Get();
	if (!Owner)
	{
		return;
	}

	UWorld* World = Owner->GetWorld();
	UMassEntitySubsystem* MassSS = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!MassSS)
	{
		return;
	}

	FMassEntityManager& Manager = MassSS->GetMutableEntityManager();
	const FMassEntityHandle StationEntity = VM->GetBoundStation();
	if (!Manager.IsEntityValid(StationEntity))
	{
		return;
	}

	const FMassEntityView StationView(Manager, StationEntity);
	const FArcCraftOutputFragment* OutputFrag = StationView.GetFragmentDataPtr<FArcCraftOutputFragment>();
	if (OutputFrag)
	{
		VM->RebuildStationOutput(OutputFrag->OutputItems, TagDisplayRegistry);
	}
	else
	{
		VM->RebuildStationOutput(TArray<FArcItemSpec>(), TagDisplayRegistry);
	}
}
