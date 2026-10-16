// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "PCGContext.h"
#include "Forest/ArcForestSimulator.h"
#include "Forest/ArcForestNoise.h"
#include "Forest/ArcForestSimTypes.h"
#include "Forest/ArcForestTerrainFields.h"
#include "UObject/StrongObjectPtr.h"
#include "Tasks/Task.h"

#include "PCGForestSimSettings.generated.h"

class UArcForestSpeciesDefinition;

/**
 * PCG node that runs the pure forest-growth ecosystem solver (Forest/ArcForestSimulator) over a
 * surface input and emits one point per surviving plant, with ecological attributes attached to
 * the point Elements metadata domain for downstream attribute-driven mesh assignment.
 *
 * THREADING (H4 — investigated against PCG's own code, do NOT relax without re-checking):
 *   The pure solver is worker-safe, but the SURFACE SAMPLING it depends on is NOT reliably
 *   thread-safe off the game thread. Empirically, UPCGLandscapeData::ProjectPoint reads the
 *   landscape cache, and *creating* cache entries is explicitly main-thread-only in editor
 *   (PCGLandscapeData.cpp:691-692 "Creation of new cache entries is not thread safe because of
 *   FLandscapeComponentDataInterface"). PCG's own PCGSurfaceSampler stays worker-safe ONLY by
 *   priming that cache via a GT-scheduled task during PrepareData before it samples off-thread;
 *   this node samples the surface directly and does not run that priming path. Sampling the
 *   surface off the game thread could therefore trigger unsafe cache creation.
 *   CHOICE: CanExecuteOnlyOnMainThread -> TRUE (governs where ExecuteInternal runs, not the tasks
 *   it spawns). ExecuteInternal is a three-phase state machine on the game thread:
 *     1. PREBAKE (GT): validate, then ProjectPoint-sample suitability + surface Z into the context
 *        grids and fill the solve params. H4-unsafe surface touches all happen here, on the GT.
 *     2. SOLVE (worker): the pure array-in/array-out solve (SeedInitial + StepIterations + Emit) is
 *        launched ONCE on a UE::Tasks worker (SolveTask) capturing only the weak context Handle;
 *        the GT then polls SolveTask.IsCompleted() one bool per frame via PauseUntilNextFrame(),
 *        so the editor never hitches on the solve. SpeciesPin keeps the species table alive for the
 *        worker's whole run. Cancellation is AbortInternal (GT-only) waiting on SolveTask.
 *     3. EMIT (GT): build the output point data from the solved survivors (NewPointData_AnyThread).
 *
 * CACHING (H3): IsCacheable -> false, matching the two existing Arc PCG nodes. A surface sim is
 * not safe to fold on the input CRC (surface changes are not reliably captured), so it re-runs.
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGForestSimSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("ForestSim")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGForestSim", "NodeTitle", "Forest Sim"); }
	virtual FText GetNodeTooltipText() const override;
	// Sampler (M7): this samples-and-projects onto a surface, it does not create from nothing.
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
	virtual FPCGElementPtr CreateElement() const override;

public:
	/** Species table (the tuning surface); array index is the emitted Species id. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest", meta = (PCG_Overridable))
	TObjectPtr<UArcForestSpeciesDefinition> Species;

	/** RNG seed for the whole run; identical seed -> identical survivors. (Named SimSeed to avoid
	 *  shadowing UPCGSettings::Seed, which drives PCG's own point-seed machinery.) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest", meta = (PCG_Overridable))
	int32 SimSeed = 0;

	/** Global density multiplier applied on top of every species' per-species TargetDensity. 1.0 = seed
	 *  each species at its authored TargetDensity; 2.0 = twice as many of every species; 0.5 = half. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest", meta = (ClampMin = "0.0", PCG_Overridable))
	float SeedingDensity = 1.0f;

	/** Fraction of a species' mature crown radius used as the seed-time minimum spacing between plants. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest", meta = (ClampMin = "0.0", PCG_Overridable))
	float SpacingFraction = 0.8f;

	/** Number of growth/competition iterations (K); controls forest maturity. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest", meta = (ClampMin = "0", PCG_Overridable))
	int32 Iterations = 32;

	/** Beer-Lambert extinction coefficient k for top-down light attenuation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest", meta = (ClampMin = "0.0", PCG_Overridable))
	float CanopyExtinction = 0.5f;

	/**
	 * Patchiness dial: the fraction of seed-time darts placed as Gaussian offspring around a clump
	 * centre instead of uniformly (clumped probability p = WPatch / (1 + WPatch)). 0 = an even
	 * scatter, 1 = half the plants grow out of clumps, higher = almost everything in clumps.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest", meta = (ClampMin = "0.0", PCG_Overridable))
	float WPatch = 1.0f;

	// Per-band horizontal competition toggles (indexed by EArcForestLayer). Default on in all bands.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest|Competition", meta = (PCG_Overridable))
	bool bCompeteField = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest|Competition", meta = (PCG_Overridable))
	bool bCompeteShrub = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest|Competition", meta = (PCG_Overridable))
	bool bCompeteUnderstory = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest|Competition", meta = (PCG_Overridable))
	bool bCompeteCanopy = true;

	// Per-band vertical self-shading toggles (indexed by EArcForestLayer). Default on for Field only.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest|Shading", meta = (PCG_Overridable))
	bool bShadeField = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest|Shading", meta = (PCG_Overridable))
	bool bShadeShrub = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest|Shading", meta = (PCG_Overridable))
	bool bShadeUnderstory = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest|Shading", meta = (PCG_Overridable))
	bool bShadeCanopy = false;

	// Habitat — terrain morphology fields DERIVED from the baked surface height grid. Nothing is
	// painted or authored per-map; these settings only say how the derived values are normalised.
	// See Forest/ArcForestTerrainFields.h for the axis convention and the resolvable-feature limit.

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest|Habitat", meta = (ClampMin = "0.0", ClampMax = "90.0", PCG_Overridable, Tooltip = "Slope in degrees that maps to a Slope field value of 1.0; steeper terrain clamps at 1.0."))
	float MaxSlopeDegrees = 45.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest|Habitat", meta = (PCG_Overridable, Tooltip = "Sun azimuth in degrees in the world XY plane: 0 = +X, 90 = +Y, increasing from +X toward +Y. A slope facing this direction reads SunExposure 1.0."))
	float SunAzimuthDegrees = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest|Habitat", meta = (PCG_Overridable, Tooltip = "Author the Elevation field's absolute normalisation range instead of deriving it from the execution bounds' Z extent."))
	bool bOverrideElevationRange = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest|Habitat", meta = (EditCondition = "bOverrideElevationRange", PCG_Overridable, Tooltip = "Absolute world Z in cm mapping to Elevation 0.0; keep it identical across partitions or an elevation-keyed species breaks at every seam."))
	float ElevationRangeMinCm = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest|Habitat", meta = (EditCondition = "bOverrideElevationRange", PCG_Overridable, Tooltip = "Absolute world Z in cm mapping to Elevation 1.0; equal to (or below) the min gives a constant 0.5 field, never a divide by zero."))
	float ElevationRangeMaxCm = 0.0f;

	/**
	 * Moisture is ANALYTIC NOISE, not a painted channel and not derived from the height grid: nothing
	 * about habitat is authored per-map. See Forest/ArcForestNoise.h for the per-type formulas and the
	 * feature-size intuition behind Frequency.
	 *
	 * PCG_Overridable on a nested USTRUCT exposes an override pin per CHILD property (Frequency,
	 * Octaves, Lacunarity, Gain, SeedOffset, Type) — PCG recurses into struct properties, see the
	 * PCG_OverridableChildProperties note in PCGCommon.h and the FStructProperty branch in
	 * PCGSettingsHelpers.cpp's RecursiveExtraction. Every sibling in this category is overridable and
	 * per-partition moisture tuning is a real use, so this is too. Exposed as one nested block rather
	 * than six flattened scalars because the same block is reused verbatim by any future
	 * noise-driven habitat axis; nesting and override pins are orthogonal.
	 *
	 * NOTHING CONSUMES THIS YET — the per-species habitat response that samples it lands with its own
	 * step.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest|Habitat", meta = (PCG_Overridable, Tooltip = "Analytic noise driving the Moisture habitat field. No terrain data is authored: the field generates itself from the sim seed."))
	FArcForestNoiseParams MoistureNoise;

	// Recruitment — new seedlings establishing under the standing stand BETWEEN iterations, as opposed to
	// the one-shot seeding pass that creates the initial stand. Where a recruit can establish is decided
	// by the SAME surface suitability that gates seeding (so a recruit can never land on a masked or
	// off-surface cell), then by the species' habitat response and its Shade Response Curve against the
	// light reaching seedling height — which is the mechanism that produces a real understory instead of
	// one even-aged cohort.

	/**
	 * Master switch. OFF by default, and that default is load-bearing: off, the solver does not even build
	 * the query-height transmittance raster recruitment needs and consumes not one extra random draw, so a
	 * run is bit-identical to one from a build without recruitment.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest|Recruitment", meta = (PCG_Overridable, Tooltip = "Let new seedlings establish under the standing stand between iterations. Off = one seeded cohort only."))
	bool bEnableRecruitment = false;

	/** Fraction of the population ceiling ATTEMPTED per iteration; habitat, shade and spacing then reject
	 *  darts, so the realised recruit count is always at or below it. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest|Recruitment", meta = (ClampMin = "0.0", EditCondition = "bEnableRecruitment", PCG_Overridable, Tooltip = "Fraction of the population ceiling attempted per iteration. An attempt rate, not an establishment rate."))
	float RecruitmentRate = 0.1f;

	/** Hard cap on the live plant count, and the base the per-iteration budget is a fraction of. 0 derives
	 *  it from the same expression seeding uses for its candidate budget. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest|Recruitment", meta = (ClampMin = "0", EditCondition = "bEnableRecruitment", PCG_Overridable, Tooltip = "Maximum live plants. 0 derives it from the species TargetDensity x Seeding Density x area, the same budget seeding uses."))
	int32 PopulationCeiling = 0;

	/** Light-grid cells per transmittance-raster cell, per axis: 4 at the solver's 100 cm cells = a 4 m
	 *  raster, ~16x cheaper than 1:1 and still resolving a mature crown's shade footprint. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest|Recruitment", meta = (ClampMin = "1", EditCondition = "bEnableRecruitment", PCG_Overridable, Tooltip = "Resolution of the shade raster recruitment reads, in light-grid cells per raster cell per axis. 4 = 4 m cells. Lower is sharper and more expensive."))
	int32 RecruitStride = 4;

	/** Height (cm) the shade raster is computed for — the shade a plant of this height would read.
	 *  Defaults to the seedling height, which is the size a recruit is actually born at. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest|Recruitment", meta = (ClampMin = "0.0", EditCondition = "bEnableRecruitment", PCG_Overridable, Tooltip = "Height in cm the shade raster is measured at. Defaults to the seedling height a recruit is born at."))
	float RecruitQueryHeightCm = ArcForestSim::SeedlingHeightCm;

	/** Absolute floor (cm) on recruit spacing, under
	 *  Max(this, SpacingFraction * (seedling radius + neighbour's LIVE radius)). Without a floor the
	 *  recruit-vs-recruit distance is only ~8 cm and recruits clot into the few darkest cells. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Forest|Recruitment", meta = (ClampMin = "0.0", EditCondition = "bEnableRecruitment", PCG_Overridable, Tooltip = "Minimum distance in cm between a recruit and its nearest neighbour. Stops recruits clotting in the darkest cells."))
	float RecruitMinSpacingCm = 100.0f;
};

/**
 * Per-execution context: holds the prebaked grids/params (produced on the GT) and the working +
 * final solve state. The pure solve runs on a UE::Tasks worker (SolveTask); the GT polls
 * SolveTask.IsCompleted() across successive PauseUntilNextFrame() invocations and only emits once it
 * completes, so the solve never hitches the editor. See UPCGForestSimSettings threading comment.
 */
