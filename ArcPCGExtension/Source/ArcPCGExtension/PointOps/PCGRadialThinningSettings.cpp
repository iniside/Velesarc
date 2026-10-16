// Copyright Lukasz Baran. All Rights Reserved.

#include "PointOps/PCGRadialThinningSettings.h"

#include "Scatter/ArcPCGScatterCore.h"

#include "PCGCommon.h"
#include "PCGContext.h"
#include "PCGData.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGSpatialData.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Utils/PCGLogErrors.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGRadialThinningSettings)

#define LOCTEXT_NAMESPACE "PCGRadialThinning"

namespace PCGRadialThinningConstants
{
	const FName PointsLabel = TEXT("Points");
}

FPCGElementPtr UPCGRadialThinningSettings::CreateElement() const
{
	return MakeShared<FPCGRadialThinningElement>();
}

#if WITH_EDITOR
FText UPCGRadialThinningSettings::GetNodeTooltipText() const
{
	// This overrides the ToolTip metadata UHT bakes from the class comment, so it is what the graph
	// editor actually shows - it must not contradict that comment. Keep the two in step.
	return LOCTEXT("NodeTooltip",
		"Removes points whose per-point exclusion SPHERES overlap: a point survives only if no already-kept "
		"point is closer than (its radius + the kept point's radius) * Overlap Factor. Points are visited "
		"from the highest priority down (priority is the radius unless you tick Use Priority Attribute), with "
		"ties broken by point index so the result is fully deterministic. The radius comes from Radius "
		"Constant Cm unless you tick Use Radius Attribute. UNLIKE the built-in Self Pruning node this is a "
		"true sphere test rather than an axis-aligned box test, so diagonally offset points are not "
		"over-pruned, and it NEVER writes to point bounds - survivors are passed through untouched, so you do "
		"not have to bake the radius into the bounds first. Thinning never crosses a data boundary: each "
		"input data is thinned alone and emitted as one output data, so merge your point sets upstream if "
		"they must thin each other.");
}
#endif

TArray<FPCGPinProperties> UPCGRadialThinningSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	// Multiple connections and multiple data are allowed, but each data is still thinned ALONE - see the
	// pin tooltip. Emplace_GetRef, not Emplace: TArray::Emplace returns an INDEX, not a reference.
	FPCGPinProperties& PointsPin = PinProperties.Emplace_GetRef(
		PCGRadialThinningConstants::PointsLabel,
		EPCGDataType::Point,
		/*bAllowMultipleConnections=*/true,
		/*bAllowMultipleData=*/true,
		LOCTEXT("PointsPinTooltip", "Points to thin. Each input data is thinned against itself only and emitted as one output data of survivors carrying that data's tags; thinning never crosses a data boundary."));
	PointsPin.SetRequiredPin();

	return PinProperties;
}

// ---------------------------------------------------------------------------
// Helpers - file-scoped namespace (unique to this .cpp)
// ---------------------------------------------------------------------------

namespace PCGRadialThinningSettingsLocal
{
	/**
	 * Reads one per-point numeric attribute off PointData through InSelector.
	 * Returns false - leaving OutValues untouched - when the selector does not resolve, resolves to a
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
		// A @Last-style selector otherwise resolves against the wrong attribute - the accessor would be
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

	/** XY cell of a world position in a uniform grid of side CellSize. Z is deliberately NOT part of the
	 *  key - see the grid comment in the accept loop. */
	FIntPoint ToCell(const FVector& Position, double InvCellSize)
	{
		return FIntPoint(
			FMath::FloorToInt32(Position.X * InvCellSize),
			FMath::FloorToInt32(Position.Y * InvCellSize));
	}
}

// ---------------------------------------------------------------------------
// Element execution
// ---------------------------------------------------------------------------

