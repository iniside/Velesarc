// Copyright Lukasz Baran. All Rights Reserved.

#include "Forest/PCGForestSimSettings.h"

#include "ArcPCGExtensionModule.h"
#include "Forest/ArcForestField.h"
#include "Forest/ArcForestSpeciesDefinition.h"
#include "Forest/ArcForestSimTypes.h"
#include "Forest/ArcForestTerrainFields.h"

#include "PCGCommon.h"
#include "PCGData.h"
#include "PCGPin.h"
#include "PCGPoint.h"
#include "PCGGraphExecutionStateInterface.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGBasePointData.h"
#include "Elements/PCGProjectionParams.h"
#include "Helpers/PCGHelpers.h"
#include "Utils/PCGLogErrors.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataCommon.h"
#include "Metadata/PCGMetadataDomain.h"

#include "Math/Box2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGForestSimSettings)

#define LOCTEXT_NAMESPACE "PCGForestSim"

FPCGElementPtr UPCGForestSimSettings::CreateElement() const
{
	return MakeShared<FPCGForestSimElement>();
}

#if WITH_EDITOR
FText UPCGForestSimSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Runs the forest-growth ecosystem simulation over a surface input and emits one point per "
		"surviving plant, with Species/Layer/CrownRadius/Height/Age/Vigor/PatchId/InstanceSeed "
		"attributes for downstream attribute-based selection and spawning (the graph decides the mesh). "
		"Samples the surface on the game thread (landscape cache is not worker-safe), then runs the "
		"solve on a worker thread and polls it so the editor never hitches.");
}
#endif

TArray<FPCGPinProperties> UPCGForestSimSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(
		PCGPinConstants::DefaultInputLabel,
		EPCGDataType::Surface,
		/*bAllowMultipleConnections=*/false,
		/*bAllowMultipleData=*/false,
		LOCTEXT("InputPinTooltip", "Surface to project the forest onto and sample suitability from."));
	return PinProperties;
}

// ---------------------------------------------------------------------------
// Helpers — file-scoped namespace (unique to this .cpp)
// ---------------------------------------------------------------------------

namespace PCGForestSimSettingsLocal
{
	/** Suitability/height grid cell size in world cm. Coarser than plant spacing; bounds memory. */
	constexpr float SuitabilityCellSizeCm = 200.0f;

	/** Hard cap on either grid dimension so large bounds grow the cell size instead of the memory. */
	constexpr int32 MaxGridDim = 256;

	/** Clamped grid dimensions for this node's footprint (shared helper + this node's cell size/cap). */
	FIntPoint ComputeGridDims(const FBox2f& Bounds2D)
	{
		return ArcForestField::ComputeGridDims(Bounds2D, SuitabilityCellSizeCm, MaxGridDim);
	}

	// CellCentre is the shared ArcForestField::CellCentre — the bake loop calls it directly.

	/** Bilinear lookup of surface Z (world cm) at a world XY position over the height grid. */
	float SampleHeightBilinear(
		const TArray<float>& HeightGrid,
		FIntPoint GridDims,
		const FBox2f& Bounds2D,
		FVector2f Pos,
		float FallbackZ)
	{
		if (HeightGrid.Num() != GridDims.X * GridDims.Y || GridDims.X <= 0 || GridDims.Y <= 0)
		{
			return FallbackZ;
		}

		const FVector2f Size = Bounds2D.GetSize();
		if (Size.X <= 0.0f || Size.Y <= 0.0f)
		{
			return HeightGrid[0];
		}

		// Position in cell-centre coordinates (cell centres sit at integer indices).
		const float Fx = (Pos.X - Bounds2D.Min.X) / Size.X * static_cast<float>(GridDims.X) - 0.5f;
		const float Fy = (Pos.Y - Bounds2D.Min.Y) / Size.Y * static_cast<float>(GridDims.Y) - 0.5f;

		const int32 X0 = FMath::Clamp(FMath::FloorToInt(Fx), 0, GridDims.X - 1);
		const int32 Y0 = FMath::Clamp(FMath::FloorToInt(Fy), 0, GridDims.Y - 1);
		const int32 X1 = FMath::Clamp(X0 + 1, 0, GridDims.X - 1);
		const int32 Y1 = FMath::Clamp(Y0 + 1, 0, GridDims.Y - 1);

		const float Tx = FMath::Clamp(Fx - static_cast<float>(X0), 0.0f, 1.0f);
		const float Ty = FMath::Clamp(Fy - static_cast<float>(Y0), 0.0f, 1.0f);

		const float H00 = HeightGrid[Y0 * GridDims.X + X0];
		const float H10 = HeightGrid[Y0 * GridDims.X + X1];
		const float H01 = HeightGrid[Y1 * GridDims.X + X0];
		const float H11 = HeightGrid[Y1 * GridDims.X + X1];

		const float HTop = FMath::Lerp(H00, H10, Tx);
		const float HBottom = FMath::Lerp(H01, H11, Tx);
		return FMath::Lerp(HTop, HBottom, Ty);
	}