struct FPCGForestSimContext : public FPCGContext
{
	/** Survivors produced by the solve phase, emitted as points by the emit phase. */
	FArcForestResult Result;

	/** Nearest-cell surface Z (world cm) over the suitability grid, used to project survivors onto the surface. */
	TArray<float> SurfaceHeightGrid;

	/**
	 * Surface-validity mask parallel to SurfaceHeightGrid: true where ProjectPoint HIT, false where it
	 * missed and the height is the fallback constant.
	 *
	 * Load-bearing because neither existing array carries a usable no-data marker: the fallback Z is an
	 * ordinary float a genuine hit can equal, and SuitabilityGrid == 0 also happens on a real hit whose
	 * Density is 0. Without this, a hole is indistinguishable from measured ground and enters the
	 * morphology finite differences as a cliff.
	 */
	TArray<bool> SurfaceHitGrid;

	/**
	 * Everything the per-species habitat response reads: the three terrain-morphology fields derived
	 * from SurfaceHeightGrid + SurfaceHitGrid, the moisture noise parameters copied from the authored
	 * settings, and the per-species response curves flattened to LUTs — all assembled on the GAME
	 * THREAD in the prebake and then read (never mutated) by the worker.
	 *
	 * The morphology fields live HERE rather than as separate context members precisely so the bundle
	 * the solver receives is the one the prebake filled: an extra copy of three MaxGridDim^2 grids per
	 * solve would be pure waste, and two places to keep in sync is one too many.
	 */
	FArcForestHabitatInputs Habitat;

