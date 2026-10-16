// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FArcEcotopeBudgetSettings;
struct FArcForestDisturbanceSettings;
struct FArcForestFrozenSpeciesSetup;
struct FArcEcotopeDeadwoodSettings;

namespace UE::ArcForest
{
	struct ARCECOTOPE_API FSettingsHashSpecies
	{
		FGuid SpeciesId;
		float RegionalArrivalPerHectarePerYear = 0.0f;
	};

	/**
	 * Bumped by any commit that changes the SHAPE of a simulated formula, never by a parameter
	 * change. ComputeSettingsHash and ComputeFrozenSpeciesEcologyHash cover parameters only, so a
	 * form change with no parameter change would otherwise let a stale snapshot validate clean
	 * and resume simulating under different physics.
	 */
	constexpr uint32 SimulationRuleVersion = 43;

	/** Stable process domains keep unrelated deterministic decisions independent. */
	enum class EDeterministicDomain : uint64
	{
		SimulationIdentity = 0x1A8C3F49D72E650Bull,
		BootstrapTotalRounding = 0x31F7C95A0D42E68Bull,
		BootstrapCellSelection = 0x4E2B91D6A37C805Full,
		BootstrapAdmission = 0x63D8A10F5C29E74Bull,
		BootstrapRecordIdentity = 0x7B40E2C9165DA83Full,
		GrowthMortality = 0x82D157AC3E609B4Full,
		GrowthDeadwoodType = 0x9AF42C7165D803EBull,
		DeadwoodRecordIdentity = 0xE84A73B2D5C1069Full,
		DeadwoodSnagFall = 0xF72DB0E4A1936C58ull,
		DeadwoodFallBearing = 0xA1528FEC369B70D4ull,
		BranchShedOccurrence = 0xB2639F0D47AC81E5ull,
		BranchShedPlacement = 0xC374A01E58BD92F6ull,
		LocalReproductionRounding = 0xA16C5E82D9473BF1ull,
		LocalDispersalDistance = 0xB2F7049A6C18E35Dull,
		LocalDispersalAngle = 0xC38D1AB75E9264F0ull,
		LocalEstablishment = 0x0E5B74D296A83FC1ull,
		RegionalArrivalRounding = 0xD49E27C8613AF50Bull,
		RegionalEnvelopeRecordSelection = 0x71C84F2A95B306DEull,
		RegionalEnvelopeEntrySelection = 0x8AD3106BE74C529Full,
		RegionalEnvelopeAcceptance = 0x9BE4217CF85D63A0ull,
		RecruitmentAttemptAllocation = 0xF61C49EA725D83B4ull,
		RecruitmentAdmission = 0x174D5AFB836E94C1ull,
		RecruitmentRecordIdentity = 0x286E6B0C947FA5D2ull,
		LateralSpreadRecordIdentity = 0x3D5A82F1C64B90E7ull,
		LateralSpreadEstablishment = 0x5EB1C3A47F208D96ull,
		PromotionSelection = 0x397F7C1DA590B6E3ull,
		PromotionRecordIdentity = 0x4A808D2EB6A1C7F4ull,
		PromotionPositionX = 0x5B919E3FC7B2D805ull,
		PromotionPositionY = 0x6CA2AF40D8C3E916ull,
		InstanceYaw = 0x84E5CA362F9D0718ull,
		InstanceRepresentativePositionX = 0x95F6DB4730AE1829ull,
		InstanceRepresentativePositionY = 0xA607EC5841BF293Aull,
		PromotionPositionCell = 0xB718FD6952C03A4Bull,
		InstanceRepresentativeCell = 0xC8290E7A63D14B5Cull,
		InstanceMeshVariant = 0xD93A1F8B74E205C6ull,
		InstanceTilt = 0xEA4B208C85F316D7ull,
		InstanceScale = 0xFB5C319D96047E28ull,
		CoverShareNoise = 0x2E9F6B18C7A340D5ull,
		PatchClusterAssignment = 0x1C7E58A0B3D9642Full,
		PatchClusterCentreX = 0x2D8F69B1C4EA7530ull,
		PatchClusterCentreY = 0x3E907AC2D5FB8641ull,
		PatchClusterOffset = 0x4FA18BD3E60C9752ull,
		PatchClusterSiteActivation = 0x6E5A9C24B17F80D3ull,
		DisturbanceOccurrence = 0x5C0D3A97E1462B8Full,
		DisturbanceCentre = 0x6D1E4BA8F2573C90ull,
		DisturbanceRadius = 0x7E2F5CB903684DA1ull,
		DisturbanceWindBearing = 0x90417EDB258A6FC3ull,
		DisturbanceKill = 0x8F306DCA14795EB2ull,
	};

	/**
	 * Field-wise 64-bit hash builder. Values are serialized explicitly rather
	 * than through object memory, so padding and container layout cannot affect
	 * deterministic simulation decisions.
	 */
	class ARCECOTOPE_API FKeyedHash64
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

	/** Constructs a keyed decision from the standard ArcForest identity fields. */
	ARCECOTOPE_API uint64 MakeKey(
		const FGuid& SimulationId,
		int32 Seed,
		EDeterministicDomain Domain,
		const FGuid& SpeciesId = FGuid(),
		int32 CellIndex = INDEX_NONE,
		int64 FieldOrdinal = 0);

	/**
	 * Constructs an annual keyed decision. QuantizedBranchInput is the
	 * integerized value used by the branch so floating-point noise cannot
	 * silently select another random stream.
	 */
	ARCECOTOPE_API uint64 MakeAnnualKey(
		const FGuid& SimulationId,
		int32 Seed,
		int32 Year,
		EDeterministicDomain Domain,
		const FGuid& SpeciesId,
		int32 CellIndex,
		uint64 EntityId,
		int32 DecisionOrdinal,
		int64 QuantizedBranchInput);

	/** Maps a key to the open interval (0, 1) using its most significant 53 bits. */
	ARCECOTOPE_API double KeyToUnitOpen(uint64 Key);

	/** Deterministic stochastic rounding for a finite non-negative value. */
	ARCECOTOPE_API bool TryRoundNonNegative(double Value, uint64 Key, int64& OutRounded);

	/** Stable identity for one volume/seed simulation lineage. */
	ARCECOTOPE_API FGuid MakeSimulationId(const FGuid& VolumeId, int32 Seed);

	/**
	 * Canonical settings hash shared by the game-thread freeze boundary and
	 * worker validation. Species entries are sorted internally.
	 */
	ARCECOTOPE_API uint64 ComputeSettingsHash(
		int32 Seed,
		float CellSizeCm,
		uint8 RecordGridLevel,
		const FArcEcotopeBudgetSettings& BudgetSettings,
		const FArcEcotopeDeadwoodSettings& DeadwoodSettings,
		double MaximumCrownOccupancy,
		double UnderstoryShade,
		double GroundCoverDensity,
		double GroundCoverPatchiness,
		TConstArrayView<FSettingsHashSpecies> Species,
		const FArcForestDisturbanceSettings& DisturbanceSettings);

	/** Recomputes the ecology hash solely from the frozen biological fields. */
	ARCECOTOPE_API uint64 ComputeFrozenSpeciesEcologyHash(const FArcForestFrozenSpeciesSetup& Species);
}
