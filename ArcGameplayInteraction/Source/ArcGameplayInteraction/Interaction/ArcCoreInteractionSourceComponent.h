// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Components/PlayerStateComponent.h"
#include "ArcTargetingPipelineTypes.h"

#include "InteractableTargetInterface.h"
#include "InteractionTypes.h"
#include "Mass/EntityHandle.h"

#include "ArcCoreInteractionSourceComponent.generated.h"

class UArcMassPlayerStateExtensionComponent;
class UArcTargetingPreset;
class UEnhancedInputComponent;
class UInputMappingContext;
struct FInputActionInstance;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARCGAMEPLAYINTERACTION_API UArcCoreInteractionSourceComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Config")
	TObjectPtr<UArcTargetingPreset> InteractionTargetingPreset;

	FArcTargetingHandle TargetingRequestHandle;

	/** The single best interaction target from the targeting pipeline. */
	UPROPERTY(BlueprintReadOnly, Category = "Interactions")
	TScriptInterface<IInteractionTarget> CurrentTarget;

	/** The current entity target when no IInteractionTarget actor is found. Mutually exclusive with CurrentTarget. */
	UPROPERTY(BlueprintReadOnly, Category = "Interactions")
	FMassEntityHandle CurrentEntityTarget;

	/** Query results (available configurations) from the current best target. */
	UPROPERTY(BlueprintReadOnly, Category = "Interactions")
	FInteractionQueryResults CurrentQueryResults;

	/** Query results from the previous target, preserved for UI transitions. */
	UPROPERTY(BlueprintReadOnly, Category = "Interactions")
	FInteractionQueryResults PreviousQueryResults;

	/** Full targeting candidates from the last completed targeting request. */
	TArray<FArcTargetCandidate> CachedCandidates;

public:
	UArcCoreInteractionSourceComponent(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Interactions")
	TScriptInterface<IInteractionTarget> GetCurrentTarget() const { return CurrentTarget; }

	UFUNCTION(BlueprintPure, Category = "Interactions")
	FMassEntityHandle GetCurrentEntityTarget() const { return CurrentEntityTarget; }

	UFUNCTION(BlueprintPure, Category = "Interactions")
	const FInteractionQueryResults& GetCurrentQueryResults() const { return CurrentQueryResults; }

	const TArray<FArcTargetCandidate>& GetCachedCandidates() const { return CachedCandidates; }

	/** Local-side notification that an interaction trigger was just dispatched.
	 *  Fires the native virtual and the BlueprintImplementableEvent.
	 *  Sent before the server has confirmed — for UI/audio cues only. */
	void NotifyTriggerInteraction();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	void OnPlayerStateReady(UArcMassPlayerStateExtensionComponent* PSExt);
	void StartTargetingLoop(APawn* Pawn);

	FDelegateHandle InitStateHandle;

	void HandleTargetingCompleted(FArcTargetingHandle InHandle);

	UFUNCTION(BlueprintImplementableEvent, Category="Interactions")
	void OnInteractionChanged();
	virtual void NativeOnInteractionChanged() {}

	/**
	 * Triggers the interaction with the current best target.
	 * Override this in blueprints or native C++ to decide how to interact.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="Interactions")
	void OnTriggerInteraction();
	virtual void NativeOnTriggerInteraction() {}

private:
	/** IMC currently pushed for the active interaction options (null when none is active). */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> ActiveMappingContext = nullptr;

	/** Input component the per-option actions are bound on (the current pawn's). */
	TWeakObjectPtr<UEnhancedInputComponent> BoundInputComponent;

	/** Handles of the per-option action bindings, kept for teardown. */
	TArray<uint32> InteractionBindingHandles;

	/** Pushes the target's IMC and binds each acquired option's input action. No-op when MappingContext is null. */
	void SetupInteractionInput(UInputMappingContext* MappingContext);

	/** Removes the pushed IMC and all per-option bindings. Safe to call when nothing is active. */
	void TeardownInteractionInput();

	/** Bound per-option handler — sends FArcInteractCommand for the pressed option. */
	void HandleInteractionOptionTriggered(const FInputActionInstance& Instance, FGameplayTag OptionTag, int32 SlotIndex);
};
