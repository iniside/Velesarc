// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"

#include "Attributes/Handlers/ArcRPGCoreDerivation.h"

// ---------------------------------------------------------------------------
// Pure derivation formula pin-tests.
//
//   CalculateModifier takes the RAW stat value (e.g. 14), not the derived
//   modifier (which would be 2). The conversion floor((V-10)/2) is inside.
// ---------------------------------------------------------------------------

TEST_CLASS(ArcRPGDerivation, "ArcRPGMechanics.Derivation")
{
	// -----------------------------------------------------------------------
	// CalculateModifier: floor((V - 10) / 2)
	// -----------------------------------------------------------------------

	TEST_METHOD(CalculateModifier_Baseline10_ReturnsZero)
	{
		float Result = ArcRPGCoreDerivation::CalculateModifier(10.f);
		ASSERT_THAT(IsNear(0.f, Result, 0.001f));
	}

	TEST_METHOD(CalculateModifier_14_Returns2)
	{
		float Result = ArcRPGCoreDerivation::CalculateModifier(14.f);
		ASSERT_THAT(IsNear(2.f, Result, 0.001f));
	}

	TEST_METHOD(CalculateModifier_7_ReturnsNeg2)
	{
		// floor((7 - 10) / 2) = floor(-1.5) = -2
		float Result = ArcRPGCoreDerivation::CalculateModifier(7.f);
		ASSERT_THAT(IsNear(-2.f, Result, 0.001f));
	}

	TEST_METHOD(CalculateModifier_20_Returns5)
	{
		float Result = ArcRPGCoreDerivation::CalculateModifier(20.f);
		ASSERT_THAT(IsNear(5.f, Result, 0.001f));
	}
};
