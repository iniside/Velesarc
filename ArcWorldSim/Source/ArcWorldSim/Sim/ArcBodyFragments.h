// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "Mass/EntityHandle.h"
#include "SmartObjectRuntime.h"
#include "SmartObjectTypes.h"

#include "ArcBodyFragments.generated.h"

UENUM()
enum class EArcBodyState : uint8
{
	Idle,
	MoveTo,
	Interaction
};

UENUM()
enum class EArcBodyCommandResult : uint8
{
	None,
	Succeeded,
	Failed,
	Cancelled,
	Aborted
};

USTRUCT()
struct ARCWORLDSIM_API FArcBodyCommand
{
	GENERATED_BODY()

	FSmartObjectSlotHandle TargetSlot;
	FSmartObjectClaimHandle ClaimHandle;
	FMassEntityHandle TargetEntity;
	FVector TargetLocation = FVector::ZeroVector;
	uint32 Serial = 0;
	EArcBodyCommandResult BehaviorResult = EArcBodyCommandResult::Succeeded;
	bool bInterruptible = true;
	bool bStartedUsing = false;

	bool IsActive() const
	{
		return Serial != 0 && ClaimHandle.IsValid() && TargetSlot.IsValid() && TargetEntity.IsValid();
	}

	void Reset()
	{
		*this = FArcBodyCommand();
	}
};

USTRUCT()
struct ARCWORLDSIM_API FArcBodyFragment : public FMassFragment
{
	GENERATED_BODY()

	EArcBodyState State = EArcBodyState::Idle;
	FArcBodyCommand Command;
	uint32 LastCommandSerial = 0;
	EArcBodyCommandResult LastCommandResult = EArcBodyCommandResult::None;
};
