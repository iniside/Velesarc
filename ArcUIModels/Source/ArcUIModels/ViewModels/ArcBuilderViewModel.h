// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ViewModels/ArcBuildingViewModel.h"

#include "ArcBuilderViewModel.generated.h"

class UArcBuilderSubsystem;

DECLARE_MULTICAST_DELEGATE_OneParam(
	FArcOnSelectedBuildingChanged,
	UArcBuildingViewModel* /*NewBuilding*/);

UCLASS(BlueprintType)
class ARCUIMODELS_API UArcBuilderViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Stable per-building VMs. Recycled by primary asset id by the struct model. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TArray<TObjectPtr<UArcBuildingViewModel>> Buildings;

	/** True while a builder tool is registered. (Name retained for widget-binding stability.) */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bSessionActive = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bIsPlacing = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bPlacementRequirementsMet = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TObjectPtr<UArcBuildingViewModel> SelectedBuilding = nullptr;

	const TArray<TObjectPtr<UArcBuildingViewModel>>& GetBuildings() const { return Buildings; }
	bool GetbSessionActive() const { return bSessionActive; }
	bool GetbIsPlacing() const { return bIsPlacing; }
	bool GetbPlacementRequirementsMet() const { return bPlacementRequirementsMet; }
	UArcBuildingViewModel* GetSelectedBuilding() const { return SelectedBuilding; }

	// -------------------------------------------------------------------------
	// Delegate fired whenever SelectedBuilding changes to a new value — the struct
	// model uses it to react to selection without polling.
	// -------------------------------------------------------------------------

	FArcOnSelectedBuildingChanged OnSelectedBuildingChangedDelegate;

	// -------------------------------------------------------------------------
	// Mutators called by StructModel
	// -------------------------------------------------------------------------

	/** Injects the resolved per-local-player builder subsystem (stored weakly). */
	void SetBuilderSubsystem(UArcBuilderSubsystem* InSubsystem);

	/** Pushes the coarse tool-registration/placement bools from the subsystem's state delegate. */
	void RefreshState(bool bInSessionActive, bool bInIsPlacing, bool bInRequirementsMet);

	/** Commits a rebuilt/recycled building list atomically. */
	void SetBuildings(TArray<TObjectPtr<UArcBuildingViewModel>>&& NewBuildings);

	// -------------------------------------------------------------------------
	// Player actions
	// -------------------------------------------------------------------------

	/**
	 * Records which building the player has selected in the UI (local state only —
	 * no subsystem call). Updates per-entry bIsSelected and fires
	 * OnSelectedBuildingChangedDelegate. Pass nullptr to clear the selection.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Builder")
	void SelectBuilding(UArcBuildingViewModel* Building);

	/**
	 * Starts placing the selected building via the injected subsystem.
	 * Returns false when the subsystem is gone or no building is selected.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Builder")
	bool StartPlacement();

	/** Sends the server place command for the current ghost. Returns false when the subsystem is gone. */
	UFUNCTION(BlueprintCallable, Category = "Arc Builder")
	bool ConfirmPlacement();

	/** Cancels the active placement ghost. Returns false when the subsystem is gone. */
	UFUNCTION(BlueprintCallable, Category = "Arc Builder")
	bool CancelPlacement();

private:
	TWeakObjectPtr<UArcBuilderSubsystem> BuilderSubsystem;
};
