// Copyright Lukasz Baran. All Rights Reserved.

// ArcForestPCGSmokeTests.cpp
// TIER 2 headless PCG smoke test (M6) for the forest-sim node: constructs a UPCGForestSimSettings,
// feeds a minimal synthetic (non-landscape) samplable surface, drives FPCGForestSimElement to
// completion across its solve/emit time-slices via the shipped PCGTestsCommon::FTestData harness,
// and asserts the output point data exists, is non-empty, and carries every required ecological
// attribute at the correct type.
//
// It runs the REAL element (Execute state machine, not a hand-call of ExecuteInternal): the two
// existing PCG element unit tests use exactly this `while (!Element->Execute(Context)) {}` pattern.
// FTestData needs GEditor/editor world, so the whole case is WITH_EDITOR only (the suite already runs
// under an editor-context automation harness).

#include "CQTest.h"

#if WITH_EDITOR

#include "Containers/Ticker.h"
#include "Misc/ScopeExit.h"

#include "ArcForestTestHelpers.h"
#include "ArcForestTestSurfaceData.h"

#include "Forest/PCGForestSimSettings.h"
#include "Forest/ArcForestSpeciesDefinition.h"
#include "Forest/ArcForestSimTypes.h"

#include "Tests/PCGTestsCommon.h"
#include "PCGContext.h"
#include "PCGCommon.h"
#include "Data/PCGBasePointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataCommon.h"
#include "Metadata/PCGMetadataDomain.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcForestSmokeTest, Log, All);

namespace ArcForestPCGSmokeTestsLocal
{
	using namespace ArcForestTestHelpers;

	/** A 30 m x 30 m flat synthetic surface centred on the origin (Z=0), fully suitable in XY. */
	UArcForestTestSurfaceData* MakeTestSurface()
	{
		UArcForestTestSurfaceData* Surface = NewObject<UArcForestTestSurfaceData>();
		Surface->Initialize(FBox(FVector(-1500.0, -1500.0, -100.0), FVector(1500.0, 1500.0, 100.0)));
		Surface->FlatSurfaceZ = 0.0f;
		return Surface;
	}

	/**
	 * Build a single dense, wide-crowned canopy species (mirrors the forced-competition shape used by
	 * ArcForestSimCoreTests's SelfThinning_CountDropsAndSpreadsOut) and drive the REAL
	 * FPCGForestSimElement to completion at a given Iterations count on the shared synthetic surface.
	 * Returns the emitted survivor point count, or -1 if the element failed to terminate/emit within
	 * the bounded slice budget (the caller asserts on that sentinel).
	 */
	/**
	 * Recruitment knobs as the NODE exposes them, so a case can drive UPCGForestSimSettings' own
	 * UPROPERTYs and therefore pin PCGForestSimSettings.cpp's prebake copy of them. Defaults reproduce
	 * the shipped node defaults, so an existing caller that omits this is byte-identical to before.
	 */
	struct FRecruitmentKnobs
	{
		bool bEnable = false;
		int32 PopulationCeiling = 0;
		float RecruitMinSpacingCm = 100.0f;
	};

