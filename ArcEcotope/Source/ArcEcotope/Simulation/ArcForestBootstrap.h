// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Simulation/ArcForestDisturbanceSettings.h"
#include "Simulation/ArcEcotopeFrozenSiteSource.h"
#include "Simulation/ArcEcotopeDeadwoodSettings.h"
#include "Simulation/ArcForestWorldSnapshot.h"

#include "ArcForestBootstrap.generated.h"

class FArcForestCancellationToken;

UENUM(BlueprintType)
enum class EArcForestStartMode : uint8
{
	BareGround,
	BootstrapCohorts,
	ContinueSnapshot,
};

/**
 * Explicit orchestration contract. Bootstrap creates or copies a snapshot but
 * never advances a year; the later growth engine owns all AnnualStepsAfterInitialization.
 */
struct ARCECOTOPE_API FArcForestStartRequest
{
	EArcForestStartMode StartMode = EArcForestStartMode::BareGround;

	/**
	 * Executed later by the growth engine. Every one of these annual steps,
	 * including spin-up, performs regional arrival and increments CurrentYear.
	 * Bootstrap itself performs neither operation.
	 */
	int32 AnnualStepsAfterInitialization = 0;

	/** Contract for the editor's "Bootstrap & Spin Up" action. */
	static bool MakeBootstrapAndSpinUp(
		EArcForestStartMode InitializationMode,
		int32 SpinUpYears,
		FArcForestStartRequest& OutRequest,
		FText& OutError);

	/** Contract for continuing an exact compatible snapshot for configured years. */
	static bool MakeContinue(
		int32 YearsToSimulate,
		FArcForestStartRequest& OutRequest,
		FText& OutError);
};

/** Value-only copy of one response curve for worker-safe evaluation. */
struct ARCECOTOPE_API FArcForestFrozenResponseRange
{
	float Minimum = 0.0f;
	float Optimum = 0.5f;
	float Maximum = 1.0f;
};

/**
 * Complete value-only ecology/setup copy. Contains no UObject references.
 *
 * It carries the authored drivers AND everything they derive, because both halves go into the ecology
 * hash: a hash over the drivers alone would leave a mistake in the derivation invisible to
 * ValidateSpecies, which is the only cross-check there is.
 */
struct ARCECOTOPE_API FArcForestFrozenSpeciesSetup
{
	FGuid SpeciesId;
	uint64 EcologyHash = 0;
	/** The source asset class flattened to the one bit the engine reads: it processes records, never assets. */
	bool bIndividualizes = false;
	float InitialDensityPerHectare = 0.0f;
	float RegionalArrivalPerHectarePerYear = 0.0f;

	/** Read raw by the annual cover capacity, so the solver reapplies the derivation's clamps at the point of use. */
	int32 TargetStemsPerHectare = 120;
	float TreeSizeM = 30.0f;
	float Slenderness = 0.5f;
	float GrowthSpeed = 0.5f;
	float Toughness = 0.5f;
	float ShadeTolerance = 0.5f;
	float PreferredTemperature = 0.5f;
	float TemperatureTolerance = 1.0f;
	float PreferredHumidity = 0.5f;
	float MeanDispersalDistanceCm = 1000.0f;
	float ExclusionSensitivity = 1.0f;
	float RecruitsPerMatureStemPerYear = 1.0f;
	float MobilityMortalitySensitivity = 1.0f;
	float LateralSpreadRatePerYear = 0.05f;

	float IndividualizationHeightCm = 100.0f;
	float MaxHeightCm = 3000.0f;
	float MaxDbhCm = 100.0f;
	int32 MaxAgeYears = 300;
	int32 MaturityAgeYears = 10;
	float MaxAnnualHeightGrowthCm = 50.0f;
	float MaxAnnualDbhGrowthCm = 1.0f;
	float HeightDbhAllometryCoefficient = 0.0f;
	float HeightDbhAllometryExponent = 0.0f;
	float CrownRadiusHeightRatio = 0.2f;
	float BiomassAllometryCoefficient = 0.0f;
	float BiomassAllometryExponent = 0.0f;
	float CrownOccupancyWeight = 1.0f;
	float BaseAnnualMortalityProbability = 0.01f;
	double OldAgeMortalityProbabilityAtMaxAge = 0.25;
	double StressMortalityMaximum = 0.6;
	double StressMortalityVigorThreshold = 0.24;
	double StressMortalitySteepness = 25.0;
	double StressMortalityCanopyFloor = 0.2;
	double VigorSmoothingRetention = 0.8;
	float AnnualPropaguleProduction = 1.0f;
	FArcForestFrozenResponseRange LightResponse;
	FArcForestFrozenResponseRange MoistureResponse;
	FArcForestFrozenResponseRange TemperatureResponse;
	float MaxSlopeDeg = 90.0f;
	float SunLoadMin = 0.0f;
	float WindExposureMax = 1.0f;
	float FertilityMin = 0.0f;
};

/**
 * Complete immutable value input for bootstrap. This may be copied to a worker
 * after AArcEcotopeVolume freezes it on the game thread.
 */
struct ARCECOTOPE_API FArcForestBootstrapInput
{
	EArcForestStartMode StartMode = EArcForestStartMode::BareGround;
	FGuid VolumeId;
	int32 Seed = 0;
	int32 MaxLiveStems = 0;
	int32 MaxLiveRecords = 0;
	int32 RecruitmentOversamplingFactor = 0;
	int32 MaxCoverRecords = 0;
	int32 MaxStateRecords = 0;
	int32 MaxDeadwoodRecords = 0;
	int32 MaxBranchRecords = 0;
	bool bEmitGrowthDiagnostics = true;
	int32 GrowthDiagnosticsIntervalYears = 20;
	uint64 SettingsHash = 0;
	FArcForestRegionalRecruitmentConfig RegionalRecruitmentConfig;
	double MaximumCrownOccupancy = 1.3;
	double UnderstoryShade = 1.0;
	double GroundCoverDensity = 1.0;
	double GroundCoverPatchiness = 0.6;
	FArcEcotopeDeadwoodSettings DeadwoodSettings;
	FArcForestDisturbanceSettings DisturbanceSettings;
	/** Authoritative immutable site storage used by the worker. */
	FArcEcotopeFrozenSiteSourcePtr SiteSource;
	TArray<FArcForestFrozenSpeciesSetup> SpeciesSetups;
	FArcForestWorldSnapshot SourceSnapshot;
};

class ARCECOTOPE_API FArcForestBootstrap
{
public:
	/**
	 * Produces a complete canonical snapshot on success. It performs no UObject
	 * access and leaves OutSnapshot unchanged on every failure. Synchronous
	 * callers may omit Cancellation; asynchronous callers pass their shared token.
	 */
	static bool CreateInitialSnapshot(
		const FArcForestBootstrapInput& Input,
		FArcForestWorldSnapshot& OutSnapshot,
		FText& OutError,
		const FArcForestCancellationToken* Cancellation = nullptr);
};
