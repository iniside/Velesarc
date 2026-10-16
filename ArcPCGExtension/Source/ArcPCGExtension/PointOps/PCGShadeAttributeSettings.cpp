// Copyright Lukasz Baran. All Rights Reserved.

#include "PointOps/PCGShadeAttributeSettings.h"

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
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Utils/PCGLogErrors.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGShadeAttributeSettings)

#define LOCTEXT_NAMESPACE "PCGShadeAttribute"

namespace PCGShadeAttributeConstants
{
	const FName PointsLabel = TEXT("Points");
	const FName OccludersLabel = TEXT("Occluders");
}

FPCGElementPtr UPCGShadeAttributeSettings::CreateElement() const
{
	return MakeShared<FPCGShadeAttributeElement>();
}

#if WITH_EDITOR
FText UPCGShadeAttributeSettings::GetNodeTooltipText() const
{
	// This overrides the ToolTip metadata UHT bakes from the class comment, so it is what the graph
	// editor actually shows — it must not contradict that comment. Keep the two in step.
	return LOCTEXT("NodeTooltip",
		"Writes a per-point transmittance attribute onto the Points input from the canopy on the Occluders "
		"input: Transmittance = Exp(-Extinction K * SUM over occluders of Density * Falloff(distance, that "
		"occluder's own radius)). THE VALUE IS TRANSMITTED LIGHT, NOT OCCLUSION — it lies in (0, 1] where 1 "
		"is full sun and values near 0 are deep shade, so filtering for shade means filtering BELOW a "
		"threshold. Radius and density come from the constants unless you tick Use Occluder Radius / "
		"Density Attribute, which then reads them per occluder so a big tree shades further and harder "
			"than a shrub; if a ticked selector does not resolve, the matching constant is used and one "
		"warning is raised. An UNCONNECTED Occluders pin means no occluders at all, so every target reads "
		"exactly 1 — wire the same points into both pins if you want self-shading. Target points never "
		"cross a data boundary, but every occluder data shades every target data. The transmittance is "
		"written BOTH into the points' Density (combined with the existing density under Density Mode, and "
		"clamped to 0-1 — this is what the debug display shades by) AND into the named float attribute.");
}
#endif

TArray<FPCGPinProperties> UPCGShadeAttributeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	// Emplace_GetRef, not Emplace: TArray::Emplace returns an INDEX, not a reference.
	FPCGPinProperties& PointsPin = PinProperties.Emplace_GetRef(
		PCGShadeAttributeConstants::PointsLabel,
		EPCGDataType::Point,
		/*bAllowMultipleConnections=*/true,
		/*bAllowMultipleData=*/true,
		LOCTEXT("PointsPinTooltip", "Points that receive the shade. They are passed through unchanged apart from the transmittance attribute; one output data is emitted per input data, carrying that data's tags."));
	PointsPin.SetRequiredPin();

	// DELIBERATELY NOT REQUIRED: an unconnected Occluders pin is the meaningful "no canopy" case and must
	// run, yielding exactly 1.0 everywhere. Marking it required would make the identity unreachable — which
	// is exactly why this one is Emplace and not Emplace_GetRef: there is no SetRequiredPin() to follow.
	PinProperties.Emplace(
		PCGShadeAttributeConstants::OccludersLabel,
		EPCGDataType::Point,
		/*bAllowMultipleConnections=*/true,
		/*bAllowMultipleData=*/true,
		LOCTEXT("OccludersPinTooltip", "Points that cast the shade. Every occluder data shades every target data. Leave it unconnected for no occluders at all, which gives every target a transmittance of exactly 1."));

	return PinProperties;
}

// ---------------------------------------------------------------------------
// Helpers — file-scoped namespace (unique to this .cpp)
// ---------------------------------------------------------------------------