bool FPCGRadialThinningElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRadialThinningElement::Execute);
	check(Context);

	const UPCGRadialThinningSettings* Settings = Context->GetInputSettings<UPCGRadialThinningSettings>();
	check(Settings);

	const bool bUseRadiusAttribute = Settings->bUseRadiusAttribute;
	const bool bUsePriorityAttribute = Settings->bUsePriorityAttribute;
	const bool bLargestFirst = Settings->bLargestFirst;
	// A negative radius or factor is meaningless and would flip the comparison below into accepting
	// everything by accident rather than by intent; clamped so "0" is the single documented way to say
	// "reject nothing".
	const double RadiusConstantCm = FMath::Max(Settings->RadiusConstantCm, 0.0);
	const double OverlapFactor = FMath::Max(static_cast<double>(Settings->OverlapFactor), 0.0);

	// ONE warning each for the whole execution, raised after every input data has been processed - not
	// once per data, which on a multi-data input would bury the graph in identical messages.
	bool bRadiusFellBackToConstant = false;
	bool bPriorityFellBackToRadius = false;

	// One output data PER input data, each carrying that input's tags.
	const TArray<FPCGTaggedData> Inputs =
		Context->InputData.GetInputsByPin(PCGRadialThinningConstants::PointsLabel);

	for (const FPCGTaggedData& InTagged : Inputs)
	{
		// COLLAPSE, then ERROR - never drop silently. Same shape as the engine's own point ops
		// (PCGTransformPoints.cpp:80-92, PCGSelectPoints.cpp:63-82) and the sibling nodes.
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

		// ------------------------------------------------------------------
		// 1. Radius and priority per point.
		// ------------------------------------------------------------------

		// EXPLICIT OPT-IN. With the toggle off no accessor is created at all and the constant is used
		// unconditionally - an untouched selector default-constructs to @Last and would otherwise bind to
		// whatever the input's last attribute happens to be (see the class comment).
		TArray<double> Radii;
		if (!bUseRadiusAttribute ||
			!PCGRadialThinningSettingsLocal::TryReadPerPointValues(
				InPointData, Settings->RadiusAttribute, NumPoints, Radii))
		{
			Radii.Init(RadiusConstantCm, NumPoints);
			// Only a FAILED opt-in read is worth a warning; the toggle being off is the intended default.
			bRadiusFellBackToConstant |= bUseRadiusAttribute;
		}
		else
		{
			for (int32 Index = 0; Index < NumPoints; ++Index)
			{
				Radii[Index] = FMath::Max(Radii[Index], 0.0);
			}
		}

		// When the priority attribute is off (or fails), priority IS the radius. That is what makes
		// bLargestFirst mean "big points win" with no extra setup.
		TArray<double> Priorities;
		if (!bUsePriorityAttribute ||
			!PCGRadialThinningSettingsLocal::TryReadPerPointValues(
				InPointData, Settings->PriorityAttribute, NumPoints, Priorities))
		{
			Priorities = Radii;
			bPriorityFellBackToRadius |= bUsePriorityAttribute;
		}

		// ------------------------------------------------------------------
		// 2. Visit order: by priority, TIES BROKEN BY POINT INDEX.
		// ------------------------------------------------------------------

		TArray<int32> Order;
		Order.SetNumUninitialized(NumPoints);
		for (int32 Index = 0; Index < NumPoints; ++Index)
		{
			Order[Index] = Index;
		}

		// TArray::Sort is an UNSTABLE introsort, so equal priorities would otherwise be ordered by
		// whatever the partitioning happened to do - which changes with the input size. The explicit
		// index tie-break is the whole determinism guarantee; do not remove it as redundant.
		Order.Sort([&Priorities, bLargestFirst](const int32& A, const int32& B) -> bool
		{
			const double PriorityA = Priorities[A];
			const double PriorityB = Priorities[B];
			if (PriorityA != PriorityB)
			{
				return bLargestFirst ? (PriorityA > PriorityB) : (PriorityA < PriorityB);
			}

			return A < B;
		});

		// ------------------------------------------------------------------
		// 3. Accept loop over a uniform grid of ACCEPTED points only.
		// ------------------------------------------------------------------

		double MaxRadius = 0.0;
		for (int32 Index = 0; Index < NumPoints; ++Index)
		{
			MaxRadius = FMath::Max(MaxRadius, Radii[Index]);
		}

		// CELL SIZE IS THE MAXIMUM REJECTION DISTANCE, not the raw radius: the largest distance that can
		// ever reject a pair is (MaxR + MaxR) * OverlapFactor, and a 3x3 neighbour scan only covers every
		// possible rejector while the cell is at least that wide. Sizing off 2*MaxRadius alone silently
		// misses rejections as soon as OverlapFactor > 1. The Max(1.0) floor is not about correctness - a
		// LARGER cell is always safe, it only over-fetches - but keeps a degenerate near-zero radius from
		// pushing world coordinates past int32 when they are divided by it.
		const double CellSize = FMath::Max(2.0 * MaxRadius * FMath::Max(1.0, OverlapFactor), 1.0);
		const double InvCellSize = 1.0 / CellSize;

		const FPCGPointTransform::ConstValueRange SourceTransforms = InPointData->GetConstTransformValueRange();

		// The grid key is XY only while the test below is a full 3D distance. That is deliberate and safe:
		// XY distance is never greater than 3D distance, so a rejector is always inside the 3x3 XY
		// neighbourhood - the Z spread only costs extra candidates that the exact test then discards.
		TMap<FIntPoint, TArray<int32>> AcceptedGrid;
		TArray<int32> Accepted;
		Accepted.Reserve(NumPoints);

		for (int32 OrderIndex = 0; OrderIndex < NumPoints; ++OrderIndex)
		{
			const int32 CandidateIndex = Order[OrderIndex];
			const FVector CandidatePosition = SourceTransforms[CandidateIndex].GetLocation();
			const double CandidateRadius = Radii[CandidateIndex];
			const FIntPoint CandidateCell =
				PCGRadialThinningSettingsLocal::ToCell(CandidatePosition, InvCellSize);

			bool bRejected = false;
			for (int32 OffsetY = -1; OffsetY <= 1 && !bRejected; ++OffsetY)
			{
				for (int32 OffsetX = -1; OffsetX <= 1 && !bRejected; ++OffsetX)
				{
					const TArray<int32>* Bucket =
						AcceptedGrid.Find(FIntPoint(CandidateCell.X + OffsetX, CandidateCell.Y + OffsetY));
					if (!Bucket)
					{
						continue;
					}

					for (int32 AcceptedIndex : *Bucket)
					{
						// TRUE SPHERE-SPHERE, not the engine's AABB overlap (PCGSelfPruning.cpp:149).
						const double RejectDistance = (CandidateRadius + Radii[AcceptedIndex]) * OverlapFactor;
						if (RejectDistance <= 0.0)
						{
							continue;
						}

						// STRICT less-than: two points exactly Ra + Rb apart are touching, not
						// overlapping, and both survive.
						const double SquaredDistance = FVector::DistSquared(
							CandidatePosition, SourceTransforms[AcceptedIndex].GetLocation());
						if (SquaredDistance < RejectDistance * RejectDistance)
						{
							bRejected = true;
							break;
						}
					}
				}
			}

			if (!bRejected)
			{
				Accepted.Add(CandidateIndex);
				AcceptedGrid.FindOrAdd(CandidateCell).Add(CandidateIndex);
			}
		}

		// Emit in ascending INPUT index order, not in visit order, so the output is a subsequence of the
		// input rather than a priority-sorted reshuffle of it. Which points survive is unaffected.
		Accepted.Sort();

		// ------------------------------------------------------------------
		// 4. Emit the survivors. NO ATTRIBUTE AND NO BOUNDS ARE WRITTEN - the non-destructive half of
		//    this node's reason to exist. PassThroughPoints is the sole authority for the copy sequence
		//    (NewPointData_AnyThread -> InitializeFromDataWithParams with bInheritSpatialData off ->
		//    SetPointsFrom -> whole-FPCGTaggedData copy) and appends the output entry itself, so nothing
		//    is added to OutputData here.
		// ------------------------------------------------------------------
		const UPCGBasePointData* OutPointData =
			ArcPCGScatterCore::PassThroughPoints(Context, InTagged, InPointData, Accepted);
		if (!OutPointData)
		{
			PCGE_LOG(Error, GraphAndLog,
				LOCTEXT("PassThroughFailed", "Failed to allocate the output point data for a Points input."));
			continue;
		}
	}

	if (bRadiusFellBackToConstant)
	{
		PCGLog::LogWarningOnGraph(
			LOCTEXT("RadiusFallback", "Radius Attribute did not resolve to a numeric per-point attribute on at least one input data; Radius Constant Cm is used for those points."),
			Context);
	}

	if (bPriorityFellBackToRadius)
	{
		PCGLog::LogWarningOnGraph(
			LOCTEXT("PriorityFallback", "Priority Attribute did not resolve to a numeric per-point attribute on at least one input data; each point's radius is used as its priority for those points."),
			Context);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
