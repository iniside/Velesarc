// Copyright Lukasz Baran. All Rights Reserved.

#include "Scatter/PCGScatterNearPointsSettings.h"

#include "ArcPCGExtensionModule.h"
#include "Scatter/ArcPCGScatterCore.h"

#include "PCGCommon.h"
#include "PCGContext.h"
#include "PCGData.h"
#include "PCGPoint.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataCommon.h"
#include "Metadata/PCGMetadataDomain.h"
#include "Utils/PCGLogErrors.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGScatterNearPointsSettings)

#define LOCTEXT_NAMESPACE "PCGScatterNearPoints"

namespace PCGScatterNearPointsConstants
{
	const FName SurfaceLabel = TEXT("Surface");
	const FName PointsLabel = TEXT("Points");
}

FPCGElementPtr UPCGScatterNearPointsSettings::CreateElement() const
{
	return MakeShared<FPCGScatterNearPointsElement>();
}

#if WITH_EDITOR
FText UPCGScatterNearPointsSettings::GetNodeTooltipText() const
{
	// This overrides the ToolTip metadata UHT bakes from the class comment, so it is what the graph
	// editor actually shows — it must not contradict that comment. Keep the two in step.
	return LOCTEXT("NodeTooltip",
		"Scatters points in an area-uniform annulus around every source point on the Points pin, "
		"projected down onto the Surface input. Use it to place understory relative to existing "
		"points (\"ferns near trees\"). Darts are queried from the top of the Surface's bounds; if the "
		"Surface reports no bounds (an unbounded surface such as World Ray Hit Query) each dart is "
		"queried at its source point's own height instead, and the effective trace origin is then "
		"whatever that surface itself defines — tune it on the source node's parameters, not here. "
		"Darts that miss the surface are dropped, never emitted at a fallback height. Randomness is "
		"derived from each source point's POSITION, so the result does not change when the same points "
		"arrive split across several data. That position is truncated to whole centimetres, so two "
		"source points inside the same 1 cm cell scatter identically on top of one another.");
}
#endif

TArray<FPCGPinProperties> UPCGScatterNearPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	// Spatial, NOT Surface: EPCGDataType::Surface is Landscape|BaseTexture|VirtualTexture and would
	// REJECT the volumes/primitives/dynamic meshes users will plug in here. This matches
	// UPCGSurfaceSamplerSettings' own Bounding Shape pin. Single connection/single data because two
	// surfaces are ambiguous — union them upstream.
	// Emplace_GetRef, not Emplace: TArray::Emplace returns an INDEX, not a reference.
	FPCGPinProperties& SurfacePin = PinProperties.Emplace_GetRef(
		PCGScatterNearPointsConstants::SurfaceLabel,
		EPCGDataType::Spatial,
		/*bAllowMultipleConnections=*/false,
		/*bAllowMultipleData=*/false,
		LOCTEXT("SurfacePinTooltip", "Surface the scattered points are projected onto. Darts that miss it are dropped."));
	SurfacePin.SetRequiredPin();

	FPCGPinProperties& PointsPin = PinProperties.Emplace_GetRef(
		PCGScatterNearPointsConstants::PointsLabel,
		EPCGDataType::Point,
		/*bAllowMultipleConnections=*/true,
		/*bAllowMultipleData=*/true,
		LOCTEXT("PointsPinTooltip", "Source points to scatter around. One output data is emitted per input data, carrying that data's tags."));
	PointsPin.SetRequiredPin();

	return PinProperties;
}

// ---------------------------------------------------------------------------
// Helpers — file-scoped namespace (unique to this .cpp)
// ---------------------------------------------------------------------------

namespace PCGScatterNearPointsSettingsLocal
{
	/** Resolve the first spatial input on the Surface pin (mirrors the sim's GetSurfaceInput). */
	const UPCGSpatialData* GetSurfaceInput(FPCGContext* Context)
	{
		const TArray<FPCGTaggedData> Inputs =
			Context->InputData.GetSpatialInputsByPin(PCGScatterNearPointsConstants::SurfaceLabel);
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

bool FPCGScatterNearPointsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGScatterNearPointsElement::Execute);
	check(Context);