	/** Pre-baked suitability in [0,1] over Bounds2D (row-major, X fastest); baked once, fed to the sliced solve. */
	TArray<float> SuitabilityGrid;

	/** Suitability/height grid dimensions (X = columns, Y = rows). */
	FIntPoint GridDims = FIntPoint::ZeroValue;

	/** Planar footprint the height grid covers (world XY cm) — the frame for the Z bilinear lookup. */
	FBox2f Bounds2D = FBox2f(ForceInit);

	/** Global tuning + Bounds2D filled once in the prebake phase; drives every solve slice. */
	FArcForestSimParams Params;

	/** Resumable working state of the solve; persists across slices so StepIterations RESUMES. */
	FArcForestSolveState SolveState;

	/** Keeps the species table alive across the yields between slices (const, read-only tuning surface). */
	TStrongObjectPtr<const UArcForestSpeciesDefinition> SpeciesPin;

	/** Worker task running the pure solve (SeedInitial + StepIterations + Emit); GT polls IsCompleted(). */
	UE::Tasks::TTask<void> SolveTask;

	/** True once the prebake phase (validation + suitability/surface bake + param fill) has run. */
	bool bPrebaked = false;

	/** True once the worker solve has been launched — guards against a double-launch on re-entry. */
	bool bSolveLaunched = false;

