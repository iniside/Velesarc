// Copyright Lukasz Baran. All Rights Reserved.

// PCGClusteredScatterTests.cpp
// Headless PCG element CQTest coverage for Scatter/PCGClusteredScatterSettings.{h,cpp} (node D,
// "Clustered Scatter"), driven through the REAL FPCGClusteredScatterElement::Execute state machine via
// the shipped PCGTestsCommon::FTestData harness — the same pattern as PCGScatterNearPointsTests.cpp.
// WITH_EDITOR only: FTestData needs an editor world.
//
// CAVEAT (pin type is NOT proven by this suite): GetSpatialInputsByPin filters on pin label +
// IsA<UPCGSpatialData>() and never checks EPCGDataType (see PCGScatterNearPointsTests.cpp's own caveat,
// which applies identically here).
//
// BOUNDS VALIDITY IS A TESTED AXIS: this node has NO fallback path (unlike its sibling Scatter Near
// Points) — invalid bounds is a hard refusal. Every case below Initializes real bounds.

#include "CQTest.h"

#if WITH_EDITOR

#include "Containers/Ticker.h"
#include "Misc/ScopeExit.h"

#include "ArcForestTestSurfaceData.h"
#include "Scatter/PCGClusteredScatterSettings.h"
#include "Scatter/ArcPCGScatterCore.h"

#include "Tests/PCGTestsCommon.h"
#include "PCGContext.h"
#include "PCGData.h"
#include "Data/PCGBasePointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataCommon.h"
#include "Metadata/PCGMetadataDomain.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcPCGClusteredScatterTest, Log, All);

namespace PCGClusteredScatterTestsLocal
{
	/** A flat, fully-suitable-in-XY surface with real (valid) bounds. */
	UArcForestTestSurfaceData* MakeValidFlatSurface(const FBox& Bounds, float FlatZ = 0.0f)
	{
		UArcForestTestSurfaceData* Surface = NewObject<UArcForestTestSurfaceData>();
		Surface->Initialize(Bounds);
		Surface->FlatSurfaceZ = FlatZ;
		return Surface;
	}

	/**
	 * A surface with valid Bounds (so it defines a real scatter region) but whose SamplePoint footprint
	 * is a SMALLER override box — this is the fixture shape that produces genuine per-offspring misses,
	 * since the node always clamps its darts back into Bounds and would otherwise never miss.
	 */
	UArcForestTestSurfaceData* MakeValidSurfaceWithSmallerFootprint(const FBox& Bounds, const FBox& Footprint, float FlatZ = 0.0f)
	{
		UArcForestTestSurfaceData* Surface = NewObject<UArcForestTestSurfaceData>();
		Surface->Initialize(Bounds);
		Surface->FlatSurfaceZ = FlatZ;
		Surface->SetFootprintOverride(Footprint);
		return Surface;
	}

	/** Collects every emitted point's world-space location across ALL output tagged data. */
	TArray<FVector> CollectOutputLocations(const FPCGDataCollection& OutputData)
	{
		TArray<FVector> Locations;
		for (const FPCGTaggedData& Tagged : OutputData.TaggedData)
		{
			const UPCGBasePointData* PointData = Cast<UPCGBasePointData>(Tagged.Data);
			if (!PointData)
			{
				continue;
			}
			const FPCGPointTransform::ConstValueRange TransformRange = PointData->GetConstTransformValueRange();
			for (int32 Index = 0; Index < PointData->GetNumPoints(); ++Index)
			{
				Locations.Add(TransformRange[Index].GetLocation());
			}
		}
		return Locations;
	}