	const UPCGScatterNearPointsSettings* Settings = Context->GetInputSettings<UPCGScatterNearPointsSettings>();
	check(Settings);

	// Single pass: every path below returns true. There is no state to carry across frames.
	const UPCGSpatialData* Surface = PCGScatterNearPointsSettingsLocal::GetSurfaceInput(Context);
	if (!Surface)
	{
		UE_LOGF(LogArcPCGForest, Warning,"ScatterNearPoints: no spatial data on the Surface pin — emitting nothing.");
		PCGLog::LogWarningOnGraph(
			LOCTEXT("NoSurface", "No spatial data on the Surface pin. Nothing to project onto, so nothing is emitted."),
			Context);
		return true;
	}

	// Every dart is dropped from the top of the surface's bounds, the same vertical-query convention
	// the sim uses. Read once: the surface does not change during the pass.
	//
	// UNBOUNDED SURFACES: an unbounded spatial input reports an INVALID FBox whose Max is the
	// zero-initialised default, so using Max.Z would drop every dart from Z=0 and make every
	// projection miss. This is not an exotic case — UPCGWorldRayHitData (the World Ray Hit Query node,
	// the idiomatic "project onto world collision" source) hardcodes FBox(EForceInit::ForceInit) as its
	// bounds (PCGWorldData.h:460, assigned from the ForceInit argument at PCGWorldQuery.cpp:230) and
	// reports IsBounded() == false. Refusing those inputs makes the node useless for world collision,
	// so instead the query height is EACH SOURCE POINT'S OWN Z — no offset, and no property to author.
	//
	// Why no height knob: on an unbounded surface this node does not choose the trace origin at all.
	// UPCGWorldRayHitData overrides SamplePoint but not ProjectPoint (PCGWorldData.h:438-448), nor does
	// UPCGSurfaceData (PCGSurfaceData.h:29-32), so base UPCGSpatialData::ProjectPoint runs
	// (PCGSpatialData.cpp:258-264) and falls through to SamplePoint — which ORTHOGONALLY PROJECTS the
	// incoming location onto the plane through QueryParams.RayOrigin with normal RayDirection
	// (PCGWorldData.cpp:447-448) before sweeping. Only X and Y survive; the height along the ray comes
	// from the query params (defaulted at PCGWorldQuery.cpp:199-211), so any Z passed here — 0 or 1e9 —
	// produces bit-identical output. Conversely UPCGWorldVolumetricData also leaves ProjectPoint
	// unoverridden (PCGWorldData.h:363) and its SamplePoint is an OVERLAP TEST AT THE QUERY LOCATION
	// (PCGWorldData.cpp:241-266), not a trace, so an arbitrary upward offset would guarantee a miss
	// there. The source's own Z is the only value that is either meaningful or harmless.
	//
	// FBox::IsValid is a uint8 DATA MEMBER (Box.h:45), not a method — compare it explicitly.
	const FBox SurfaceBounds = Surface->GetBounds();
	const bool bUseBoundsZ = SurfaceBounds.IsValid != 0;
	// Only meaningful when bUseBoundsZ; the fallback reads QueryZ off each source point below.
	const double BoundsQueryZ = bUseBoundsZ ? SurfaceBounds.Max.Z : 0.0;

	if (!bUseBoundsZ)
	{
		// ONCE, outside every loop. Warning, not error: the user keeps their points, but should be told
		// that the query height is no longer this node's to control, and where the real knob lives.
		UE_LOGF(LogArcPCGForest, Warning,
			"ScatterNearPoints: the Surface input reports invalid bounds — querying at each source point's own height. "
			"The effective trace origin is whatever that surface defines; tune it on the source node, not here.");
		PCGLog::LogWarningOnGraph(
			LOCTEXT("InvalidSurfaceBounds", "The Surface input reports no bounds (an unbounded surface, e.g. World Ray Hit Query), so there is no bounds height to drop darts from; each dart is queried at its source point's own height. The trace origin is then whatever that surface itself defines — for World Ray Hit Query the query's own ray parameters, which discard the height passed in. If points are missing, fix it on the SOURCE node (e.g. the World Ray Hit Query ray origin, direction and length), not here. An unbounded VOLUMETRIC input is not traced at all: it is sampled as an overlap at each source point's own height, so it yields points only where the volume actually contains those heights."),
			Context);
	}

