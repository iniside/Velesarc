// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Player/ArcCorePlayerState.h"
#include "Types/TargetingSystemTypes.h"
#include "MassEntityTypes.h"

#include "InteractableTargetInterface.h"
#include "InteractionTypes.h"
#include "MassEntityHandle.h"

#include "ArcCoreInteractionSourceComponent.generated.h"

class UTargetingPreset;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARCCORE_API UArcCoreInteractionSourceComponent : public UArcCorePlayerStateComponent
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Config")
	TObjectPtr<UTargetingPreset> InteractionTargetingPreset;

	FTargetingRequestHandle TargetingRequestHandle;

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

	/** Full targeting results from the last completed targeting request. */
	FTargetingDefaultResultsSet CachedTargetingResults;

public:
	UArcCoreInteractionSourceComponent(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Interactions")
	TScriptInterface<IInteractionTarget> GetCurrentTarget() const { return CurrentTarget; }

	UFUNCTION(BlueprintPure, Category = "Interactions")
	FMassEntityHandle GetCurrentEntityTarget() const { return CurrentEntityTarget; }

	UFUNCTION(BlueprintPure, Category = "Interactions")
	const FInteractionQueryResults& GetCurrentQueryResults() const { return CurrentQueryResults; }

	const FTargetingDefaultResultsSet& GetCachedTargetingResults() const { return CachedTargetingResults; }

protected:
	virtual void BeginPlay() override;
	virtual void OnPawnDataReady(APawn* Pawn) override;

	void HandleTargetingCompleted(FTargetingRequestHandle InTargetingRequestHandle);

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
};