	int32 RunDenseForestElementToCompletion(int32 Iterations, int32 SimSeed,
		const FRecruitmentKnobs& Recruitment = FRecruitmentKnobs())
	{
		TArray<FArcForestSpeciesEntry> Table;
		FArcForestSpeciesEntry DenseOak = MakeSpecies(TEXT("DenseOak"), EArcForestLayer::Canopy, 2000.0f, 650.0f, 0.25f,
			2.0f, 400.0f, 0.02f); // TargetDensity 2.0 = dense (mirrors core SelfThinning fixture)
		DenseOak.LightNeeded = 0.5f;
		DenseOak.ShadeTolerance = 0.3f;
		Table.Add(MoveTemp(DenseOak));
		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeDefinition(MoveTemp(Table));

		PCGTestsCommon::FTestData TestData;

		UPCGForestSimSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGForestSimSettings>(TestData);
		if (Settings == nullptr)
		{
			return -1;
		}
		Settings->Species = Def.Get();
		Settings->SimSeed = SimSeed;
		Settings->SeedingDensity = 4.0f;
		Settings->Iterations = Iterations;
		Settings->CanopyExtinction = 0.5f;
		Settings->WPatch = 2.0f;
		// Tight seed spacing so the dense DenseOak stand's mature crowns overlap and competition thins it
		// across iterations — required for ResumableElement_MoreIterationsThinMore to observe a difference
		// between a 1-iteration and a 16-iteration run (at the 0.8 default nothing dies, counts are equal).
		Settings->SpacingFraction = 0.25f;

		// Recruitment is driven through the NODE's UPROPERTYs, which is the whole point of the
		// recruitment smoke case: it is the only thing in the suite that exercises
		// PCGForestSimSettings.cpp's prebake copy of these into FArcForestSimParams.
		Settings->bEnableRecruitment = Recruitment.bEnable;
		Settings->PopulationCeiling = Recruitment.PopulationCeiling;
		Settings->RecruitMinSpacingCm = Recruitment.RecruitMinSpacingCm;

		FPCGTaggedData& Input = TestData.InputData.TaggedData.Emplace_GetRef();
		Input.Pin = PCGPinConstants::DefaultInputLabel;
		Input.Data = MakeTestSurface();

		FPCGElementPtr Element = TestData.Settings->GetElement();
		if (!Element.IsValid())
		{
			return -1;
		}

		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

		// PauseUntilNextFrame() (see below) lazily creates a FPCGContextHandle on the context via
		// GetOrCreateHandle(); once that Handle exists, FPCGContext's destructor ensures !Handle to
		// catch a raw delete that should have gone through FPCGContext::Release instead (see
		// FPCGContext::~FPCGContext, PCGContext.cpp:56). The engine's own PCG test harness
		// (PCGDeterminismTestsCommon.cpp) uses exactly this Release(Context.Release()) pattern —
		// mirror it here so every early-return path (and the normal one) releases correctly.
		ON_SCOPE_EXIT
		{
			FPCGContext::Release(Context.Release());
		};

		// The resumable element (post Step 3) yields (returns false) once per prebake + per
		// MaxItersPerSlice=4 step slice + solve-done, so a 16-iteration run needs several more
		// Execute() calls than a 1-iteration run; 200 is a generous bound well above either case.
		int32 Guard = 0;
		while (!Element->Execute(Context.Get()))
		{
			// The Step 5 worker-offload element yields via FPCGContext::PauseUntilNextFrame(), which
			// defers the unpause onto FPCGModule::ExecuteNextTick(...), drained only by
			// FPCGModule::Tick registered on FTSTicker::GetCoreTicker(). Pump it here to simulate the
			// editor's real per-frame tick and clear bIsPaused so the next Execute() can proceed.
			FTSTicker::GetCoreTicker().Tick(0.016f);

			if (++Guard > 200)
			{
				return -1; // never spin forever if a phase misbehaves
			}
		}

		const TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
		if (Outputs.Num() != 1)
		{
			return -1;
		}

		const UPCGBasePointData* PointData = Cast<UPCGBasePointData>(Outputs[0].Data);
		return PointData != nullptr ? PointData->GetNumPoints() : -1;
	}
}

