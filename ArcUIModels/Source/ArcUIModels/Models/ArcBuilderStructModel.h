// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcFrontendMVVMModelComponent.h"
#include "Mass/EntityHandle.h"
#include "GameplayTagContainer.h"

#include "ArcBuilderStructModel.generated.h"

class UArcBuilderSubsystem;
class UArcBuilderViewModel;
class UMassAgentSubsystem;
class UArcMassItemEventSubsystem;

USTRUCT(BlueprintType)
struct ARCUIMODELS_API FArcBuilderStructModel : public FArcFrontendMVVMStructModel
{
	GENERATED_BODY()

	/** Name used to register the UArcBuilderViewModel in the MVVM global collection. */
	UPROPERTY(EditAnywhere)
	FName ContextName = TEXT("Builder");

	/** Gameplay tag used to register the VM in UArcFrontendMVVMModelComponent::ViewModels. */
	UPROPERTY(EditAnywhere)
	FGameplayTag RegistrationTag;

	FArcBuilderStructModel()
	{
		bTickForLocalOnly = true;
	}

	virtual void CreateViewModels(UArcFrontendMVVMModelComponent& Owner) override;
	virtual void Initialize(UArcFrontendMVVMModelComponent& Owner) override;
	virtual void Deinitialize(UArcFrontendMVVMModelComponent& Owner) override;
	virtual void Tick(float DeltaTime, UArcFrontendMVVMModelComponent& Component) override;

private:
	// -------------------------------------------------------------------------
	// Helpers
	// -------------------------------------------------------------------------

	// Resolves the local player's builder subsystem (PlayerState -> PlayerController ->
	// LocalPlayer -> UArcBuilderSubsystem) and, on success, subscribes OnBuilderStateChanged,
	// injects the subsystem into the VM, and kicks the initial state refresh. Every hop is
	// null-guarded: GetPlayerController() is nullptr for every remote player's PlayerState on
	// a client (bail silently) and can be nullptr for the local player early in join — the
	// Tick retry covers that. Idempotent once bound.
	bool TryBindToSubsystem(UArcFrontendMVVMModelComponent& Owner);

	// Resolves the local player's Mass entity (PlayerState -> Pawn -> MassAgentComponent)
	// and, on success, commits PlayerEntity, subscribes the player item events, and kicks
	// the initial affordability refresh. Returns false when the entity isn't associated
	// yet — the MassAgentSubsystem entity-associated delegate subscribed in Initialize
	// retries then. Idempotent once bound.
	bool TryBindToPlayerEntity(UArcFrontendMVVMModelComponent& Owner);

	// Self-discriminating handler for the subsystem's coarse OnBuilderStateChanged rail —
	// it also fires on requirement flips every ghost-drag update, so the buildings list is
	// rebuilt only on the unregistered->registered tool transition; every broadcast still
	// pushes the cheap state bools (UE_MVVM_SET_PROPERTY_VALUE no-ops when unchanged).
	void OnBuilderStateChanged();

	// Pulls the subsystem's cached GetAvailableBuildings and rebuilds the VM list, recycling
	// existing building VMs by primary asset id (pointer-identity fallback for definitions
	// whose id is invalid — unset BuildingId collapses keys).
	void RebuildBuildingsList();

	// Re-reads the player's item store and re-runs RefreshFrom on the EXISTING building
	// VMs only (no list rebuild) — the player item-event rail.
	void RefreshAffordability();

	// -------------------------------------------------------------------------
	// Cached state
	// -------------------------------------------------------------------------

	TWeakObjectPtr<UArcBuilderViewModel>           BuilderVM;
	TWeakObjectPtr<UArcFrontendMVVMModelComponent> OwnerComp;
	FMassEntityHandle                              PlayerEntity;

	// Builder-subsystem subscription — bound once in TryBindToSubsystem, torn down in Deinitialize.
	TWeakObjectPtr<UArcBuilderSubsystem> BoundBuilderSubsystem;
	FDelegateHandle                      BuilderStateChangedHandle;

	// Last observed tool-registration flag — discriminates registration transitions on the coarse delegate.
	bool bLastToolRegistered = false;

	// Entity-association late-bind — the pawn's MassAgentComponent frequently hasn't
	// associated its entity yet when Initialize fires (same race the crafting model handles).
	TWeakObjectPtr<UMassAgentSubsystem> BoundAgentSubsystem;
	FDelegateHandle                     EntityAssociatedHandle;

	// Player item-event subscriptions — bound once in TryBindToPlayerEntity, torn down in
	// Deinitialize. Re-run RefreshAffordability() so bCanAfford tracks live inventory.
	TWeakObjectPtr<UArcMassItemEventSubsystem> BoundItemEventSubsystem;
	FDelegateHandle                            ItemAddedHandle;
	FDelegateHandle                            ItemRemovedHandle;
	FDelegateHandle                            ItemStacksChangedHandle;
};
