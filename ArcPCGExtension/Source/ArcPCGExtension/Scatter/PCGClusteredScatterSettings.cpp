// Copyright Lukasz Baran. All Rights Reserved.

#include "Scatter/PCGClusteredScatterSettings.h"

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

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGClusteredScatterSettings)

#define LOCTEXT_NAMESPACE "PCGClusteredScatter"

namespace PCGClusteredScatterConstants
{
	const FName SurfaceLabel = TEXT("Surface");
}

FPCGElementPtr UPCGClusteredScatterSettings::CreateElement() const
{
	return MakeShared<FPCGClusteredScatterElement>();
}

#if WITH_EDITOR
FText UPCGClusteredScatterSettings::GetNodeTooltipText() const
{
	// This overrides the ToolTip metadata UHT bakes from the class comment, so it is what the graph
	// editor actually shows — it must not contradict that comment. Keep the two in step.
	return LOCTEXT("NodeTooltip",
		"Generates a clustered (patchy) point cloud over the Surface input: patch centres are placed "
		"uniformly at random across the surface's XY bounds at Parent Density Per M2, and each patch "
		"spawns Points Per Parent children at Gaussian offsets of Patch Radius Sigma Cm, projected down "
		"onto the surface. Use it for patchy undergrowth. Offspring that miss the surface are dropped, "
		"never emitted at a fallback height. The Surface must report valid bounds: unlike Scatter Near "
		"Points, this node has no input points, so an unbounded surface leaves it with neither an XY "
		"region to scatter within nor a height to query from, and it emits nothing. Max Points binds "
		"before generation by reducing the patch count, dropping whole patches rather than half of one.");
}
#endif

TArray<FPCGPinProperties> UPCGClusteredScatterSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	// Spatial, NOT Surface: EPCGDataType::Surface is Landscape|BaseTexture|VirtualTexture and would
	// REJECT the volumes/primitives/dynamic meshes users will plug in here. This matches
	// UPCGSurfaceSamplerSettings' own Bounding Shape pin, and its sibling Scatter Near Points. Single
	// connection/single data because two surfaces are ambiguous — union them upstream.
	// Emplace_GetRef, not Emplace: TArray::Emplace returns an INDEX, not a reference.
	FPCGPinProperties& SurfacePin = PinProperties.Emplace_GetRef(
		PCGClusteredScatterConstants::SurfaceLabel,
		EPCGDataType::Spatial,
		/*bAllowMultipleConnections=*/false,
		/*bAllowMultipleData=*/false,
		LOCTEXT("SurfacePinTooltip", "Surface the clustered points are projected onto. Its XY bounds are also the region patches are placed in, so it must be bounded."));
	SurfacePin.SetRequiredPin();

	return PinProperties;
}

// ---------------------------------------------------------------------------
// Helpers — file-scoped namespace (unique to this .cpp)
// ---------------------------------------------------------------------------

namespace PCGClusteredScatterSettingsLocal
{
	/** Resolve the first spatial input on the Surface pin (mirrors the sim's GetSurfaceInput). */
	const UPCGSpatialData* GetSurfaceInput(FPCGContext* Context)
	{
		const TArray<FPCGTaggedData> Inputs =
			Context->InputData.GetSpatialInputsByPin(PCGClusteredScatterConstants::SurfaceLabel);
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

bool FPCGClusteredScatterElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGClusteredScatterElement::Execute);
	check(Context);

	const UPCGClusteredScatterSettings* Settings = Context->GetInputSettings<UPCGClusteredScatterSettings>();
	check(Settings);

	// Single pass: every path below returns true. There is no state to carry across frames.
	const UPCGSpatialData* Surface = PCGClusteredScatterSettingsLocal::GetSurfaceInput(Context);
	if (!Surface)
	{
		UE_LOGF(LogArcPCGForest, Warning,"ClusteredScatter: no spatial data on the Surface pin — emitting nothing.");
		PCGLog::LogWarningOnGraph(
			LOCTEXT("NoSurface", "No spatial data on the Surface pin. Nothing to project onto, so nothing is emitted."),
			Context);
		return true;
	}

