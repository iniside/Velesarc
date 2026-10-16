// Copyright Lukasz Baran. All Rights Reserved.

// PCGScatterNearPointsTests.cpp
// Headless PCG element CQTest coverage for Scatter/PCGScatterNearPointsSettings.{h,cpp} (node A,
// "Scatter Near Points"), driven through the REAL FPCGScatterNearPointsElement::Execute state
// machine via the shipped PCGTestsCommon::FTestData harness — the same pattern as
// ArcForestPCGSmokeTests.cpp. WITH_EDITOR only: FTestData needs an editor world.
//
// This is the first test in this module to build a POINT-DATA input. The Surface pin is
// UArcForestTestSurfaceData (a UPCGVolumeData subclass) — its SamplePoint override is what the base
// UPCGSpatialData::ProjectPoint falls through to (Node A never overrides ProjectPoint itself).
//
// CAVEAT (pin type is NOT proven by this suite): GetSpatialInputsByPin (PCGData.cpp) filters on pin
// label + IsA<UPCGSpatialData>() and never checks EPCGDataType, so a green suite here is not evidence
// about what the Surface pin accepts in a real graph connection — that is enforced by the pin's
// declared EPCGDataType::Spatial at editor-connection time, which this headless harness never reaches.
//
// CAVEAT (the invalid-bounds case does NOT prove the World Ray Hit case): UArcForestTestSurfaceData's
// SamplePoint (with the bHonorQueryZ extension added for this suite) reports back whatever Z the CALLER
// queried at. UPCGWorldRayHitData does the opposite — SamplePoint there ORTHOGONALLY PROJECTS the query
// onto the ray-origin plane and discards the incoming Z outright (PCGWorldData.cpp:447-448, see the
// Step 2b header comment on PCGScatterNearPointsSettings.h). So InvalidBounds_* below only proves that
// the node's "no bounds -> query at each source's own Z" BRANCH RUNS and still emits points — it is not
// evidence about the real trace origin on a World Ray Hit Query input.
//
// TEST-FIXTURE EXTENSION (in scope — it is test code, not production): UArcForestTestSurfaceData grew
// two new fields for this suite, FootprintOverride and bHonorQueryZ (both default to "off", so every
// existing forest-sim test is unaffected):
//   - FootprintOverride decouples SamplePoint's XY containment check from GetBounds(), which is
//     required to exercise the invalid-bounds branch: that branch needs GetBounds().IsValid() == false
//     (so Initialize() must never be called), but SamplePoint would otherwise use that same invalid
//     GetBounds() as its footprint and reject every query, making the branch indistinguishable from
//     "everything missed".
//   - bHonorQueryZ makes SamplePoint report the incoming query Z instead of FlatSurfaceZ/HeightFunction,
//     which is what makes the QueryZ the node actually computed (Bounds.Max.Z vs SourceLocation.Z)
//     OBSERVABLE in the output point's Z.
//
// BOUNDS VALIDITY IS A TESTED AXIS, NOT AN ACCIDENT: FBox(ForceInit) is invalid, so a fixture left at
// default bounds silently takes the invalid-bounds fallback path. Every case below either calls
// Initialize() with a real box (valid-bounds path) or deliberately withholds it (invalid-bounds path),
// never leaving it to chance.
//
// THIS WHOLE FILE IS #if WITH_EDITOR (below), and the module also allows Program targets — under a
// Program target every TEST_METHOD in this file compiles to nothing and a run of the
// ArcPCGExtension.Scatter.NearPoints filter still reports SUCCESS with zero tests executed. A verify
// step for this suite MUST assert an expected FOUND COUNT (9 TEST_METHODs as of this writing), never
// merely "no failures". ArcPCGScatterCoreTests.cpp (the sibling pure-math suite, 9 TEST_METHODs) is NOT
// gated behind WITH_EDITOR, so it does not share this failure mode.

#include "CQTest.h"

#if WITH_EDITOR

#include "Containers/ArrayView.h"
#include "Containers/Ticker.h"
#include "Misc/ScopeExit.h"

#include "ArcForestTestSurfaceData.h"
#include "Scatter/PCGScatterNearPointsSettings.h"

#include "Tests/PCGTestsCommon.h"
#include "PCGContext.h"
#include "PCGData.h"
#include "PCGPoint.h"
#include "Data/PCGBasePointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataCommon.h"
#include "Metadata/PCGMetadataDomain.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcPCGScatterNearPointsTest, Log, All);

