// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcServiceLogic.h"

#include "Sim/ArcBodySlotSelection.h"
#include "SmartObjectRequestTypes.h"

namespace ArcServiceLogicPrivate
{
bool IsInSettlement(
	const ArcWorldSim::FServiceRecord& Service,
	const FMassEntityHandle Settlement,
	TConstArrayView<ArcWorldSim::FSettlementBound> Centers)
{
	return Centers.IsEmpty() || ArcWorldSim::FindSettlementAt(Service.Location, Centers) == Settlement;
}
}

namespace ArcWorldSim
{
bool FindServicePlace(
	FMassEntityManager& EntityManager,
	USmartObjectSubsystem& SmartObjectSubsystem,
	const FMassEntityHandle PreferredService,
	const FMassEntityHandle Settlement,
	const FVector& FromLocation,
	const FGameplayTag RequiredSlotTag,
	TConstArrayView<FServiceRecord> Services,
	TConstArrayView<FSettlementBound> Centers,
	TFunctionRef<bool(FMassEntityHandle)> IsEligible,
	FMassEntityHandle& OutService,
	FSmartObjectRequestResult& OutRequestResult)
{
	OutService = FMassEntityHandle();
	OutRequestResult = FSmartObjectRequestResult(FSmartObjectHandle());
	const FMassEntityHandle ResolvedSettlement = Settlement.IsValid()
		? Settlement
		: FindSettlementAt(FromLocation, Centers);

	float BestDistSq = 0.0f;
	bool bFound = false;
	for (const FServiceRecord& Service : Services)
	{
		if (!Service.SlotTags.HasTag(RequiredSlotTag)
			|| !ArcServiceLogicPrivate::IsInSettlement(Service, ResolvedSettlement, Centers)
			|| !IsEligible(Service.Entity))
		{
			continue;
		}

		FSmartObjectRequestResult CandidateRequest{FSmartObjectHandle()};
		if (!FindAvailableSlot(
			EntityManager,
			SmartObjectSubsystem,
			Service.Entity,
			RequiredSlotTag,
			CandidateRequest))
		{
			continue;
		}

		if (Service.Entity == PreferredService)
		{
			OutService = Service.Entity;
			OutRequestResult = CandidateRequest;
			return true;
		}

		const float DistSq = FVector::DistSquared(FromLocation, Service.Location);
		if (!bFound || DistSq < BestDistSq)
		{
			bFound = true;
			BestDistSq = DistSq;
			OutService = Service.Entity;
			OutRequestResult = CandidateRequest;
		}
	}

	return bFound;
}
}
