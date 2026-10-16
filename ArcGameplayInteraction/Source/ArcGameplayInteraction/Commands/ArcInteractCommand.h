// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ArcReplicatedCommand.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "UObject/Object.h"

#include "ArcInteractCommand.generated.h"

class APawn;

/**
 * Replacement for the retired UArcGA_Interact ability. Sent from the local
 * player to trigger an interaction against either an actor IInteractionTarget
 * or a Mass entity. The pawn doing the interaction and the chosen target are
 * both captured client-side at trigger time so the command is fully
 * self-contained.
 */
USTRUCT(BlueprintType)
struct ARCGAMEPLAYINTERACTION_API FArcInteractCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

	/** Pawn performing the interaction (captured client-side at trigger time). */
	UPROPERTY()
	TObjectPtr<APawn> InteractorPawn = nullptr;

	/** Actor target chosen by the client; expected to implement IInteractionTarget. */
	UPROPERTY()
	TObjectPtr<UObject> TargetObject = nullptr;

	/** Entity target chosen by the client when no actor target is present. */
	UPROPERTY()
	FMassEntityHandle EntityTarget;

	/** Eye-view origin at trigger time (client-side). */
	UPROPERTY()
	FVector InteractionLocation = FVector::ZeroVector;

	/** Eye-view direction at trigger time (client-side). */
	UPROPERTY()
	FVector InteractionDirection = FVector::ForwardVector;

	/** Tag identifying which interaction option the player chose; empty = legacy single-option path (use slot default). */
	UPROPERTY()
	FGameplayTag OptionTag;

	/** Slot index to claim; INDEX_NONE = claim slot 0 (legacy default). */
	UPROPERTY()
	int32 SlotIndex = INDEX_NONE;

	FArcInteractCommand() = default;

	FArcInteractCommand(APawn* InInteractorPawn
		, UObject* InTargetObject
		, FMassEntityHandle InEntityTarget
		, const FVector& InInteractionLocation
		, const FVector& InInteractionDirection
		, const FGameplayTag& InOptionTag
		, int32 InSlotIndex)
		: InteractorPawn(InInteractorPawn)
		, TargetObject(InTargetObject)
		, EntityTarget(InEntityTarget)
		, InteractionLocation(InInteractionLocation)
		, InteractionDirection(InInteractionDirection)
		, OptionTag(InOptionTag)
		, SlotIndex(InSlotIndex)
	{
	}

	virtual bool CanSendCommand() const override;
	virtual bool Execute(UObject* WorldContext) override;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcInteractCommand::StaticStruct();
	}

	virtual ~FArcInteractCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcInteractCommand>
	: public TStructOpsTypeTraitsBase2<FArcInteractCommand>
{
	enum
	{
		WithCopy = true
	};
};
