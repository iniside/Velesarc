// Copyright Lukasz Baran. All Rights Reserved.

// PCGRadialThinningTests.cpp
// Headless PCG element CQTest coverage for PointOps/PCGRadialThinningSettings.{h,cpp} (node C, "Radial
// Thinning"), driven through the REAL FPCGRadialThinningElement::Execute state machine via the shipped
// PCGTestsCommon::FTestData harness — the same pattern as PCGScatterNearPointsTests.cpp. WITH_EDITOR
// only: FTestData needs an editor world.
//
// This node samples no surface, so no ArcForestTestSurfaceData fixture is needed — every input here is
// a bare point set built directly, with a per-point radius supplied via RadiusConstantCm (no attribute
// selector cases are exercised here — those are covered by the shared TryReadPerPointValues shape this
// node borrows from its siblings, already pinned indirectly by PCGShadeAttributeTests.cpp's constant-
// fallback coverage).

#include "CQTest.h"

#if WITH_EDITOR

#include "Containers/Ticker.h"
#include "Misc/ScopeExit.h"

#include "PointOps/PCGRadialThinningSettings.h"

#include "Tests/PCGTestsCommon.h"
#include "PCGContext.h"
#include "PCGData.h"
#include "PCGPoint.h"
#include "Data/PCGBasePointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataCommon.h"
#include "Metadata/PCGMetadataDomain.h"

namespace PCGRadialThinningTestsLocal
{
	/** Point data with exactly Locations.Num() points, each transform set to the given world location. */
	UPCGBasePointData* MakeSourcePoints(TArrayView<const FVector> Locations)
	{
		UPCGBasePointData* PointData = PCGTestsCommon::CreateEmptyPointData<UPCGPointData>();
		PointData->SetNumPoints(Locations.Num());

		TPCGValueRange<FTransform> TransformRange = PointData->GetTransformValueRange();
		for (int32 Index = 0; Index < Locations.Num(); ++Index)
		{
			TransformRange[Index] = FTransform(Locations[Index]);
		}
		return PointData;
	}

	/** Sets a distinct, deterministic BoundsMin/BoundsMax pair on every point, so PassThroughPoints'
	 *  non-destructive claim can be checked bit-for-bit against the input. */
	void SetDistinctBounds(UPCGBasePointData* PointData)
	{
		TPCGValueRange<FVector> BoundsMinRange = PointData->GetBoundsMinValueRange();
		TPCGValueRange<FVector> BoundsMaxRange = PointData->GetBoundsMaxValueRange();
		for (int32 Index = 0; Index < PointData->GetNumPoints(); ++Index)
		{
			const double Extent = 10.0 + static_cast<double>(Index);
			BoundsMinRange[Index] = FVector(-Extent, -Extent, -Extent);
			BoundsMaxRange[Index] = FVector(Extent, Extent, Extent);
		}
	}

	/** Collects every surviving point's world-space location, in the output's own (ascending-input) order. */
	TArray<FVector> CollectLocations(const UPCGBasePointData* PointData)
	{
		TArray<FVector> Locations;
		const FPCGPointTransform::ConstValueRange TransformRange = PointData->GetConstTransformValueRange();
		for (int32 Index = 0; Index < PointData->GetNumPoints(); ++Index)
		{
			Locations.Add(TransformRange[Index].GetLocation());
		}
		return Locations;
	}

	/** Sorts a copy of Locations lexicographically so two sets can be order-independently compared. */
	TArray<FVector> SortedCopy(const TArray<FVector>& Locations)
	{
		TArray<FVector> Sorted = Locations;
		Sorted.Sort([](const FVector& A, const FVector& B)
		{
			if (A.X != B.X) { return A.X < B.X; }
			if (A.Y != B.Y) { return A.Y < B.Y; }
			return A.Z < B.Z;
		});
		return Sorted;
	}

	/**
	 * Runs the real element to completion (single-pass, but the spin-guard mirrors the shared harness
	 * pattern). Returns false if the guard tripped — callers MUST assert on this.
	 */
	bool RunToCompletion(FPCGElementPtr Element, FPCGContext* Context)
	{
		int32 Guard = 0;
		while (!Element->Execute(Context))
		{
			FTSTicker::GetCoreTicker().Tick(0.016f);
			if (++Guard > 64)
			{
				return false; // never spin forever if the element misbehaves
			}
		}
		return true;
	}
}

