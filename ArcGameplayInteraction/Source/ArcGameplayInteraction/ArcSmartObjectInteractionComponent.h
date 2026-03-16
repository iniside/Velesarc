// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcGameplayInteractionContext.h"
#include "InteractableTargetInterface.h"
#include "InteractionTypes.h"
#include "Components/ActorComponent.h"
#include "ArcSmartObjectInteractionComponent.generated.h"

struct FInteractionContext;

/**
 * Component that implements IInteractionTarget and executes a smart object
 * interaction on behalf of its owning actor.
 *
 * The targeting system discovers this component by searching the actor's
 * components for IInteractionTarget implementors. When BeginInteraction is
 * called, it resolves the smart object handle from the actor's underlying
 * Mass entity (via UInstancedActorsComponent or UMassAgentComponent),
 * claims a slot, activates the behavior StateTree, and ticks it until
 * completion.
 */
UCLASS(BlueprintType, ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class ARCGAMEPLAYINTERACTION_API UArcSmartObjectInteractionComponent : public UActorComponent, public IInteractionTarget
{
	GENERATED_BODY()

public:
	UArcSmartObjectInteractionComponent(const FObjectInitializer& ObjectInitializer);

	//~ IInteractionTarget interface
	virtual void AppendTargetConfiguration(const FInteractionContext& Context, FInteractionQueryResults& OutResults) const override;
	virtual void BeginInteraction(const FInteractionContext& Context) override;
	//~ End IInteractionTarget interface

	/**
	 * Start a smart object interaction on this actor's smart object for the given pawn.
	 * Resolves the smart object handle from the underlying Mass entity.
	 *
	 * @param Interactor The pawn performing the interaction.
	 * @return true if the interaction was started successfully.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool StartInteraction(APawn* Interactor);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsInteracting() const;

protected:
	/** Interaction configurations to advertise when queried by the targeting system. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (BaseStruct = "/Script/InteractableInterface.InteractionTargetConfiguration"))
	TArray<FInstancedStruct> TargetConfigs;

	UPROPERTY()
	FArcGameplayInteractionContext GameplayInteractionContext;
};