	// HARD REFUSAL on invalid bounds, and deliberately NOT the per-source fallback its sibling Scatter
	// Near Points takes. That node keeps its source points on an unbounded surface and only has to pick a
	// query HEIGHT (each source's own Z). This node derives BOTH the height and the entire XY scatter
	// REGION from these bounds: without them there is no extent to place patches within and no input
	// points to key a height from, so there is genuinely nothing left to generate. Emitting patches over
	// a zero-extent default FBox would put every point on top of the origin.
	//
	// FBox::IsValid is a uint8 DATA MEMBER (Box.h:45), not a method — compare it explicitly.
	const FBox SurfaceBounds = Surface->GetBounds();
	if (SurfaceBounds.IsValid == 0)
	{
		UE_LOGF(LogArcPCGForest, Warning,"ClusteredScatter: the Surface input reports invalid bounds — there is no region to scatter in, emitting nothing.");
		PCGLog::LogWarningOnGraph(
			LOCTEXT("InvalidSurfaceBounds", "The Surface input reports no bounds (an unbounded surface, e.g. World Ray Hit Query), so this node has neither an XY region to place patches in nor a height to query from, and emits nothing. Unlike Scatter Near Points it has no source points to fall back on. Bound the surface upstream — intersect it with a volume, or use a bounded surface."),
			Context);
		return true;
	}

	const double SizeX = SurfaceBounds.Max.X - SurfaceBounds.Min.X;
	const double SizeY = SurfaceBounds.Max.Y - SurfaceBounds.Min.Y;
	// PCG works in centimetres; the density is authored per square METRE, so 1 m^2 = 10 000 cm^2.
	const double AreaM2 = FMath::Max(SizeX, 0.0) * FMath::Max(SizeY, 0.0) / 10000.0;

	// Every offspring drops from the top of the surface's bounds, the same vertical-query convention the
	// sim and Scatter Near Points use. Read once: the surface does not change during the pass.
	const double QueryZ = SurfaceBounds.Max.Z;

	const double PatchRadiusSigmaCm = Settings->PatchRadiusSigmaCm;
	if (!(PatchRadiusSigmaCm > UE_KINDA_SMALL_NUMBER))
	{
		// A degenerate sigma collapses every child onto its parent, i.e. PointsPerParent coincident
		// points per patch. That is never what is wanted, and silently emitting it looks like the node
		// ignoring the count, so refuse it explicitly.
		UE_LOGF(LogArcPCGForest, Warning,"ClusteredScatter: PatchRadiusSigmaCm (%.4f) is not above zero — emitting nothing.", PatchRadiusSigmaCm);
		PCGLog::LogWarningOnGraph(
			LOCTEXT("DegenerateSigma", "Patch Radius Sigma Cm must be above zero; at zero every point in a patch would land on its patch centre. Nothing is emitted."),
			Context);
		return true;
	}

	const int32 PointsPerParent = FMath::Max(Settings->PointsPerParent, 0);
	if (PointsPerParent == 0)
	{
		PCGLog::LogWarningOnGraph(
			LOCTEXT("NoPointsPerParent", "Points Per Parent is 0, so no points can be generated."),
			Context);
		return true;
	}

	// Parent count = round(density * area), the deterministic candidate count of
	// FArcForestPatchField::Build (ArcForestPatchField.cpp:79-80) — WITHOUT its suitability/habitat
	// thinning, which needs a field this standalone node does not have. Computed in double and clamped
	// before the narrowing cast: a large landscape times a user-authored density can exceed int32.
	const double ParentCountReal =
		FMath::RoundToDouble(FMath::Max(Settings->ParentDensityPerM2, 0.0) * AreaM2);
	int32 NumParents = static_cast<int32>(
		FMath::Clamp(ParentCountReal, 0.0, static_cast<double>(MAX_int32)));

	if (NumParents <= 0)
	{
		UE_LOGF(LogArcPCGForest, Warning,"ClusteredScatter: density %.6f over %.2f m^2 rounds to zero patches — emitting nothing.",
			Settings->ParentDensityPerM2, AreaM2);
		PCGLog::LogWarningOnGraph(
			LOCTEXT("NoParents", "Parent Density Per M2 over the Surface's area rounds to zero patches, so nothing is emitted. Raise the density, or use a larger surface."),
			Context);
		return true;
	}

