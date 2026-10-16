// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataMaps/ArcTerrainDataMapTypes.h"
#include "Debug/ArcForestDebugDrawTypes.h"
#include "Ecotope/ArcEcotopeModel.h"
#include "Engine/EngineTypes.h"
#include "Foliage/ArcEcotopeFoliageFieldPayload.h"
#include "GameFramework/Actor.h"
#include "Misc/DataValidation.h"
#include "Simulation/ArcEcotopeBudgetSettings.h"
#include "Simulation/ArcForestBootstrap.h"
#include "Simulation/ArcForestSimulationState.h"
#include "Simulation/ArcEcotopeSiteFieldSettings.h"
#include "Simulation/ArcEcotopeSiteChunkStorage.h"
#include "Simulation/ArcEcotopeSiteMetadata.h"
#include "Simulation/ArcEcotopeTerrainDataSettings.h"
#include "Species/ArcEcotopeSpeciesModel.h"

#if WITH_EDITORONLY_DATA
#include "Serialization/EditorBulkData.h"
#endif

#include "ArcEcotopeVolume.generated.h"

class UArcEcotopeTerrainWeightStamp;
class UBoxComponent;

/** Outcome of folding the instance content hash over the partition actors a volume owns. */
enum class EArcEcotopeContentHashResult : uint8
{
	Computed,
	/** At least one owned actor is streamed out, so no hash comparable with the recorded one exists. */
	Skipped,
	Failed,
};

/** One partition actor a bake owns, whether it carries entities or instanced geometry. */
USTRUCT()
struct ARCECOTOPE_API FArcEcotopeEmittedActor
{
	GENERATED_BODY()

	/** Resolves an actor that is loaded, including one spawned this session and not yet written to a desc. */
	UPROPERTY()
	TSoftObjectPtr<AActor> Actor;

	/** World Partition pin key; a soft path cannot bring back an unloaded actor. */
	UPROPERTY()
	FGuid ActorGuid;
};

UCLASS(BlueprintType)
class ARCECOTOPE_API AArcEcotopeVolume : public AActor
{
	GENERATED_BODY()

public:
	AArcEcotopeVolume();

	/** Stable identity for deterministic simulation lineage; normal duplication generates a new identity. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|State")
	FGuid VolumeId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	TObjectPtr<UBoxComponent> VolumeBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Run")
	TObjectPtr<UArcEcotopeTerrainWeightStamp> TerrainWeightStamp;

#if WITH_EDITORONLY_DATA
	/**
	 * Placed-entity partition actors this volume owns. They carry a grid guid derived from VolumeId, so no
	 * other volume, PCG owner or hand placement shares one.
	 */
	UPROPERTY()
	TArray<FArcEcotopeEmittedActor> EmittedEntityActors;

	/**
	 * Instance partition actors this volume owns. Their grid guid is derived from VolumeId, the
	 * renderer's grid size and its runtime grid name, so no other volume shares one.
	 */
	UPROPERTY()
	TArray<FArcEcotopeEmittedActor> InstancePartitionActors;
#endif

