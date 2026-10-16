// Copyright Lukasz Baran. All Rights Reserved.

// PCGPointCrowdingTests.cpp
// Headless PCG element CQTest coverage for PointOps/PCGPointCrowdingSettings.{h,cpp} (node E, "Point
// Crowding"), driven through the REAL FPCGPointCrowdingElement::Execute state machine via the shipped
// PCGTestsCommon::FTestData harness — the same pattern as PCGScatterNearPointsTests.cpp. WITH_EDITOR
// only: FTestData needs an editor world.
//
// This node samples no surface, so no ArcForestTestSurfaceData fixture is needed — every input here is
// a bare point set built directly.

#include "CQTest.h"

#if WITH_EDITOR

#include "Containers/Ticker.h"
#include "Misc/ScopeExit.h"

#include "PointOps/PCGPointCrowdingSettings.h"
#include "Scatter/ArcPCGScatterCore.h"

#include "Tests/PCGTestsCommon.h"
#include "PCGContext.h"
#include "PCGData.h"
#include "PCGPoint.h"
#include "Data/PCGBasePointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataCommon.h"
#include "Metadata/PCGMetadataDomain.h"

namespace PCGPointCrowdingTestsLocal
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

	/** Reads the float attribute Name off every point of PointData, in point-index order. */
	TArray<float> ReadFloatAttribute(const UPCGBasePointData* PointData, FName Name)
	{
		TArray<float> Values;
		const UPCGMetadata* Metadata = PointData->ConstMetadata();
		check(Metadata);
		const FPCGMetadataDomain* Domain = Metadata->GetConstMetadataDomain(PCGMetadataDomainID::Elements);
		check(Domain);
		const FPCGMetadataAttribute<float>* Attr = Domain->GetConstTypedAttribute<float>(Name);
		check(Attr);

		const FPCGPointMetadataEntry::ConstValueRange EntryRange = PointData->GetConstMetadataEntryValueRange();
		Values.SetNumUninitialized(PointData->GetNumPoints());
		for (int32 Index = 0; Index < PointData->GetNumPoints(); ++Index)
		{
			Values[Index] = Attr->GetValueFromItemKey(EntryRange[Index]);
		}
		return Values;
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

TEST_CLASS(ArcPCGPointCrowding, "ArcPCGExtension.PointOps.Crowding")
{
	// A point with no neighbour inside the search radius (and bIncludeSelf off) must get EXACTLY 0.
	TEST_METHOD(NoNeighboursInRange_GetsExactlyZero)
	{
		using namespace PCGPointCrowdingTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGPointCrowdingSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGPointCrowdingSettings>(TestData);
		ASSERT_THAT(IsTrue(Settings != nullptr, TEXT("Settings must be generated")));
		Settings->SearchRadiusCm = 100.0;
		Settings->Falloff = EArcPCGFalloff::Linear;
		Settings->bIncludeSelf = false;

		// Two points, 1000 cm apart — far outside the 100 cm search radius of each other.
		const TArray<FVector> Locations = { FVector(0.0, 0.0, 0.0), FVector(1000.0, 0.0, 0.0) };
		FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
		PointsInput.Pin = PCGPointCrowdingConstants::PointsLabel;
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
		ASSERT_THAT(IsTrue(OutPointData != nullptr && OutPointData->GetNumPoints() == 2, TEXT("Both points pass through")));

		const TArray<float> Crowding = ReadFloatAttribute(OutPointData, Settings->OutputAttributeName);
		ASSERT_THAT(IsTrue(Crowding[0] == 0.0f, TEXT("A point with zero neighbours in range must get EXACTLY 0")));
		ASSERT_THAT(IsTrue(Crowding[1] == 0.0f, TEXT("A point with zero neighbours in range must get EXACTLY 0")));
	}

	// bIncludeSelf must shift EVERY value by exactly EvaluateFalloff(Mode, 0, R) — the kernel's own
	// exactly-1-at-d==0 guarantee, here checked end to end through the node.
	TEST_METHOD(IncludeSelf_ShiftsEveryValueByExactlyFalloffAtZero)
	{
		using namespace PCGPointCrowdingTestsLocal;

		const TArray<FVector> Locations = {
			FVector(0.0, 0.0, 0.0), FVector(50.0, 0.0, 0.0), FVector(0.0, 60.0, 0.0)
		};

		TFunction<TArray<float>(bool)> RunCrowding = [&Locations](bool bIncludeSelf) -> TArray<float>
		{
			PCGTestsCommon::FTestData TestData;
			UPCGPointCrowdingSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGPointCrowdingSettings>(TestData);
			Settings->SearchRadiusCm = 200.0;
			Settings->Falloff = EArcPCGFalloff::Linear;
			Settings->bIncludeSelf = bIncludeSelf;
			// EXPLICIT: bNormalize defaults to TRUE (Density is 0-1 and the raw score is unbounded).
			// This test asserts a property of the RAW score — the exact shift of EvaluateFalloff(0) — so
			// it must pin the flag rather than inherit whatever the default happens to be.
			Settings->bNormalize = false;

			FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
			PointsInput.Pin = PCGPointCrowdingConstants::PointsLabel;
			PointsInput.Data = MakeSourcePoints(Locations);

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
			return ReadFloatAttribute(OutPointData, TEXT("Crowding"));
		};

		const TArray<float> WithoutSelf = RunCrowding(false);
		const TArray<float> WithSelf = RunCrowding(true);

		const float ExpectedShift = ArcPCGScatterCore::EvaluateFalloff(EArcPCGFalloff::Linear, 0.0, 200.0);
		ASSERT_THAT(IsTrue(ExpectedShift == 1.0f, TEXT("Test-setup sanity: the kernel must be exactly 1 at d == 0")));

		ASSERT_THAT(AreEqual(WithoutSelf.Num(), WithSelf.Num(), TEXT("Both runs must produce the same point count")));
		for (int32 Index = 0; Index < WithoutSelf.Num(); ++Index)
		{
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(WithSelf[Index], WithoutSelf[Index] + ExpectedShift, 1e-5f),
				TEXT("bIncludeSelf must shift EVERY value by EXACTLY EvaluateFalloff(Mode, 0, R), never a ")
				TEXT("different amount and never only some points")));
		}
	}

	// Constant mode on a hand-placed lattice must yield the EXACT INTEGER neighbour count — the
	// assertion that dies if the falloff is misapplied (e.g. Constant accidentally decaying with
	// distance instead of being a flat 1 inside the radius).
	TEST_METHOD(ConstantMode_HandPlacedLattice_YieldsExactIntegerNeighbourCount)
	{
		using namespace PCGPointCrowdingTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGPointCrowdingSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGPointCrowdingSettings>(TestData);
		Settings->Falloff = EArcPCGFalloff::Constant;
		Settings->bIncludeSelf = false;
		// EXPLICIT: bNormalize defaults to TRUE. The whole point of this case is the EXACT INTEGER
		// neighbour count, which only survives unnormalised.
		Settings->bNormalize = false;
		Settings->SearchRadiusCm = 150.0; // captures orthogonal neighbours (100 cm) but not diagonals (141.4 cm is < 150, so it DOES capture diagonals too — see comment below)

		// A 3x3 grid, spacing 100 cm. With radius 150 cm: orthogonal neighbours are 100 cm away (in
		// range) and diagonal neighbours are 100*Sqrt(2) = 141.42 cm away (also in range, since 141.42 <
		// 150). So each interior point (the centre, index 4) has 8 neighbours; each edge-centre point has
		// 5; each corner has 3.
		const TArray<FVector> Locations = {
			FVector(-100.0, -100.0, 0.0), FVector(0.0, -100.0, 0.0), FVector(100.0, -100.0, 0.0),
			FVector(-100.0,    0.0, 0.0), FVector(0.0,    0.0, 0.0), FVector(100.0,    0.0, 0.0),
			FVector(-100.0,  100.0, 0.0), FVector(0.0,  100.0, 0.0), FVector(100.0,  100.0, 0.0)
		};
		const TArray<int32> ExpectedNeighbourCount = { 3, 5, 3, 5, 8, 5, 3, 5, 3 };

		FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
		PointsInput.Pin = PCGPointCrowdingConstants::PointsLabel;
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
		ASSERT_THAT(IsTrue(OutPointData != nullptr && OutPointData->GetNumPoints() == 9, TEXT("All 9 lattice points pass through")));

		const TArray<float> Crowding = ReadFloatAttribute(OutPointData, Settings->OutputAttributeName);
		for (int32 Index = 0; Index < 9; ++Index)
		{
			ASSERT_THAT(IsTrue(Crowding[Index] == static_cast<float>(ExpectedNeighbourCount[Index]),
				TEXT("Constant mode must yield the EXACT INTEGER neighbour count on the hand-placed lattice")));
		}
	}

	// bNormalize must put the busiest point's value at EXACTLY 1.
	TEST_METHOD(Normalize_PutsMaxAtExactlyOne)
	{
		using namespace PCGPointCrowdingTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGPointCrowdingSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGPointCrowdingSettings>(TestData);
		Settings->Falloff = EArcPCGFalloff::Constant;
		Settings->bIncludeSelf = false;
		Settings->SearchRadiusCm = 150.0;
		Settings->bNormalize = true;

		const TArray<FVector> Locations = {
			FVector(-100.0, -100.0, 0.0), FVector(0.0, -100.0, 0.0), FVector(100.0, -100.0, 0.0),
			FVector(-100.0,    0.0, 0.0), FVector(0.0,    0.0, 0.0), FVector(100.0,    0.0, 0.0),
			FVector(-100.0,  100.0, 0.0), FVector(0.0,  100.0, 0.0), FVector(100.0,  100.0, 0.0)
		};

		FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
		PointsInput.Pin = PCGPointCrowdingConstants::PointsLabel;
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

		const TArray<float> Crowding = ReadFloatAttribute(OutPointData, Settings->OutputAttributeName);
		float MaxValue = 0.0f;
		for (float Value : Crowding)
		{
			MaxValue = FMath::Max(MaxValue, Value);
		}
		ASSERT_THAT(IsTrue(MaxValue == 1.0f, TEXT("bNormalize must put the busiest point's value at EXACTLY 1")));
	}

	// Input tags must survive to the output — dies if PassThroughPoints drops its Add_GetRef(InTagged)
	// whole-struct copy.
	TEST_METHOD(InputTags_SurviveToOutput)
	{
		using namespace PCGPointCrowdingTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGPointCrowdingSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGPointCrowdingSettings>(TestData);
		ASSERT_THAT(IsTrue(Settings != nullptr, TEXT("Settings must be generated")));

		const TArray<FVector> Locations = { FVector(0.0, 0.0, 0.0), FVector(50.0, 0.0, 0.0) };
		FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
		PointsInput.Pin = PCGPointCrowdingConstants::PointsLabel;
		PointsInput.Data = MakeSourcePoints(Locations);
		PointsInput.Tags.Add(TEXT("SomeInputTag"));

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
		ASSERT_THAT(IsTrue(Context->OutputData.TaggedData[0].Tags.Contains(TEXT("SomeInputTag")),
			TEXT("Input tags must survive to the output — a dropped Add_GetRef(InTagged) copy would lose this")));
	}

	// Two input tagged datas must produce two output datas, and neighbours must NOT leak across them:
	// a lone point in each data set must read 0 crowding even though its counterpart in the OTHER data
	// sits at the exact same location.
	TEST_METHOD(TwoInputDatas_ProduceTwoOutputsWithNoCrossLeakage)
	{
		using namespace PCGPointCrowdingTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGPointCrowdingSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGPointCrowdingSettings>(TestData);
		Settings->Falloff = EArcPCGFalloff::Constant;
		Settings->SearchRadiusCm = 500.0;
		Settings->bIncludeSelf = false;

		// Both data sets have a single point AT THE SAME LOCATION — if neighbour queries leaked across
		// data boundaries, each point would see the other's point as a neighbour at distance 0 and read
		// a nonzero crowding value.
		const FVector SharedLocation(10.0, 20.0, 0.0);
		FPCGTaggedData& FirstInput = TestData.InputData.TaggedData.Emplace_GetRef();
		FirstInput.Pin = PCGPointCrowdingConstants::PointsLabel;
		FirstInput.Data = MakeSourcePoints(MakeArrayView(&SharedLocation, 1));

		FPCGTaggedData& SecondInput = TestData.InputData.TaggedData.Emplace_GetRef();
		SecondInput.Pin = PCGPointCrowdingConstants::PointsLabel;
		SecondInput.Data = MakeSourcePoints(MakeArrayView(&SharedLocation, 1));

		FPCGElementPtr Element = TestData.Settings->GetElement();
		ASSERT_THAT(IsTrue(Element.IsValid(), TEXT("Settings must produce an element")));

		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();
		ON_SCOPE_EXIT
		{
			FPCGContext::Release(Context.Release());
		};

		ASSERT_THAT(IsTrue(RunToCompletion(Element, Context.Get()),
			TEXT("The element must finish within the spin-guard budget, not hang")));

		ASSERT_THAT(AreEqual(2, Context->OutputData.TaggedData.Num(),
			TEXT("Two input tagged datas must produce TWO output datas — one per input, never merged")));

		for (const FPCGTaggedData& OutTagged : Context->OutputData.TaggedData)
		{
			const UPCGBasePointData* OutPointData = Cast<UPCGBasePointData>(OutTagged.Data);
			ASSERT_THAT(IsTrue(OutPointData != nullptr && OutPointData->GetNumPoints() == 1, TEXT("Each output data must carry its own single point")));
			const TArray<float> Crowding = ReadFloatAttribute(OutPointData, Settings->OutputAttributeName);
			ASSERT_THAT(IsTrue(Crowding[0] == 0.0f,
				TEXT("Neighbour queries must NEVER cross a data boundary — a co-located point in the OTHER ")
				TEXT("data must not be counted as a neighbour")));
		}
	}
};

#endif // WITH_EDITOR