	// The MaxPoints budget binds HERE, BEFORE anything is generated — not by truncating the result
	// afterwards, which would pay the full generation cost of a run that is then thrown away, and would
	// also cut one patch in half (a visibly clipped clump). Reducing the parent count instead drops WHOLE
	// patches. Integer division floors, which is what makes the product fit the budget.
	const int32 MaxPoints = FMath::Max(Settings->MaxPoints, 0);
	const int64 UnclampedTotal = static_cast<int64>(NumParents) * static_cast<int64>(PointsPerParent);
	if (UnclampedTotal > static_cast<int64>(MaxPoints))
	{
		const int32 ClampedParents = MaxPoints / PointsPerParent;
		UE_LOGF(LogArcPCGForest, Warning,"ClusteredScatter: %lld points requested exceeds MaxPoints (%d) — reducing patches from %d to %d.",
			UnclampedTotal, MaxPoints, NumParents, ClampedParents);
		PCGLog::LogWarningOnGraph(
			LOCTEXT("MaxPointsBound", "The requested point count exceeds Max Points; the patch count has been reduced to fit the budget before generating. Lower Parent Density Per M2 or Points Per Parent, or raise Max Points."),
			Context);
		NumParents = ClampedParents;

		if (NumParents <= 0)
		{
			// MaxPoints is smaller than a single patch. Already warned above; nothing to emit.
			return true;
		}
	}

	// Node seed, resolved ONCE. Context->GetSeed() (the engine convention — PCGSurfaceSampler.cpp:56,
	// PCGTransformPoints.cpp:70), never Settings->Seed: it mixes the node's Seed with the execution
	// source's seed, so two actors running the same graph reroll independently, while staying
	// partition-stable because a partition-local component copies Seed verbatim from the original
	// (PCGComponent.cpp:447).
	const int32 NodeSeed = Context->GetSeed();

	// STAGE 1 — parents. ONE serial stream for the whole parent process, keyed at index 0: parent
	// placement is inherently sequential and there is no point input, so the chunk-invariance problem
	// that forces its sibling onto MakeStreamForPosition cannot arise. Two draws per parent, in the
	// order (X, Y), so the sequence is stable under any change to the offspring stage.
	FRandomStream ParentRng = ArcPCGScatterCore::MakeStreamForIndex(
		NodeSeed, ArcPCGScatterCore::ClusteredScatterParentSalt, 0);

	TArray<FVector2D> ParentPositions;
	ParentPositions.Reserve(NumParents);
	for (int32 ParentIndex = 0; ParentIndex < NumParents; ++ParentIndex)
	{
		const double ParentX = ParentRng.FRandRange(SurfaceBounds.Min.X, SurfaceBounds.Max.X);
		const double ParentY = ParentRng.FRandRange(SurfaceBounds.Min.Y, SurfaceBounds.Max.Y);
		ParentPositions.Emplace(ParentX, ParentY);
	}

	// STAGE 2 — offspring. The clamped budget bounds this by construction, so the reserve is int32-safe.
	const int32 PointBudget = NumParents * PointsPerParent;
	TArray<FTransform> AcceptedTransforms;
	TArray<int32> AcceptedPatchIds;
	// Surface suitability, straight off the projection. Discarding it left every emitted point at the
	// FPCGPoint default of 1.0, which is what the debug display shades by.
	TArray<float> AcceptedDensities;
	AcceptedTransforms.Reserve(PointBudget);
	AcceptedPatchIds.Reserve(PointBudget);
	AcceptedDensities.Reserve(PointBudget);

	for (int32 ParentIndex = 0; ParentIndex < NumParents; ++ParentIndex)
	{
		const FVector2D& ParentPosition = ParentPositions[ParentIndex];

		// One stream PER PARENT, keyed on the parent index: a patch's children then do not depend on how
		// many patches precede it, so changing the density does not reshuffle every surviving patch.
		FRandomStream Rng = ArcPCGScatterCore::MakeStreamForIndex(
			NodeSeed, ArcPCGScatterCore::ClusteredScatterOffspringSalt, ParentIndex);

		for (int32 ChildIndex = 0; ChildIndex < PointsPerParent; ++ChildIndex)
		{
			// Unconditional, exactly one call per child: SampleGaussianOffset's 2-draw contract keeps the
			// stream aligned regardless of whether the projection below hits or misses.
			const FVector2D Offset = ArcPCGScatterCore::SampleGaussianOffset(Rng, PatchRadiusSigmaCm);

			// CLAMPED back into the region, never redrawn — the sim's own rule
			// (ArcForestSimulator.cpp:318-322). A redraw would make the draw count per child variable and
			// desynchronise the stream; clamping keeps it fixed. The visible cost is a slight
			// accumulation of points against the boundary for patches centred near the edge.
			const FVector2D QueryXY(
				FMath::Clamp(ParentPosition.X + Offset.X, SurfaceBounds.Min.X, SurfaceBounds.Max.X),
				FMath::Clamp(ParentPosition.Y + Offset.Y, SurfaceBounds.Min.Y, SurfaceBounds.Max.Y));

			FPCGPoint ProjectedPoint;
			if (!ArcPCGScatterCore::ProjectOntoSurface(Surface, QueryXY, QueryZ, ProjectedPoint))
			{
				// Miss: DROPPED. No fallback Z — a floating point is worse than a missing one.
				continue;
			}

			AcceptedTransforms.Add(ProjectedPoint.Transform);
			AcceptedPatchIds.Add(ParentIndex);
			AcceptedDensities.Add(ProjectedPoint.Density);
		}
	}