TEST_CLASS(ArcPCGRadialThinning, "ArcPCGExtension.PointOps.RadialThinning")
{
	// Two points exactly Ra + Rb apart: touching, not overlapping — the rejection test is STRICT
	// less-than, so both must survive.
	TEST_METHOD(ExactlySumOfRadiiApart_BothSurvive)
	{
		using namespace PCGRadialThinningTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGRadialThinningSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGRadialThinningSettings>(TestData);
		ASSERT_THAT(IsTrue(Settings != nullptr, TEXT("Settings must be generated")));
		Settings->RadiusConstantCm = 100.0; // Ra == Rb == 100 -> sum == 200
		Settings->OverlapFactor = 1.0f;

		const TArray<FVector> Locations = { FVector(0.0, 0.0, 0.0), FVector(200.0, 0.0, 0.0) };
		FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
		PointsInput.Pin = PCGRadialThinningConstants::PointsLabel;
		PointsInput.Data = MakeSourcePoints(Locations);

		FPCGElementPtr Element = TestData.Settings->GetElement();
		ASSERT_THAT(IsTrue(Element.IsValid(), TEXT("Settings must produce an element")));

		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
		ON_SCOPE_EXIT
		{
			FPCGContext::Release(Context.Release());
		};

		ASSERT_THAT(IsTrue(RunToCompletion(Element, Context.Get()),
			TEXT("The element must finish within the spin-guard budget, not hang")));

		ASSERT_THAT(IsTrue(Context->OutputData.TaggedData.Num() == 1, TEXT("One output tagged data expected")));
		const UPCGBasePointData* OutPointData = Cast<UPCGBasePointData>(Context->OutputData.TaggedData[0].Data);
		ASSERT_THAT(IsTrue(OutPointData != nullptr, TEXT("Output must be point data")));
		ASSERT_THAT(AreEqual(2, OutPointData->GetNumPoints(),
			TEXT("Exactly Ra + Rb apart is TOUCHING, not overlapping — the strict < test must let both survive")));
	}

	// At (Ra + Rb) - epsilon, one of the pair must die (the strict-< boundary mutant). Both points share
	// the same constant radius here, so priority is tied and the deterministic index tie-break decides —
	// LargestFirstFalse_InvertsSurvivor below covers the case with genuinely DIFFERENT priorities.
	TEST_METHOD(JustBelowSumOfRadii_LowerPriorityDies)
	{
		using namespace PCGRadialThinningTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGRadialThinningSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGRadialThinningSettings>(TestData);
		Settings->bUseRadiusAttribute = false;
		Settings->bUsePriorityAttribute = false;
		Settings->bLargestFirst = true;
		Settings->OverlapFactor = 1.0f;

		Settings->RadiusConstantCm = 100.0; // Ra == Rb == 100 -> sum == 200
		const double JustBelowSum = 200.0 - 0.01;

		const TArray<FVector> Locations = { FVector(0.0, 0.0, 0.0), FVector(JustBelowSum, 0.0, 0.0) };
		FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
		PointsInput.Pin = PCGRadialThinningConstants::PointsLabel;
		PointsInput.Data = MakeSourcePoints(Locations);

		FPCGElementPtr Element = TestData.Settings->GetElement();
		ASSERT_THAT(IsTrue(Element.IsValid(), TEXT("Settings must produce an element")));

		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
		ON_SCOPE_EXIT
		{
			FPCGContext::Release(Context.Release());
		};

		ASSERT_THAT(IsTrue(RunToCompletion(Element, Context.Get()),
			TEXT("The element must finish within the spin-guard budget, not hang")));

		const UPCGBasePointData* OutPointData = Cast<UPCGBasePointData>(Context->OutputData.TaggedData[0].Data);
		ASSERT_THAT(IsTrue(OutPointData != nullptr, TEXT("Output must be point data")));
		ASSERT_THAT(AreEqual(1, OutPointData->GetNumPoints(),
			TEXT("Just below Ra + Rb, exactly one of the equal-priority pair must be rejected (ties broken ")
			TEXT("by point index — index 0 wins, so it must be the survivor)")));

		const TArray<FVector> Survivors = CollectLocations(OutPointData);
		ASSERT_THAT(IsTrue(Survivors[0].Equals(Locations[0], 1e-6),
			TEXT("With equal priority, ties break by point index — index 0 must be the survivor")));
	}

	// The corner case that separates SPHERE from AABB: two points diagonally offset so their axis-
	// aligned bounding boxes overlap but their true spheres do not. Both must survive — regressing to a
	// box test would wrongly reject one.
	TEST_METHOD(DiagonalOffset_AABBOverlapsButSpheresDoNot_BothSurvive)
	{
		using namespace PCGRadialThinningTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGRadialThinningSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGRadialThinningSettings>(TestData);
		Settings->RadiusConstantCm = 100.0; // Ra == Rb == 100 -> sum == 200 -> reject sphere-distance < 200
		Settings->OverlapFactor = 1.0f;

		// dx == dy == 180: an AABB test using +/-100 half-extents around each centre would see the boxes
		// overlap on BOTH axes (180 < 200 on each), but the true Euclidean distance is
		// Sqrt(180^2 + 180^2) = 254.6, comfortably ABOVE the 200 cm sum-of-radii, so the true sphere test
		// must accept both.
		const double Offset = 180.0;
		const TArray<FVector> Locations = { FVector(0.0, 0.0, 0.0), FVector(Offset, Offset, 0.0) };
		const double TrueDistance = FVector::Dist2D(Locations[0], Locations[1]);
		ASSERT_THAT(IsTrue(TrueDistance > 200.0,
			TEXT("Test-setup sanity: the true sphere distance must exceed Ra + Rb, or this case proves nothing")));

		FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
		PointsInput.Pin = PCGRadialThinningConstants::PointsLabel;
		PointsInput.Data = MakeSourcePoints(Locations);

		FPCGElementPtr Element = TestData.Settings->GetElement();
		ASSERT_THAT(IsTrue(Element.IsValid(), TEXT("Settings must produce an element")));

		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
		ON_SCOPE_EXIT
		{
			FPCGContext::Release(Context.Release());
		};

		ASSERT_THAT(IsTrue(RunToCompletion(Element, Context.Get()),
			TEXT("The element must finish within the spin-guard budget, not hang")));

		const UPCGBasePointData* OutPointData = Cast<UPCGBasePointData>(Context->OutputData.TaggedData[0].Data);
		ASSERT_THAT(IsTrue(OutPointData != nullptr, TEXT("Output must be point data")));
		ASSERT_THAT(AreEqual(2, OutPointData->GetNumPoints(),
			TEXT("A true sphere test must accept both diagonally-offset points, even though their AABBs overlap — ")
			TEXT("this is what dies if the node 'simplifies' back to a bounds test")));
	}

	// OverlapFactor = 1.5 must reject a pair that survives at 1.0, AND a pair separated by more than one
	// naive (OverlapFactor-unaware) grid cell at that factor must still be correctly rejected — the
	// cell-sizing bug: sizing the grid off the raw radius instead of the REJECTION distance
	// (2*MaxRadius*OverlapFactor) would place these two points in non-adjacent cells and the 3x3
	// neighbour scan would silently miss the rejection.
	TEST_METHOD(OverlapFactorAboveOne_RejectsAndCellSizingCoversTheGap)
	{
		using namespace PCGRadialThinningTestsLocal;

		// Part 1: a pair that survives at OverlapFactor = 1.0 must be rejected at 1.5.
		{
			PCGTestsCommon::FTestData TestData;
			UPCGRadialThinningSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGRadialThinningSettings>(TestData);
			Settings->RadiusConstantCm = 100.0; // sum == 200; reject distance at factor 1.5 == 300
			Settings->OverlapFactor = 1.5f;

			const TArray<FVector> Locations = { FVector(0.0, 0.0, 0.0), FVector(200.0, 0.0, 0.0) }; // survives at 1.0 (touching)
			FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
			PointsInput.Pin = PCGRadialThinningConstants::PointsLabel;
			PointsInput.Data = MakeSourcePoints(Locations);

			FPCGElementPtr Element = TestData.Settings->GetElement();
			ASSERT_THAT(IsTrue(Element.IsValid(), TEXT("Settings must produce an element")));

			TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
			ON_SCOPE_EXIT
			{
				FPCGContext::Release(Context.Release());
			};

			ASSERT_THAT(IsTrue(RunToCompletion(Element, Context.Get()),
				TEXT("The element must finish within the spin-guard budget, not hang")));

			const UPCGBasePointData* OutPointData = Cast<UPCGBasePointData>(Context->OutputData.TaggedData[0].Data);
			ASSERT_THAT(IsTrue(OutPointData != nullptr, TEXT("Output must be point data")));
			ASSERT_THAT(AreEqual(1, OutPointData->GetNumPoints(),
				TEXT("A pair that survives at OverlapFactor 1.0 (distance == Ra+Rb) must be REJECTED at 1.5 ")
				TEXT("(reject distance 300 > distance 200)")));
		}

		// Part 2: the cell-sizing bug. Distance 550, radii 100 (sum 200), OverlapFactor 3.0 -> reject
		// distance 600 (> 550, so rejection MUST fire). A naive cell size of 2*MaxRadius = 200 (ignoring
		// OverlapFactor) buckets these two points into grid cells 2 apart (floor(0/200)=0,
		// floor(550/200)=2), which a 3x3 (+/-1) neighbour scan would NOT find — silently letting both
		// survive. The correct cell size, 2*MaxRadius*OverlapFactor = 600, buckets both into cell 0.
		{
			PCGTestsCommon::FTestData TestData;
			UPCGRadialThinningSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGRadialThinningSettings>(TestData);
			Settings->RadiusConstantCm = 100.0;
			Settings->OverlapFactor = 3.0f;

			const TArray<FVector> Locations = { FVector(0.0, 0.0, 0.0), FVector(550.0, 0.0, 0.0) };
			FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
			PointsInput.Pin = PCGRadialThinningConstants::PointsLabel;
			PointsInput.Data = MakeSourcePoints(Locations);

			FPCGElementPtr Element = TestData.Settings->GetElement();
			ASSERT_THAT(IsTrue(Element.IsValid(), TEXT("Settings must produce an element")));

			TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
			ON_SCOPE_EXIT
			{
				FPCGContext::Release(Context.Release());
			};

			ASSERT_THAT(IsTrue(RunToCompletion(Element, Context.Get()),
				TEXT("The element must finish within the spin-guard budget, not hang")));

			const UPCGBasePointData* OutPointData = Cast<UPCGBasePointData>(Context->OutputData.TaggedData[0].Data);
			ASSERT_THAT(IsTrue(OutPointData != nullptr, TEXT("Output must be point data")));
			ASSERT_THAT(AreEqual(1, OutPointData->GetNumPoints(),
				TEXT("A pair separated by more than one NAIVE (OverlapFactor-unaware) grid cell must still be ")
				TEXT("correctly rejected — sizing the grid off the raw radius instead of the rejection distance ")
				TEXT("would silently miss this pair and let both survive")));
		}
	}

	// bLargestFirst = false must INVERT which of a pair survives: the smaller-radius point wins instead
	// of the larger one.
	TEST_METHOD(LargestFirstFalse_InvertsSurvivor)
	{
		using namespace PCGRadialThinningTestsLocal;

		// Distinct radii (300 and 100) via the priority-attribute-off default (priority == radius), at a
		// distance inside the rejection band for BOTH orderings (350 < sum 400).
		TFunction<FVector(bool)> RunSurvivor = [](bool bLargestFirst) -> FVector
		{
			PCGTestsCommon::FTestData TestData;
			UPCGRadialThinningSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGRadialThinningSettings>(TestData);
			Settings->bUseRadiusAttribute = true;
			Settings->RadiusAttribute.SetAttributeName(TEXT("Radius"));
			Settings->bLargestFirst = bLargestFirst;
			Settings->OverlapFactor = 1.0f;

			const TArray<FVector> Locations = { FVector(0.0, 0.0, 0.0), FVector(350.0, 0.0, 0.0) };
			UPCGBasePointData* PointData = MakeSourcePoints(Locations);

			FPCGMetadataDomain* Domain = PointData->MutableMetadata()->GetMetadataDomain(PCGMetadataDomainID::Elements);
			check(Domain);
			FPCGMetadataAttribute<double>* RadiusAttr = Domain->FindOrCreateAttribute<double>(TEXT("Radius"), 0.0);
			check(RadiusAttr);
			FPCGPointMetadataEntry::ValueRange EntryRange = PointData->GetMetadataEntryValueRange(/*bAllocate=*/true);
			Domain->InitializeOnSet(EntryRange[0]);
			RadiusAttr->SetValue(EntryRange[0], 300.0); // point 0: big radius
			Domain->InitializeOnSet(EntryRange[1]);
			RadiusAttr->SetValue(EntryRange[1], 100.0); // point 1: small radius

			FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
			PointsInput.Pin = PCGRadialThinningConstants::PointsLabel;
			PointsInput.Data = PointData;

			FPCGElementPtr Element = TestData.Settings->GetElement();
			check(Element.IsValid());

			TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
			ON_SCOPE_EXIT
			{
				FPCGContext::Release(Context.Release());
			};

			verify(RunToCompletion(Element, Context.Get()));
			const UPCGBasePointData* OutPointData = Cast<UPCGBasePointData>(Context->OutputData.TaggedData[0].Data);
			check(OutPointData && OutPointData->GetNumPoints() == 1);
			return CollectLocations(OutPointData)[0];
		};

		const FVector SurvivorWhenLargestFirst = RunSurvivor(true);
		const FVector SurvivorWhenSmallestFirst = RunSurvivor(false);

		ASSERT_THAT(IsTrue(SurvivorWhenLargestFirst.Equals(FVector(0.0, 0.0, 0.0), 1e-6),
			TEXT("bLargestFirst = true must let the 300-radius point (index 0) survive")));
		ASSERT_THAT(IsTrue(SurvivorWhenSmallestFirst.Equals(FVector(350.0, 0.0, 0.0), 1e-6),
			TEXT("bLargestFirst = false must INVERT this: the 100-radius point (index 1) must survive instead")));
	}

	// Output point bounds must be BIT-IDENTICAL to input bounds for every survivor — the non-destructive
	// claim. Dies if the node starts writing extents (e.g. baking the exclusion radius into bounds, the
	// engine's own SelfPruning behaviour that this node exists to avoid).
	TEST_METHOD(SurvivorBounds_AreBitIdenticalToInput)
	{
		using namespace PCGRadialThinningTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGRadialThinningSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGRadialThinningSettings>(TestData);
		Settings->RadiusConstantCm = 10.0; // small radius so nothing is rejected — every point survives
		Settings->OverlapFactor = 1.0f;

		const TArray<FVector> Locations = { FVector(0.0, 0.0, 0.0), FVector(1000.0, 0.0, 0.0), FVector(2000.0, 0.0, 0.0) };
		UPCGBasePointData* InPointData = MakeSourcePoints(Locations);
		SetDistinctBounds(InPointData);

		const TArray<FVector> ExpectedBoundsMin = { InPointData->GetBoundsMin(0), InPointData->GetBoundsMin(1), InPointData->GetBoundsMin(2) };
		const TArray<FVector> ExpectedBoundsMax = { InPointData->GetBoundsMax(0), InPointData->GetBoundsMax(1), InPointData->GetBoundsMax(2) };

		FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
		PointsInput.Pin = PCGRadialThinningConstants::PointsLabel;
		PointsInput.Data = InPointData;

		FPCGElementPtr Element = TestData.Settings->GetElement();
		ASSERT_THAT(IsTrue(Element.IsValid(), TEXT("Settings must produce an element")));

		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
		ON_SCOPE_EXIT
		{
			FPCGContext::Release(Context.Release());
		};

		ASSERT_THAT(IsTrue(RunToCompletion(Element, Context.Get()),
			TEXT("The element must finish within the spin-guard budget, not hang")));

		const UPCGBasePointData* OutPointData = Cast<UPCGBasePointData>(Context->OutputData.TaggedData[0].Data);
		ASSERT_THAT(IsTrue(OutPointData != nullptr, TEXT("Output must be point data")));
		ASSERT_THAT(AreEqual(3, OutPointData->GetNumPoints(), TEXT("No point should be rejected at this large spacing")));

		for (int32 Index = 0; Index < 3; ++Index)
		{
			ASSERT_THAT(IsTrue(OutPointData->GetBoundsMin(Index).Equals(ExpectedBoundsMin[Index], 0.0),
				TEXT("BoundsMin must be BIT-IDENTICAL to the input — this node must never write bounds")));
			ASSERT_THAT(IsTrue(OutPointData->GetBoundsMax(Index).Equals(ExpectedBoundsMax[Index], 0.0),
				TEXT("BoundsMax must be BIT-IDENTICAL to the input — this node must never write bounds")));
		}
	}

	// A ten-point run must be ORDER-INVARIANT under input permutation, given the deterministic tie-break:
	// with distinct radii (no ties), sorting by priority does not depend on the points' original array
	// order, so permuting the input must not change WHICH points survive.
	TEST_METHOD(TenPoints_OrderInvariantUnderPermutation)
	{
		using namespace PCGRadialThinningTestsLocal;

		const TArray<FVector> Locations = {
			FVector(0.0, 0.0, 0.0), FVector(120.0, 0.0, 0.0), FVector(240.0, 0.0, 0.0),
			FVector(360.0, 0.0, 0.0), FVector(0.0, 500.0, 0.0), FVector(120.0, 500.0, 0.0),
			FVector(240.0, 500.0, 0.0), FVector(0.0, 1000.0, 0.0), FVector(120.0, 1000.0, 0.0),
			FVector(240.0, 1000.0, 0.0)
		};
		// Distinct radii, so priority order has no ties and is fully determined by these values
		// regardless of input array position.
		const TArray<double> Radii = { 55.0, 48.0, 41.0, 34.0, 27.0, 20.0, 13.0, 6.0, 60.0, 53.0 };

		TFunction<TArray<FVector>(const TArray<int32>&)> RunThinning =
			[&Locations, &Radii](const TArray<int32>& OrderIndices) -> TArray<FVector>
		{
			PCGTestsCommon::FTestData TestData;
			UPCGRadialThinningSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGRadialThinningSettings>(TestData);
			Settings->bUseRadiusAttribute = true;
			Settings->RadiusAttribute.SetAttributeName(TEXT("Radius"));
			Settings->OverlapFactor = 1.0f;

			TArray<FVector> PermutedLocations;
			TArray<double> PermutedRadii;
			for (int32 SourceIndex : OrderIndices)
			{
				PermutedLocations.Add(Locations[SourceIndex]);
				PermutedRadii.Add(Radii[SourceIndex]);
			}

			UPCGBasePointData* PointData = MakeSourcePoints(PermutedLocations);
			FPCGMetadataDomain* Domain = PointData->MutableMetadata()->GetMetadataDomain(PCGMetadataDomainID::Elements);
			check(Domain);
			FPCGMetadataAttribute<double>* RadiusAttr = Domain->FindOrCreateAttribute<double>(TEXT("Radius"), 0.0);
			check(RadiusAttr);
			FPCGPointMetadataEntry::ValueRange EntryRange = PointData->GetMetadataEntryValueRange(/*bAllocate=*/true);
			for (int32 Index = 0; Index < PermutedRadii.Num(); ++Index)
			{
				Domain->InitializeOnSet(EntryRange[Index]);
				RadiusAttr->SetValue(EntryRange[Index], PermutedRadii[Index]);
			}

			FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
			PointsInput.Pin = PCGRadialThinningConstants::PointsLabel;
			PointsInput.Data = PointData;

			FPCGElementPtr Element = TestData.Settings->GetElement();
			check(Element.IsValid());

			TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
			ON_SCOPE_EXIT
			{
				FPCGContext::Release(Context.Release());
			};

			verify(RunToCompletion(Element, Context.Get()));
			const UPCGBasePointData* OutPointData = Cast<UPCGBasePointData>(Context->OutputData.TaggedData[0].Data);
			check(OutPointData);
			return SortedCopy(CollectLocations(OutPointData));
		};

		const TArray<int32> IdentityOrder = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
		const TArray<int32> PermutedOrder = { 7, 2, 9, 0, 5, 3, 8, 1, 6, 4 };

		const TArray<FVector> SurvivorsIdentity = RunThinning(IdentityOrder);
		const TArray<FVector> SurvivorsPermuted = RunThinning(PermutedOrder);

		ASSERT_THAT(IsTrue(SurvivorsIdentity.Num() > 0, TEXT("Non-vacuity: some points must survive")));
		ASSERT_THAT(AreEqual(SurvivorsIdentity.Num(), SurvivorsPermuted.Num(),
			TEXT("Permuting the input must not change HOW MANY points survive")));
		for (int32 Index = 0; Index < SurvivorsIdentity.Num(); ++Index)
		{
			ASSERT_THAT(IsTrue(SurvivorsIdentity[Index].Equals(SurvivorsPermuted[Index], 1e-6),
				TEXT("The SET of surviving positions must be identical under input permutation, given ")
				TEXT("distinct (tie-free) radii — the deterministic priority ordering must not depend on ")
				TEXT("original array position")));
		}
	}
};

#endif // WITH_EDITOR
