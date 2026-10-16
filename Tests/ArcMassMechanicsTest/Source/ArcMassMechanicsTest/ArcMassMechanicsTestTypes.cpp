// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanicsTestTypes.h"

#include "Sources/ArcSourceItemResolver.h"

namespace ArcMassMechanicsTestTags
{
	UE_DEFINE_GAMEPLAY_TAG(Key_A, "Test.MassMechanics.Key.A");
	UE_DEFINE_GAMEPLAY_TAG(Key_B, "Test.MassMechanics.Key.B");
	UE_DEFINE_GAMEPLAY_TAG(Tier_Pos1, "Test.MassMechanics.Tier.Pos1");
	UE_DEFINE_GAMEPLAY_TAG(Tier_Pos2, "Test.MassMechanics.Tier.Pos2");
	UE_DEFINE_GAMEPLAY_TAG(Tier_Pos3, "Test.MassMechanics.Tier.Pos3");
	UE_DEFINE_GAMEPLAY_TAG(Tier_Neg1, "Test.MassMechanics.Tier.Neg1");
	UE_DEFINE_GAMEPLAY_TAG(Tier_Neg2, "Test.MassMechanics.Tier.Neg2");
	UE_DEFINE_GAMEPLAY_TAG(Tier_Neg3, "Test.MassMechanics.Tier.Neg3");
	UE_DEFINE_GAMEPLAY_TAG(KeyB_Tier1, "Test.MassMechanics.KeyB.Tier1");
}

namespace ArcMassMechanicsTest
{
	int32 AppliedCounts[SlotCount] = {};
	int32 RemovedCounts[SlotCount] = {};
	int32 SourceProbeCount = 0;
	const FArcItemData* LastProbedSourceItem = nullptr;

	void ResetCounts()
	{
		SourceProbeCount = 0;
		LastProbedSourceItem = nullptr;
		for (int32 Index = 0; Index < SlotCount; ++Index)
		{
			AppliedCounts[Index] = 0;
			RemovedCounts[Index] = 0;
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

	int32 TotalRemoved()
	{
		int32 Sum = 0;
		for (int32 Index = 0; Index < SlotCount; ++Index)
		{
			Sum += RemovedCounts[Index];
		}
		return Sum;
	}
}

float FArcMechTest_FixedTarget::GetTarget(const FArcResourceContext& Context) const
{
	return Value;
}

float FArcMechTest_FixedRate::GetRate(const FArcResourceContext& Context) const
{
	return Rate;
}

bool FArcMechTest_FixedCost::CheckCost(const FArcMassAbilityCostContext& Context, FArcCostCheckResult* OutResult) const
{
	return ArcCosts::CheckResource(Context, Resource, Meta, Amount, OutResult);
}

void FArcMechTest_FixedCost::ApplyCost(const FArcMassAbilityCostContext& Context) const
{
	ArcCosts::PayIntoMeta(Context, Meta, Amount);
}

void FArcMechTest_CountComponent::OnApplied(const FArcEffectContext& Context) const
{
	if (Slot >= 0 && Slot < ArcMassMechanicsTest::SlotCount)
	{
		++ArcMassMechanicsTest::AppliedCounts[Slot];
	}
}

void FArcMechTest_CountComponent::OnRemoved(const FArcEffectContext& Context, EArcEffectRemovalReason Reason) const
{
	if (Slot >= 0 && Slot < ArcMassMechanicsTest::SlotCount)
	{
		++ArcMassMechanicsTest::RemovedCounts[Slot];
	}
}

void FArcMechTest_SourceItemProbe::OnApplied(const FArcEffectContext& Context) const
{
	++ArcMassMechanicsTest::SourceProbeCount;
	ArcMassMechanicsTest::LastProbedSourceItem = ArcAbilityKit::FindSourceItem(Context);
}

