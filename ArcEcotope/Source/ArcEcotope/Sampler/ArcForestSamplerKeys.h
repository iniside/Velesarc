// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Determinism/ArcForestDeterministic.h"

namespace UE::ArcForest::Sampler
{
	constexpr uint64 SamplerCandidatePosition = 0x1F3A62D8B49E7C05ull;
	constexpr uint64 SamplerSpeciesChoice = 0x2A4B73E9C50F8D16ull;
	constexpr uint64 SamplerHeightJitter = 0x3B5C84FAD6109E27ull;
	constexpr uint64 SamplerAcceptance = 0x4C6D950BE721AF38ull;
	constexpr uint64 SamplerPriority = 0x5D7EA61CF832B049ull;
	constexpr uint64 SamplerDiagnosticsReference = 0x6E8FB72D0943C15Aull;

	inline uint64 MakeSamplerKey(
		const FGuid& SimulationId,
		const int32 Seed,
		const uint64 Domain,
		const FGuid& SpeciesId = FGuid(),
		const int32 CellIndex = INDEX_NONE,
		const int64 FieldOrdinal = 0)
	{
		FKeyedHash64 Hash(Domain);
		Hash.AddGuid(SimulationId);
		Hash.AddInt32(Seed);
		Hash.AddGuid(SpeciesId);
		Hash.AddInt32(CellIndex);
		Hash.AddInt64(FieldOrdinal);
		return Hash.Finalize();
	}
}
