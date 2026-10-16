// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRPGMechanicsTestTypes.h"

namespace ArcRPGMechanicsTestTags
{
	UE_DEFINE_GAMEPLAY_TAG(Input, "Test.RPGMechanics.Input");
	UE_DEFINE_GAMEPLAY_TAG(Domain_A, "ArcSpell.Element.TestA");
	UE_DEFINE_GAMEPLAY_TAG(Domain_B, "ArcSpell.Element.TestB");
	UE_DEFINE_GAMEPLAY_TAG(SlotTorso, "Test.RPGMechanics.Combat.Slot.Torso");
	UE_DEFINE_GAMEPLAY_TAG(SlotShield, "Test.RPGMechanics.Combat.Slot.Shield");
}

namespace ArcRPGMechanicsTest
{
	int32 AppliedCounts[SlotCount] = {};

	void ResetCounts()
	{
		for (int32 Index = 0; Index < SlotCount; ++Index)
		{
			AppliedCounts[Index] = 0;
		}
	}

	int32 TotalApplied()
	{
		int32 Sum = 0;
		for (int32 Index = 0; Index < SlotCount; ++Index)
		{
			Sum += AppliedCounts[Index];
		}
		return Sum;
	}
}

void FArcRPGMechTest_CountComponent::OnApplied(const FArcEffectContext& Context) const
{
	if (Slot >= 0 && Slot < ArcRPGMechanicsTest::SlotCount)
	{
		++ArcRPGMechanicsTest::AppliedCounts[Slot];
	}
}
