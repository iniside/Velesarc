// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcFrontendMVVMModelComponent.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "QuickBar/ArcMassQuickBarOperations.h"

#include "ArcQuickBarStructModel.generated.h"

class UArcQuickBarViewModel;
class UArcQuickBarSlotViewModel;
class UArcMassItemEventSubsystem;
class UArcMassQuickBarConfig;
class UArcTagDisplayRegistry;
class UMassAgentComponent;
struct FArcMassQuickBarEntry;
struct FArcMassQuickBarSharedFragment;
struct FMassEntityManager;

// One authored bar binding: selects which bar of the entity's quick-bar config to mirror.
USTRUCT(BlueprintType)
struct ARCUIMODELS_API FArcQuickBarBinding
{
	GENERATED_BODY()

	// Which bar (FArcMassQuickBarEntry::BarId) to mirror. Also the tag the bar VM
	// is registered under in the component's ViewModels map (resolvers key on it).
	UPROPERTY(EditAnywhere, meta = (Categories = "QuickBar,BarId"))
	FGameplayTag BarId;

	// Optional global-collection context ({Class, ContextName} must be unique per VM;
	// NAME_None skips global registration — the tag map is the primary lookup path).
	UPROPERTY(EditAnywhere)
	FName ContextName;
};

// Per-bar runtime state, one entry per successfully-resolved binding. Not reflected —
// holds the registration key pair and a live weak ref to the built bar VM.
struct FArcQuickBarBarRuntime
{
	FGameplayTag BarId;
	FName ContextName;

	// Weak ref — the strong ref lives in the component's ViewModels UPROPERTY TMap.
	TWeakObjectPtr<UArcQuickBarViewModel> BarVM;
};

/**
 * Mirrors N Mass quick bars (one per authored binding) into one UArcQuickBarViewModel each, plus one
 * UArcQuickBarSlotViewModel per authored slot, each composing an item VM + ability VM. Bars are found
 * by FArcMassQuickBarEntry::BarId across BOTH the config's ungrouped QuickBars and Groups[].Bars.
 *
 * Registration convention: each bar VM is registered in the component's ViewModels map keyed by its
 * BarId directly — slot resolvers compose (BarId, QuickSlotId) lookups on top of that single-tag map.
 * Editor-picker caveat: the component's ViewModels map and its BP API filter tag pickers to the
 * "ViewModel" categories, so QuickBar.* keys can't be hand-authored there or on
 * UArcFrontendMVVMComponentResolver::ViewModelTag — BarId keys are reachable via this C++
 * registration path and interface-driven resolvers only.
 *
 * All VM creation happens AFTER the entity is resolved (CreateViewModels runs before the entity
 * exists and before the slot schema — which lives on the entity's shared fragment — is reachable).
 * Initialize attempts an immediate bind and also subscribes to UMassAgentSubsystem entity-association
 * to late-bind once the local player's pawn's MassAgentComponent has associated its entity.
 */
USTRUCT(BlueprintType)
struct ARCUIMODELS_API FArcQuickBarStructModel : public FArcFrontendMVVMStructModel
{
	GENERATED_BODY()

	FArcQuickBarStructModel();

	// The set of quick bars this single model drives. Each binding builds one
	// UArcQuickBarViewModel registered under its BarId (+ optional ContextName globally).
	UPROPERTY(EditAnywhere)
	TArray<FArcQuickBarBinding> BarBindings;

	/** Tag-display registry used to resolve the item rarity label + color. */
	UPROPERTY(EditAnywhere, Category = "Display")
	TObjectPtr<UArcTagDisplayRegistry> TagDisplayRegistry;

	virtual void Initialize(UArcFrontendMVVMModelComponent& Component) override;
	virtual void Tick(float DeltaTime, UArcFrontendMVVMModelComponent& Component) override;
	virtual void Deinitialize(UArcFrontendMVVMModelComponent& Component) override;

	// Locate the bar whose BarId matches across every config of the shared fragment, searching
	// ungrouped QuickBars AND Groups[].Bars. Config/Bar are null on miss.
	static FArcMassQuickBarLocation FindBarEntry(const FArcMassQuickBarSharedFragment& Shared, FGameplayTag InBarId);

private:
	// Resolves BoundEntity, builds every resolved binding's bar + slot VMs from the entity's
	// shared-fragment config, subscribes to item-slot-changed, and does an initial refresh.
	// Called from Initialize and from the OnMassAgentComponentEntityAssociated callback.
	// Returns true if binding succeeded (BoundEntity now valid + VMs built).
	bool TryBindToEntity(UArcFrontendMVVMModelComponent& Component);

	// Re-resolve every bar's every slot's item + ability state against the current fragment data.
	void RefreshAllSlots(UArcFrontendMVVMModelComponent& Component);

	// One unified per-slot refresh path used by both Tick and the item-slot-changed event.
	void RefreshSlot(UArcQuickBarSlotViewModel* SlotVM, FMassEntityManager& Manager);

	FMassEntityHandle BoundEntity;
	TWeakObjectPtr<UArcMassItemEventSubsystem> BoundEventSubsystem;

	// One entry per successfully-resolved binding; holds the per-bar VM and registration key.
	TArray<FArcQuickBarBarRuntime> BarRuntimes;

	TArray<FDelegateHandle> SlotChangedHandles;
	FDelegateHandle EntityAssociatedHandle;
};
