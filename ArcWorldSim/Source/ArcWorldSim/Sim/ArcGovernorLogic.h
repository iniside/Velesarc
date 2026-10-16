// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"

struct FMassEntityManager;

namespace ArcWorldSim
{
struct FStaffableBuilding
{
	FMassEntityHandle Entity;
	FVector Location = FVector::ZeroVector;
	bool bGathering = false;
	int32 JobSlots = 1;
	int32 Assigned = 0;
};

void AssignWorkforce(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Center,
	TArray<FMassEntityHandle>& Unassigned,
	TArray<FStaffableBuilding>& StaffableForThisCenter,
	int32 ChainCountForThisCenter,
	int32& CarrierCount);
}