	const double MinRadiusCm = FMath::Max(Settings->MinRadiusCm, 0.0);
	double MaxRadiusCm = FMath::Max(Settings->MaxRadiusCm, 0.0);
	if (MaxRadiusCm < MinRadiusCm)
	{
		// CLAMP UP, never swap: a user dragging Min past Max gets a degenerate ring at Min, which is
		// visibly what they asked for, rather than a silently inverted annulus. Warned ONCE, here,
		// outside every loop.
		UE_LOGF(LogArcPCGForest, Warning,"ScatterNearPoints: MaxRadiusCm (%.2f) is below MinRadiusCm (%.2f) — clamping Max up to Min.",
			MaxRadiusCm, MinRadiusCm);
		PCGLog::LogWarningOnGraph(
			LOCTEXT("MaxRadiusBelowMin", "Max Radius Cm is below Min Radius Cm; clamping Max up to Min (a degenerate ring)."),
			Context);
		MaxRadiusCm = MinRadiusCm;
	}

	const int32 PointsPerSource = FMath::Max(Settings->PointsPerSource, 0);
	const bool bWriteParentIndex =
		Settings->bWriteParentIndexAttribute && !Settings->ParentIndexAttributeName.IsNone();
	if (Settings->bWriteParentIndexAttribute && Settings->ParentIndexAttributeName.IsNone())
	{
		// The checkbox is on but the name was cleared. Silently writing nothing would look like the
		// node ignoring the checkbox, so say which of the two settings actually won.
		PCGLog::LogWarningOnGraph(
			LOCTEXT("ParentIndexNameNone", "Write Parent Index Attribute is enabled but Parent Index Attribute Name is empty; no parent-index attribute will be written."),
			Context);
	}

	// One output data PER input data, each carrying that input's tags.
	const TArray<FPCGTaggedData> Inputs =
		Context->InputData.GetInputsByPin(PCGScatterNearPointsConstants::PointsLabel);

	// Node seed, resolved ONCE outside every loop. Read below through MakeStreamForPosition.
	const int32 NodeSeed = Context->GetSeed();

