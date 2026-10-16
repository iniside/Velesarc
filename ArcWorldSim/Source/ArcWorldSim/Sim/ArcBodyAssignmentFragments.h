// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "Mass/EntityHandle.h"
#include "UObject/PrimaryAssetId.h"

#include "ArcBodyAssignmentFragments.generated.h"

USTRUCT()
struct ARCWORLDSIM_API FArcSettlementMembershipFragment : public FMassFragment
{
	GENERATED_BODY()

	FMassEntityHandle Settlement;
};

USTRUCT()
struct ARCWORLDSIM_API FArcWorkAssignmentFragment : public FMassFragment
{
	GENERATED_BODY()

	FMassEntityHandle Workplace;
};

UENUM()
enum class EArcGatherAssignmentPhase : uint8
{
	Gather,
	Drop
};

USTRUCT()
struct ARCWORLDSIM_API FArcGatherAssignmentFragment : public FMassFragment
{
	GENERATED_BODY()

	FMassEntityHandle Workplace;
	FMassEntityHandle GatherArea;
	FPrimaryAssetId Item;
	int32 Quantity = 0;
	uint32 ActiveCommandSerial = 0;
	EArcGatherAssignmentPhase Phase = EArcGatherAssignmentPhase::Gather;
};

USTRUCT()
struct ARCWORLDSIM_API FArcCarrierAssignmentFragment : public FMassFragment
{
	GENERATED_BODY()

	bool bAssigned = false;
};

USTRUCT()
struct ARCWORLDSIM_API FArcCarryCapabilityFragment : public FMassFragment
{
	GENERATED_BODY()

	int32 Capacity = 10;
};
