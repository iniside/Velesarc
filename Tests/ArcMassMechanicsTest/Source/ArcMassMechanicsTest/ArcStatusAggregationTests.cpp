// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMassMechanicsTestTypes.h"
#include "ArcMechTestSupport.h"
#include "ArcStatusTestHarness.h"
#include "ArcStatusTestTypes.h"
#include "ArcMassMechanics/Aggregation/ArcPolicy_ConditionalHighestWins.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectFunctions.h"
#include "Fragments/ArcEffectStackFragment.h"

namespace ArcStatusAggregationTestsPrivate
{
	UArcEffectDefinition* LoadStatus(const TCHAR* Name)
	{
		return ArcMechTest::LoadTestAsset<UArcEffectDefinition>(*FString::Printf(TEXT("Statuses/%s"), Name));
	}
}

TEST_CLASS(ArcStatus_ConditionalHighestWins, "ArcMassMechanics.Statuses.ConditionalHighestWins")
{
	FActorTestSpawner Spawner;
	ArcStatusTest::FStatusHarness Harness;
	FMassEntityHandle Target;

	BEFORE_EACH()
	{
		ArcStatusTest::SuppressTestLogs();
		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
	}

	bool CreateTarget(const TCHAR* HandlerConfigName)
	{
		ArcStatusTest::FStatusEntityOptions Options;
		Options.TierValue = 10.f;
		Options.HandlerConfigOverride = ArcMechTest::LoadTestAsset<UArcAttributeHandlerConfig>(*FString::Printf(TEXT("Handlers/%s"), HandlerConfigName));
		if (!Options.HandlerConfigOverride)
		{
			return false;
		}
		Target = Harness.CreateEntity(Options);
		return true;
	}

	bool ApplyMod(const TCHAR* Name)
	{
		UArcEffectDefinition* Mod = ArcStatusAggregationTestsPrivate::LoadStatus(Name);
		if (!Mod)
		{
			return false;
		}
		ArcEffects::TryApplyEffect(*Harness.EntityManager, Target, Mod, FMassEntityHandle());
		return true;
	}

	float Recalculated()
	{
		Harness.Recalculate(Target);
		return Harness.GetTierValue(Target);
	}

	TEST_METHOD(AddBonuses_OnlyLargestCounts)
	{
		ASSERT_THAT(IsTrue(CreateTarget(TEXT("AHC_Status_ConditionalCh0"))));
		ASSERT_THAT(IsTrue(ApplyMod(TEXT("ED_CondHW_Add3"))));
		ASSERT_THAT(IsTrue(ApplyMod(TEXT("ED_CondHW_Add5"))));
		ASSERT_THAT(IsNear(15.f, Recalculated(), 0.001f));
	}

	TEST_METHOD(AddPenalties_AllStack_NextToLargestBonus)
	{
		ASSERT_THAT(IsTrue(CreateTarget(TEXT("AHC_Status_ConditionalCh0"))));
		ASSERT_THAT(IsTrue(ApplyMod(TEXT("ED_CondHW_Add5"))));
		ASSERT_THAT(IsTrue(ApplyMod(TEXT("ED_CondHW_Add3"))));
		ASSERT_THAT(IsTrue(ApplyMod(TEXT("ED_CondHW_AddPenalties"))));
		ASSERT_THAT(IsNear(12.f, Recalculated(), 0.001f));
	}

	TEST_METHOD(PositiveDivideAdditive_IsPenalty_AllStack)
	{
		ASSERT_THAT(IsTrue(CreateTarget(TEXT("AHC_Status_ConditionalCh0"))));
		ASSERT_THAT(IsTrue(ApplyMod(TEXT("ED_CondHW_DivAdd0_5"))));
		ASSERT_THAT(IsTrue(ApplyMod(TEXT("ED_CondHW_DivAdd0_5"))));
		ASSERT_THAT(IsNear(5.f, Recalculated(), 0.001f));
	}

	TEST_METHOD(NegativeDivideAdditive_IsBonus_StrongestPicked)
	{
		ASSERT_THAT(IsTrue(CreateTarget(TEXT("AHC_Status_ConditionalCh0"))));
		ASSERT_THAT(IsTrue(ApplyMod(TEXT("ED_CondHW_DivAddNeg0_2"))));
		ASSERT_THAT(IsTrue(ApplyMod(TEXT("ED_CondHW_DivAddNeg0_5"))));
		ASSERT_THAT(IsNear(20.f, Recalculated(), 0.001f));
	}

	TEST_METHOD(MultiplyCompound_LargestBonusAndEveryPenalty)
	{
		ASSERT_THAT(IsTrue(CreateTarget(TEXT("AHC_Status_ConditionalCh0"))));
		ASSERT_THAT(IsTrue(ApplyMod(TEXT("ED_CondHW_MulComp1_2"))));
		ASSERT_THAT(IsTrue(ApplyMod(TEXT("ED_CondHW_MulComp1_5"))));
		ASSERT_THAT(IsTrue(ApplyMod(TEXT("ED_CondHW_MulComp0_5"))));
		ASSERT_THAT(IsNear(7.5f, Recalculated(), 0.001f));
	}

	TEST_METHOD(BonusesPickedPerOperation)
	{
		ASSERT_THAT(IsTrue(CreateTarget(TEXT("AHC_Status_ConditionalCh0"))));
		ASSERT_THAT(IsTrue(ApplyMod(TEXT("ED_CondHW_Add5"))));
		ASSERT_THAT(IsTrue(ApplyMod(TEXT("ED_CondHW_Add3"))));
		ASSERT_THAT(IsTrue(ApplyMod(TEXT("ED_CondHW_MulAdd0_5"))));
		ASSERT_THAT(IsTrue(ApplyMod(TEXT("ED_CondHW_MulAdd0_1"))));
		ASSERT_THAT(IsNear(22.5f, Recalculated(), 0.001f));
	}

	TEST_METHOD(Override_IsUnaffected_LastWins)
	{
		ASSERT_THAT(IsTrue(CreateTarget(TEXT("AHC_Status_ConditionalCh0"))));
		ASSERT_THAT(IsTrue(ApplyMod(TEXT("ED_CondHW_Override50"))));
		ASSERT_THAT(IsTrue(ApplyMod(TEXT("ED_CondHW_Override40"))));
		ASSERT_THAT(IsNear(40.f, Recalculated(), 0.001f));
	}

	TEST_METHOD(NonConditionalChannel_StacksAllBonuses)
	{
		ASSERT_THAT(IsTrue(CreateTarget(TEXT("AHC_Status_ConditionalCh1"))));
		ASSERT_THAT(IsTrue(ApplyMod(TEXT("ED_CondHW_Add3"))));
		ASSERT_THAT(IsTrue(ApplyMod(TEXT("ED_CondHW_Add5"))));
		ASSERT_THAT(IsTrue(ApplyMod(TEXT("ED_CondHW_Add3_Ch1"))));
		ASSERT_THAT(IsTrue(ApplyMod(TEXT("ED_CondHW_Add5_Ch1"))));
		ASSERT_THAT(IsNear(23.f, Recalculated(), 0.001f));
	}
};