	/** Persistent authoritative forest state, owned as an actor default subobject. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Instanced, Category = "Debug|State")
	TObjectPtr<UArcForestSimulationState> SimulationState;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run")
	int32 Seed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Quality", meta = (ClampMin = "1.0", Units = "cm"))
	float CellSizeCm = 200.0f;

	/** Mip level of the grid that cohorts, patches and per-cell aggregates are keyed on. Part of the settings hash. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Quality", meta = (ClampMin = "0", ClampMax = "8"))
	uint8 RecordGridLevel = 2;

	/**
	 * Execution-only chunk core width in base cells for the scalable simulation pipeline. It is intentionally
	 * excluded from the ecological settings hash while the fixed-seed partition A/B checks remain in place.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Quality", meta = (ClampMin = "1"))
	int32 ChunkBaseCellSpan = 256;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run", meta = (ClampMin = "1"))
	int32 YearsPerRun = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run", meta = (ClampMin = "0"))
	int32 SpinUpYears = 80;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Quality", meta = (ShowOnlyInnerProperties))
	FArcEcotopeBudgetSettings BudgetSettings;

	/**
	 * Per-year growth, cell and crown diagnostics, read by species calibration. This property decides
	 * whether a year GATHERS them; whether the lines are printed is separate, and needs
	 * LogArcForestGrowth raised to VeryVerbose. Gathering every year costs about 76.9 s per 1000
	 * years, so a production bake turns this off. Never part of the settings hash: a baked forest
	 * stays valid.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug|Diagnostics")
	bool bEmitGrowthDiagnostics = true;

	/**
	 * Samples diagnostics every Nth simulated year; the final year of a run always samples, because
	 * every calibration band is read off that row. The default 20 costs about 3.8 s per 1000 years.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug|Diagnostics", meta = (ClampMin = "1"))
	int32 GrowthDiagnosticsIntervalYears = 20;

	/** World-space thickness of debug lines and rings. DebugDraw shrinks these with distance, so a kilometre-wide volume needs metres, not centimetres. 0 draws a perspective-independent one-pixel hairline. */
	UPROPERTY(EditAnywhere, Category = "Debug", DisplayName = "Debug Line Thickness",
		meta = (ClampMin = "0.0", Units = "cm"))
	float DebugLineThicknessCm = 200.0f;

	UPROPERTY(EditAnywhere, Category = "Debug|Structure", DisplayName = "Draw Volume Bounds")
	bool bDrawVolumeBounds = false;
	UPROPERTY(EditAnywhere, Category = "Debug|Structure", DisplayName = "Draw Chunk Grid")
	bool bDrawChunkGrid = false;
	UPROPERTY(EditAnywhere, Category = "Debug|Structure", DisplayName = "Draw Instance Partitions")
	bool bDrawInstancePartitions = false;

	UPROPERTY(EditAnywhere, Category = "Debug|Site", DisplayName = "Draw Site Field")
	bool bDrawSiteField = false;
	UPROPERTY(EditAnywhere, Category = "Debug|Site", DisplayName = "Site Field",
		meta = (EditCondition = "bDrawSiteField",
		InvalidEnumValues = "SunLoad, WindExposure, SkyViewFactor, Temperature, Count"))
	EArcEcotopeSiteField SiteField = EArcEcotopeSiteField::Moisture;

	UPROPERTY(EditAnywhere, Category = "Debug|Site", DisplayName = "Site Field Greyscale",
		meta = (EditCondition = "bDrawSiteField",
		ToolTip = "Draws the field as luminance, so it compares directly against the raw terrain data map sheet. Off restores the colour ramp."))
	bool bSiteFieldGreyscale = true;

	UPROPERTY(EditAnywhere, Category = "Debug|Site", DisplayName = "Site Field Stride",
		meta = (EditCondition = "bDrawSiteField", ClampMin = "0",
		ToolTip = "0 auto-fits the sheet to Max Drawn Site Cells; above 0 samples every Nth cell explicitly."))
	int32 SiteFieldStride = 0;

	UPROPERTY(EditAnywhere, Category = "Debug|Site", DisplayName = "Site Field Cell Scale",
		meta = (EditCondition = "bDrawSiteField", ClampMin = "0.05", ClampMax = "1.0",
		ToolTip = "Shrinks each sampled quad about its own centre as a fraction of its stride footprint: 1.0 is a continuous sheet, 0.5 leaves a regular gap grid."))
	float SiteFieldCellScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Debug|Terrain Data", DisplayName = "Draw Terrain Data Map")
	bool bDrawTerrainDataMap = false;

	UPROPERTY(EditAnywhere, Category = "Debug|Terrain Data", DisplayName = "Terrain Data Map",
		meta = (EditCondition = "bDrawTerrainDataMap", InvalidEnumValues = "Count"))
	EArcTerrainDataMap TerrainDataMap = EArcTerrainDataMap::Deposits;