	/** True once the solve is finished and Result is populated (or a prebake early-out short-circuited it). */
	bool bSolveDone = false;
};

class FPCGForestSimElement : public IPCGElementWithCustomContext<FPCGForestSimContext>
{
public:
	// H4: surface sampling is not worker-safe (landscape cache creation is GT-only in editor) — pin to GT.
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* InContext) const override { return true; }
	// H3: a surface sim is not safe to fold on the input CRC — matches the other Arc PCG nodes.
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
	// The output CRC must reflect the ACTUAL emitted points, not just settings: this node re-runs
	// (IsCacheable=false) and produces different points when the execution bounds change (e.g. the
	// PCGVolume is moved), but downstream CACHEABLE nodes (Filter Attribute Elements, etc.) key their
	// cache on this output's CRC. Without a full data CRC the moved-volume run yields the same CRC and
	// downstream serves STALE cached points until manually inspected. Full data CRC forces the refresh.
	virtual bool ShouldComputeFullOutputDataCrc(FPCGContext* Context) const override { return true; }

	// Cancellation guard: PCG's Cancel only Wait()s its own ExecutingTask, never a task we launch, so
	// the worker solve must be drained here before the context/grids it reads are freed. AbortInternal
	// runs on the GT and always precedes context Release for a paused task, so this Wait() is the
	// load-bearing lifetime guarantee (a ~FPCGForestSimContext Wait() would risk a worker-thread
	// self-deadlock since the context can be deleted on the worker thread).
	virtual void AbortInternal(FPCGContext* InContext) const override
	{
		if (FPCGForestSimContext* Context = static_cast<FPCGForestSimContext*>(InContext))
		{
			if (Context->SolveTask.IsValid())
			{
				Context->SolveTask.Wait();
			}
		}
	}

protected:
	virtual bool SupportsBasePointDataInputs(FPCGContext* InContext) const override { return true; }
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
