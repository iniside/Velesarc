// Copyright Lukasz Baran. All Rights Reserved.

// PCGShadeAttributeTests.cpp
// Headless PCG element CQTest coverage for PointOps/PCGShadeAttributeSettings.{h,cpp} (node B, "Shade
// Attribute"), driven through the REAL FPCGShadeAttributeElement::Execute state machine via the shipped
// PCGTestsCommon::FTestData harness — the same pattern as PCGScatterNearPointsTests.cpp. WITH_EDITOR
// only: FTestData needs an editor world.
//
// This node samples no surface, so no ArcForestTestSurfaceData fixture is needed — every input here is
// a bare point set built directly.

#include "CQTest.h"

#if WITH_EDITOR

#include "Containers/Ticker.h"
#include "Misc/ScopeExit.h"

#include "PointOps/PCGShadeAttributeSettings.h"
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

namespace PCGShadeAttributeTestsLocal
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

	/** Adds a per-point double attribute (e.g. a per-occluder radius) to PointData. */
	void AddDoubleAttribute(UPCGBasePointData* PointData, FName AttributeName, TArrayView<const double> Values)
	{
		check(PointData->GetNumPoints() == Values.Num());
		FPCGMetadataDomain* Domain = PointData->MutableMetadata()->GetMetadataDomain(PCGMetadataDomainID::Elements);
		check(Domain);
		FPCGMetadataAttribute<double>* Attr = Domain->FindOrCreateAttribute<double>(AttributeName, 0.0);
		check(Attr);

		FPCGPointMetadataEntry::ValueRange EntryRange = PointData->GetMetadataEntryValueRange(/*bAllocate=*/true);
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			Domain->InitializeOnSet(EntryRange[Index]);
			Attr->SetValue(EntryRange[Index], Values[Index]);
		}
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