	UPROPERTY(EditAnywhere, Category = "Debug|Terrain Data", DisplayName = "Terrain Data Map Gain",
		meta = (EditCondition = "bDrawTerrainDataMap", ClampMin = "0.01", ClampMax = "1000.0",
		ToolTip = "Multiplies the reading before the grey clamp. It scales the overlay only and never the bake, which reads the map ungained."))
	float TerrainDataMapGain = 1.0f;

	/** View state only: outside the settings hash and every frozen run input, so it cannot invalidate a bake. */
	UPROPERTY(EditAnywhere, Category = "Debug|Disturbance", DisplayName = "Draw Disturbance Preview")
	bool bDrawDisturbancePreview = false;
	UPROPERTY(EditAnywhere, Category = "Debug|Disturbance", DisplayName = "Preview Disturbance Kind",
		meta = (EditCondition = "bDrawDisturbancePreview"))
	EArcForestDisturbancePreviewKind PreviewDisturbanceKind = EArcForestDisturbancePreviewKind::Fire;
	UPROPERTY(EditAnywhere, Category = "Debug|Disturbance", DisplayName = "Preview Disturbance Year",
		meta = (EditCondition = "bDrawDisturbancePreview", ClampMin = "1"))
	int32 PreviewDisturbanceYear = 1;
	/** Years to scan forward from Preview Disturbance Year for the first one that fires. 0 previews that exact year only. */
	UPROPERTY(EditAnywhere, Category = "Debug|Disturbance", DisplayName = "Preview Search Years",
		meta = (EditCondition = "bDrawDisturbancePreview", ClampMin = "0", ClampMax = "10000"))
	int32 PreviewDisturbanceSearchYears = 1000;

	UPROPERTY(EditAnywhere, Category = "Debug|Stand", DisplayName = "Draw Individuals")
	bool bDrawIndividuals = false;
	UPROPERTY(EditAnywhere, Category = "Debug|Stand", DisplayName = "Draw Individual Crowns",
		meta = (EditCondition = "bDrawIndividuals"))
	bool bDrawIndividualCrowns = true;
	UPROPERTY(EditAnywhere, Category = "Debug|Stand", DisplayName = "Stand Colour Mode")
	EArcForestDebugStandColor StandColorMode = EArcForestDebugStandColor::Species;
	UPROPERTY(EditAnywhere, Category = "Debug|Stand", DisplayName = "Draw Deadwood")
	bool bDrawDeadwood = false;
	UPROPERTY(EditAnywhere, Category = "Debug|Stand", DisplayName = "Draw Cohorts")
	bool bDrawCohorts = false;
	UPROPERTY(EditAnywhere, Category = "Debug|Stand", DisplayName = "Draw Patches")
	bool bDrawPatches = false;

	/** Empty draws every species. Otherwise only the listed species are drawn. */
	UPROPERTY(EditAnywhere, Category = "Debug|Stand", DisplayName = "Debug Species Filter",
		meta = (ToolTip = "Restricts the individual, deadwood, cohort and patch layers to the listed species. Empty draws every species."))
	TArray<TObjectPtr<UArcEcotopeSpeciesModel>> DebugSpeciesFilter;

	/** Ground-cover species live as cohorts and patches rather than individual stems. */
	UPROPERTY(EditAnywhere, Category = "Debug|Stand", DisplayName = "Draw Ground Cover Species",
		meta = (ToolTip = "Off restricts every stand layer to species whose growth form individualizes into stems, which is what the GPU scatter layer does not draw."))
	bool bDrawGroundCoverSpecies = false;