	for (const FPCGTaggedData& InTagged : Inputs)
	{
		// COLLAPSE, then ERROR — never drop silently. A type-compatible but non-point input (a volume,
		// a surface, a param) used to produce no output and no diagnostic, which reads to the user as
		// the node being broken. Same shape as the engine's own point ops
		// (PCGTransformPoints.cpp:80-92, PCGSelectPoints.cpp:63-82).
		const UPCGSpatialData* InSpatialData = Cast<UPCGSpatialData>(InTagged.Data);
		if (!InSpatialData)
		{
			PCGE_LOG(Error, GraphAndLog,
				LOCTEXT("InputMissingSpatialData", "Unable to get Spatial data from the Points input."));
			continue;
		}

		const UPCGBasePointData* InPointData = InSpatialData->ToBasePointData(Context);
		if (!InPointData)
		{
			PCGE_LOG(Error, GraphAndLog,
				LOCTEXT("InputMissingPointData", "Unable to get Point data from the Points input."));
			continue;
		}

		const int32 NumSources = InPointData->GetNumPoints();
		const FPCGPointTransform::ConstValueRange SourceTransforms = InPointData->GetConstTransformValueRange();

		TArray<FTransform> AcceptedTransforms;
		TArray<int32> AcceptedParents;
		// Surface suitability, straight off the projection. Discarding it left every emitted point at
		// the FPCGPoint default of 1.0, which is what the debug display shades by.
		TArray<float> AcceptedDensities;

		// Reserve the dart budget only when it fits an int32; a budget that does not is already fatal
		// on memory grounds and the arrays would grow into the same wall either way.
		const int64 DartBudget = static_cast<int64>(NumSources) * static_cast<int64>(PointsPerSource);
		if (DartBudget > 0 && DartBudget <= static_cast<int64>(MAX_int32))
		{
			AcceptedTransforms.Reserve(static_cast<int32>(DartBudget));
			AcceptedParents.Reserve(static_cast<int32>(DartBudget));
			AcceptedDensities.Reserve(static_cast<int32>(DartBudget));
		}

		for (int32 SourceIndex = 0; SourceIndex < NumSources; ++SourceIndex)
		{
			const FVector SourceLocation = SourceTransforms[SourceIndex].GetLocation();

			// Query HEIGHT. Bounded surface: the hoisted bounds top, identical for every source.
			// Unbounded surface: THIS source's own Z — which is why it is read here and not hoisted. No
			// offset is added, for the reason spelled out at the bUseBoundsZ computation above: on the
			// unbounded surfaces that reach this branch the value is either discarded outright (ray hit)
			// or is an overlap position an offset would only push out of the volume (volumetric). Either
			// way it is a single Z handed to ProjectOntoSurface, whose signature is unchanged;
			// ArcPCGScatterCore knows nothing about this branch.
			//
			// This is NOT the same thing as the "no fallback Z" rule further down. That rule is about a
			// dart that MISSES: it is dropped, never emitted at a substitute height. This branch only
			// decides where the query STARTS. A miss from either height is still a miss.
			const double QueryZ = bUseBoundsZ ? BoundsQueryZ : SourceLocation.Z;

			// POSITION-derived, never SourceIndex: indices restart at 0 in every tagged data, so an
			// index-derived stream would replay identical offsets when the same points arrive split
			// across two data.
			//
			// Context->GetSeed() (the engine convention — PCGSurfaceSampler.cpp:56,
			// PCGTransformPoints.cpp:70) mixes the node's Seed with the execution source's seed, which
			// for a component is Component->Seed (PCGComponentExecutionState.cpp:18-22). That is still
			// PARTITION-STABLE: every partition-local component copies the value verbatim from the
			// original (PCGComponent.cpp:447 `Seed = Original->Seed;`), so a partitioned landscape gets
			// a bit-identical stream for the same source point in every partition — no seam. What it
			// buys over the raw Settings->Seed is that two actors running the same graph reroll
			// independently instead of scattering in lockstep.
			//
			// Truncation caveat: MakeStreamForPosition keys on ComputeSeedFromPosition, which truncates
			// to integer centimetres (PCGHelpers.cpp:50-53) — sources within the same 1 cm cell share a
			// stream and scatter coincidentally.
			FRandomStream Rng = ArcPCGScatterCore::MakeStreamForPosition(
				NodeSeed, ArcPCGScatterCore::ScatterNearPointsSalt, SourceLocation);

			for (int32 DartIndex = 0; DartIndex < PointsPerSource; ++DartIndex)
			{
				// Unconditional, exactly one call per dart: SampleAnnulus' 2-draw contract keeps the
				// stream aligned regardless of whether the dart below hits or misses.
				const FVector2D Offset = ArcPCGScatterCore::SampleAnnulus(Rng, MinRadiusCm, MaxRadiusCm);
				const FVector2D QueryXY(SourceLocation.X + Offset.X, SourceLocation.Y + Offset.Y);

				FPCGPoint ProjectedPoint;
				if (!ArcPCGScatterCore::ProjectOntoSurface(Surface, QueryXY, QueryZ, ProjectedPoint))
				{
					// Miss: DROPPED. No fallback Z — a floating point is worse than a missing one.
					continue;
				}

				AcceptedTransforms.Add(ProjectedPoint.Transform);
				AcceptedParents.Add(SourceIndex);
				AcceptedDensities.Add(ProjectedPoint.Density);
			}
		}

		// Generator emit path: NewPointData_AnyThread -> SetNumPoints -> AllocateProperties -> ranges
		// with bAllocate=false -> index-write. NEVER NewObject<UPCGPointData>. SetNumPoints and
		// AllocateProperties invalidate previously-fetched ranges (PCGBasePointData.h:134), so the
		// ranges are fetched only after both.
		const int32 NumAccepted = AcceptedTransforms.Num();
		UPCGBasePointData* OutPointData = FPCGContext::NewPointData_AnyThread(Context);
		OutPointData->SetNumPoints(NumAccepted);

		// MetadataEntry is allocated ONLY when something writes it. Allocating it unconditionally, and
		// then calling InitializeOnSet per point, mints a metadata key for every point even when the
		// parent-index attribute is switched off — pure cost for a value nobody reads.
		//
		// Density and Bounds are NOT optional. An unallocated property is not absent — it reads back as
		// one broadcast constant (FPCGPointArrayProperty::Value), i.e. the FPCGPoint defaults
		// Density = 1 and BoundsMin/Max = -1/+1 (PCGPoint.h:47-53). The debug display scales its boxes
		// off those bounds (FPCGDebugVisualizationSettings::ScaleMethod defaults to Extents,
		// PCGDebug.h:33) and shades them by Density (PCGSpatialDataVisualization.cpp:245), so leaving
		// them unallocated yields uniform 2 cm cubes — invisible on a landscape. Downstream spawners
		// and Radial Thinning read the same bounds.
		EPCGPointNativeProperties AllocatedProperties =
			EPCGPointNativeProperties::Transform | EPCGPointNativeProperties::Seed |
			EPCGPointNativeProperties::Density |
			EPCGPointNativeProperties::BoundsMin | EPCGPointNativeProperties::BoundsMax;
		if (bWriteParentIndex)
		{
			AllocatedProperties |= EPCGPointNativeProperties::MetadataEntry;
		}
		OutPointData->AllocateProperties(AllocatedProperties);

		if (NumAccepted > 0)
		{
			FPCGPointTransform::ValueRange TransformRange = OutPointData->GetTransformValueRange(/*bAllocate=*/false);
			FPCGPointSeed::ValueRange SeedRange = OutPointData->GetSeedValueRange(/*bAllocate=*/false);
			FPCGPointDensity::ValueRange DensityRange = OutPointData->GetDensityValueRange(/*bAllocate=*/false);
			FPCGPointBoundsMin::ValueRange BoundsMinRange = OutPointData->GetBoundsMinValueRange(/*bAllocate=*/false);
			FPCGPointBoundsMax::ValueRange BoundsMaxRange = OutPointData->GetBoundsMaxValueRange(/*bAllocate=*/false);

			const FVector PointExtents = Settings->PointExtents;

			for (int32 Index = 0; Index < NumAccepted; ++Index)
			{
				const FTransform& AcceptedTransform = AcceptedTransforms[Index];
				TransformRange[Index] = AcceptedTransform;
				SeedRange[Index] = PCGHelpers::ComputeSeedFromPosition(AcceptedTransform.GetLocation());
				DensityRange[Index] = AcceptedDensities[Index];
				BoundsMinRange[Index] = -PointExtents;
				BoundsMaxRange[Index] = PointExtents;
			}

			if (bWriteParentIndex)
			{
				FPCGMetadataDomain* Domain =
					OutPointData->MutableMetadata()->GetMetadataDomain(PCGMetadataDomainID::Elements);
				check(Domain);

				FPCGMetadataAttribute<int32>* ParentIndexAttr =
					Domain->FindOrCreateAttribute<int32>(Settings->ParentIndexAttributeName, INDEX_NONE);
				if (ParentIndexAttr)
				{
					FPCGPointMetadataEntry::ValueRange EntryRange = OutPointData->GetMetadataEntryValueRange(/*bAllocate=*/false);
					for (int32 Index = 0; Index < NumAccepted; ++Index)
					{
						Domain->InitializeOnSet(EntryRange[Index]);
						ParentIndexAttr->SetValue(EntryRange[Index], AcceptedParents[Index]);
					}
				}
			}
		}

		// Tags live on FPCGTaggedData, NOT on UPCGData — nothing else carries them, so an output entry
		// built from scratch silently loses every input tag. Copying the whole struct and overwriting
		// only .Data is the cheapest way to preserve every field, including ones this node does not
		// enumerate and would otherwise have to keep in sync as FPCGTaggedData grows.
		FPCGTaggedData& OutTagged = Context->OutputData.TaggedData.Add_GetRef(InTagged);
		OutTagged.Data = OutPointData;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
