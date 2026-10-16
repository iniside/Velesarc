// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcNamedPrimaryAssetId.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemId.h"
#include "Mass/EntityHandle.h"
#include "MVVMViewModelBase.h"
#include "ViewModels/ArcIngredientCandidateViewModel.h"
#include "ViewModels/ArcRecipeViewModel.h"

#include "ArcCraftingViewModel.generated.h"

struct FArcCraftInputSourceGroup;

class UArcItemViewModel;
class UArcIngredientCandidateViewModel;
class UArcCraftQueueEntryViewModel;
class UArcTagDisplayRegistry;

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FArcOnBoundStationChanged,
	FMassEntityHandle /*OldStation*/,
	FMassEntityHandle /*NewStation*/);

DECLARE_MULTICAST_DELEGATE_OneParam(
	FArcOnSelectedRecipeChanged,
	UArcRecipeViewModel* /*NewRecipe*/);

DECLARE_MULTICAST_DELEGATE(FArcOnInputPreviewChanged);

UCLASS(BlueprintType)
class ARCUIMODELS_API UArcCraftingViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	// -------------------------------------------------------------------------
	// FieldNotify properties (read by widget bindings)
	// -------------------------------------------------------------------------

	/** Stable per-recipe VMs. Recycled by RecipeId on each refresh. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TArray<TObjectPtr<UArcRecipeViewModel>> Recipes;

	/** Per-queue-entry VMs for server-committed entries. Order mirrors the station's Entries array. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TArray<TObjectPtr<UArcCraftQueueEntryViewModel>> Queue;

	/**
	 * Station input view: the server input rows, then the local preview rows (bPreview = true).
	 * Use WithdrawInputItem to take a row back out.
	 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TArray<TObjectPtr<UArcIngredientCandidateViewModel>> StationInput;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bHasInputPreview = false;

	/**
	 * Flat list of all crafted output items waiting in the station's output storage.
	 * Populated and cleared by the StructModel on OutputChanged signals.
	 * Use WithdrawOutputItem to move an output item to the player's inventory.
	 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TArray<TObjectPtr<UArcItemViewModel>> StationOutput;

	/**
	 * Whether this station exposes a craft queue to the player.
	 * False when the delivery mode is _EntityItemStore (output goes straight to
	 * the instigator's inventory without a visible queue).
	 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bHasQueue = true;

	/** The station's ingredient source accepts deposits; without it the preview cannot be edited. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bSupportsManualDeposit = false;

	/** Display name of the bound crafting station. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText StationDisplayName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	int32 CraftAmount = 1;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bShowStationOutput = false;

	int32 GetCraftAmount() const { return CraftAmount; }
	bool GetbShowStationOutput() const { return bShowStationOutput; }

	UFUNCTION(BlueprintCallable, Category = "Arc Crafting")
	void SetCraftAmount(int32 NewAmount);

	UFUNCTION(BlueprintCallable, Category = "Arc Crafting")
	void SetbShowStationOutput(bool bNewShowStationOutput);

	const TArray<TObjectPtr<UArcRecipeViewModel>>& GetRecipes() const { return Recipes; }
	const TArray<TObjectPtr<UArcCraftQueueEntryViewModel>>& GetQueue() const { return Queue; }
	const TArray<TObjectPtr<UArcIngredientCandidateViewModel>>& GetStationInput() const { return StationInput; }
	bool GetbHasInputPreview() const { return bHasInputPreview; }
	const TArray<TObjectPtr<UArcItemViewModel>>& GetStationOutput() const { return StationOutput; }
	bool GetbHasQueue() const { return bHasQueue; }
	bool GetbSupportsManualDeposit() const { return bSupportsManualDeposit; }
	const FText& GetStationDisplayName() const { return StationDisplayName; }

	// -------------------------------------------------------------------------
	// Plain (non-FieldNotify) context fields — set by StructModel at bind time
	// -------------------------------------------------------------------------

	FMassEntityHandle BoundStation;
	FMassEntityHandle InstigatorEntity;

	FMassEntityHandle GetBoundStation() const { return BoundStation; }
	FMassEntityHandle GetInstigatorEntity() const { return InstigatorEntity; }

	// -------------------------------------------------------------------------
	// Delegate fired whenever BoundStation changes
	// -------------------------------------------------------------------------

	FArcOnBoundStationChanged OnBoundStationChangedDelegate;

	// -------------------------------------------------------------------------
	// Delegate fired whenever SelectedRecipe changes to a new value — the struct
	// model uses it to recount ingredient availability on selection.
	// -------------------------------------------------------------------------

	FArcOnSelectedRecipeChanged OnSelectedRecipeChangedDelegate;

	/** Fired after a local preview edit; the struct model recounts recipe availability. */
	FArcOnInputPreviewChanged OnInputPreviewChangedDelegate;

	// -------------------------------------------------------------------------
	// Mutators called by StructModel
	// -------------------------------------------------------------------------

	/** Updates BoundStation, drops the preview and fires OnBoundStationChangedDelegate when the value differs. */
	void SetBoundStation(FMassEntityHandle NewStation);

	/** Caches the instigator entity (player-side Mass entity). */
	void SetInstigatorEntity(FMassEntityHandle NewEntity);

	// -------------------------------------------------------------------------
	// FieldNotify setters (called by StructModel to push fresh data)
	// -------------------------------------------------------------------------

	void SetRecipes(TArray<TObjectPtr<UArcRecipeViewModel>>&& NewRecipes);
	void SetQueue(TArray<TObjectPtr<UArcCraftQueueEntryViewModel>>&& NewQueue);
	void SetbHasQueue(bool bNewHasQueue);
	void SetbSupportsManualDeposit(bool bNewSupportsManualDeposit);
	void SetStationDisplayName(const FText& NewName);

	/** Stores the server station input and rebuilds the StationInput view. TagReg is kept for later preview rebuilds. */
	void SetServerInput(TConstArrayView<FArcItemSpec> Specs, const UArcTagDisplayRegistry* TagReg);

	/** Loose items in the player's inventory stores. Preview rows are clamped to them; no preview-change broadcast. */
	void SetInventoryRows(TArray<FArcCraftStoreItemRow>&& NewRows);

	const TArray<FArcCraftStoreItemRow>& GetPreviewRows() const { return PreviewRows; }

	/** Inventory rows minus the stacks already in the preview; rows left with no stacks are dropped. */
	TArray<FArcCraftStoreItemRow> GetInventoryRowsAfterPreview() const;

	/** Rebuilds StationOutput from station output specs. Called by StructModel on OutputChanged. */
	void RebuildStationOutput(const TArray<FArcItemSpec>& Specs, const UArcTagDisplayRegistry* TagReg);

	/** Clears the server input and the StationInput view. Called by StructModel on unbind. */
	void ClearStationInput();

	/** Clears StationOutput and notifies bindings. Called by StructModel on unbind. */
	void ClearStationOutput();

	// -------------------------------------------------------------------------
	// Player actions
	// -------------------------------------------------------------------------

	/** Records which recipe the player has selected in the UI. */
	UFUNCTION(BlueprintCallable, Category = "Arc Crafting")
	void SelectRecipe(UArcRecipeViewModel* InRecipe);

	/** Adds up to Stacks of the inventory item Item to the preview, capped by the stacks it holds. Sends nothing. */
	UFUNCTION(BlueprintCallable, Category = "Arc Crafting")
	bool TransferItemToStation(UArcItemViewModel* Item, int32 Stacks);

	/**
	 * Takes Stacks of an input view row back out. A preview row is edited locally; a server row sends
	 * FArcTransferItemFromStationCommand.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Crafting", meta = (WorldContext = "WorldContextObject"))
	bool WithdrawInputItem(UObject* WorldContextObject, UArcIngredientCandidateViewModel* Row, int32 Stacks);

	/**
	 * Sends FArcWithdrawCraftOutputCommand to move Item from the station's crafted output
	 * to the player's inventory. The item id is read off Item.
	 * WorldContextObject resolves the local player controller.
	 * Returns false if context/Item is invalid.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Crafting", meta = (WorldContext = "WorldContextObject"))
	bool WithdrawOutputItem(UObject* WorldContextObject, UArcItemViewModel* Item, int32 Stacks);

	/** Sends FArcWithdrawCraftOutputCommand for every StationOutput row with its full stacks. Returns true if at least one command was sent. */
	UFUNCTION(BlueprintCallable, Category = "Arc Crafting", meta = (WorldContext = "WorldContextObject"))
	bool WithdrawAllOutput(UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Arc Crafting", meta = (WorldContext = "WorldContextObject"))
	bool CraftFromStationInput(UObject* WorldContextObject, int32 Amount = 1);

	/** Adds each required slot's deficit for Amount crafts of SelectedRecipe from the inventory to the preview. Sends nothing. */
	UFUNCTION(BlueprintCallable, Category = "Arc Crafting")
	bool FillStationInput(int32 Amount = 1);

	/** Adds the first inventory item fitting Slot, up to the stacks the slot still needs, to the preview. Sends nothing. */
	UFUNCTION(BlueprintCallable, Category = "Arc Crafting")
	bool AddFirstMatchToInput(UArcRecipeIngredientViewModel* Slot);

	UFUNCTION(BlueprintCallable, Category = "Arc Crafting")
	void ClearInputPreview();

	// -------------------------------------------------------------------------
	// FieldNotify: selected recipe
	// -------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TObjectPtr<UArcRecipeViewModel> SelectedRecipe = nullptr;

	UArcRecipeViewModel* GetSelectedRecipe() const { return SelectedRecipe; }

	// -------------------------------------------------------------------------
	// FieldNotify: hovered / pinned / resolved detail ingredient (transient UI state)
	// -------------------------------------------------------------------------

	/** Ingredient currently under the pointer. Cleared on unhover and on recipe change. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TObjectPtr<UArcRecipeIngredientViewModel> HoveredIngredient = nullptr;

	/** Ingredient the player has pinned by clicking. Wins over hover for DetailIngredient. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TObjectPtr<UArcRecipeIngredientViewModel> PinnedIngredient = nullptr;

	/** Resolved detail target: PinnedIngredient when set, else HoveredIngredient. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TObjectPtr<UArcRecipeIngredientViewModel> DetailIngredient = nullptr;

	UArcRecipeIngredientViewModel* GetHoveredIngredient() const { return HoveredIngredient; }
	UArcRecipeIngredientViewModel* GetPinnedIngredient() const { return PinnedIngredient; }
	UArcRecipeIngredientViewModel* GetDetailIngredient() const { return DetailIngredient; }

	/** Sets the hovered ingredient (pass nullptr to clear) and recomputes DetailIngredient. */
	UFUNCTION(BlueprintCallable, Category = "Arc Crafting")
	void SetHoveredIngredient(UArcRecipeIngredientViewModel* InIngredient);

	/** Pins InIngredient; re-pinning the same ingredient toggles the pin off. Recomputes DetailIngredient. */
	UFUNCTION(BlueprintCallable, Category = "Arc Crafting")
	void PinIngredient(UArcRecipeIngredientViewModel* InIngredient);

private:
	/** Recomputes DetailIngredient = PinnedIngredient ? PinnedIngredient : HoveredIngredient. */
	void RefreshDetailIngredient();

	void RebuildStationInputView();

	/** Adds up to Stacks of the inventory row ItemId to the preview; returns the stacks added. */
	int32 AddToPreview(const FArcItemId& ItemId, int32 Stacks);

	int32 CountInputViewStacks(const UArcRecipeIngredientViewModel& Slot) const;

	void NotifyPreviewChanged();

	void RemoveSentFromPreview(TConstArrayView<FArcCraftInputSourceGroup> Sent);

	/** Private push setter for the StationOutput FieldNotify array. */
	void SetStationOutput(TArray<TObjectPtr<UArcItemViewModel>>&& NewStationOutput);

	UPROPERTY(Transient)
	TArray<FArcItemSpec> ServerInputSpecs;

	TWeakObjectPtr<const UArcTagDisplayRegistry> InputTagRegistry;

	UPROPERTY(Transient)
	TArray<FArcCraftStoreItemRow> InventoryRows;

	UPROPERTY(Transient)
	TArray<FArcCraftStoreItemRow> PreviewRows;

	/** Recycle pool for StationOutput VMs, keyed by ItemId. */
	UPROPERTY(Transient)
	TMap<FArcItemId, TObjectPtr<UArcItemViewModel>> StationOutputById;

	/**
	 * Returns true when the shared prerequisites for server commands are met:
	 * WorldContextObject is non-null, BoundStation is valid, and InstigatorEntity is valid.
	 * Recipe- or item-specific guards are checked separately per function.
	 */
	bool HasValidCommandContext(const UObject* WorldContextObject) const;
};
