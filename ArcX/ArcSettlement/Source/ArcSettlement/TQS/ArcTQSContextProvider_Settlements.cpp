// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSContextProvider_Settlements.h"
#include "ArcSettlementSubsystem.h"
#include "ArcSettlementData.h"
#include "Engine/World.h"

void FArcTQSContextProvider_Settlements::GenerateContextLocations(
	const FArcTQSQueryContext& QueryContext,
	TArray<FVector>& OutLocations) const
{
	UWorld* World = QueryContext.World.Get();
	if (!World)
	{
		return;
	}

	const UArcSettlementSubsystem* Subsystem = World->GetSubsystem<UArcSettlementSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	// Gather candidate settlements
	TArray<FArcSettlementHandle> CandidateHandles;

	if (SearchRadius > 0.0f)
	{
		Subsystem->QuerySettlementsInRadius(QueryContext.QuerierLocation, SearchRadius, CandidateHandles);
	}
	else
	{
		// All settlements
		const auto& AllSettlements = Subsystem->GetAllSettlements();
		AllSettlements.GetKeys(CandidateHandles);
	}

	// Filter by tags and collect locations
	struct FSettlementCandidate
	{
		FVector Location;
		float DistanceSq;
	};

	TArray<FSettlementCandidate> Candidates;
	Candidates.Reserve(CandidateHandles.Num());

	const bool bHasTagFilter = !SettlementTagFilter.IsEmpty();

	for (const FArcSettlementHandle& Handle : CandidateHandles)
	{
		FArcSettlementData Data;
		if (!Subsystem->GetSettlementData(Handle, Data))
		{
			continue;
		}

		// Apply tag filter
		if (bHasTagFilter && !SettlementTagFilter.Matches(Data.Tags))
		{
			continue;
		}

		FSettlementCandidate Candidate;
		Candidate.Location = Data.Location;
		Candidate.DistanceSq = FVector::DistSquared(QueryContext.QuerierLocation, Data.Location);
		Candidates.Add(MoveTemp(Candidate));
	}

	// Sort by distance (closest first)
	Candidates.Sort([](const FSettlementCandidate& A, const FSettlementCandidate& B)
	{
		return A.DistanceSq < B.DistanceSq;
	});

	// Take up to MaxLocations
	const int32 Count = FMath::Min(Candidates.Num(), MaxLocations);
	OutLocations.Reserve(OutLocations.Num() + Count);

	for (int32 i = 0; i < Count; ++i)
	{
		OutLocations.Add(Candidates[i].Location);
	}
}
