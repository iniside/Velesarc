// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"

namespace ArcTownDeterministic
{
	inline constexpr uint32 SimulationRuleVersion = 20;

	/** Explicit constants: inserting a domain must never renumber the streams below it. */
	enum class EDeterministicDomain : uint64
	{
		CoreSiteChoice        = 0x9E3779B97F4A7C15ull,
		SeedRoadDirection     = 0xC2B2AE3D27D4EB4Full,
		RoadExpansionPick     = 0x165667B19E3779F9ull,
		RoadExpansionAngle    = 0x27D4EB2F165667C5ull,
		TrafficTripOrigin     = 0xD6E8FEB86659FD93ull,
		TrafficTripTarget     = 0xA0761D6478BD642Full,
		BuildingCountRoll     = 0xE7037ED1A0B428DBull,
		BuildingCandidatePick = 0x8EBC6AF09C88C6E3ull,
		BuildingTypePick      = 0x589965CC75374CC3ull,
		BuildingYawJitter     = 0x1D8E4E27C47D124Full,
		NewFrontSite          = 0x2545F4914F6CDD1Dull,
		NodeReopenProbe       = 0xBF58476D1CE4E5B9ull,
		RoadBranchSpacing     = 0x3C79AC492BA7B653ull,
		RoadBranchSide        = 0x1B03738712FAD5C9ull,
		PlotFrontageRoll      = 0xECEFE37B9E250D03ull,
		PlotDepthRoll         = 0xB5BAB1CD888417A5ull,
		RoadSegmentMeshPick   = 0x9FB21C651E98DF25ull,
	};

	class ARCTOWN_API FKeyedHash64
	{
	public:
		explicit FKeyedHash64(uint64 Key);

		void AddUInt8(uint8 Value);
		void AddUInt32(uint32 Value);
		void AddInt32(int32 Value);
		void AddUInt64(uint64 Value);
		void AddInt64(int64 Value);
		void AddGuid(const FGuid& Value);

		uint64 Finalize() const;

	private:
		uint64 Hash;
	};

	ARCTOWN_API FGuid MakeSimulationId(const FGuid& TownId, int32 Seed);

	ARCTOWN_API uint64 MakeKey(
		const FGuid& SimulationId,
		int32 Seed,
		EDeterministicDomain Domain,
		int32 EntityId,
		int32 Ordinal);

	/**
	 * QuantizedBranchInput is the integerized value the branch actually turns on, so floating-point
	 * noise cannot silently select a different stream for an identical logical state.
	 */
	ARCTOWN_API uint64 MakeAnnualKey(
		const FGuid& SimulationId,
		int32 Seed,
		EDeterministicDomain Domain,
		int32 Year,
		int32 EntityId,
		int32 Ordinal,
		int64 QuantizedBranchInput);

	ARCTOWN_API double KeyToUnitOpen(uint64 Key);

	ARCTOWN_API bool TryRoundNonNegative(double Value, uint64 Key, int32& OutRounded);
}