	/** Mean nearest-neighbour distance across a position set (brute force; test-scale N only). */
	double MeanNearestNeighbour(const TArray<FVector>& Positions)
	{
		const int32 Count = Positions.Num();
		if (Count < 2)
		{
			return 0.0;
		}
		double Sum = 0.0;
		for (int32 I = 0; I < Count; ++I)
		{
			double BestSq = TNumericLimits<double>::Max();
			for (int32 J = 0; J < Count; ++J)
			{
				if (J == I)
				{
					continue;
				}
				const double DistSq = FVector::DistSquared2D(Positions[I], Positions[J]);
				if (DistSq < BestSq)
				{
					BestSq = DistSq;
				}
			}
			Sum += FMath::Sqrt(BestSq);
		}
		return Sum / Count;
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

TEST_CLASS(ArcPCGClusteredScatter, "ArcPCGExtension.Scatter.Clustered")
{
	// On a flat, valid-bounds surface whose footprint equals its bounds, every offspring's clamped XY
	// necessarily lies inside the footprint too, so misses are provably zero. The output count must then
	// be EXACTLY NumParents * PointsPerParent, and PatchId must partition the output into NumParents
	// groups of exactly PointsPerParent each, covering every parent index.
	TEST_METHOD(FlatValidSurface_ExactCountAndPatchIdPartitionsOutput)
	{
		using namespace PCGClusteredScatterTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGClusteredScatterSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGClusteredScatterSettings>(TestData);
		ASSERT_THAT(IsTrue(Settings != nullptr, TEXT("Settings must be generated")));

		// 10000 cm x 10000 cm = 1 hectare (10000 m^2 -> AreaM2 = 10000). Density 0.001 -> NumParents = 10.
		const FBox Bounds(FVector(-5000.0, -5000.0, -100.0), FVector(5000.0, 5000.0, 100.0));
		Settings->ParentDensityPerM2 = 0.001;
		Settings->PatchRadiusSigmaCm = 50.0;
		Settings->PointsPerParent = 8;
		Settings->MaxPoints = 100000; // no clamp

		FPCGTaggedData& SurfaceInput = TestData.InputData.TaggedData.Emplace_GetRef();
		SurfaceInput.Pin = PCGClusteredScatterConstants::SurfaceLabel;
		SurfaceInput.Data = MakeValidFlatSurface(Bounds, 0.0f);

		FPCGElementPtr Element = TestData.Settings->GetElement();
		ASSERT_THAT(IsTrue(Element.IsValid(), TEXT("Settings must produce an element")));

		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
		ON_SCOPE_EXIT
		{
			FPCGContext::Release(Context.Release());
		};

		ASSERT_THAT(IsTrue(RunToCompletion(Element, Context.Get()),
			TEXT("The element must finish within the spin-guard budget, not hang")));

		ASSERT_THAT(IsTrue(Context->OutputData.TaggedData.Num() == 1, TEXT("A generator must emit exactly one output tagged data")));
		const UPCGBasePointData* PointData = Cast<UPCGBasePointData>(Context->OutputData.TaggedData[0].Data);
		ASSERT_THAT(IsTrue(PointData != nullptr, TEXT("Output must be point data")));

		const int32 ExpectedNumParents = 10;
		const int32 ExpectedTotal = ExpectedNumParents * Settings->PointsPerParent;
		ASSERT_THAT(AreEqual(ExpectedTotal, PointData->GetNumPoints(),
			TEXT("On a flat surface whose footprint equals its bounds, misses are provably zero — the ")
			TEXT("output count must be EXACTLY NumParents * PointsPerParent")));

		const UPCGMetadata* Metadata = PointData->ConstMetadata();
		ASSERT_THAT(IsTrue(Metadata != nullptr, TEXT("Output must carry metadata")));
		const FPCGMetadataDomain* Domain = Metadata->GetConstMetadataDomain(PCGMetadataDomainID::Elements);
		ASSERT_THAT(IsTrue(Domain != nullptr, TEXT("Output must have the Elements metadata domain")));
		const FPCGMetadataAttribute<int32>* PatchIdAttr = Domain->GetConstTypedAttribute<int32>(Settings->PatchIdAttributeName);
		ASSERT_THAT(IsTrue(PatchIdAttr != nullptr, TEXT("PatchId attribute must be present as int32")));

		const FPCGPointMetadataEntry::ConstValueRange EntryRange = PointData->GetConstMetadataEntryValueRange();
		TMap<int32, int32> CountByPatchId;
		for (int32 Index = 0; Index < PointData->GetNumPoints(); ++Index)
		{
			const int32 PatchId = PatchIdAttr->GetValueFromItemKey(EntryRange[Index]);
			ASSERT_THAT(IsTrue(PatchId >= 0 && PatchId < ExpectedNumParents,
				TEXT("Every PatchId must index the parent set actually generated")));
			CountByPatchId.FindOrAdd(PatchId)++;
		}

		ASSERT_THAT(AreEqual(ExpectedNumParents, CountByPatchId.Num(),
			TEXT("Every one of the NumParents patch ids must appear — a collapsed/shared metadata entry key ")
			TEXT("would shrink this set")));
		for (const TPair<int32, int32>& Pair : CountByPatchId)
		{
			ASSERT_THAT(AreEqual(Settings->PointsPerParent, Pair.Value,
				TEXT("Each patch must contribute EXACTLY PointsPerParent points on the miss-free flat fixture — ")
				TEXT("PatchId must PARTITION the output, not merely tag it")));
		}
	}

	// Children of one patch must be measurably CLUSTERED: with a small PatchRadiusSigmaCm relative to
	// the parent spacing, the observed mean nearest-neighbour distance across the whole output must sit
	// well below the uniform-Poisson (CSR) expectation 0.5/Sqrt(density) for the same point count and
	// area — the Clark-Evans shape used in ArcForestPatchClusteringTests.cpp.
	TEST_METHOD(SmallSigma_ChildrenAreMeasurablyClustered)
	{
		using namespace PCGClusteredScatterTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGClusteredScatterSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGClusteredScatterSettings>(TestData);

		// 10000 cm x 10000 cm again (AreaM2 = 10000), but a much lower parent density (5 parents) each
		// with many tightly-clustered children (sigma = 50 cm, i.e. two orders of magnitude below the
		// parent spacing), so the whole-output mean-NN is dominated by intra-patch spacing, not
		// inter-patch spacing.
		const FBox Bounds(FVector(-5000.0, -5000.0, -100.0), FVector(5000.0, 5000.0, 100.0));
		Settings->ParentDensityPerM2 = 0.0005; // -> NumParents = 5
		Settings->PatchRadiusSigmaCm = 50.0;
		Settings->PointsPerParent = 30;

		FPCGTaggedData& SurfaceInput = TestData.InputData.TaggedData.Emplace_GetRef();
		SurfaceInput.Pin = PCGClusteredScatterConstants::SurfaceLabel;
		SurfaceInput.Data = MakeValidFlatSurface(Bounds, 0.0f);

		FPCGElementPtr Element = TestData.Settings->GetElement();
		ASSERT_THAT(IsTrue(Element.IsValid(), TEXT("Settings must produce an element")));

		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
		ON_SCOPE_EXIT
		{
			FPCGContext::Release(Context.Release());
		};

		ASSERT_THAT(IsTrue(RunToCompletion(Element, Context.Get()),
			TEXT("The element must finish within the spin-guard budget, not hang")));

		const TArray<FVector> Locations = CollectOutputLocations(Context->OutputData);
		ASSERT_THAT(IsTrue(Locations.Num() >= 100, TEXT("Non-vacuity: expect close to the full 150-point budget on a miss-free fixture")));

		const double AreaCm2 = (Bounds.Max.X - Bounds.Min.X) * (Bounds.Max.Y - Bounds.Min.Y);
		const double Density = static_cast<double>(Locations.Num()) / AreaCm2;
		const double PoissonExpectedMeanNN = 0.5 / FMath::Sqrt(Density);
		const double ObservedMeanNN = MeanNearestNeighbour(Locations);

		UE_LOGF(LogArcPCGClusteredScatterTest, Display,"[Clustering] N=%d observedMeanNN=%.3f poissonExpectedMeanNN=%.3f",
			Locations.Num(), ObservedMeanNN, PoissonExpectedMeanNN);

		ASSERT_THAT(IsTrue(ObservedMeanNN < 0.25 * PoissonExpectedMeanNN,
			TEXT("Children of a patch process with PatchRadiusSigmaCm two orders of magnitude below the ")
			TEXT("parent spacing must be measurably clustered — the observed mean-NN must sit well below ")
			TEXT("the uniform-Poisson (CSR) expectation, not merely below it")));
	}

	// PatchRadiusSigmaCm just above the degenerate-refusal threshold collapses every child onto (very
	// near) its parent: with a single parent, every child's pairwise distance to every other child must
	// be tiny relative to what a normal 400 cm default sigma would produce.
	TEST_METHOD(TinySigma_CollapsesChildrenOntoParent)
	{
		using namespace PCGClusteredScatterTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGClusteredScatterSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGClusteredScatterSettings>(TestData);

		// 1000 cm x 1000 cm -> AreaM2 = 100. Density 0.01 -> NumParents = 1, well away from the bounds
		// edge so the offset-clamp never engages and cannot be blamed for the collapse.
		const FBox Bounds(FVector(-500.0, -500.0, -100.0), FVector(500.0, 500.0, 100.0));
		Settings->ParentDensityPerM2 = 0.01;
		Settings->PatchRadiusSigmaCm = 0.01; // tiny but > UE_KINDA_SMALL_NUMBER, so not refused
		Settings->PointsPerParent = 20;

		FPCGTaggedData& SurfaceInput = TestData.InputData.TaggedData.Emplace_GetRef();
		SurfaceInput.Pin = PCGClusteredScatterConstants::SurfaceLabel;
		SurfaceInput.Data = MakeValidFlatSurface(Bounds, 0.0f);

		FPCGElementPtr Element = TestData.Settings->GetElement();
		ASSERT_THAT(IsTrue(Element.IsValid(), TEXT("Settings must produce an element")));

		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
		ON_SCOPE_EXIT
		{
			FPCGContext::Release(Context.Release());
		};

		ASSERT_THAT(IsTrue(RunToCompletion(Element, Context.Get()),
			TEXT("The element must finish within the spin-guard budget, not hang")));

		const TArray<FVector> Locations = CollectOutputLocations(Context->OutputData);
		ASSERT_THAT(AreEqual(Settings->PointsPerParent, Locations.Num(),
			TEXT("With a single parent well inside the bounds, every child must hit — no misses expected")));

		double MaxPairwiseDistance = 0.0;
		for (int32 I = 0; I < Locations.Num(); ++I)
		{
			for (int32 J = I + 1; J < Locations.Num(); ++J)
			{
				MaxPairwiseDistance = FMath::Max(MaxPairwiseDistance, FVector::Dist2D(Locations[I], Locations[J]));
			}
		}

		UE_LOGF(LogArcPCGClusteredScatterTest, Display,"[TinySigma] maxPairwiseDistance=%.6f", MaxPairwiseDistance);

		// At sigma = 0.01 cm, the Gaussian radius = Sigma*Sqrt(-2*ln(U)) is sub-millimetre for any
		// plausible U; 1 cm is a generous ceiling that is nonetheless four orders of magnitude below the
		// 400 cm default sigma, so this cannot be satisfied by an implementation that ignores
		// PatchRadiusSigmaCm and uses the default instead.
		ASSERT_THAT(IsTrue(MaxPairwiseDistance < 1.0,
			TEXT("A degenerate-small sigma must collapse every child onto (very near) its parent")));
	}

	// Surface misses, tested in isolation: Bounds define the scatter region, but the surface's
	// SamplePoint footprint (via FootprintOverride) is a strictly SMALLER box — the LEFT HALF of Bounds
	// — so darts whose clamped XY falls in the right half are genuine misses. PatchRadiusSigmaCm is kept
	// small relative to the half-width, so each patch's children stay close enough to their own parent
	// that (with 20 independently-placed parents split ~50/50 across the two halves) it is a
	// virtual certainty that SOME parents fall in the footprint half (producing hits) and some do not
	// (producing misses) — P(all 20 parents land on the same side) = 2 * 0.5^20 ~= 2e-6.
	TEST_METHOD(BoundedFootprintSmallerThanBounds_SomeOffspringMiss)
	{
		using namespace PCGClusteredScatterTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGClusteredScatterSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGClusteredScatterSettings>(TestData);

		const FBox Bounds(FVector(-1000.0, -1000.0, -100.0), FVector(1000.0, 1000.0, 100.0));
		const FBox Footprint(FVector(-1000.0, -1000.0, -100.0), FVector(0.0, 1000.0, 100.0)); // left half only
		Settings->ParentDensityPerM2 = 0.05; // NumParents = 20 over the 400 m^2 Bounds
		Settings->PatchRadiusSigmaCm = 50.0; // small vs. the 2000 cm bounds width -> children stay near their parent
		Settings->PointsPerParent = 10;

		FPCGTaggedData& SurfaceInput = TestData.InputData.TaggedData.Emplace_GetRef();
		SurfaceInput.Pin = PCGClusteredScatterConstants::SurfaceLabel;
		SurfaceInput.Data = MakeValidSurfaceWithSmallerFootprint(Bounds, Footprint, 0.0f);

		FPCGElementPtr Element = TestData.Settings->GetElement();
		ASSERT_THAT(IsTrue(Element.IsValid(), TEXT("Settings must produce an element")));

		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
		ON_SCOPE_EXIT
		{
			FPCGContext::Release(Context.Release());
		};

		ASSERT_THAT(IsTrue(RunToCompletion(Element, Context.Get()),
			TEXT("The element must finish within the spin-guard budget, not hang")));

		const TArray<FVector> Locations = CollectOutputLocations(Context->OutputData);
		const int32 Requested = 20 * Settings->PointsPerParent;

		UE_LOGF(LogArcPCGClusteredScatterTest, Display,"[SurfaceMisses] accepted=%d of %d requested", Locations.Num(), Requested);

		ASSERT_THAT(IsTrue(Locations.Num() > 0, TEXT("Some offspring must land inside the footprint and be emitted")));
		ASSERT_THAT(IsTrue(Locations.Num() < Requested,
			TEXT("Some offspring must land in the Bounds-but-outside-Footprint half and be DROPPED — if this ")
			TEXT("fails, the miss path silently stopped dropping points or the fixture no longer discriminates ")
			TEXT("bounds from footprint")));

		for (const FVector& Location : Locations)
		{
			ASSERT_THAT(IsTrue(Location.X >= -1000.0 - KINDA_SMALL_NUMBER && Location.X <= 0.0 + KINDA_SMALL_NUMBER,
				TEXT("Every EMITTED point must lie within the footprint half, never at a fallback")));
			ASSERT_THAT(IsTrue(Location.Y >= -1000.0 - KINDA_SMALL_NUMBER && Location.Y <= 1000.0 + KINDA_SMALL_NUMBER,
				TEXT("Every EMITTED point must lie within the footprint half, never at a fallback")));
		}
	}

	// MaxPoints must bind by REDUCING WHOLE PATCHES: 20 parents * 8 points = 160 requested, MaxPoints =
	// 100 (not a multiple of 8) must reduce to floor(100/8) = 12 parents -> exactly 96 points, an EXACT
	// multiple of PointsPerParent, never a truncated 100.
	TEST_METHOD(MaxPointsBinds_ReducesWholePatches)
	{
		using namespace PCGClusteredScatterTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGClusteredScatterSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGClusteredScatterSettings>(TestData);

		const FBox Bounds(FVector(-5000.0, -5000.0, -100.0), FVector(5000.0, 5000.0, 100.0));
		Settings->ParentDensityPerM2 = 0.002; // NumParents = 20 over the 10000 m^2 Bounds
		Settings->PatchRadiusSigmaCm = 50.0;
		Settings->PointsPerParent = 8;
		Settings->MaxPoints = 100;

		FPCGTaggedData& SurfaceInput = TestData.InputData.TaggedData.Emplace_GetRef();
		SurfaceInput.Pin = PCGClusteredScatterConstants::SurfaceLabel;
		SurfaceInput.Data = MakeValidFlatSurface(Bounds, 0.0f);

		FPCGElementPtr Element = TestData.Settings->GetElement();
		ASSERT_THAT(IsTrue(Element.IsValid(), TEXT("Settings must produce an element")));

		// Deleting the MaxPoints warning from production leaves every other assertion in this test
		// green, so it must be pinned directly.
		TestRunner->AddExpectedMessage(TEXT("reducing patches"), ELogVerbosity::Warning,
			EAutomationExpectedMessageFlags::Contains, 1);

		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
		ON_SCOPE_EXIT
		{
			FPCGContext::Release(Context.Release());
		};

		ASSERT_THAT(IsTrue(RunToCompletion(Element, Context.Get()),
			TEXT("The element must finish within the spin-guard budget, not hang")));

		const TArray<FVector> Locations = CollectOutputLocations(Context->OutputData);
		ASSERT_THAT(AreEqual(96, Locations.Num(),
			TEXT("MaxPoints must reduce the PARENT count to floor(MaxPoints/PointsPerParent), giving an ")
			TEXT("EXACT multiple of PointsPerParent (96), never a truncated 100")));
	}

	// bWritePatchIdAttribute = false must suppress the attribute entirely, not just leave it unfilled.
	TEST_METHOD(WritePatchIdFalse_OmitsPatchIdAttribute)
	{
		using namespace PCGClusteredScatterTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGClusteredScatterSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGClusteredScatterSettings>(TestData);

		const FBox Bounds(FVector(-5000.0, -5000.0, -100.0), FVector(5000.0, 5000.0, 100.0));
		Settings->ParentDensityPerM2 = 0.001;
		Settings->PatchRadiusSigmaCm = 50.0;
		Settings->PointsPerParent = 8;
		Settings->bWritePatchIdAttribute = false;

		FPCGTaggedData& SurfaceInput = TestData.InputData.TaggedData.Emplace_GetRef();
		SurfaceInput.Pin = PCGClusteredScatterConstants::SurfaceLabel;
		SurfaceInput.Data = MakeValidFlatSurface(Bounds, 0.0f);

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
		const UPCGBasePointData* PointData = Cast<UPCGBasePointData>(Context->OutputData.TaggedData[0].Data);
		ASSERT_THAT(IsTrue(PointData != nullptr && PointData->GetNumPoints() > 0,
			TEXT("Non-vacuity: the run must emit points even with the attribute switched off")));

		const UPCGMetadata* Metadata = PointData->ConstMetadata();
		ASSERT_THAT(IsTrue(Metadata != nullptr, TEXT("Output must carry metadata even with the attribute switched off")));
		const FPCGMetadataDomain* Domain = Metadata->GetConstMetadataDomain(PCGMetadataDomainID::Elements);
		ASSERT_THAT(IsTrue(Domain != nullptr, TEXT("Output must have the Elements metadata domain")));

		// HasAttribute, not GetConstTypedAttribute<int32>() != nullptr: the latter also returns null for
		// "present but wrong type" and cannot distinguish that from "omitted".
		ASSERT_THAT(IsTrue(!Domain->HasAttribute(Settings->PatchIdAttributeName),
			TEXT("bWritePatchIdAttribute = false must omit the attribute entirely, not merely leave it unfilled")));
	}
};

#endif // WITH_EDITOR