namespace PCGScatterNearPointsTestsLocal
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

	/** A flat, fully-suitable-in-XY surface with real (valid) bounds. */
	UArcForestTestSurfaceData* MakeValidFlatSurface(const FBox& Bounds, float FlatZ = 0.0f)
	{
		UArcForestTestSurfaceData* Surface = NewObject<UArcForestTestSurfaceData>();
		Surface->Initialize(Bounds);
		Surface->FlatSurfaceZ = FlatZ;
		return Surface;
	}

	/**
	 * A surface that reports INVALID bounds (Initialize() is deliberately never called), but whose
	 * SamplePoint still accepts the given footprint via FootprintOverride. This is the fixture shape
	 * that exercises the node's Step 2b fallback branch.
	 */
	UArcForestTestSurfaceData* MakeInvalidBoundsSurfaceWithFootprint(const FBox& Footprint)
	{
		UArcForestTestSurfaceData* Surface = NewObject<UArcForestTestSurfaceData>();
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

	/** Sorts a copy of Locations lexicographically (X, then Y, then Z) so two sets can be order-independently compared. */
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
	 * pattern). Returns false if the guard tripped — callers MUST assert on this: a hung element that
	 * silently breaks out here would otherwise go on to fail later on a misleading, unrelated count
	 * assertion instead of the actual "the element never finished" defect.
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

