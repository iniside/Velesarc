// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcFrontendMVVMModelComponent.h"
#include "UObject/PrimaryAssetId.h"
#include "Mass/EntityHandle.h"
#include "GameplayTagContainer.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "ViewModels/ArcIngredientCandidateViewModel.h"

#include "ArcCraftingStructModel.generated.h"

class UArcCraftingViewModel;
class UArcRecipeViewModel;
class UArcTagDisplayRegistry;
class UArcStatDisplayRegistry;
class UMassAgentSubsystem;
class UMassSignalSubsystem;
class UArcMassItemEventSubsystem;
struct FMassEntityManager;

USTRUCT(BlueprintType)
struct ARCUIMODELS_API FArcCraftingStructModel : public FArcFrontendMVVMStructModel
{
	GENERATED_BODY()

	/** Name used to register the UArcCraftingViewModel in the MVVM global collection. */
	UPROPERTY(EditAnywhere)
	FName ContextName = TEXT("Crafting");

	/** Gameplay tag used to register the VM in UArcFrontendMVVMModelComponent::ViewModels. */
	UPROPERTY(EditAnywhere)
	FGameplayTag RegistrationTag;

	/** Tag-display registry used to resolve name + icon for generic (tag-based) recipe ingredients. */
	UPROPERTY(EditAnywhere, Category = "Display")
	TObjectPtr<UArcTagDisplayRegistry> TagDisplayRegistry;

	/** Stat-display registry used to resolve required-item stat-row labels + suffixes. */
	UPROPERTY(EditAnywhere, Category = "Display")
	TObjectPtr<UArcStatDisplayRegistry> StatDisplayRegistry;

	/** Item stores on the player entity whose loose items can go into the station input preview. */
	UPROPERTY(EditAnywhere, Category = "Inventory",
		meta = (MetaStruct = "/Script/ArcMassItemsRuntime.ArcMassItemStoreFragment"))
	TArray<TObjectPtr<UScriptStruct>> InventoryStoreTypes;

	/** Upper bound of a recipe's CraftableFromInput count. */
	UPROPERTY(EditAnywhere, Category = "Display", meta = (ClampMin = "1"))
	int32 MaxCraftableCount = 99;

	FArcCraftingStructModel()
	{
		bTickForLocalOnly = true;
		InventoryStoreTypes.Add(FArcMassItemStoreFragment::StaticStruct());
	}

	virtual void CreateViewModels(UArcFrontendMVVMModelComponent& Owner) override;
	virtual void Initialize(UArcFrontendMVVMModelComponent& Owner) override;
	virtual void Deinitialize(UArcFrontendMVVMModelComponent& Owner) override;
	virtual void Tick(float DeltaTime, UArcFrontendMVVMModelComponent& Component) override;

private:
	// -------------------------------------------------------------------------
	// Helpers
	// -------------------------------------------------------------------------

	// Resolves the local player's Mass entity (PlayerState -> Pawn -> MassAgentComponent)
	// and, on success, commits PlayerEntity, subscribes the player item events, and kicks
	// the initial population (hand-craft bind or availability recount). Returns false when
	// the entity isn't associated yet — the MassAgentSubsystem entity-associated delegate
	// subscribed in Initialize retries then. Idempotent once bound.
	bool TryBindToPlayerEntity(UArcFrontendMVVMModelComponent& Owner);

	void OnBoundStationChanged(FMassEntityHandle Old, FMassEntityHandle New);
	void OnSelectedRecipeChanged(UArcRecipeViewModel* NewRecipe);
	void OnInputPreviewChanged();
	void OnQueueChanged(FName SignalName, TConstArrayView<FMassEntityHandle> Entities);
	void OnInputChanged(FName SignalName, TConstArrayView<FMassEntityHandle> Entities);
	void OnOutputChanged(FName SignalName, TConstArrayView<FMassEntityHandle> Entities);
	void RebuildRecipesForStation(FMassEntityHandle Station);
	void RebuildQueueForStation(FMassEntityHandle Station);

	// Re-runs UArcRecipeViewModel::RefreshFrom against the inventory rows, the preview and the server input.
	// OnlyRecipe != nullptr restricts the refresh to that single recipe VM (selection rail);
	// nullptr refreshes every recipe VM (player item-event, station input and preview rails).
	void RefreshRecipeAvailability(UArcRecipeViewModel* OnlyRecipe);

	TArray<FArcCraftStoreItemRow> GatherInventoryRows(FMassEntityManager& Manager) const;
	void RefreshAllRecipeAvailability() { RefreshRecipeAvailability(nullptr); }

	// Lightweight push of the bound station's input/output item lists to the VM —
	// no recipe iteration (distinct from RefreshAllRecipeAvailability).
	void RefreshStationInput();
	void RefreshStationOutput();

	// -------------------------------------------------------------------------
	// Cached state
	// -------------------------------------------------------------------------

	TWeakObjectPtr<UArcCraftingViewModel>          CraftingVM;
	TWeakObjectPtr<UArcFrontendMVVMModelComponent> OwnerComp;
	FMassEntityHandle                              PlayerEntity;

	// Entity-association late-bind — the pawn's MassAgentComponent frequently hasn't
	// associated its entity yet when Initialize fires (same race the bag model handles).
	TWeakObjectPtr<UMassAgentSubsystem> BoundAgentSubsystem;
	FDelegateHandle                     EntityAssociatedHandle;

	// Player item-event subscriptions, one set per InventoryStoreTypes entry — bound once in
	// TryBindToPlayerEntity, torn down in Deinitialize.
	TWeakObjectPtr<UArcMassItemEventSubsystem> BoundItemEventSubsystem;
	TArray<FDelegateHandle>                    ItemEventHandles;

	// Queue-signal and input-signal subscriptions — rebound on every station change
	TWeakObjectPtr<UMassSignalSubsystem> BoundSignalSubsystem;
	FDelegateHandle                      QueueSignalHandle;
	FDelegateHandle                      InputSignalHandle;
	FDelegateHandle                      OutputSignalHandle;
	FMassEntityHandle                    SubscribedStationEntity;

	// Recipe record ids discovered on last RebuildRecipesForStation.
	TArray<FPrimaryAssetId> CachedStationRecipeIds;
};
