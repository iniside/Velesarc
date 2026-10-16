// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ArcForestWorldSnapshot.generated.h"

class FArcEcotopeChunkLayout;
class FArcEcotopeSiteChunkReader;
struct FArcEcotopeSiteMetadata;

struct ARCECOTOPE_API FArcForestWorldSiteValidationStats
{
	int32 TotalChunkCount = 0;
	int32 ActiveChunkCount = 0;
	int64 IndividualCount = 0;
	int64 CohortCount = 0;
	int64 PatchCount = 0;
	int64 DeadwoodCount = 0;
	int64 AcquireCount = 0;
	int64 FailedAcquireCount = 0;
	int64 ScheduleBytes = 0;
	int64 PeakScheduleBytes = 0;
	int64 ScheduleBudgetBytes = 0;
};

UENUM(BlueprintType)
enum class EArcForestLifeStatus : uint8
{
	Alive,
	DeadStanding,
	DeadDowned,
	MAX UMETA(Hidden),
};

UENUM(BlueprintType)
enum class EArcForestDeadwoodType : uint8
{
	Snag,
	Log,
	Stump,
	Uprooted,
	Branch,
	MAX UMETA(Hidden),
};

/** Usability of a serialized forest state in its current volume context. */
UENUM(BlueprintType)
enum class EArcForestSimulationStateStatus : uint8
{
	Empty,
	Valid,
	Incompatible,
	Invalid,
	InvalidVersion,
};

/** Persisted regional-recruitment lineage; changing the proposal bound requires a fresh simulation state. */
USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcForestRegionalRecruitmentConfig
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recruitment", meta = (ClampMin = "1", ClampMax = "1024"))
	int32 MaxProposalRounds = 256;

	bool IsValid() const;

	bool operator==(const FArcForestRegionalRecruitmentConfig& Other) const
	{
		return MaxProposalRounds == Other.MaxProposalRounds;
	}
};

USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcForestSpeciesEcologyHash
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Identity")
	FGuid SpeciesId;

	UPROPERTY(VisibleAnywhere, Category = "Identity")
	uint64 EcologyHash = 0;
};

USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcForestIndividualRecord
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Identity")
	uint64 Id = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Identity")
	FGuid SpeciesId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	FVector LocalPosition = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	int32 AgeYears = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float HeightCm = 30.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float DbhCm = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float CrownRadiusCm = 0.0f;

	/** Authoritative serialized live biomass; DBH remains independent size state. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float BiomassKg = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float Vigor = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	EArcForestLifeStatus LifeStatus = EArcForestLifeStatus::Alive;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	int32 NextShedYear = MAX_int32;
};

USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcForestCohortRecord
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Identity")
	uint64 Id = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Identity")
	FGuid SpeciesId;

	/** Cell on the baked site's RecordGridLevel grid, not the base grid. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	int32 CellIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	int32 BirthYear = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	int32 HeightBin = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	int64 StemCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float MeanHeightCm = 30.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float MeanAgeYears = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float MeanVigor = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float BiomassKg = 0.0f;
};

USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcForestPatchRecord
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Identity")
	uint64 Id = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Identity")
	FGuid SpeciesId;

	/** Cell on the baked site's RecordGridLevel grid, not the base grid. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	int32 CellIndex = INDEX_NONE;

	/** A stem count for discrete growth, or a cover-equivalent for colonial growth. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float StemOrCoverEquivalent = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float MeanHeightCm = 30.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float MeanAgeYears = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float MeanVigor = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float BiomassKg = 0.0f;
};

USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcForestDeadwoodRecord
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Identity")
	uint64 Id = 0;

	/** Provenance of the individual, cohort, or patch that produced this record; it may repeat across records. */
	UPROPERTY(VisibleAnywhere, Category = "Identity")
	uint64 SourceEntityId = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Identity")
	FGuid SpeciesId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	EArcForestDeadwoodType Type = EArcForestDeadwoodType::Snag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	FTransform LocalTransform = FTransform::Identity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	FVector DimensionsCm = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	int32 DeathYear = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	float InitialBiomassKg = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	int32 RetireYear = 0;

	/** MAX_int32 for a form that never falls, including every record that is not a snag. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	int32 FallYear = MAX_int32;
};

namespace UE::ArcEcotope::Deadwood
{
	inline uint8 CalculateDecayClass(const FArcForestDeadwoodRecord& Record, const int32 Year)
	{
		return static_cast<uint8>(FMath::Clamp((Year - Record.DeathYear) / 5, 0, 5));
	}
}

/**
 * Authoritative, volume-local forest state. Canonical order is ecology hashes
 * by SpeciesId and every record array by its stable identifier. The bulk record
 * and per-cell arrays are intentionally not editor-visible: FPropertyRowGenerator
 * generates one row per element and expands the tree eagerly, so exposing them
 * stalls the editor on selection.
 */
USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcForestWorldSnapshot
{
	GENERATED_BODY()

	/** Defaults to the current version so a default-constructed snapshot validates as Empty, not InvalidVersion. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Metadata")
	int32 SchemaVersion = GetCurrentSchemaVersion();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Metadata")
	FGuid SimulationId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Metadata")
	int32 CurrentYear = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Metadata")
	int32 Seed = 0;

	UPROPERTY(VisibleAnywhere, Category = "Metadata")
	uint64 SiteSnapshotHash = 0;

	UPROPERTY(VisibleAnywhere, Category = "Metadata")
	uint64 SettingsHash = 0;

	/** Authoritative recruitment proposal bound captured when this state was bootstrapped. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Metadata")
	FArcForestRegionalRecruitmentConfig RegionalRecruitmentConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Metadata")
	TArray<FArcForestSpeciesEcologyHash> SpeciesEcologyHashes;

	UPROPERTY(VisibleAnywhere, Category = "Metadata")
	uint64 StateRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Records")
	TArray<FArcForestIndividualRecord> Individuals;

	UPROPERTY(BlueprintReadOnly, Category = "Records")
	TArray<FArcForestCohortRecord> Cohorts;

	UPROPERTY(BlueprintReadOnly, Category = "Records")
	TArray<FArcForestPatchRecord> Patches;

	UPROPERTY(BlueprintReadOnly, Category = "Records")
	TArray<FArcForestDeadwoodRecord> Deadwood;

	/** Canonical field-wise checksum of every field above. */
	UPROPERTY(VisibleAnywhere, Category = "Metadata")
	uint64 StateChecksum = 0;

	static int32 GetCurrentSchemaVersion();

	bool IsEmpty() const;

	/**
	 * Validates intrinsic data and, when supplied, settings and enabled-species
	 * compatibility. The stored checksum is always checked.
	 */
	EArcForestSimulationStateStatus Validate(
		FText& OutError,
		uint64 ExpectedSettingsHash = 0,
		const TArray<FArcForestSpeciesEcologyHash>* ExpectedSpeciesEcologyHashes = nullptr) const;

	/** Validates only record-to-site compatibility through an already validated storage-neutral reader. */
	bool ValidateSiteCompatibility(
		const FArcEcotopeSiteMetadata& SiteMetadata,
		const FArcEcotopeChunkLayout& SiteLayout,
		const FArcEcotopeSiteChunkReader& SiteReader,
		int64 ScheduleBudgetBytes,
		FArcForestWorldSiteValidationStats& OutStats,
		FText& OutError) const;

	uint64 CalculateCanonicalChecksum() const;

	/** Sorts canonical arrays, normalizes signed zero, then refreshes StateChecksum. */
	void CanonicalizeAndRefreshChecksum();
};
