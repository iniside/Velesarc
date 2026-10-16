// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMassMechanics/Combat/ArcMechCombatSlots.h"

TEST_CLASS(ArcMechCombat_ArmorCurve, "ArcMassMechanics.Combat.ArmorCurve")
{
	TEST_METHOD(PositiveK_MitigatesByArmorOverArmorPlusKTimesDamage)
	{
		const FArcArmorCurve Curve;
		ASSERT_THAT(IsNear(6.25f, Curve.Mitigate(10.f, 60.f, 10.f), 0.0001f));
		ASSERT_THAT(IsNear(5.f, Curve.Mitigate(10.f, 100.f, 10.f), 0.0001f));
	}

	TEST_METHOD(ZeroOrNegativeK_IsNoCurve_TheDamagePassesUnchanged)
	{
		const FArcArmorCurve Curve;
		ASSERT_THAT(IsNear(10.f, Curve.Mitigate(10.f, 60.f, 0.f), 0.0001f));
		ASSERT_THAT(IsNear(10.f, Curve.Mitigate(10.f, 60.f, -5.f), 0.0001f));
	}

	TEST_METHOD(NoArmorOrNoDamage_PassesUnchanged)
	{
		const FArcArmorCurve Curve;
		ASSERT_THAT(IsNear(10.f, Curve.Mitigate(10.f, 0.f, 10.f), 0.0001f));
		ASSERT_THAT(IsNear(0.f, Curve.Mitigate(0.f, 60.f, 10.f), 0.0001f));
	}
};
