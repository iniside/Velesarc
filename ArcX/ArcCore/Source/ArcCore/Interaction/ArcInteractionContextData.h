// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "InteractionTypes.h"
#include "ArcInteractionContextData.generated.h"

class APawn;
class APlayerState;
class APlayerController;

/**
 * Extended interaction context data that provides rich information about
 * the interaction instigator. Populated by UArcGA_Interact when starting
 * an interaction from the ability system.
 *
 * Targets can cast ContextData to this type to access pawn, controller,
 * player state, and other useful information without having to resolve
 * them from the base Instigator interface.
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcInteractionContextData : public FInteractionContextData
{
	GENERATED_BODY()

	/** The pawn performing the interaction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TWeakObjectPtr<APawn> InteractorPawn;

	/** The player controller driving the interaction (null for AI). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TWeakObjectPtr<APlayerController> PlayerController;

	/** The player state of the interacting player (null for AI). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TWeakObjectPtr<APlayerState> PlayerState;

	/** World location of the instigator at the moment the interaction was triggered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FVector InteractionLocation = FVector::ZeroVector;

	/** View direction of the instigator at the moment the interaction was triggered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FVector InteractionDirection = FVector::ForwardVector;
};
