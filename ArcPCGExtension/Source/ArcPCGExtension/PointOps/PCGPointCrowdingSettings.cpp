// Copyright Lukasz Baran. All Rights Reserved.

#include "PointOps/PCGPointCrowdingSettings.h"

#include "Scatter/ArcPCGScatterCore.h"

#include "PCGCommon.h"
#include "PCGContext.h"
#include "PCGData.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGSpatialData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataCommon.h"
#include "Metadata/PCGMetadataDomain.h"
#include "Utils/PCGLogErrors.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGPointCrowdingSettings)

#define LOCTEXT_NAMESPACE "PCGPointCrowding"

namespace PCGPointCrowdingConstants
{
	const FName PointsLabel = TEXT("Points");
}

FPCGElementPtr UPCGPointCrowdingSettings::CreateElement() const
{
	return MakeShared<FPCGPointCrowdingElement>();
}

#if WITH_EDITOR
FText UPCGPointCrowdingSettings::GetNodeTooltipText() const
{
	// This overrides the ToolTip metadata UHT bakes from the class comment, so it is what the graph
	// editor actually shows — it must not contradict that comment. Keep the two in step.
	return LOCTEXT("NodeTooltip",
		"Writes a per-point crowding score onto the input points, which are otherwise passed through "
		"unchanged: the falloff-weighted count of each point's neighbours within Search Radius Cm. In "
		"Constant mode the value is the exact integer neighbour count; Linear and Gaussian give a smooth "
		"density proxy. Neighbour queries NEVER cross data boundaries — each input data is measured "
		"against itself alone and produces one output data, so union your point sets upstream (Merge or "
		"Gather) if you want them to crowd each other. The score is written BOTH into the points' Density "
		"(combined with the existing density under Density Mode, and clamped to 0-1 — this is what the "
		"debug display shades by) AND into the named float attribute. Normalize rescales each input data "
		"independently so its busiest point reads 1; it is ON by default because the raw score is an "
		"unbounded neighbour count while Density is 0-1, so without it Set mode would clamp nearly "
		"everything to 1.");
}
#endif

TArray<FPCGPinProperties> UPCGPointCrowdingSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	// Multiple connections and multiple data are allowed, but each data is still measured ALONE — see the
	// pin tooltip. Emplace_GetRef, not Emplace: TArray::Emplace returns an INDEX, not a reference.
	FPCGPinProperties& PointsPin = PinProperties.Emplace_GetRef(
		PCGPointCrowdingConstants::PointsLabel,
		EPCGDataType::Point,
		/*bAllowMultipleConnections=*/true,
		/*bAllowMultipleData=*/true,
		LOCTEXT("PointsPinTooltip", "Points to measure. Each input data is measured against itself only and emitted as one output data carrying that data's tags; neighbours never cross a data boundary."));
	PointsPin.SetRequiredPin();

	return PinProperties;
}

// ---------------------------------------------------------------------------
// Element execution
// ---------------------------------------------------------------------------

