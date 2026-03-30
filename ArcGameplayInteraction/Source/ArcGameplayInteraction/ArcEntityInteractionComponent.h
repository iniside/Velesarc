// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Player/ArcCorePlayerState.h"
#include "ArcGameplayInteractionContext.h"
#include "MassEntityTypes.h"
#include "ArcEntityInteractionComponent.generated.h"

struct FArcInteractionContextData;

/**
 * Player-side component that executes SmartObject interactions for Mass entity
 * targets that have no actor. Mirrors the execution flow of
 * UArcSmartObjectInteractionComponent but driven from the source (player) side.
 */
UCLASS(BlueprintType, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARCGAMEPLAYINTERACTION_API UArcEntityInteractionComponent : public UArcCorePlayerStateComponent
{
	GENERATED_BODY()

public:
	UArcEntityInteractionComponent(const FObjectInitializer& ObjectInitializer);

	/**
	 * Execute a SmartObject interaction for the given entity.
	 * Claims a slot, activates the behavior StateTree, and ticks it until completion.
	 *
	 * @param Entity The Mass entity to interact with.
	 * @param ContextData Rich interaction context from the ability.
	 * @return true if the interaction was started successfully.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool ExecuteEntityInteraction(FMassEntityHandle Entity, const FArcInteractionContextData& ContextData);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsInteracting() const;

protected:
	UPROPERTY()
	FArcGameplayInteractionContext GameplayInteractionContext;
};