TEST_CLASS(ArcForestPCGSmoke, "ArcPCGExtension.ForestSim.PCGSmoke")
{
	// Run the forest-sim node end to end on a synthetic surface and validate the output contract.
	//
	// REFERENCE RUN (SimSeed 555, 3000x3000 cm surface, density 2.0, 16 iterations):
	//   output point count = 1782 (> 0); all 9 attributes present + typed. The count is documented
	//   rather than pinned exactly because it also depends on the PCG test-harness bounds resolution.
	TEST_METHOD(Element_EmitsAttributedPointsOnSyntheticSurface)
	{
		using namespace ArcForestPCGSmokeTestsLocal;

		// Species table (kept GC-rooted for the whole run).
		TArray<FArcForestSpeciesEntry> Table;
		FArcForestSpeciesEntry Oak = MakeSpecies(TEXT("Oak"), EArcForestLayer::Canopy, 2200.0f, 500.0f, 0.15f,
			0.02f, 350.0f, 0.05f); // TargetDensity 0.02 = sparse canopy
		Oak.LightNeeded = 0.55f;
		Oak.ShadeTolerance = 0.3f;
		Table.Add(MoveTemp(Oak));
		FArcForestSpeciesEntry Fern = MakeSpecies(TEXT("Fern"), EArcForestLayer::Field, 120.0f, 90.0f, 0.2f,
			0.3f, 200.0f, 0.12f); // TargetDensity 0.3 = dense ground cover
		Fern.LightNeeded = 0.25f;
		Fern.ShadeTolerance = 0.9f;
		Table.Add(MoveTemp(Fern));
		TStrongObjectPtr<UArcForestSpeciesDefinition> Def = MakeDefinition(MoveTemp(Table));

		PCGTestsCommon::FTestData TestData;

		UPCGForestSimSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGForestSimSettings>(TestData);
		ASSERT_THAT(IsTrue(Settings != nullptr, TEXT("Settings must be generated")));
		Settings->Species = Def.Get();
		Settings->SimSeed = 555;
		Settings->SeedingDensity = 2.0f;
		Settings->Iterations = 16;
		Settings->CanopyExtinction = 0.5f;
		Settings->WPatch = 2.0f;

		// Surface input on the default input pin.
		FPCGTaggedData& Input = TestData.InputData.TaggedData.Emplace_GetRef();
		Input.Pin = PCGPinConstants::DefaultInputLabel;
		Input.Data = MakeTestSurface();

		FPCGElementPtr Element = TestData.Settings->GetElement();
		ASSERT_THAT(IsTrue(Element.IsValid(), TEXT("Forest-sim settings must produce an element")));

		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

		// See RunDenseForestElementToCompletion's ON_SCOPE_EXIT comment: PauseUntilNextFrame() lazily
		// creates a FPCGContextHandle, and the context destructor ensures !Handle on a raw delete —
		// release through the real FPCGContext::Release API instead (mirrors the engine's own
		// PCGDeterminismTestsCommon.cpp harness pattern).
		ON_SCOPE_EXIT
		{
			FPCGContext::Release(Context.Release());
		};

		// Drive the element to completion across its solve (returns false / yields) and emit
		// (returns true) time-slices.
		int32 Guard = 0;
		while (!Element->Execute(Context.Get()))
		{
			// See RunDenseForestElementToCompletion above: pump the core ticker so the deferred
			// PauseUntilNextFrame() unpause (FPCGModule::Tick) actually runs between Execute() calls.
			FTSTicker::GetCoreTicker().Tick(0.016f);

			if (++Guard > 1024)
			{
				break; // never spin forever if a phase misbehaves
			}
		}

		ASSERT_THAT(IsTrue(Guard <= 1024, TEXT("Element must terminate within a bounded number of slices")));

		const TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
		ASSERT_THAT(IsTrue(Outputs.Num() == 1, TEXT("Element must emit exactly one output data")));

		const UPCGBasePointData* PointData = Cast<UPCGBasePointData>(Outputs[0].Data);
		ASSERT_THAT(IsTrue(PointData != nullptr, TEXT("Output must be point data")));

		const int32 NumPoints = PointData->GetNumPoints();
		UE_LOGF(LogArcForestSmokeTest, Display,"[Smoke] output points=%d", NumPoints);
		ASSERT_THAT(IsTrue(NumPoints > 0, TEXT("Forest node must emit at least one plant point")));

		const UPCGMetadata* Metadata = PointData->ConstMetadata();
		ASSERT_THAT(IsTrue(Metadata != nullptr, TEXT("Output point data must carry metadata")));

		const FPCGMetadataDomain* Domain = Metadata->GetConstMetadataDomain(PCGMetadataDomainID::Elements);
		ASSERT_THAT(IsTrue(Domain != nullptr, TEXT("Output must have the Elements metadata domain")));

		// Each required attribute must be PRESENT and of the correct type — GetConstTypedAttribute<T>
		// returns null on either a missing name or a type mismatch, so a single non-null check covers
		// both the output contract's presence and typing.
		// Species is emitted as FName (the authored species Name, not the numeric id) — see
		// PCGForestSimSettings.cpp `FindOrCreateAttribute<FName>(TEXT("Species"), NAME_None)`. This
		// assertion previously read <int32>: correct while Species WAS int32, but after the FName
		// migration (commit 573982384796) GetConstTypedAttribute<int32> returns null on the type
		// mismatch, so the stale <int32> read was RED-if-run — this fix realigns the type, it did not
		// un-vacuum a green-on-failure check.
		ASSERT_THAT(IsTrue(Domain->GetConstTypedAttribute<FName>(TEXT("Species")) != nullptr,
			TEXT("Species attribute must be present as FName")));
		ASSERT_THAT(IsTrue(Domain->GetConstTypedAttribute<int32>(TEXT("Layer")) != nullptr,
			TEXT("Layer attribute must be present as int32")));
		ASSERT_THAT(IsTrue(Domain->GetConstTypedAttribute<float>(TEXT("CrownRadius")) != nullptr,
			TEXT("CrownRadius attribute must be present as float")));
		ASSERT_THAT(IsTrue(Domain->GetConstTypedAttribute<float>(TEXT("Height")) != nullptr,
			TEXT("Height attribute must be present as float")));
		ASSERT_THAT(IsTrue(Domain->GetConstTypedAttribute<int32>(TEXT("Age")) != nullptr,
			TEXT("Age attribute must be present as int32")));
		ASSERT_THAT(IsTrue(Domain->GetConstTypedAttribute<float>(TEXT("Vigor")) != nullptr,
			TEXT("Vigor attribute must be present as float")));
		ASSERT_THAT(IsTrue(Domain->GetConstTypedAttribute<int32>(TEXT("PatchId")) != nullptr,
			TEXT("PatchId attribute must be present as int32")));
		ASSERT_THAT(IsTrue(Domain->GetConstTypedAttribute<int32>(TEXT("InstanceSeed")) != nullptr,
			TEXT("InstanceSeed attribute must be present as int32")));
	}

	// STEP 4 (plan: 2026-07-23-0803-arcpcgextension-forest-timeslice-worker-plan.md) — the previously
	// wrong branch: the pre-Step-3 element unconditionally set `Context->bSolved = true` after running
	// only the FIRST time-slice's worth of StepIterations, regardless of Settings->Iterations. So an
	// Iterations=1 run and an Iterations=16 run on the SAME seed/surface/species produced the IDENTICAL
	// survivor count (both only ever executed ~1 iteration's worth of competition). The old smoke
	// assertion (`NumPoints > 0`, above) passed on both the bug and the fix — green-on-failure. This
	// test instead asserts the counts DIFFER: the Step 3 resumable fix drives StepIterations across
	// slices until `SolveState.Iteration >= Settings->Iterations`, so the 16-iteration run experiences
	// 16x the competition/mortality passes of the 1-iteration run on the same dense, wide-crowned
	// canopy population (mirrors ArcForestSimCoreTests's SelfThinning_CountDropsAndSpreadsOut shape) and
	// MUST thin more. RED on the reverted bug (equal counts); GREEN on the resumable fix.
	TEST_METHOD(ResumableElement_MoreIterationsThinMore)
	{
		using namespace ArcForestPCGSmokeTestsLocal;

		const int32 NumPointsOneIter = RunDenseForestElementToCompletion(/*Iterations=*/1, /*SimSeed=*/777);
		const int32 NumPointsSixteenIter = RunDenseForestElementToCompletion(/*Iterations=*/16, /*SimSeed=*/777);

		UE_LOGF(LogArcForestSmokeTest, Display,"[Smoke] resumable element: iter1=%d iter16=%d", NumPointsOneIter, NumPointsSixteenIter);

		ASSERT_THAT(IsTrue(NumPointsOneIter >= 0,
			TEXT("Iterations=1 run must terminate within the slice budget and emit point data")));
		ASSERT_THAT(IsTrue(NumPointsSixteenIter >= 0,
			TEXT("Iterations=16 run must terminate within the slice budget and emit point data")));
		ASSERT_THAT(IsTrue(NumPointsOneIter != NumPointsSixteenIter,
			TEXT("16 competition iterations must thin the dense stand differently than 1 iteration; equal ")
			TEXT("counts reproduce the old unconditional bSolved=true bug where the element only ever ran ")
			TEXT("~1 iteration's worth of StepIterations regardless of Settings->Iterations")));
		ASSERT_THAT(IsTrue(NumPointsSixteenIter <= NumPointsOneIter,
			TEXT("More competition rounds must not increase survivor count (mortality is monotonic non-increasing)")));
	}

	// STEP 12 FIXES — the previously-wrong branch is a MISSING WIRE, not a wrong computation.
	//
	// Step 11 added six recruitment UPROPERTYs to UPCGForestSimSettings and six lines in
	// PCGForestSimSettings.cpp's prebake block that copy them into FArcForestSimParams. NOTHING in the
	// suite exercised that copy: every other case leaves bEnableRecruitment at its `false` default, and
	// all fourteen ArcForestRecruitmentTests build FArcForestSimParams by hand and call the solver
	// directly, bypassing the node entirely. So deleting any of those six copy lines — or copying
	// RecruitStride into RecruitQueryHeightCm — left the whole suite green while the node silently
	// produced a non-recruiting stand for a designer who had switched Recruitment on. That is the
	// failure class this rollout introduced, and it is the one path the user actually drives.
	//
	// Three arms, each pinning one copied line by a DIFFERENT mechanism:
	//   OFF          — the baseline.
	//   ON           — recruitment must change the emitted count.  Pins `bEnableRecruitment`.
	//   ON + wall    — RecruitMinSpacingCm far larger than the surface diagonal, so every recruit dart
	//                  is spacing-rejected and the run must collapse back onto the OFF count exactly.
	//                  Pins `RecruitMinSpacingCm` (a dropped copy leaves the 100 cm default, which
	//                  admits recruits, so the identity breaks).
	// PopulationCeiling is pinned by the ON arm staying at or below the authored ceiling.
	//
	// STILL UNPINNED by this case, and recorded rather than left silent: `RecruitmentRate`,
	// `RecruitStride` and `RecruitQueryHeightCm`. Rate only scales an attempt budget the ceiling already
	// bounds; the other two feed the shade raster, whose effect needs an authored ShadeResponseCurve to
	// become observable in an emitted count.
	TEST_METHOD(Element_RecruitmentSettingsReachTheSolver)
	{
		using namespace ArcForestPCGSmokeTestsLocal;

		// The ceiling MUST BIND, or its assertion is green-on-failure. MEASURED: this fixture emits 34
		// with recruitment off and 324 with recruitment on under a non-binding ceiling, while a DROPPED
		// PopulationCeiling copy leaves 0, which the solver derives as
		// TargetDensity(2.0) x SeedingDensity(4.0) x 900 m2 = 7200 — so any ceiling above ~324 is
		// satisfied by the dropped-copy run too and proves nothing. 100 sits between the baseline 34 and
		// the unbounded 324, so the authored value is the only thing that can hold the count down.
		constexpr int32 AuthoredCeiling = 100;

		FRecruitmentKnobs Off;

		FRecruitmentKnobs On;
		On.bEnable = true;
		On.PopulationCeiling = AuthoredCeiling;
		On.RecruitMinSpacingCm = 100.0f;

		// A spacing floor larger than the 3000x3000 cm surface's diagonal (~4243 cm) means the nearest
		// neighbour is inside the rejection radius for EVERY dart, whatever the stand looks like.
		FRecruitmentKnobs OnWithSpacingWall;
		OnWithSpacingWall.bEnable = true;
		OnWithSpacingWall.PopulationCeiling = AuthoredCeiling;
		OnWithSpacingWall.RecruitMinSpacingCm = 100000.0f;

		const int32 NumOff = RunDenseForestElementToCompletion(/*Iterations=*/8, /*SimSeed=*/909, Off);
		const int32 NumOn = RunDenseForestElementToCompletion(/*Iterations=*/8, /*SimSeed=*/909, On);
		const int32 NumWall =
			RunDenseForestElementToCompletion(/*Iterations=*/8, /*SimSeed=*/909, OnWithSpacingWall);

		UE_LOGF(LogArcForestSmokeTest, Display,"[Smoke] MEASURED recruitment wiring: off=%d on=%d spacingWall=%d ceiling=%d",
			NumOff, NumOn, NumWall, AuthoredCeiling);

		ASSERT_THAT(IsTrue(NumOff >= 0 && NumOn >= 0 && NumWall >= 0,
			TEXT("All three element runs must terminate within the slice budget and emit point data")));
		ASSERT_THAT(IsTrue(NumOff > 0,
			TEXT("Non-vacuity: the baseline run must emit points, else 'the counts differ' below could be "
				"satisfied by an empty stand")));
		ASSERT_THAT(IsTrue(NumOn != NumOff,
			TEXT("bEnableRecruitment must reach the solver: with it on, the emitted count must differ from "
				"the recruitment-off baseline. RED if the prebake stops copying bEnableRecruitment")));
		ASSERT_THAT(IsTrue(NumOn > NumOff,
			TEXT("Recruitment only ADDS plants (it appends at stage h and its recruits face no mortality "
				"that iteration), so the enabled run must exceed the baseline, not merely differ")));
		ASSERT_THAT(IsTrue(NumOn <= AuthoredCeiling,
			TEXT("PopulationCeiling must reach the solver: the emitted count may never exceed the authored "
				"ceiling. The ceiling is deliberately set BELOW what this fixture reaches unbounded (324), "
				"so a dropped copy — which derives 7200 instead — overshoots it and goes RED")));
		ASSERT_THAT(IsTrue(NumWall == NumOff,
			TEXT("RecruitMinSpacingCm must reach the solver: a spacing floor beyond the surface diagonal "
				"rejects every dart, so the run must collapse back onto the baseline count EXACTLY. RED if "
				"the prebake stops copying it — the 100 cm default admits recruits and the count grows")));
	}
};

#endif // WITH_EDITOR