TEST_CLASS(ArcPCGScatterNearPoints, "ArcPCGExtension.Scatter.NearPoints")
{
	// Previously-wrong branch #1: an unconnected Surface pin must yield a clean, empty output and must
	// not crash the element (the pre-fix path had no such guard).
	TEST_METHOD(NoSurfaceInput_EmitsEmptyOutputAndDoesNotCrash)
	{
		using namespace PCGScatterNearPointsTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGScatterNearPointsSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGScatterNearPointsSettings>(TestData);
		ASSERT_THAT(IsTrue(Settings != nullptr, TEXT("Settings must be generated")));

		// Points pin is wired, but Surface is deliberately left unconnected.
		const FVector SourceLocation(0.0, 0.0, 0.0);
		FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
		PointsInput.Pin = PCGScatterNearPointsConstants::PointsLabel;
		PointsInput.Data = MakeSourcePoints(MakeArrayView(&SourceLocation, 1));

		FPCGElementPtr Element = TestData.Settings->GetElement();
		ASSERT_THAT(IsTrue(Element.IsValid(), TEXT("Settings must produce an element")));

		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
		ON_SCOPE_EXIT
		{
			FPCGContext::Release(Context.Release());
		};

		ASSERT_THAT(IsTrue(RunToCompletion(Element, Context.Get()),
			TEXT("The element must finish within the spin-guard budget, not hang")));

		ASSERT_THAT(IsTrue(Context->OutputData.TaggedData.Num() == 0,
			TEXT("No Surface input must emit NOTHING, not a fallback or an empty-but-present point data")));
	}

	// Previously-wrong branch #2: a dart that lands outside the surface's XY footprint must be DROPPED,
	// never emitted at a fallback height. The footprint (1000x1000 cm square) is deliberately smaller
	// than the source spread + MaxRadiusCm (800 cm), so with the default flat fixture every XY inside
	// bounds would be accepted and this case would be vacuous without that mismatch.
	TEST_METHOD(DartsOutsideFootprint_AreDroppedNotEmittedAtFallback)
	{
		using namespace PCGScatterNearPointsTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGScatterNearPointsSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGScatterNearPointsSettings>(TestData);
		Settings->MinRadiusCm = 0.0;
		Settings->MaxRadiusCm = 800.0; // exceeds the 500 cm half-width of the footprint below
		Settings->PointsPerSource = 300;

		const FBox Footprint(FVector(-500.0, -500.0, -100.0), FVector(500.0, 500.0, 100.0));
		FPCGTaggedData& SurfaceInput = TestData.InputData.TaggedData.Emplace_GetRef();
		SurfaceInput.Pin = PCGScatterNearPointsConstants::SurfaceLabel;
		SurfaceInput.Data = MakeValidFlatSurface(Footprint, 0.0f);

		const FVector SourceLocation(0.0, 0.0, 0.0); // centred in the footprint
		FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
		PointsInput.Pin = PCGScatterNearPointsConstants::PointsLabel;
		PointsInput.Data = MakeSourcePoints(MakeArrayView(&SourceLocation, 1));

		FPCGElementPtr Element = TestData.Settings->GetElement();
		ASSERT_THAT(IsTrue(Element.IsValid(), TEXT("Settings must produce an element")));

		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
		ON_SCOPE_EXIT
		{
			FPCGContext::Release(Context.Release());
		};

		ASSERT_THAT(IsTrue(RunToCompletion(Element, Context.Get()),
			TEXT("The element must finish within the spin-guard budget, not hang")));

		const TArray<FVector> OutputLocations = CollectOutputLocations(Context->OutputData);
		UE_LOGF(LogArcPCGScatterNearPointsTest, Display,"[DartsOutsideFootprint] accepted=%d of %d darts", OutputLocations.Num(), Settings->PointsPerSource);

		// Non-vacuity in BOTH directions: some darts must hit (proves the happy path still works) and
		// some must miss (proves the annulus actually reaches outside the footprint at MaxRadiusCm=800).
		ASSERT_THAT(IsTrue(OutputLocations.Num() > 0,
			TEXT("Some darts must land inside the footprint and be emitted")));
		ASSERT_THAT(IsTrue(OutputLocations.Num() < Settings->PointsPerSource,
			TEXT("Some darts must land outside the footprint and be DROPPED — if this fails, either the ")
			TEXT("miss path silently stopped dropping points or the fixture no longer rejects off-footprint XY")));

		for (const FVector& Location : OutputLocations)
		{
			ASSERT_THAT(IsTrue(Location.X >= -500.0 - KINDA_SMALL_NUMBER && Location.X <= 500.0 + KINDA_SMALL_NUMBER,
				TEXT("Every emitted point's X must lie within the footprint — a fallback-Z regression would ")
				TEXT("still emit the raw, off-footprint dart XY")));
			ASSERT_THAT(IsTrue(Location.Y >= -500.0 - KINDA_SMALL_NUMBER && Location.Y <= 500.0 + KINDA_SMALL_NUMBER,
				TEXT("Every emitted point's Y must lie within the footprint")));
		}
	}

	// Previously-wrong branch #3: ParentIndex must index the SOURCE set the point came from.
	TEST_METHOD(ParentIndexAttribute_ValuesIndexTheSourceSet)
	{
		using namespace PCGScatterNearPointsTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGScatterNearPointsSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGScatterNearPointsSettings>(TestData);
		Settings->MinRadiusCm = 10.0;
		Settings->MaxRadiusCm = 50.0;
		Settings->PointsPerSource = 6;
		Settings->bWriteParentIndexAttribute = true;

		// Large footprint relative to the small annulus: every dart hits, so ParentIndex coverage is
		// not accidentally starved by misses.
		const FBox Footprint(FVector(-5000.0, -5000.0, -100.0), FVector(5000.0, 5000.0, 100.0));
		FPCGTaggedData& SurfaceInput = TestData.InputData.TaggedData.Emplace_GetRef();
		SurfaceInput.Pin = PCGScatterNearPointsConstants::SurfaceLabel;
		SurfaceInput.Data = MakeValidFlatSurface(Footprint, 0.0f);

		const TArray<FVector> SourceLocations = {
			FVector(0.0, 0.0, 0.0), FVector(500.0, 0.0, 0.0), FVector(0.0, 500.0, 0.0),
			FVector(500.0, 500.0, 0.0), FVector(-500.0, -500.0, 0.0)
		};
		const int32 NumSources = SourceLocations.Num();
		FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
		PointsInput.Pin = PCGScatterNearPointsConstants::PointsLabel;
		PointsInput.Data = MakeSourcePoints(SourceLocations);

		FPCGElementPtr Element = TestData.Settings->GetElement();
		ASSERT_THAT(IsTrue(Element.IsValid(), TEXT("Settings must produce an element")));

		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
		ON_SCOPE_EXIT
		{
			FPCGContext::Release(Context.Release());
		};

		ASSERT_THAT(IsTrue(RunToCompletion(Element, Context.Get()),
			TEXT("The element must finish within the spin-guard budget, not hang")));

		ASSERT_THAT(IsTrue(Context->OutputData.TaggedData.Num() == 1,
			TEXT("One input tagged data must yield exactly one output tagged data")));

		const UPCGBasePointData* PointData = Cast<UPCGBasePointData>(Context->OutputData.TaggedData[0].Data);
		ASSERT_THAT(IsTrue(PointData != nullptr, TEXT("Output must be point data")));
		ASSERT_THAT(IsTrue(PointData->GetNumPoints() > 0, TEXT("Non-vacuity: the run must emit points")));

		const UPCGMetadata* Metadata = PointData->ConstMetadata();
		ASSERT_THAT(IsTrue(Metadata != nullptr, TEXT("Output must carry metadata")));
		const FPCGMetadataDomain* Domain = Metadata->GetConstMetadataDomain(PCGMetadataDomainID::Elements);
		ASSERT_THAT(IsTrue(Domain != nullptr, TEXT("Output must have the Elements metadata domain")));
		const FPCGMetadataAttribute<int32>* ParentIndexAttr = Domain->GetConstTypedAttribute<int32>(Settings->ParentIndexAttributeName);
		ASSERT_THAT(IsTrue(ParentIndexAttr != nullptr, TEXT("ParentIndex attribute must be present as int32")));

		// The fixture makes the TRUE check free: sources are 500 cm apart and the annulus is 10-50 cm,
		// so the correct parent is provably the NEAREST source. A range check alone (0 <= idx <
		// NumSources) stays green under three mutants: a constant 0 for every point, a reversed mapping
		// (NumSources-1-SourceIndex), and every point sharing metadata entry key 0 (a plausible
		// InitializeOnSet/EntryRange misuse). The distance check below kills the first two; the observed
		// set covering every source index kills the third (a shared entry key collapses onto ONE value,
		// not a set of 5).
		const FPCGPointTransform::ConstValueRange OutTransformRange = PointData->GetConstTransformValueRange();
		const FPCGPointMetadataEntry::ConstValueRange EntryRange = PointData->GetConstMetadataEntryValueRange();
		TSet<int32> ObservedParentIndices;
		for (int32 Index = 0; Index < PointData->GetNumPoints(); ++Index)
		{
			const int32 ParentIndex = ParentIndexAttr->GetValueFromItemKey(EntryRange[Index]);
			ASSERT_THAT(IsTrue(ParentIndex >= 0 && ParentIndex < NumSources,
				TEXT("Every ParentIndex value must index the source set actually supplied")));

			const FVector& PointLocation = OutTransformRange[Index].GetLocation();
			const double DistanceToClaimedParent = FVector::Dist2D(PointLocation, SourceLocations[ParentIndex]);
			ASSERT_THAT(IsTrue(DistanceToClaimedParent <= Settings->MaxRadiusCm + KINDA_SMALL_NUMBER,
				TEXT("The claimed ParentIndex must be the source the point was actually scattered around — ")
				TEXT("a constant-0 or reversed-mapping ParentIndex would point at a source far outside the ")
				TEXT("annulus radius")));

			ObservedParentIndices.Add(ParentIndex);
		}

		ASSERT_THAT(IsTrue(ObservedParentIndices.Num() == NumSources,
			TEXT("Every one of the 5 sources must appear as SOME point's ParentIndex — a shared metadata ")
			TEXT("entry key (all points aliasing entry 0) would collapse this set to a single value")));
	}

	// Previously-wrong branch #4: the same source points delivered as ONE tagged data vs. TWO must
	// produce SET-IDENTICAL output positions, not merely an equal count — a count-only assertion also
	// passes under an index-derived (not position-derived) stream, which is exactly the regression this
	// pins. The footprint is large enough that every dart hits in both runs, so nothing here depends on
	// which tagged data a source happened to land in.
	TEST_METHOD(ChunkInvariance_SplitInputProducesSetIdenticalPositions)
	{
		using namespace PCGScatterNearPointsTestsLocal;

		const TArray<FVector> SourceLocations = {
			FVector(0.0, 0.0, 0.0), FVector(1000.0, 0.0, 0.0), FVector(2000.0, 0.0, 0.0),
			FVector(0.0, 1000.0, 0.0), FVector(1000.0, 1000.0, 0.0), FVector(2000.0, 1000.0, 0.0)
		};
		const FBox Footprint(FVector(-3000.0, -3000.0, -100.0), FVector(5000.0, 5000.0, 100.0));

		TFunction<TArray<FVector>(const TArray<TArrayView<const FVector>>&)> RunScatter =
			[&Footprint](const TArray<TArrayView<const FVector>>& DataGroups) -> TArray<FVector>
		{
			PCGTestsCommon::FTestData TestData;
			UPCGScatterNearPointsSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGScatterNearPointsSettings>(TestData);
			Settings->MinRadiusCm = 50.0;
			Settings->MaxRadiusCm = 150.0;
			Settings->PointsPerSource = 5;

			FPCGTaggedData& SurfaceInput = TestData.InputData.TaggedData.Emplace_GetRef();
			SurfaceInput.Pin = PCGScatterNearPointsConstants::SurfaceLabel;
			SurfaceInput.Data = MakeValidFlatSurface(Footprint, 0.0f);

			for (const TArrayView<const FVector>& Group : DataGroups)
			{
				FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
				PointsInput.Pin = PCGScatterNearPointsConstants::PointsLabel;
				PointsInput.Data = MakeSourcePoints(Group);
			}

			FPCGElementPtr Element = TestData.Settings->GetElement();
			check(Element.IsValid());

			TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
			ON_SCOPE_EXIT
			{
				FPCGContext::Release(Context.Release());
			};

			// verify(), NOT check(): check() does not evaluate its expression when DO_CHECK == 0, which
			// would silently skip the element entirely in such a build. ASSERT_THAT is unusable here
			// because it expands to a bare `return` inside a lambda returning TArray<FVector>.
			verify(RunToCompletion(Element, Context.Get()));
			return SortedCopy(CollectOutputLocations(Context->OutputData));
		};

		const TArrayView<const FVector> WholeGroup(SourceLocations);
		const TArray<TArrayView<const FVector>> OneChunk = { WholeGroup };
		const TArray<FVector> CombinedResult = RunScatter(OneChunk);

		const TArrayView<const FVector> FirstHalf(SourceLocations.GetData(), 3);
		const TArrayView<const FVector> SecondHalf(SourceLocations.GetData() + 3, 3);
		const TArray<TArrayView<const FVector>> TwoChunks = { FirstHalf, SecondHalf };
		const TArray<FVector> SplitResult = RunScatter(TwoChunks);

		ASSERT_THAT(IsTrue(CombinedResult.Num() > 0, TEXT("Non-vacuity: the combined run must emit points")));
		ASSERT_THAT(AreEqual(CombinedResult.Num(), SplitResult.Num(),
			TEXT("Splitting the same source points across two tagged data must not change the accepted count")));

		for (int32 Index = 0; Index < CombinedResult.Num() && Index < SplitResult.Num(); ++Index)
		{
			ASSERT_THAT(IsTrue(CombinedResult[Index].Equals(SplitResult[Index], 1e-6),
				TEXT("The SET of output positions must be identical whether the source points arrive as ")
				TEXT("one tagged data or split across two — position-derived seeding must not depend on chunking")));
		}
	}

	// Previously-wrong branch #5: MaxRadiusCm < MinRadiusCm must CLAMP Max up to Min (a degenerate ring
	// exactly at MinRadiusCm), never swap the two into a still-valid but inverted annulus.
	TEST_METHOD(MaxRadiusBelowMin_ClampsUpToMinNotSwapped)
	{
		using namespace PCGScatterNearPointsTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGScatterNearPointsSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGScatterNearPointsSettings>(TestData);
		Settings->MinRadiusCm = 200.0;
		Settings->MaxRadiusCm = 50.0; // below Min — must clamp UP to 200, not swap to a [50,200] annulus
		Settings->PointsPerSource = 40;

		const FBox Footprint(FVector(-2000.0, -2000.0, -100.0), FVector(2000.0, 2000.0, 100.0));
		FPCGTaggedData& SurfaceInput = TestData.InputData.TaggedData.Emplace_GetRef();
		SurfaceInput.Pin = PCGScatterNearPointsConstants::SurfaceLabel;
		SurfaceInput.Data = MakeValidFlatSurface(Footprint, 0.0f);

		const FVector SourceLocation(0.0, 0.0, 0.0);
		FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
		PointsInput.Pin = PCGScatterNearPointsConstants::PointsLabel;
		PointsInput.Data = MakeSourcePoints(MakeArrayView(&SourceLocation, 1));

		FPCGElementPtr Element = TestData.Settings->GetElement();
		ASSERT_THAT(IsTrue(Element.IsValid(), TEXT("Settings must produce an element")));

		// Deleting the clamp warning from production leaves every other assertion in this test green, so
		// it must be pinned directly. Registered before RunToCompletion, which is what actually emits it.
		// Occurrences = 2: the node logs it BOTH via UE_LOG(LogArcPCGForest, Warning, ...) (console/log
		// file) and via PCGLog::LogWarningOnGraph (the graph node warning), and both message texts
		// contain this substring.
		TestRunner->AddExpectedMessage(TEXT("clamping Max up to Min"), ELogVerbosity::Warning,
			EAutomationExpectedMessageFlags::Contains, 2);

		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
		ON_SCOPE_EXIT
		{
			FPCGContext::Release(Context.Release());
		};

		ASSERT_THAT(IsTrue(RunToCompletion(Element, Context.Get()),
			TEXT("The element must finish within the spin-guard budget, not hang")));

		const TArray<FVector> OutputLocations = CollectOutputLocations(Context->OutputData);
		ASSERT_THAT(AreEqual(Settings->PointsPerSource, OutputLocations.Num(),
			TEXT("Every dart must hit inside the generous footprint")));

		for (const FVector& Location : OutputLocations)
		{
			const double DistanceFromSource = FVector::Dist2D(Location, SourceLocation);
			// A swap (instead of a clamp-up) would give a [50, 200] annulus, whose radii vary — the
			// tight tolerance here only survives a degenerate ring pinned exactly at MinRadiusCm.
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(DistanceFromSource, 200.0, 1e-6),
				TEXT("With Max clamped up to Min, every dart must land at EXACTLY MinRadiusCm — a swap ")
				TEXT("would instead vary distances across [50, 200]")));
		}
	}

	// Previously-wrong branch #6 (Step 2, the valid-bounds side): the query origin for a bounded
	// surface must be the surface's OWN bounds top (Bounds.Max.Z), not the source point's Z.
	TEST_METHOD(ValidBounds_QueryHeightIsSurfaceBoundsMaxZ)
	{
		using namespace PCGScatterNearPointsTestsLocal;

		constexpr double SurfaceTopZ = 250.0;

		PCGTestsCommon::FTestData TestData;
		UPCGScatterNearPointsSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGScatterNearPointsSettings>(TestData);
		Settings->MinRadiusCm = 0.0;
		Settings->MaxRadiusCm = 10.0;
		Settings->PointsPerSource = 1;

		const FBox Bounds(FVector(-2000.0, -2000.0, -100.0), FVector(2000.0, 2000.0, SurfaceTopZ));
		UArcForestTestSurfaceData* Surface = MakeValidFlatSurface(Bounds, 0.0f);
		Surface->bHonorQueryZ = true; // makes the QUERIED Z observable in the output point's Z

		FPCGTaggedData& SurfaceInput = TestData.InputData.TaggedData.Emplace_GetRef();
		SurfaceInput.Pin = PCGScatterNearPointsConstants::SurfaceLabel;
		SurfaceInput.Data = Surface;

		// Source Z is deliberately far from SurfaceTopZ, so a regression that reads SourceLocation.Z
		// instead of Bounds.Max.Z would be caught by the assertion below.
		const FVector SourceLocation(0.0, 0.0, -999.0);
		FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
		PointsInput.Pin = PCGScatterNearPointsConstants::PointsLabel;
		PointsInput.Data = MakeSourcePoints(MakeArrayView(&SourceLocation, 1));

		FPCGElementPtr Element = TestData.Settings->GetElement();
		ASSERT_THAT(IsTrue(Element.IsValid(), TEXT("Settings must produce an element")));

		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
		ON_SCOPE_EXIT
		{
			FPCGContext::Release(Context.Release());
		};

		ASSERT_THAT(IsTrue(RunToCompletion(Element, Context.Get()),
			TEXT("The element must finish within the spin-guard budget, not hang")));

		const TArray<FVector> OutputLocations = CollectOutputLocations(Context->OutputData);
		ASSERT_THAT(IsTrue(OutputLocations.Num() == 1, TEXT("The single dart must hit inside bounds")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(OutputLocations[0].Z, SurfaceTopZ, 1e-6),
			TEXT("A valid-bounds surface must be queried at Bounds.Max.Z, not the source point's own Z")));
	}

	// Step 3 gap-closure: every OTHER case in this suite uses a flat surface (HeightFunction unbound),
	// so nothing exercises a surface whose height VARIES with XY. Binds an analytic HeightFunction and
	// asserts both that the emitted Z follows it and that the darts land at distinct XY — the second is
	// what actually distinguishes "projected at its own scattered XY" from "projected at the source XY".
	TEST_METHOD(ValidBounds_ProjectsOntoHeightFunction)
	{
		using namespace PCGScatterNearPointsTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGScatterNearPointsSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGScatterNearPointsSettings>(TestData);
		Settings->MinRadiusCm = 10.0;
		Settings->MaxRadiusCm = 50.0;
		Settings->PointsPerSource = 8;

		const FBox Bounds(FVector(-2000.0, -2000.0, -100.0), FVector(2000.0, 2000.0, 500.0));
		UArcForestTestSurfaceData* Surface = MakeValidFlatSurface(Bounds, 0.0f);
		// A tilted plane, Z = X + 2*Y (cm) — derivable on paper at any (X, Y). It is enough here: what
		// matters is that Z VARIES with XY, so a projection that ignored the surface would show up as a
		// constant Z. (Not a curved surface; the suite has no coverage of one.)
		Surface->HeightFunction = [](float X, float Y) -> float
		{
			return X + 2.0f * Y;
		};

		FPCGTaggedData& SurfaceInput = TestData.InputData.TaggedData.Emplace_GetRef();
		SurfaceInput.Pin = PCGScatterNearPointsConstants::SurfaceLabel;
		SurfaceInput.Data = Surface;

		const FVector SourceLocation(100.0, -50.0, 0.0);
		FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
		PointsInput.Pin = PCGScatterNearPointsConstants::PointsLabel;
		PointsInput.Data = MakeSourcePoints(MakeArrayView(&SourceLocation, 1));

		FPCGElementPtr Element = TestData.Settings->GetElement();
		ASSERT_THAT(IsTrue(Element.IsValid(), TEXT("Settings must produce an element")));

		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
		ON_SCOPE_EXIT
		{
			FPCGContext::Release(Context.Release());
		};

		ASSERT_THAT(IsTrue(RunToCompletion(Element, Context.Get()),
			TEXT("The element must finish within the spin-guard budget, not hang")));

		const TArray<FVector> OutputLocations = CollectOutputLocations(Context->OutputData);
		ASSERT_THAT(IsTrue(OutputLocations.Num() > 0, TEXT("Non-vacuity: at least one dart must hit")));

		// The Z relation ALONE cannot tell emitted-XY from source-XY: UArcForestTestSurfaceData echoes
		// back whatever XY it was queried at and overwrites only Z, so f(XY) == f(XY) holds either way.
		// Dispersion is what discriminates — under a "query at the source XY" regression all 8 darts
		// collapse onto one location.
		TSet<FIntVector2> DistinctXY;
		for (const FVector& Location : OutputLocations)
		{
			DistinctXY.Add(FIntVector2(FMath::RoundToInt(Location.X), FMath::RoundToInt(Location.Y)));
		}
		ASSERT_THAT(IsTrue(DistinctXY.Num() > 1,
			TEXT("Darts must be projected at their own scattered XY, not all at the source XY — ")
			TEXT("a single distinct XY across 8 darts means the annulus offset was dropped")));

		for (const FVector& Location : OutputLocations)
		{
			const double ExpectedZ = Location.X + 2.0 * Location.Y;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Location.Z, ExpectedZ, 1e-3),
				TEXT("Every emitted point's Z must follow HeightFunction at its own XY — ")
				TEXT("a flat-projection regression would emit a constant Z")));
		}
	}

	// Previously-wrong branch #7 (Step 2b, the fix this suite must pin): an unbounded surface must
	// query EACH SOURCE POINT'S OWN Z and STILL EMIT POINTS. RED on a regression to Step 2's original
	// hard refusal (warn + empty output on invalid bounds).
	TEST_METHOD(InvalidBounds_QueryHeightIsSourcePointZ_PointsStillEmitted)
	{
		using namespace PCGScatterNearPointsTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGScatterNearPointsSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGScatterNearPointsSettings>(TestData);
		Settings->MinRadiusCm = 0.0;
		Settings->MaxRadiusCm = 10.0;
		Settings->PointsPerSource = 1;

		const FBox Footprint(FVector(-2000.0, -2000.0, -100.0), FVector(2000.0, 2000.0, 100.0));
		UArcForestTestSurfaceData* Surface = MakeInvalidBoundsSurfaceWithFootprint(Footprint); // Initialize() NEVER called
		Surface->bHonorQueryZ = true;
		ASSERT_THAT(IsTrue(Surface->GetBounds().IsValid == 0,
			TEXT("Test setup check: the surface must report INVALID bounds for this case to be meaningful")));

		FPCGTaggedData& SurfaceInput = TestData.InputData.TaggedData.Emplace_GetRef();
		SurfaceInput.Pin = PCGScatterNearPointsConstants::SurfaceLabel;
		SurfaceInput.Data = Surface;

		constexpr double SourceZ = 777.0;
		const FVector SourceLocation(0.0, 0.0, SourceZ);
		FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
		PointsInput.Pin = PCGScatterNearPointsConstants::PointsLabel;
		PointsInput.Data = MakeSourcePoints(MakeArrayView(&SourceLocation, 1));

		FPCGElementPtr Element = TestData.Settings->GetElement();
		ASSERT_THAT(IsTrue(Element.IsValid(), TEXT("Settings must produce an element")));

		// Deleting the invalid-bounds graph warning from production leaves every other assertion in this
		// test green, so it must be pinned directly. Registered before RunToCompletion, which is what
		// actually emits it.
		TestRunner->AddExpectedMessage(TEXT("querying at each source point's own height"), ELogVerbosity::Warning,
			EAutomationExpectedMessageFlags::Contains, 1);

		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
		ON_SCOPE_EXIT
		{
			FPCGContext::Release(Context.Release());
		};

		ASSERT_THAT(IsTrue(RunToCompletion(Element, Context.Get()),
			TEXT("The element must finish within the spin-guard budget, not hang")));

		const TArray<FVector> OutputLocations = CollectOutputLocations(Context->OutputData);
		ASSERT_THAT(IsTrue(OutputLocations.Num() == 1,
			TEXT("An unbounded surface must still emit points — a regression to Step 2's hard refusal ")
			TEXT("would leave this empty")));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(OutputLocations[0].Z, SourceZ, 1e-6),
			TEXT("On an unbounded surface, the query height must be the SOURCE POINT'S OWN Z")));
	}

	// bWriteParentIndexAttribute = false must suppress the attribute entirely, not just skip filling it.
	TEST_METHOD(WriteParentIndexFalse_OmitsParentIndexAttribute)
	{
		using namespace PCGScatterNearPointsTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGScatterNearPointsSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGScatterNearPointsSettings>(TestData);
		Settings->MinRadiusCm = 10.0;
		Settings->MaxRadiusCm = 50.0;
		Settings->PointsPerSource = 4;
		Settings->bWriteParentIndexAttribute = false;

		const FBox Footprint(FVector(-2000.0, -2000.0, -100.0), FVector(2000.0, 2000.0, 100.0));
		FPCGTaggedData& SurfaceInput = TestData.InputData.TaggedData.Emplace_GetRef();
		SurfaceInput.Pin = PCGScatterNearPointsConstants::SurfaceLabel;
		SurfaceInput.Data = MakeValidFlatSurface(Footprint, 0.0f);

		const FVector SourceLocation(0.0, 0.0, 0.0);
		FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
		PointsInput.Pin = PCGScatterNearPointsConstants::PointsLabel;
		PointsInput.Data = MakeSourcePoints(MakeArrayView(&SourceLocation, 1));

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

		// GetConstTypedAttribute<int32>() != nullptr also returns null for "present but wrong type", so
		// it cannot distinguish "omitted" from "written as, say, a float" — use HasAttribute, which
		// checks presence under any type.
		ASSERT_THAT(IsTrue(!Domain->HasAttribute(Settings->ParentIndexAttributeName),
			TEXT("bWriteParentIndexAttribute = false must omit the attribute entirely, not merely leave it unfilled")));
	}
};

#endif // WITH_EDITOR
