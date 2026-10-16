// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "Sim/ArcSettlementBounds.h"

struct FMassEntityManager;
struct FSmartObjectRequestResult;
class USmartObjectSubsystem;

namespace ArcWorldSim
{
struct FServiceRecord
{
	FMassEntityHandle Entity;
	FVector Location = FVector::ZeroVector;
	FGameplayTagContainer SlotTags;
};

bool FindServicePlace(
	FMassEntityManager& EntityManager,
	USmartObjectSubsystem& SmartObjectSubsystem,
	FMassEntityHandle PreferredService,
	FMassEntityHandle Settlement,
	const FVector& FromLocation,
	FGameplayTag RequiredSlotTag,
	TConstArrayView<FServiceRecord> Services,
	TConstArrayView<FSettlementBound> Centers,
	TFunctionRef<bool(FMassEntityHandle)> IsEligible,
	FMassEntityHandle& OutService,
	FSmartObjectRequestResult& OutRequestResult);
}