	UPROPERTY(EditAnywhere, Category = "Debug|Labels", DisplayName = "Draw Labels")
	bool bDrawLabels = false;

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Debug|Budget", meta = (ClampMin = "0",
		ToolTip = "0 draws every filtered stem. A positive value truncates to that many, taken in record order; it is an escape hatch for when the DebugDraw pools cannot take the load, not a resolution control."))
	int32 MaxDrawnStems = 0;
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Debug|Budget", meta = (ClampMin = "0",
		ToolTip = "0 draws every filtered cohort or patch cell, applied to each layer separately. A positive value truncates to that many, taken in record order; it is an escape hatch for when the DebugDraw pools cannot take the load, not a resolution control."))
	int32 MaxDrawnCellRecords = 0;
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Debug|Budget", meta = (ClampMin = "0",
		ToolTip = "0 draws every filtered deadwood piece. A positive value truncates to that many, taken in record order; it is an escape hatch for when the DebugDraw pools cannot take the load, not a resolution control."))
	int32 MaxDrawnDeadwood = 0;
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Debug|Budget", meta = (ClampMin = "0",
		ToolTip = "How many samples the site field and terrain data map overlays get between them, which is what sets their resolution. The default leaves headroom under UE.DebugDraw.MaxTriangles; raising it much further drops primitives and logs a warning. 0 draws at Site Field Stride instead, unbudgeted - and since Site Field Stride also defaults to 0, that means full resolution with no cap at all."))
	int32 MaxDrawnSiteCells = 50000;
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Debug|Budget", meta = (ClampMin = "1",
		ToolTip = "How many samples the disturbance preview overlay gets, which is what sets its resolution. It warns when that is too coarse to show the finest detail in the noise, because the preview is then not the field the run will actually produce; if the record grid itself is too coarse for it, no budget can fix that and the warning says so. Raising it much past the project's debug triangle budget drops primitives."))
	int32 MaxDrawnDisturbancePreviewCells = 25000;
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Debug|Budget", meta = (ClampMin = "0",
		ToolTip = "0 draws every chunk rectangle. A positive value truncates to that many, taken in layout order; it is an escape hatch for when the DebugDraw pools cannot take the load, not a resolution control."))
	int32 MaxDrawnChunks = 0;

	/** True when any layer is on. bDrawLabels and bDrawIndividualCrowns are modifiers and enable nothing. */
	bool HasDebugOverlayEnabled() const
	{
		return bDrawVolumeBounds || bDrawChunkGrid || bDrawInstancePartitions || bDrawSiteField
			|| bDrawTerrainDataMap || bDrawDisturbancePreview || bDrawIndividuals || bDrawDeadwood
			|| bDrawCohorts || bDrawPatches;
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ecotope")
	TObjectPtr<UArcEcotopeModel> Ecotope;

	const UArcEcotopeModel& GetEcotopeOrDefault() const
	{
		return Ecotope != nullptr ? *Ecotope : *GetDefault<UArcEcotopeModel>();
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run",
		meta = (DisplayName = "Edge Gradient Band", ClampMin = "0.0", Units = "cm"))
	double EdgeGradientBandCm = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run",
		meta = (DisplayName = "Edge Gradient Sharpness", ClampMin = "0.0001"))
	double EdgeGradientSharpness = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quality", meta = (ShowOnlyInnerProperties))
	FArcEcotopeSiteFieldSettings SiteFieldSettings;

	UPROPERTY(EditAnywhere, Category = "Ground", meta = (ShowOnlyInnerProperties))
	FArcEcotopeTerrainDataSettings TerrainDataSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Quality", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxInvalidFraction = 0.1f;

	/** Authoritative scalar contract for the baked chunked site. */
	UPROPERTY(VisibleAnywhere, NonTransactional, Category = "Debug|State")
	FArcEcotopeSiteMetadata SiteMetadata;

#if WITH_EDITORONLY_DATA
	/** Editor-only metadata for the non-transactional authoritative chunk payloads. */
	UPROPERTY(NonTransactional)
	FArcEcotopeSiteChunkManifest SiteChunkManifest;