bool FPCGPointCrowdingElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGPointCrowdingElement::Execute);
	check(Context);

	const UPCGPointCrowdingSettings* Settings = Context->GetInputSettings<UPCGPointCrowdingSettings>();
	check(Settings);

	// Negative radii would hand FBoxCenterAndExtent a negative extent; clamped to zero, which the kernel
	// degenerates to a Dirac at d == 0 (coincident points only).
	const double SearchRadiusCm = FMath::Max(Settings->SearchRadiusCm, 0.0);
	const EArcPCGFalloff Falloff = Settings->Falloff;
	const bool bIncludeSelf = Settings->bIncludeSelf;
	const bool bNormalize = Settings->bNormalize;
	const EArcPCGDensityMode DensityMode = Settings->DensityMode;

	const bool bWriteAttribute = !Settings->OutputAttributeName.IsNone();
	if (!bWriteAttribute)
	{
		// The density write below still happens — clearing the name only drops the named attribute, it does
		// not turn the node into a pure pass-through. Say which of the two outputs survived.
		PCGLog::LogWarningOnGraph(
			LOCTEXT("OutputNameNone", "Output Attribute Name is empty, so no crowding attribute is written; the crowding score is still written into the points' Density under Density Mode."),
			Context);
	}

	// One output data PER input data, each carrying that input's tags.
	const TArray<FPCGTaggedData> Inputs =
		Context->InputData.GetInputsByPin(PCGPointCrowdingConstants::PointsLabel);

	for (const FPCGTaggedData& InTagged : Inputs)
	{
		// COLLAPSE, then ERROR — never drop silently. Same shape as the engine's own point ops
		// (PCGTransformPoints.cpp:80-92, PCGSelectPoints.cpp:63-82) and both scatter siblings.
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

		const int32 NumPoints = InPointData->GetNumPoints();

		// Every point survives — this node decorates, it never filters.
		TArray<int32> AllIndices;
		AllIndices.SetNumUninitialized(NumPoints);
		for (int32 Index = 0; Index < NumPoints; ++Index)
		{
			AllIndices[Index] = Index;
		}

		// PassThroughPoints is the sole authority for the copy sequence (NewPointData_AnyThread ->
		// InitializeFromDataWithParams with bInheritSpatialData off -> SetPointsFrom -> whole-FPCGTaggedData
		// copy). It also appends the output entry, so nothing is added to OutputData here.
		UPCGBasePointData* OutPointData =
			ArcPCGScatterCore::PassThroughPoints(Context, InTagged, InPointData, AllIndices);
		if (!OutPointData)
		{
			PCGE_LOG(Error, GraphAndLog,
				LOCTEXT("PassThroughFailed", "Failed to allocate the output point data for a Points input."));
			continue;
		}

		// NOT gated on bWriteAttribute any more: the density write is unconditional, so the score has to be
		// computed even when the attribute name was cleared.
		if (NumPoints == 0)
		{
			continue;
		}

		// READ-ONLY traversal of an input this node does not modify. The octree is unsafe for concurrent
		// MUTATION (PCGBasePointData.h:454); priming it here builds it once up front instead of racing to
		// build it lazily inside the loop.
		InPointData->PrimePointOctree();

		const FPCGPointTransform::ConstValueRange SourceTransforms = InPointData->GetConstTransformValueRange();

		TArray<float> Crowding;
		Crowding.SetNumZeroed(NumPoints);

		for (int32 SelfIndex = 0; SelfIndex < NumPoints; ++SelfIndex)
		{
			const FVector Centre = SourceTransforms[SelfIndex].GetLocation();

			float Sum = 0.0f;
			// ArcPCGScatterCore's query, not UPCGOctreeQueries': the engine's is a PCG_API TEMPLATE defined
			// in its own header, so outside the PCG module that definition is dllimport-illegal (MSVC
			// C2491). The core's is the same octree query — see ArcPCGScatterCore.h.
			ArcPCGScatterCore::ForEachPointInsideSphere(InPointData, Centre, SearchRadiusCm,
				[&Sum, SelfIndex, bIncludeSelf, Falloff, SearchRadiusCm](int32 NeighbourIndex, double SquaredDistance) -> void
				{
					// The query is over the SAME data, so index identity is sufficient here (unlike the
					// two-pin Shade node, which must also compare data pointers).
					if (NeighbourIndex == SelfIndex && !bIncludeSelf)
					{
						return;
					}

					Sum += ArcPCGScatterCore::EvaluateFalloff(
						Falloff, FMath::Sqrt(FMath::Max(SquaredDistance, 0.0)), SearchRadiusCm);
				});

			Crowding[SelfIndex] = Sum;
		}

		if (bNormalize)
		{
			// SECOND PASS over THIS data only: the maximum is per input data, deliberately not global.
			float MaxCrowding = 0.0f;
			for (int32 Index = 0; Index < NumPoints; ++Index)
			{
				MaxCrowding = FMath::Max(MaxCrowding, Crowding[Index]);
			}

			// Guard the all-zero case (no point has a neighbour): dividing would produce NaNs, so the
			// values are left as the zeros they already are.
			if (MaxCrowding > UE_SMALL_NUMBER)
			{
				const float InvMax = 1.0f / MaxCrowding;
				for (int32 Index = 0; Index < NumPoints; ++Index)
				{
					Crowding[Index] *= InvMax;
				}
			}
		}

		// DENSITY WRITE. PCG's convention for a per-point scalar, and the only channel the debug display
		// shades by (PCGSpatialDataVisualization.cpp:245) — without it these points render unshaded.
		//
		// bAllocate = TRUE is load-bearing: PassThroughPoints copies the INPUT's native ranges, and the
		// input's Density may be UNALLOCATED, i.e. one broadcast constant rather than a per-point array.
		// Writing through a non-allocating range would then either write into a shared single value or
		// nothing at all. Fetched AFTER the pass-through and never cached across it — SetNumPoints and
		// AllocateProperties invalidate ranges (PCGBasePointData.h:129-138).
		{
			FPCGPointDensity::ValueRange DensityRange = OutPointData->GetDensityValueRange(/*bAllocate=*/true);
			for (int32 Index = 0; Index < NumPoints; ++Index)
			{
				DensityRange[Index] = ArcPCGScatterCore::ApplyDensityMode(
					DensityMode, DensityRange[Index], Crowding[Index]);
			}
		}
		// DensityRange is deliberately scoped out above: the attribute path below can allocate the
		// MetadataEntry array, which invalidates it.

		if (!bWriteAttribute)
		{
			continue;
		}

		// Attribute write, same shape as the scatter siblings'. bOverrideParent stays at its default true:
		// an input that already carried an attribute of this name must be SHADOWED by the values computed
		// here, not silently left showing the inherited ones.
		FPCGMetadataDomain* Domain =
			OutPointData->MutableMetadata()->GetMetadataDomain(PCGMetadataDomainID::Elements);
		check(Domain);

		FPCGMetadataAttribute<float>* CrowdingAttr =
			Domain->FindOrCreateAttribute<float>(Settings->OutputAttributeName, 0.0f);
		if (!CrowdingAttr)
		{
			PCGE_LOG(Error, GraphAndLog,
				LOCTEXT("AttributeCreationFailed", "Failed to create the crowding attribute; check the Output Attribute Name is a valid attribute name."));
			continue;
		}

		// bAllocate = TRUE, unlike the generator path: PassThroughPoints only carries a MetadataEntry array
		// over when the INPUT had one allocated, and a fresh point set from a generator does not.
		FPCGPointMetadataEntry::ValueRange EntryRange = OutPointData->GetMetadataEntryValueRange(/*bAllocate=*/true);
		for (int32 Index = 0; Index < NumPoints; ++Index)
		{
			// InitializeOnSet turns an invalid key into a fresh entry and a PARENT-referential key into a
			// child of it (PCGMetadataDomain.cpp:1244-1273), so the inherited attribute values copied by
			// SetPointsFrom survive the write below.
			Domain->InitializeOnSet(EntryRange[Index]);
			CrowdingAttr->SetValue(EntryRange[Index], Crowding[Index]);
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