namespace PCGShadeAttributeSettingsLocal
{
	/** One resolved occluder data: its points plus a per-point radius and density, already fallen back to
	 *  the constants where a selector did not resolve. */
	struct FResolvedOccluders
	{
		const UPCGBasePointData* PointData = nullptr;
		/**
		 * The PRE-CONVERSION UPCGData this was collapsed from, and the only stable identity available
		 * for self-exclusion. PointData is NOT usable for that: ToBasePointData allocates a fresh
		 * UPCGPointArrayData wrapper on every call for a plain UPCGPointData (UPCGSpatialData::
		 * ToPointArrayData, PCGSpatialData.cpp:114-133 — caching exists only on
		 * UPCGSpatialDataWithPointCache subclasses), so the same input wired into both pins yields two
		 * different pointers and a pointer test on PointData can never fire.
		 */
		const UPCGData* SourceData = nullptr;
		TArray<double> Radii;
		TArray<double> Densities;
	};

	/**
	 * Reads one per-point numeric attribute off PointData through InSelector.
	 * Returns false — leaving OutValues untouched — when the selector does not resolve, resolves to a
	 * non-numeric attribute, or does not yield exactly ExpectedNum values (e.g. a Data-domain attribute,
	 * which yields one). The caller then falls back to its constant.
	 */
	bool TryReadPerPointValues(const UPCGBasePointData* PointData, const FPCGAttributePropertyInputSelector& InSelector,
		int32 ExpectedNum, TArray<double>& OutValues)
	{
		if (!PointData || ExpectedNum <= 0)
		{
			return false;
		}

		// CopyAndFixLast FIRST, BEFORE the accessor is created (PCGAttributePropertySelector.h:242).
		// A @Last-style selector otherwise resolves against the wrong attribute — the accessor would be
		// built for the literal name "@Last" rather than for the attribute it stands for.
		const FPCGAttributePropertyInputSelector Selector = InSelector.CopyAndFixLast(PointData);

		// bQuiet: a selector that does not resolve is a SUPPORTED state here (it selects the constant), so
		// the helper's own error must be suppressed in favour of this node's single warning.
		const TUniquePtr<const IPCGAttributeAccessor> Accessor =
			PCGAttributeAccessorHelpers::CreateConstAccessor(PointData, Selector, /*bQuiet=*/true);
		const TUniquePtr<const IPCGAttributeAccessorKeys> Keys =
			PCGAttributeAccessorHelpers::CreateConstKeys(PointData, Selector);

		if (!Accessor.IsValid() || !Keys.IsValid() || Keys->GetNum() != ExpectedNum)
		{
			return false;
		}

		TArray<double> Values;
		// AllowBroadcastAndConstructible is what lets an int32 or float attribute feed a double read; a
		// non-numeric attribute simply fails here and takes the constant.
		if (!PCGAttributeAccessorHelpers::ExtractAllValues<double>(Accessor.Get(), Keys.Get(), Values,
			EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible))
		{
			return false;
		}

		if (Values.Num() != ExpectedNum)
		{
			return false;
		}

		OutValues = MoveTemp(Values);
		return true;
	}
}

// ---------------------------------------------------------------------------
// Element execution
// ---------------------------------------------------------------------------