TEST_CLASS(ArcPCGShadeAttribute, "ArcPCGExtension.PointOps.Shade")
{
	// An UNCONNECTED Occluders pin must yield EXACTLY 1.0 for every target — the empty-occluder-set
	// identity that motivates the node's whole sign convention.
	TEST_METHOD(UnconnectedOccluders_EveryTargetGetsExactlyOne)
	{
		using namespace PCGShadeAttributeTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGShadeAttributeSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGShadeAttributeSettings>(TestData);
		ASSERT_THAT(IsTrue(Settings != nullptr, TEXT("Settings must be generated")));
		Settings->ExtinctionK = 0.5f;

		const TArray<FVector> Locations = { FVector(0.0, 0.0, 0.0), FVector(50.0, 0.0, 0.0) };
		FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
		PointsInput.Pin = PCGShadeAttributeConstants::PointsLabel;
		PointsInput.Data = MakeSourcePoints(Locations);
		// Occluders pin deliberately left unconnected.

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
		ASSERT_THAT(IsTrue(OutPointData != nullptr && OutPointData->GetNumPoints() == 2, TEXT("Both targets pass through")));

		const TArray<float> Transmittance = ReadFloatAttribute(OutPointData, Settings->OutputAttributeName);
		for (float Value : Transmittance)
		{
			ASSERT_THAT(IsTrue(Value == 1.0f, TEXT("An unconnected Occluders pin must yield EXACTLY 1.0 for every target")));
		}
	}

	// Same points wired into BOTH pins: with bIncludeSelf = false there must be NO d==0 self-
	// contribution (dies if the pointer-identity self-exclusion is dropped), and with bIncludeSelf =
	// true the result must be exactly Exp(-k*Density) — the exact self-shading contribution, since the
	// single occluder sits at distance 0 from the single target.
	TEST_METHOD(SamePointsBothPins_SelfExclusionIsExactAndToggleable)
	{
		using namespace PCGShadeAttributeTestsLocal;

		const FVector Location(0.0, 0.0, 0.0);
		constexpr float ExtinctionK = 0.5f;
		constexpr float Density = 2.0f;

		TFunction<float(bool)> RunSingleTargetTransmittance = [&Location](bool bIncludeSelf) -> float
		{
			PCGTestsCommon::FTestData TestData;
			UPCGShadeAttributeSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGShadeAttributeSettings>(TestData);
			Settings->ExtinctionK = ExtinctionK;
			Settings->OccluderDensityConstant = Density;
			Settings->OccluderRadiusConstantCm = 300.0;
			Settings->bIncludeSelf = bIncludeSelf;

			UPCGBasePointData* PointData = MakeSourcePoints(MakeArrayView(&Location, 1));

			FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
			PointsInput.Pin = PCGShadeAttributeConstants::PointsLabel;
			PointsInput.Data = PointData;

			// SAME point data object wired into both pins — this is what makes the pointer-identity
			// self-exclusion check observable.
			FPCGTaggedData& OccludersInput = TestData.InputData.TaggedData.Emplace_GetRef();
			OccludersInput.Pin = PCGShadeAttributeConstants::OccludersLabel;
			OccludersInput.Data = PointData;

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
			return ReadFloatAttribute(OutPointData, TEXT("Transmittance"))[0];
		};

		const float TransmittanceExcludingSelf = RunSingleTargetTransmittance(false);
		ASSERT_THAT(IsTrue(TransmittanceExcludingSelf == 1.0f,
			TEXT("With the ONLY occluder being the target itself and bIncludeSelf = false, there must be NO ")
			TEXT("d == 0 self-contribution — Sum stays 0 and Transmittance stays exactly 1")));

		const float TransmittanceIncludingSelf = RunSingleTargetTransmittance(true);
		const float ExpectedTransmittance = FMath::Exp(-ExtinctionK * Density);
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(TransmittanceIncludingSelf, ExpectedTransmittance, 1e-5f),
			TEXT("With bIncludeSelf = true and the occluder at distance 0, Transmittance must be EXACTLY ")
			TEXT("Exp(-ExtinctionK * Density)")));
	}

	// A single occluder at distance 0 from the target yields exactly Exp(-k*Density) — the base
	// Beer-Lambert identity, on a genuinely SEPARATE occluder data (not the self-exclusion path above).
	TEST_METHOD(SingleOccluderAtDistanceZero_YieldsExactBeerLambert)
	{
		using namespace PCGShadeAttributeTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGShadeAttributeSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGShadeAttributeSettings>(TestData);
		constexpr float ExtinctionK = 0.75f;
		constexpr float Density = 1.5f;
		Settings->ExtinctionK = ExtinctionK;
		Settings->OccluderDensityConstant = Density;
		Settings->OccluderRadiusConstantCm = 500.0;

		const FVector TargetLocation(0.0, 0.0, 0.0);
		FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
		PointsInput.Pin = PCGShadeAttributeConstants::PointsLabel;
		PointsInput.Data = MakeSourcePoints(MakeArrayView(&TargetLocation, 1));

		const FVector OccluderLocation(0.0, 0.0, 0.0); // same world position, but a SEPARATE data object
		FPCGTaggedData& OccludersInput = TestData.InputData.TaggedData.Emplace_GetRef();
		OccludersInput.Pin = PCGShadeAttributeConstants::OccludersLabel;
		OccludersInput.Data = MakeSourcePoints(MakeArrayView(&OccluderLocation, 1));

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
		ASSERT_THAT(IsTrue(OutPointData != nullptr && OutPointData->GetNumPoints() == 1, TEXT("Single target passes through")));

		const float Transmittance = ReadFloatAttribute(OutPointData, Settings->OutputAttributeName)[0];
		const float Expected = FMath::Exp(-ExtinctionK * Density);
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Transmittance, Expected, 1e-5f),
			TEXT("A single occluder at distance 0 must yield EXACTLY Exp(-ExtinctionK * Density)")));
	}

	// Doubling ExtinctionK must SQUARE the transmittance — Exp(-2kS) == Exp(-kS)^2.
	TEST_METHOD(DoublingExtinctionK_SquaresTransmittance)
	{
		using namespace PCGShadeAttributeTestsLocal;

		TFunction<float(float)> RunTransmittance = [](float ExtinctionK) -> float
		{
			PCGTestsCommon::FTestData TestData;
			UPCGShadeAttributeSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGShadeAttributeSettings>(TestData);
			Settings->ExtinctionK = ExtinctionK;
			Settings->OccluderDensityConstant = 1.0f;
			Settings->OccluderRadiusConstantCm = 400.0;
			Settings->Falloff = EArcPCGFalloff::Linear;

			const FVector TargetLocation(0.0, 0.0, 0.0);
			FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
			PointsInput.Pin = PCGShadeAttributeConstants::PointsLabel;
			PointsInput.Data = MakeSourcePoints(MakeArrayView(&TargetLocation, 1));

			const FVector OccluderLocation(150.0, 0.0, 0.0);
			FPCGTaggedData& OccludersInput = TestData.InputData.TaggedData.Emplace_GetRef();
			OccludersInput.Pin = PCGShadeAttributeConstants::OccludersLabel;
			OccludersInput.Data = MakeSourcePoints(MakeArrayView(&OccluderLocation, 1));

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
			return ReadFloatAttribute(OutPointData, TEXT("Transmittance"))[0];
		};

		const float TransmittanceAtK = RunTransmittance(0.4f);
		const float TransmittanceAtDoubleK = RunTransmittance(0.8f);

		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(TransmittanceAtDoubleK, TransmittanceAtK * TransmittanceAtK, 1e-4f),
			TEXT("Doubling ExtinctionK must SQUARE the transmittance: Exp(-2kS) == Exp(-kS)^2")));
	}

	// Per-occluder radius: a large-radius and a small-radius occluder at EQUAL distance from a single
	// target must contribute DIFFERENTLY — dies if the implementation regresses to a single global
	// search radius (which would make both occluders reachable or both unreachable together, rather
	// than each governed by its own per-point radius).
	TEST_METHOD(PerOccluderRadius_DifferentRadiiAtEqualDistanceContributeDifferently)
	{
		using namespace PCGShadeAttributeTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGShadeAttributeSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGShadeAttributeSettings>(TestData);
		constexpr float ExtinctionK = 0.5f;
		constexpr float Density = 1.0f;
		Settings->ExtinctionK = ExtinctionK;
		Settings->OccluderDensityConstant = Density;
		Settings->Falloff = EArcPCGFalloff::Linear;
		Settings->bUseOccluderRadiusAttribute = true;
		Settings->OccluderRadiusAttribute.SetAttributeName(TEXT("OccluderRadius"));

		const FVector TargetLocation(0.0, 0.0, 0.0);
		FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
		PointsInput.Pin = PCGShadeAttributeConstants::PointsLabel;
		PointsInput.Data = MakeSourcePoints(MakeArrayView(&TargetLocation, 1));

		// Both occluders sit at the SAME distance (200 cm) from the target, but with DIFFERENT radii:
		// the small-radius one (150 cm < 200 cm distance) cannot reach the target at all (contributes
		// exactly 0), while the large-radius one (500 cm) does. A single global search radius would make
		// both reachable or both unreachable together.
		const TArray<FVector> OccluderLocations = { FVector(200.0, 0.0, 0.0), FVector(0.0, 200.0, 0.0) };
		UPCGBasePointData* OccluderData = MakeSourcePoints(OccluderLocations);
		const TArray<double> Radii = { 150.0, 500.0 };
		AddDoubleAttribute(OccluderData, TEXT("OccluderRadius"), Radii);

		FPCGTaggedData& OccludersInput = TestData.InputData.TaggedData.Emplace_GetRef();
		OccludersInput.Pin = PCGShadeAttributeConstants::OccludersLabel;
		OccludersInput.Data = OccluderData;

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
		ASSERT_THAT(IsTrue(OutPointData != nullptr && OutPointData->GetNumPoints() == 1, TEXT("Single target passes through")));

		const float Transmittance = ReadFloatAttribute(OutPointData, Settings->OutputAttributeName)[0];

		// EXACT expected value: the 150 cm-radius occluder contributes 0 (200 cm > its own radius); only
		// the 500 cm-radius occluder contributes, at Linear falloff Max(0, 1 - 200/500) = 0.6.
		const float ExpectedSum = 0.0f + Density * ArcPCGScatterCore::EvaluateFalloff(EArcPCGFalloff::Linear, 200.0, 500.0);
		const float ExpectedTransmittance = FMath::Exp(-ExtinctionK * ExpectedSum);
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Transmittance, ExpectedTransmittance, 1e-5f),
			TEXT("Each occluder must be governed by its OWN radius — a single global search radius would ")
			TEXT("produce a different (both-reachable or both-unreachable) result")));
	}

	// bUseOccluderRadiusAttribute = false must use the CONSTANT even when the occluder data carries a
	// numeric attribute that @Last would otherwise pick up — the exact footgun the toggle exists to close.
	TEST_METHOD(RadiusToggleOff_UsesConstantEvenWhenAttributePresent)
	{
		using namespace PCGShadeAttributeTestsLocal;

		PCGTestsCommon::FTestData TestData;
		UPCGShadeAttributeSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGShadeAttributeSettings>(TestData);
		constexpr float ExtinctionK = 0.5f;
		constexpr float Density = 1.0f;
		Settings->ExtinctionK = ExtinctionK;
		Settings->OccluderDensityConstant = Density;
		Settings->Falloff = EArcPCGFalloff::Linear;
		Settings->bUseOccluderRadiusAttribute = false; // toggle OFF
		Settings->OccluderRadiusConstantCm = 500.0;

		const FVector TargetLocation(0.0, 0.0, 0.0);
		FPCGTaggedData& PointsInput = TestData.InputData.TaggedData.Emplace_GetRef();
		PointsInput.Pin = PCGShadeAttributeConstants::PointsLabel;
		PointsInput.Data = MakeSourcePoints(MakeArrayView(&TargetLocation, 1));

		// The occluder's LAST attribute (the one @Last would resolve to) is a huge, wildly different
		// numeric value — if the toggle-off path accidentally picked it up as the radius, the result
		// would differ sharply from the constant-radius expectation below.
		const FVector OccluderLocation(200.0, 0.0, 0.0);
		UPCGBasePointData* OccluderData = MakeSourcePoints(MakeArrayView(&OccluderLocation, 1));
		const TArray<double> DecoyValues = { 99999.0 };
		AddDoubleAttribute(OccluderData, TEXT("SomeOtherNumericAttribute"), DecoyValues);

		FPCGTaggedData& OccludersInput = TestData.InputData.TaggedData.Emplace_GetRef();
		OccludersInput.Pin = PCGShadeAttributeConstants::OccludersLabel;
		OccludersInput.Data = OccluderData;

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
		ASSERT_THAT(IsTrue(OutPointData != nullptr && OutPointData->GetNumPoints() == 1, TEXT("Single target passes through")));

		const float Transmittance = ReadFloatAttribute(OutPointData, Settings->OutputAttributeName)[0];
		const float ExpectedSum = Density * ArcPCGScatterCore::EvaluateFalloff(EArcPCGFalloff::Linear, 200.0, 500.0);
		const float ExpectedTransmittance = FMath::Exp(-ExtinctionK * ExpectedSum);
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Transmittance, ExpectedTransmittance, 1e-5f),
			TEXT("With the radius toggle OFF, the CONSTANT must be used even though the occluder data ")
			TEXT("carries a numeric last-attribute that @Last would otherwise resolve to")));
	}
};

#endif // WITH_EDITOR