	/** Resolve the first spatial (surface) input on the default pin. */
	const UPCGSpatialData* GetSurfaceInput(FPCGContext* Context)
	{
		const TArray<FPCGTaggedData> Inputs =
			Context->InputData.GetSpatialInputsByPin(PCGPinConstants::DefaultInputLabel);
		for (const FPCGTaggedData& Input : Inputs)
		{
			const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);
			if (SpatialData)
			{
				return SpatialData;
			}
		}
		return nullptr;
	}
}

// ---------------------------------------------------------------------------
// Element execution
// ---------------------------------------------------------------------------

bool FPCGForestSimElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGForestSimElement::Execute);
	check(InContext);

	using namespace PCGForestSimSettingsLocal;

	FPCGForestSimContext* Context = static_cast<FPCGForestSimContext*>(InContext);

	const UPCGForestSimSettings* Settings = Context->GetInputSettings<UPCGForestSimSettings>();
	check(Settings);

	// ------------------------------------------------------------------
	// Prebake phase (runs once): validate, then sample suitability + surface Z on the GT (H4: never
	// sample the surface off-GT) and fill the solve params. Yields the GT after the heavy bake.
	// Validation early-outs set bSolveDone so the emit phase still runs and emits an empty result.
	// ------------------------------------------------------------------
	if (!Context->bPrebaked)
	{
		Context->bPrebaked = true;

		if (!Settings->Species || Settings->Species->Species.IsEmpty())
		{
			UE_LOGF(LogArcPCGForest, Warning,"ForestSim: no Species definition set (or it is empty) — nothing to grow. Assign a UArcForestSpeciesDefinition on the node.");
			PCGLog::LogWarningOnGraph(
				LOCTEXT("NoSpecies", "No Species definition set (or it is empty). Nothing to grow."),
				Context);
			Context->bSolveDone = true;
			return false;
		}
		UE_LOGF(LogArcPCGForest, Log,"ForestSim: species table has %d entries.",
			Settings->Species->Species.Num());

		const UPCGSpatialData* SurfaceIn = GetSurfaceInput(Context);
		if (!SurfaceIn)
		{
			UE_LOGF(LogArcPCGForest, Warning,"ForestSim: no surface input connected on the default pin — cannot project a forest. Connect a Surface (e.g. a landscape/terrain sampler) to the input.");
			PCGLog::LogWarningOnGraph(
				LOCTEXT("NoSurface", "No surface input connected. Cannot project a forest."),
				Context);
			Context->bSolveDone = true;
			return false;
		}

		// Region to fill: the execution-source (PCG actor) bounds, falling back to the surface bounds.
		FBox Bounds(ForceInit);
		const TCHAR* BoundsSource = TEXT("none");
		if (Context->ExecutionSource.IsValid())
		{
			Bounds = Context->ExecutionSource->GetExecutionState().GetBounds();
			BoundsSource = TEXT("ExecutionSource");
		}
		if (!Bounds.IsValid || Bounds.GetSize().IsNearlyZero())
		{
			Bounds = SurfaceIn->GetBounds();
			BoundsSource = TEXT("SurfaceInput");
		}
		if (!Bounds.IsValid || Bounds.GetSize().X <= 0.0 || Bounds.GetSize().Y <= 0.0)
		{
			UE_LOGF(LogArcPCGForest, Warning,"ForestSim: execution bounds are empty (source=%ls, valid=%d, size=%ls) — cannot generate a forest.",
				BoundsSource, Bounds.IsValid ? 1 : 0, *Bounds.GetSize().ToString());
			PCGLog::LogWarningOnGraph(
				LOCTEXT("BadBounds", "Execution bounds are empty. Cannot generate a forest."),
				Context);
			Context->bSolveDone = true;
			return false;
		}
		UE_LOGF(LogArcPCGForest, Log,"ForestSim: bounds source=%ls min=%ls max=%ls size=%ls",
			BoundsSource, *Bounds.Min.ToString(), *Bounds.Max.ToString(), *Bounds.GetSize().ToString());

		const FBox2f Bounds2D(
			FVector2f(static_cast<float>(Bounds.Min.X), static_cast<float>(Bounds.Min.Y)),
			FVector2f(static_cast<float>(Bounds.Max.X), static_cast<float>(Bounds.Max.Y)));

		const FIntPoint GridDims = ComputeGridDims(Bounds2D);
		const int32 CellCount = GridDims.X * GridDims.Y;
		UE_LOGF(LogArcPCGForest, Log,"ForestSim: suitability grid %dx%d (%d cells) at %.0f cm/cell.",
			GridDims.X, GridDims.Y, CellCount, SuitabilityCellSizeCm);

		// Pre-bake suitability + surface height on the GT (H4: never sample the surface off-GT).
		Context->SuitabilityGrid.SetNumUninitialized(CellCount);
		Context->SurfaceHeightGrid.SetNumUninitialized(CellCount);
		Context->SurfaceHitGrid.SetNumUninitialized(CellCount);
		Context->GridDims = GridDims;
		Context->Bounds2D = Bounds2D;

		const float QueryZ = static_cast<float>(Bounds.Max.Z);
		const float FallbackZ = static_cast<float>(Bounds.GetCenter().Z);
		const FPCGProjectionParams ProjParams;

		int32 ProjectHits = 0;
		float SuitMin = TNumericLimits<float>::Max();
		float SuitMax = -TNumericLimits<float>::Max();
		double SuitSum = 0.0;
		int32 NonZeroCells = 0;

		for (int32 Row = 0; Row < GridDims.Y; ++Row)
		{
			for (int32 Col = 0; Col < GridDims.X; ++Col)
			{
				const int32 CellIndex = Row * GridDims.X + Col;
				const FVector2f Centre = ArcForestField::CellCentre(Bounds2D, GridDims, Col, Row);
				const FTransform QueryTransform(FVector(Centre.X, Centre.Y, QueryZ));

				FPCGPoint OutPoint;
				if (SurfaceIn->ProjectPoint(QueryTransform, FBox(ForceInit), ProjParams, OutPoint, nullptr))
				{
					Context->SuitabilityGrid[CellIndex] = FMath::Clamp(OutPoint.Density, 0.0f, 1.0f);
					Context->SurfaceHeightGrid[CellIndex] = static_cast<float>(OutPoint.Transform.GetLocation().Z);
					Context->SurfaceHitGrid[CellIndex] = true;
					++ProjectHits;
				}
				else
				{
					// Off-surface / cold cache: unsuitable, keep a sane Z for any nearby survivor.
					Context->SuitabilityGrid[CellIndex] = 0.0f;
					Context->SurfaceHeightGrid[CellIndex] = FallbackZ;
					// The morphology derivation MUST know this Z is not a measurement: FallbackZ is an
					// ordinary float a genuine hit can equal, so it is not a no-data marker on its own.
					Context->SurfaceHitGrid[CellIndex] = false;
				}

				const float Suit = Context->SuitabilityGrid[CellIndex];
				SuitMin = FMath::Min(SuitMin, Suit);
				SuitMax = FMath::Max(SuitMax, Suit);
				SuitSum += Suit;
				NonZeroCells += (Suit > 0.0f) ? 1 : 0;
			}
		}

		const float SuitMean = CellCount > 0 ? static_cast<float>(SuitSum / CellCount) : 0.0f;
		UE_LOGF(LogArcPCGForest, Log,"ForestSim: suitability bake — ProjectPoint hits=%d/%d, suitability min=%.3f max=%.3f mean=%.3f, nonZeroCells=%d.",
			ProjectHits, CellCount, SuitMin, SuitMax, SuitMean, NonZeroCells);
		if (NonZeroCells == 0)
		{
			UE_LOGF(LogArcPCGForest, Warning,
				"ForestSim: suitability is ZERO in every cell — the sim will seed no candidates and emit no points. "
				"Likely causes: surface ProjectPoint returned Density=0 everywhere (hits=%d/%d), the input surface does not overlap the bounds, or the surface density mask is empty.",
				ProjectHits, CellCount);
		}

		// Derive the terrain-morphology habitat fields from the height grid that was just filled — one
		// pass over an existing array, NO new ProjectPoint calls. They land straight in Context->Habitat,
		// which the per-species response reads below — see the flatten call at the end of this block.
		{
			FArcForestTerrainFieldParams TerrainParams;
			TerrainParams.MaxSlopeDegrees = Settings->MaxSlopeDegrees;
			TerrainParams.SunAzimuthDegrees = Settings->SunAzimuthDegrees;
			if (Settings->bOverrideElevationRange)
			{
				TerrainParams.ElevationMinCm = Settings->ElevationRangeMinCm;
				TerrainParams.ElevationMaxCm = Settings->ElevationRangeMaxCm;
			}
			else
			{
				// Default: the execution bounds' Z extent. Absolute (not the grid's observed extrema) so
				// the same hillside normalises identically in neighbouring partition cells.
				TerrainParams.ElevationMinCm = static_cast<float>(Bounds.Min.Z);
				TerrainParams.ElevationMaxCm = static_cast<float>(Bounds.Max.Z);
			}

			ArcForestTerrainFields::Derive(
				Context->SurfaceHeightGrid,
				Context->SurfaceHitGrid,
				GridDims,
				Bounds2D,
				TerrainParams,
				Context->Habitat.SlopeField,
				Context->Habitat.SunExposureField,
				Context->Habitat.ElevationField);

			// Moisture is analytic, so the bundle carries the PARAMETERS, not a baked grid — the
			// response samples ArcForestNoise::SampleNoise01 at the exact position it needs.
			Context->Habitat.MoistureNoise = Settings->MoistureNoise;

			// Flatten authored response curves to LUTs on the GAME THREAD, before UE::Tasks::Launch.
			// This is the only place FRuntimeFloatCurve is touched, and it must stay that way: the solve
			// polls across frames while the editor stays interactive, so a designer dragging curve keys
			// mid-solve would otherwise mutate a TArray the worker is reading.
			if (Settings->Species)
			{
				ArcForestSpeciesHabitat::FlattenFieldResponses(*Settings->Species, Context->Habitat);
			}

			UE_LOGF(LogArcPCGForest, Log,"ForestSim: morphology fields derived — %dx%d cells, MaxSlope=%.1f deg, SunAzimuth=%.1f deg (0=+X, 90=+Y), elevation range [%.0f, %.0f] cm (%ls).",
				GridDims.X, GridDims.Y, Settings->MaxSlopeDegrees, Settings->SunAzimuthDegrees,
				TerrainParams.ElevationMinCm, TerrainParams.ElevationMaxCm,
				Settings->bOverrideElevationRange ? TEXT("authored") : TEXT("from execution bounds"));
		}

		FArcForestSimParams& Params = Context->Params;
		Params.Seed = Settings->SimSeed;
		Params.SeedingDensity = Settings->SeedingDensity;
		Params.Iterations = Settings->Iterations;
		Params.CanopyExtinction = Settings->CanopyExtinction;
		Params.WPatch = Settings->WPatch;
		Params.SpacingFraction = Settings->SpacingFraction;
		Params.IntraLayerCompetition[static_cast<int32>(EArcForestLayer::Field)] = Settings->bCompeteField;
		Params.IntraLayerCompetition[static_cast<int32>(EArcForestLayer::Shrub)] = Settings->bCompeteShrub;
		Params.IntraLayerCompetition[static_cast<int32>(EArcForestLayer::Understory)] = Settings->bCompeteUnderstory;
		Params.IntraLayerCompetition[static_cast<int32>(EArcForestLayer::Canopy)] = Settings->bCompeteCanopy;
		Params.IntraLayerShading[static_cast<int32>(EArcForestLayer::Field)] = Settings->bShadeField;
		Params.IntraLayerShading[static_cast<int32>(EArcForestLayer::Shrub)] = Settings->bShadeShrub;
		Params.IntraLayerShading[static_cast<int32>(EArcForestLayer::Understory)] = Settings->bShadeUnderstory;
		Params.IntraLayerShading[static_cast<int32>(EArcForestLayer::Canopy)] = Settings->bShadeCanopy;
		// Recruitment. bEnableRecruitment gates both the transmittance raster and the recruitment stage,
		// so the remaining values are inert while it is off.
		Params.bEnableRecruitment = Settings->bEnableRecruitment;
		Params.RecruitmentRate = Settings->RecruitmentRate;
		Params.PopulationCeiling = Settings->PopulationCeiling;
		Params.RecruitStride = Settings->RecruitStride;
		Params.RecruitQueryHeightCm = Settings->RecruitQueryHeightCm;
		Params.RecruitMinSpacingCm = Settings->RecruitMinSpacingCm;
		Params.Bounds2D = Bounds2D;

		// Pin the species table alive for the whole worker run (and the poll frames after it).
		Context->SpeciesPin.Reset(Settings->Species);

		return false; // Yield the game thread after the heavy bake; the worker solve launches next.
	}

	// ------------------------------------------------------------------
	// Solve phase (worker): the pure solve (SeedInitial + StepIterations + Emit) is array-in/array-out
	// and worker-safe, so it is launched ONCE on a UE::Tasks worker capturing only the weak context
	// Handle. The GT then polls SolveTask.IsCompleted() one bool per PauseUntilNextFrame() — no GT
	// hitch. SpeciesPin keeps the species table alive for the worker's whole run.
	// ------------------------------------------------------------------
	if (!Context->bSolveDone)
	{
		if (!Context->bSolveLaunched)
		{
			// Set the guard BEFORE Launch so an early re-entry cannot double-launch the solve.
			Context->bSolveLaunched = true;
			Context->SolveTask = UE::Tasks::Launch(UE_SOURCE_LOCATION,
				[Handle = Context->GetOrCreateHandle()]()
				{
					FPCGContext::FSharedContext<FPCGForestSimContext> Shared(Handle);
					FPCGForestSimContext* C = Shared.Get();
					if (C)
					{
						const FArcForestSimulator Simulator;
						// Habitat is the TRAILING argument on both; StepIterations takes it AFTER MaxIters.
						// Neither of these is the Simulate() pair, so the overload hazard documented on
						// FArcForestSimulator (passing Habitat as a 5th argument silently selects the
						// non-yielding completion overload) does not arise here.
						Simulator.SeedInitial(
							C->SolveState, C->Params, *C->SpeciesPin, C->SuitabilityGrid, C->GridDims,
							C->Habitat);
						Simulator.StepIterations(
							C->SolveState, C->Params, *C->SpeciesPin, C->SuitabilityGrid, C->GridDims,
							/*MaxIters=*/0, C->Habitat);
						C->Result = Simulator.Emit(C->SolveState, *C->SpeciesPin);
						// NOTE: do NOT write bSolveDone here. It is a GT-only control flag; the worker
						// setting it would race the GT's `if (!bSolveDone)` gate (a window where the flag
						// is true but IsCompleted() is still false lets the GT skip the IsCompleted()
						// acquire and read Result before it is published). The GT sets it below, only
						// after observing IsCompleted() == true.
					}
				});

			// Pause: PauseUntilNextFrame() sets bIsPaused and schedules the unpause tick itself —
			// never pre-set bIsPaused (it would trip the ensure and hang the node forever).
			Context->PauseUntilNextFrame();
			return false;
		}

		if (!Context->SolveTask.IsCompleted())
		{
			Context->PauseUntilNextFrame();
			return false;
		}

		// Task complete: IsCompleted() establishes happens-before, so Context->Result is fully published
		// to this thread. Set the GT-only done flag HERE (never on the worker) so this IsCompleted()
		// acquire is the sole gate to the emit phase; fall through.
		Context->bSolveDone = true;
		UE_LOGF(LogArcPCGForest, Log,"ForestSim: solve done (worker) — seed=%d seedingDensity=%.3f iterations=%d -> %d survivors.",
			Context->Params.Seed, Context->Params.SeedingDensity, Context->Params.Iterations, Context->Result.Num());
		if (Context->Result.Num() == 0)
		{
			UE_LOGF(LogArcPCGForest, Warning,
				"ForestSim: solver produced ZERO survivors — node will emit an empty point set. "
				"Check the suitability stats above (all-zero suitability seeds nothing), SeedingDensity>0, and that competition/mortality did not thin the whole stand.");
		}
	}

	// ------------------------------------------------------------------
	// Emit phase: build the output point data from the cached survivors.
	// ------------------------------------------------------------------
	const FArcForestResult& Result = Context->Result;
	const int32 NumSurvivors = Result.Num();

	UPCGBasePointData* PointData = FPCGContext::NewPointData_AnyThread(Context);
	PointData->SetNumPoints(NumSurvivors);
	PointData->AllocateProperties(
		EPCGPointNativeProperties::Transform |
		EPCGPointNativeProperties::Seed |
		EPCGPointNativeProperties::MetadataEntry);

	if (NumSurvivors > 0)
	{
		FPCGMetadataDomain* Domain =
			PointData->MutableMetadata()->GetMetadataDomain(PCGMetadataDomainID::Elements);
		check(Domain);

		// Species carries the authored NAME from the data asset (FArcForestSpeciesEntry::Name), not the
		// array index — so downstream Select/Filter-by-Attribute can match the species by its authored name.
		FPCGMetadataAttribute<FName>* SpeciesAttr = Domain->FindOrCreateAttribute<FName>(TEXT("Species"), NAME_None);
		FPCGMetadataAttribute<int32>* LayerAttr = Domain->FindOrCreateAttribute<int32>(TEXT("Layer"), 0);
		FPCGMetadataAttribute<float>* CrownRadiusAttr = Domain->FindOrCreateAttribute<float>(TEXT("CrownRadius"), 0.0f);
		FPCGMetadataAttribute<float>* HeightAttr = Domain->FindOrCreateAttribute<float>(TEXT("Height"), 0.0f);
		FPCGMetadataAttribute<int32>* AgeAttr = Domain->FindOrCreateAttribute<int32>(TEXT("Age"), 0);
		FPCGMetadataAttribute<float>* VigorAttr = Domain->FindOrCreateAttribute<float>(TEXT("Vigor"), 0.0f);
		FPCGMetadataAttribute<int32>* PatchIdAttr = Domain->FindOrCreateAttribute<int32>(TEXT("PatchId"), INDEX_NONE);
		FPCGMetadataAttribute<int32>* InstanceSeedAttr = Domain->FindOrCreateAttribute<int32>(TEXT("InstanceSeed"), 0);

		FPCGPointTransform::ValueRange TransformRange = PointData->GetTransformValueRange(/*bAllocate=*/false);
		FPCGPointSeed::ValueRange SeedRange = PointData->GetSeedValueRange(/*bAllocate=*/false);
		FPCGPointMetadataEntry::ValueRange EntryRange = PointData->GetMetadataEntryValueRange(/*bAllocate=*/false);

		const TArray<FArcForestSpeciesEntry>& Table = Settings->Species->Species;

		for (int32 Index = 0; Index < NumSurvivors; ++Index)
		{
			const FVector2f PosXY = Result.PosXY[Index];
			const float FallbackZ = Context->SurfaceHeightGrid.Num() > 0 ? Context->SurfaceHeightGrid[0] : 0.0f;
			const float SurfaceZ = SampleHeightBilinear(
				Context->SurfaceHeightGrid, Context->GridDims, Context->Bounds2D, PosXY, FallbackZ);

			const FVector Location(PosXY.X, PosXY.Y, SurfaceZ);
			TransformRange[Index] = FTransform(Location);

			const int32 PointSeed = PCGHelpers::ComputeSeedFromPosition(Location);
			SeedRange[Index] = PointSeed;

			Domain->InitializeOnSet(EntryRange[Index]);

			const int32 SpeciesId = Result.Species[Index];
			const FName SpeciesName = Table.IsValidIndex(SpeciesId) ? Table[SpeciesId].Name : NAME_None;
			SpeciesAttr->SetValue(EntryRange[Index], SpeciesName);
			LayerAttr->SetValue(EntryRange[Index], static_cast<int32>(Result.MatureLayer[Index]));
			CrownRadiusAttr->SetValue(EntryRange[Index], Result.CrownRadius[Index]);
			HeightAttr->SetValue(EntryRange[Index], Result.Height[Index]);
			AgeAttr->SetValue(EntryRange[Index], Result.Age[Index]);
			VigorAttr->SetValue(EntryRange[Index], Result.Vigor[Index]);
			PatchIdAttr->SetValue(EntryRange[Index], Result.PatchId[Index]);
			InstanceSeedAttr->SetValue(EntryRange[Index], PointSeed);
		}
	}

	Context->OutputData.TaggedData.Emplace_GetRef().Data = PointData;
	UE_LOGF(LogArcPCGForest, Log,"ForestSim: emitted %d points.", NumSurvivors);
	return true;
}

#undef LOCTEXT_NAMESPACE