bool FPCGShadeAttributeElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGShadeAttributeElement::Execute);
	check(Context);

	const UPCGShadeAttributeSettings* Settings = Context->GetInputSettings<UPCGShadeAttributeSettings>();
	check(Settings);

	const EArcPCGFalloff Falloff = Settings->Falloff;
	const bool bIncludeSelf = Settings->bIncludeSelf;
	const double ExtinctionK = FMath::Max(static_cast<double>(Settings->ExtinctionK), 0.0);
	const double RadiusConstantCm = FMath::Max(Settings->OccluderRadiusConstantCm, 0.0);
	const double DensityConstant = FMath::Max(static_cast<double>(Settings->OccluderDensityConstant), 0.0);
	const bool bUseRadiusAttribute = Settings->bUseOccluderRadiusAttribute;
	const bool bUseDensityAttribute = Settings->bUseOccluderDensityAttribute;
	const EArcPCGDensityMode DensityMode = Settings->DensityMode;

	const bool bWriteAttribute = !Settings->OutputAttributeName.IsNone();
	if (!bWriteAttribute)
	{
		// The density write below still happens — clearing the name only drops the named attribute, it does
		// not turn the node into a pure pass-through. Say which of the two outputs survived.
		PCGLog::LogWarningOnGraph(
			LOCTEXT("OutputNameNone", "Output Attribute Name is empty, so no transmittance attribute is written; the transmittance is still written into the points' Density under Density Mode."),
			Context);
	}

	// ------------------------------------------------------------------
	// Resolve the occluder set ONCE. It does not depend on the target data, and an unconnected pin
	// legitimately leaves it empty — Sum is then 0 and every target gets Exp(0) == 1 exactly.
	// ------------------------------------------------------------------
	const TArray<FPCGTaggedData> OccluderInputs =
		Context->InputData.GetInputsByPin(PCGShadeAttributeConstants::OccludersLabel);

	TArray<PCGShadeAttributeSettingsLocal::FResolvedOccluders> Occluders;
	Occluders.Reserve(OccluderInputs.Num());

	// ONE warning each, raised after the whole occluder pass — not once per data, which on a multi-data
	// canopy would bury the graph in identical messages.
	bool bRadiusFellBackToConstant = false;
	bool bDensityFellBackToConstant = false;

	for (const FPCGTaggedData& OccluderTagged : OccluderInputs)
	{
		// COLLAPSE, then ERROR — never drop silently. Same shape as the engine's own point ops and the
		// sibling nodes.
		const UPCGSpatialData* OccluderSpatialData = Cast<UPCGSpatialData>(OccluderTagged.Data);
		if (!OccluderSpatialData)
		{
			PCGE_LOG(Error, GraphAndLog,
				LOCTEXT("OccluderMissingSpatialData", "Unable to get Spatial data from the Occluders input."));
			continue;
		}

		const UPCGBasePointData* OccluderPointData = OccluderSpatialData->ToBasePointData(Context);
		if (!OccluderPointData)
		{
			PCGE_LOG(Error, GraphAndLog,
				LOCTEXT("OccluderMissingPointData", "Unable to get Point data from the Occluders input."));
			continue;
		}

		const int32 NumOccluderPoints = OccluderPointData->GetNumPoints();
		if (NumOccluderPoints == 0)
		{
			continue;
		}

		PCGShadeAttributeSettingsLocal::FResolvedOccluders& Resolved = Occluders.Emplace_GetRef();
		Resolved.PointData = OccluderPointData;
		Resolved.SourceData = OccluderTagged.Data;

		// EXPLICIT OPT-IN. With the toggle off no accessor is created at all and the constant is used
		// unconditionally — an untouched selector default-constructs to @Last and would otherwise bind to
		// whatever the occluders' last attribute happens to be (see the class comment).
		if (!bUseRadiusAttribute ||
			!PCGShadeAttributeSettingsLocal::TryReadPerPointValues(
				OccluderPointData, Settings->OccluderRadiusAttribute, NumOccluderPoints, Resolved.Radii))
		{
			Resolved.Radii.Init(RadiusConstantCm, NumOccluderPoints);
			// Only a FAILED opt-in read is worth a warning; the toggle being off is the intended default.
			bRadiusFellBackToConstant |= bUseRadiusAttribute;
		}

		if (!bUseDensityAttribute ||
			!PCGShadeAttributeSettingsLocal::TryReadPerPointValues(
				OccluderPointData, Settings->OccluderDensityAttribute, NumOccluderPoints, Resolved.Densities))
		{
			Resolved.Densities.Init(DensityConstant, NumOccluderPoints);
			bDensityFellBackToConstant |= bUseDensityAttribute;
		}
	}

	if (bRadiusFellBackToConstant)
	{
		PCGLog::LogWarningOnGraph(
			LOCTEXT("RadiusFallback", "Occluder Radius Attribute did not resolve to a numeric per-point attribute on at least one Occluders data; Occluder Radius Constant Cm is used for those occluders."),
			Context);
	}

	if (bDensityFellBackToConstant)
	{
		PCGLog::LogWarningOnGraph(
			LOCTEXT("DensityFallback", "Occluder Density Attribute did not resolve to a numeric per-point attribute on at least one Occluders data; Occluder Density Constant is used for those occluders."),
			Context);
	}

	// ------------------------------------------------------------------
	// Targets: one output data PER input data, each carrying that input's tags.
	// ------------------------------------------------------------------
	const TArray<FPCGTaggedData> TargetInputs =
		Context->InputData.GetInputsByPin(PCGShadeAttributeConstants::PointsLabel);

	for (const FPCGTaggedData& InTagged : TargetInputs)
	{
		const UPCGSpatialData* InSpatialData = Cast<UPCGSpatialData>(InTagged.Data);
		if (!InSpatialData)
		{
			PCGE_LOG(Error, GraphAndLog,
				LOCTEXT("InputMissingSpatialData", "Unable to get Spatial data from the Points input."));
			continue;
		}

		const UPCGBasePointData* TargetPointData = InSpatialData->ToBasePointData(Context);
		if (!TargetPointData)
		{
			PCGE_LOG(Error, GraphAndLog,
				LOCTEXT("InputMissingPointData", "Unable to get Point data from the Points input."));
			continue;
		}

		const int32 NumTargets = TargetPointData->GetNumPoints();

		// Every point survives — this node decorates, it never filters.
		TArray<int32> AllIndices;
		AllIndices.SetNumUninitialized(NumTargets);
		for (int32 Index = 0; Index < NumTargets; ++Index)
		{
			AllIndices[Index] = Index;
		}

		// PassThroughPoints is the sole authority for the copy sequence and appends the output entry
		// itself, so nothing is added to OutputData here.
		UPCGBasePointData* OutPointData =
			ArcPCGScatterCore::PassThroughPoints(Context, InTagged, TargetPointData, AllIndices);
		if (!OutPointData)
		{
			PCGE_LOG(Error, GraphAndLog,
				LOCTEXT("PassThroughFailed", "Failed to allocate the output point data for a Points input."));
			continue;
		}

		// NOT gated on bWriteAttribute any more: the density write is unconditional, so the transmittance has
		// to be computed even when the attribute name was cleared.
		if (NumTargets == 0)
		{
			continue;
		}

		// TRAVERSAL IS OCCLUDER-OUTWARD: the query radius is per occluder, so each occluder queries the
		// TARGETS' octree with its OWN centre and radius. It is therefore the TARGET data that is primed
		// here, not the occluders'. (Target-inward with a global max radius would be bit-identical —
		// over-fetch then filter by d <= R_j never under-fetches — so this is a speed choice, not a
		// correctness one.) Read-only traversal: the octree is unsafe for concurrent MUTATION
		// (PCGBasePointData.h:454), which this does not do.
		TargetPointData->PrimePointOctree();

		// double accumulator: Sum is an optical depth built from up to millions of small terms, and the
		// exponential below amplifies its error.
		TArray<double> Sums;
		Sums.SetNumZeroed(NumTargets);

		for (const PCGShadeAttributeSettingsLocal::FResolvedOccluders& Resolved : Occluders)
		{
			const UPCGBasePointData* OccluderPointData = Resolved.PointData;

			// Self-exclusion identity, decided ONCE per occluder data: are these two pins fed by the same
			// upstream data? Compared on the pre-conversion UPCGData (see FResolvedOccluders::SourceData) —
			// the collapsed point-data pointers differ even for one shared input.
			const bool bSameSourceData = (Resolved.SourceData != nullptr) && (Resolved.SourceData == InTagged.Data);

			const int32 NumOccluderPoints = OccluderPointData->GetNumPoints();
			const FPCGPointTransform::ConstValueRange OccluderTransforms =
				OccluderPointData->GetConstTransformValueRange();

			// SERIAL, deliberately: the callback SCATTER-WRITES into Sums, indexed by target, so two
			// occluders processed concurrently would race on the same element. Parallelising this needs
			// per-thread accumulators plus a reduction, not a ParallelFor over this loop.
			for (int32 OccluderIndex = 0; OccluderIndex < NumOccluderPoints; ++OccluderIndex)
			{
				const double OccluderRadiusCm = FMath::Max(Resolved.Radii[OccluderIndex], 0.0);
				const double OccluderDensity = Resolved.Densities[OccluderIndex];
				if (OccluderDensity == 0.0)
				{
					// Contributes exactly nothing; skip the octree query rather than the arithmetic.
					continue;
				}

				const FVector OccluderPosition = OccluderTransforms[OccluderIndex].GetLocation();

				// ArcPCGScatterCore's query, not UPCGOctreeQueries': the engine's is a PCG_API TEMPLATE
				// defined in its own header, so outside the PCG module that definition is dllimport-illegal
				// (MSVC C2491). The core's is the same octree query — see ArcPCGScatterCore.h.
				ArcPCGScatterCore::ForEachPointInsideSphere(TargetPointData, OccluderPosition, OccluderRadiusCm,
					[&Sums, bSameSourceData, OccluderIndex, OccluderRadiusCm, OccluderDensity, bIncludeSelf, Falloff]
					(int32 TargetIndex, double SquaredDistance) -> void
					{
						// SAME SOURCE DATA *AND* SAME INDEX. An index-only test would wrongly drop the pair
						// (occluder 7, target 7) whenever two genuinely different point sets are wired up.
						if (!bIncludeSelf && bSameSourceData && OccluderIndex == TargetIndex)
						{
							return;
						}

						const double Distance = FMath::Sqrt(FMath::Max(SquaredDistance, 0.0));
						Sums[TargetIndex] += OccluderDensity *
							static_cast<double>(ArcPCGScatterCore::EvaluateFalloff(Falloff, Distance, OccluderRadiusCm));
					});
			}
		}

		// Beer-Lambert, resolved once per target ahead of BOTH writes. Result in (0, 1]: exactly 1 when
		// nothing reaches the point, tending to 0 under a dense canopy. TRANSMITTED LIGHT, not occlusion.
		TArray<float> Transmittances;
		Transmittances.SetNumUninitialized(NumTargets);
		for (int32 Index = 0; Index < NumTargets; ++Index)
		{
			Transmittances[Index] = static_cast<float>(FMath::Exp(-ExtinctionK * Sums[Index]));
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
			for (int32 Index = 0; Index < NumTargets; ++Index)
			{
				DensityRange[Index] = ArcPCGScatterCore::ApplyDensityMode(
					DensityMode, DensityRange[Index], Transmittances[Index]);
			}
		}
		// DensityRange is deliberately scoped out above: the attribute path below can allocate the
		// MetadataEntry array, which invalidates it.

		if (!bWriteAttribute)
		{
			continue;
		}

		// Attribute write, same shape as the sibling nodes'. bOverrideParent stays at its default true: an
		// input that already carried an attribute of this name must be SHADOWED by the values computed
		// here, not silently left showing the inherited ones.
		FPCGMetadataDomain* Domain =
			OutPointData->MutableMetadata()->GetMetadataDomain(PCGMetadataDomainID::Elements);
		check(Domain);

		FPCGMetadataAttribute<float>* TransmittanceAttr =
			Domain->FindOrCreateAttribute<float>(Settings->OutputAttributeName, 1.0f);
		if (!TransmittanceAttr)
		{
			PCGE_LOG(Error, GraphAndLog,
				LOCTEXT("AttributeCreationFailed", "Failed to create the transmittance attribute; check the Output Attribute Name is a valid attribute name."));
			continue;
		}

		// bAllocate = TRUE: PassThroughPoints only carries a MetadataEntry array over when the INPUT had
		// one allocated, and a fresh point set from a generator does not.
		FPCGPointMetadataEntry::ValueRange EntryRange = OutPointData->GetMetadataEntryValueRange(/*bAllocate=*/true);
		for (int32 Index = 0; Index < NumTargets; ++Index)
		{
			// InitializeOnSet turns an invalid key into a fresh entry and a PARENT-referential key into a
			// child of it (PCGMetadataDomain.cpp:1244-1273), so the inherited attribute values copied by
			// SetPointsFrom survive the write below.
			Domain->InitializeOnSet(EntryRange[Index]);
			TransmittanceAttr->SetValue(EntryRange[Index], Transmittances[Index]);
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