	const bool bWritePatchId =
		Settings->bWritePatchIdAttribute && !Settings->PatchIdAttributeName.IsNone();
	if (Settings->bWritePatchIdAttribute && Settings->PatchIdAttributeName.IsNone())
	{
		// The checkbox is on but the name was cleared. Silently writing nothing would look like the node
		// ignoring the checkbox, so say which of the two settings actually won.
		PCGLog::LogWarningOnGraph(
			LOCTEXT("PatchIdNameNone", "Write Patch Id Attribute is enabled but Patch Id Attribute Name is empty; no patch-id attribute will be written."),
			Context);
	}

	// Generator emit path, identical in shape to Scatter Near Points': NewPointData_AnyThread ->
	// SetNumPoints -> AllocateProperties -> ranges with bAllocate=false -> index-write. NEVER
	// NewObject<UPCGPointData>. SetNumPoints and AllocateProperties invalidate previously-fetched ranges
	// (PCGBasePointData.h:134), so the ranges are fetched only after both.
	const int32 NumAccepted = AcceptedTransforms.Num();
	UPCGBasePointData* OutPointData = FPCGContext::NewPointData_AnyThread(Context);
	OutPointData->SetNumPoints(NumAccepted);

	// MetadataEntry is allocated ONLY when something writes it. Allocating it unconditionally, and then
	// calling InitializeOnSet per point, mints a metadata key for every point even when the patch-id
	// attribute is switched off — pure cost for a value nobody reads.
	//
	// Density and Bounds are NOT optional. An unallocated property is not absent — it reads back as one
	// broadcast constant (FPCGPointArrayProperty::Value), i.e. the FPCGPoint defaults Density = 1 and
	// BoundsMin/Max = -1/+1 (PCGPoint.h:47-53). The debug display sizes its boxes off those bounds
	// (ScaleMethod defaults to Extents, PCGDebug.h:33) and shades them by Density
	// (PCGSpatialDataVisualization.cpp:245), so leaving them unallocated yields uniform 2 cm cubes —
	// invisible on a landscape. Downstream spawners and Arc Radial Thinning read the same bounds.
	EPCGPointNativeProperties AllocatedProperties =
		EPCGPointNativeProperties::Transform | EPCGPointNativeProperties::Seed |
		EPCGPointNativeProperties::Density |
		EPCGPointNativeProperties::BoundsMin | EPCGPointNativeProperties::BoundsMax;
	if (bWritePatchId)
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

		if (bWritePatchId)
		{
			FPCGMetadataDomain* Domain =
				OutPointData->MutableMetadata()->GetMetadataDomain(PCGMetadataDomainID::Elements);
			check(Domain);

			FPCGMetadataAttribute<int32>* PatchIdAttr =
				Domain->FindOrCreateAttribute<int32>(Settings->PatchIdAttributeName, INDEX_NONE);
			if (PatchIdAttr)
			{
				FPCGPointMetadataEntry::ValueRange EntryRange = OutPointData->GetMetadataEntryValueRange(/*bAllocate=*/false);
				for (int32 Index = 0; Index < NumAccepted; ++Index)
				{
					Domain->InitializeOnSet(EntryRange[Index]);
					PatchIdAttr->SetValue(EntryRange[Index], AcceptedPatchIds[Index]);
				}
			}
		}
	}

	// A generator has no input tagged data to carry tags from — the Surface is what is sampled, not what
	// is transformed — so the output entry is fresh, exactly as the engine's own generators build theirs
	// (PCGCreatePointsSphere.cpp:465-469). The pin label is left at NAME_None and rewritten by
	// PCGElement.cpp:1209-1218 because this node declares a single output pin.
	FPCGTaggedData& OutTagged = Context->OutputData.TaggedData.Emplace_GetRef();
	OutTagged.Data = OutPointData;

	return true;
}

#undef LOCTEXT_NAMESPACE