#endif

	/** Forest field derived when a snapshot is committed. NonTransactional: a transaction inlines
	 *  every one of its tens of megabytes. */
	UPROPERTY(NonTransactional)
	FArcEcotopeFoliageFieldPayload FoliageFieldPayload;

	/** Snapshot checksum represented by the saved native instance components. */
	UPROPERTY(VisibleAnywhere, NonTransactional, Category = "Debug|State")
	uint64 InstanceSnapshotHash = 0;

	/** Canonical enabled-species visual hash represented by the saved components. */
	UPROPERTY(VisibleAnywhere, NonTransactional, Category = "Debug|State")
	uint64 InstanceVisualHash = 0;

	/** Canonical field-wise hash recomputed from the actual saved native components. */
	UPROPERTY(VisibleAnywhere, NonTransactional, Category = "Debug|State")
	uint64 InstanceComponentContentHash = 0;

	/** Explicit derived-state marker: the authoritative snapshot is valid but has no complete saved representation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, NonTransactional, Category = "Debug|State")
	bool bInstanceRepresentationRequiresRebuild = false;

	/** Ceiling on all state records: MaxLiveRecords + MaxCoverRecords + MaxDeadwoodRecords + MaxBranchRecords, clamped to int32. */
	int32 GetMaxStateRecords() const;

	/** Canonical hash of inputs that affect simulation after a snapshot exists. */
	uint64 ComputeSettingsHash() const;

	/** Enabled species ecology hashes sorted canonically by SpeciesId. */
	TArray<FArcForestSpeciesEcologyHash> BuildEnabledSpeciesEcologyHashes() const;

	/** Enabled species visual hashes sorted canonically by SpeciesId. */
	uint64 ComputeEnabledSpeciesVisualHash() const;

	/** Drops the recorded representation without touching the partition actors that carry it. */
	void ClearInstanceRepresentation(bool bRequiresRebuild);

	/** Returns the scalar contract of the authoritative chunked site. */
	FArcEcotopeSiteMetadata GetActiveSiteMetadata() const;

#if WITH_EDITOR
	/**
	 * Game-thread UObject boundary. On success OutInput contains only values and
	 * can be passed to FArcForestBootstrap on a worker.
	 */
	bool FreezeBootstrapInput(
		EArcForestStartMode StartMode,
		FArcForestBootstrapInput& OutInput,
		FText& OutError) const;
#endif

	virtual void PostInitProperties() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void PostRegisterAllComponents() override;

#if WITH_EDITOR
	/** Atomically publishes a validated authoritative chunk source. */
	bool ReplaceSiteWithValidatedChunks(
		FArcEcotopeSiteMetadata&& InMetadata,
		FArcEcotopeSiteChunkManifest&& InManifest,
		TArray<FSharedBuffer>&& InPayloads,
		FText& OutError);

	/** Validates authoritative chunk metadata and payload sizes without loading payload bytes. */
	bool ValidateSiteChunkMetadata(FText& OutError) const;

	/** Creates a worker-safe immutable source from the validated editor bulk payloads. */
	bool FreezeSiteChunkSource(FArcEcotopeFrozenSiteSourcePtr& OutSource, FText& OutError) const;

	/** Records the snapshot and visual identity the bake committed onto this volume's partition actors. */
	bool CommitInstanceRepresentation(uint64 InSnapshotHash, uint64 InVisualHash, FText& OutError);

	/**
	 * Folds the canonical content hash of every instance partition actor this volume owns. An owned actor
	 * that is streamed out yields Skipped rather than a partial hash: a mismatch is what wipes the bake.
	 */
	EArcEcotopeContentHashResult ComputeInstanceComponentContentHash(uint64& OutHash, FText& OutError) const;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostEditImport() override;
	virtual void PostEditUndo() override;
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
#if WITH_EDITORONLY_DATA
	/** Manually serialized after Super::Serialize; deliberately excluded from transactions and cook. */
	TArray<UE::Serialization::FEditorBulkData> SiteChunkPayloads;
#endif

#if WITH_EDITOR
	/** Clears the chunk authority and every derived simulation record, keeping the site scalars and the foliage field. */
	void ClearSiteChunkAuthority();

	/** Clears every baked-site authority and derived simulation record after an invalid load/copy. */
	void ClearBakedSiteAndSimulationState();
#endif
};
