// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMassMechanicsTestTypes.h"
#include "ArcMechTestSupport.h"
#include "ArcStatusTestHarness.h"
#include "ArcMassMechanics/Statuses/ArcStatusTasks.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectFunctions.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Mass/EntityManager.h"

namespace ArcStatusTaskTestsPrivate
{
	UArcEffectDefinition* LoadStatus(const TCHAR* Name)
	{
		return ArcMechTest::LoadTestAsset<UArcEffectDefinition>(*FString::Printf(TEXT("Statuses/%s"), Name));
	}
}

TEST_CLASS(ArcStatus_Upkeep, "ArcMassMechanics.Statuses.Upkeep")
{
	FActorTestSpawner Spawner;
	ArcStatusTest::FStatusHarness Harness;

	BEFORE_EACH()
	{
		ArcStatusTest::SuppressTestLogs();
		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
	}

	FMassEntityHandle CreateWithMana(float Mana)
	{
		ArcStatusTest::FStatusEntityOptions Options;
		Options.Mana = Mana;
		return Harness.CreateEntity(Options);
	}

	void Period(FMassEntityHandle Payer)
	{
		Harness.TickDuration(1.f);
		Harness.DrainPendingOps(Payer);
	}

	TEST_METHOD(SourcePayer_PaysEachPeriodThroughMeta_ThenRemovedWhenBroke)
	{
		const FMassEntityHandle Source = CreateWithMana(25.f);
		const FMassEntityHandle Target = CreateWithMana(100.f);
		UArcEffectDefinition* Upkeep = ArcStatusTaskTestsPrivate::LoadStatus(TEXT("ED_Upkeep"));
		ASSERT_THAT(IsNotNull(Upkeep));
		const FArcEffectApplyResult Result = ArcEffects::TryApplyEffect(*Harness.EntityManager, Target,
			Upkeep, Source);
		ASSERT_THAT(IsTrue(Result.Handle.IsValid()));
		ASSERT_THAT(IsNear(25.f, Harness.GetMana(Source), 0.001f));

		Period(Source);
		ASSERT_THAT(IsNear(15.f, Harness.GetMana(Source), 0.001f));
		ASSERT_THAT(IsNear(100.f, Harness.GetMana(Target), 0.001f));
		ASSERT_THAT(IsNotNull(Harness.FindActive(Target, Result.Handle)));

		Period(Source);
		ASSERT_THAT(IsNear(5.f, Harness.GetMana(Source), 0.001f));
		ASSERT_THAT(IsNotNull(Harness.FindActive(Target, Result.Handle)));

		Period(Source);
		ASSERT_THAT(IsNear(5.f, Harness.GetMana(Source), 0.001f));
		ASSERT_THAT(IsNull(Harness.FindActive(Target, Result.Handle)));
	}

	TEST_METHOD(OwnerPayer_PaysFromOwner)
	{
		const FMassEntityHandle Source = CreateWithMana(100.f);
		const FMassEntityHandle Target = CreateWithMana(30.f);
		UArcEffectDefinition* UpkeepOwner = ArcStatusTaskTestsPrivate::LoadStatus(TEXT("ED_Upkeep_Owner"));
		ASSERT_THAT(IsNotNull(UpkeepOwner));
		const FArcEffectApplyResult Result = ArcEffects::TryApplyEffect(*Harness.EntityManager, Target,
			UpkeepOwner, Source);

		Period(Target);
		ASSERT_THAT(IsNear(20.f, Harness.GetMana(Target), 0.001f));
		ASSERT_THAT(IsNear(100.f, Harness.GetMana(Source), 0.001f));
		ASSERT_THAT(IsNotNull(Harness.FindActive(Target, Result.Handle)));
	}

	TEST_METHOD(DestroyedSource_RemovesEffect)
	{
		const FMassEntityHandle Source = CreateWithMana(100.f);
		const FMassEntityHandle Target = CreateWithMana(100.f);
		UArcEffectDefinition* Upkeep = ArcStatusTaskTestsPrivate::LoadStatus(TEXT("ED_Upkeep"));
		ASSERT_THAT(IsNotNull(Upkeep));
		const FArcEffectApplyResult Result = ArcEffects::TryApplyEffect(*Harness.EntityManager, Target,
			Upkeep, Source);
		Harness.EntityManager->DestroyEntity(Source);

		Harness.TickDuration(1.f);
		ASSERT_THAT(IsNull(Harness.FindActive(Target, Result.Handle)));
	}

	TEST_METHOD(NoSource_RemovesEffect)
	{
		const FMassEntityHandle Target = CreateWithMana(100.f);
		UArcEffectDefinition* Upkeep = ArcStatusTaskTestsPrivate::LoadStatus(TEXT("ED_Upkeep"));
		ASSERT_THAT(IsNotNull(Upkeep));
		const FArcEffectApplyResult Result = ArcEffects::TryApplyEffect(*Harness.EntityManager, Target,
			Upkeep, FMassEntityHandle());

		Harness.TickDuration(1.f);
		ASSERT_THAT(IsNull(Harness.FindActive(Target, Result.Handle)));
	}

	TEST_METHOD(PayerWithoutResourceAttribute_RemovesEffect)
	{
		ArcStatusTest::FStatusEntityOptions NoSet;
		NoSet.bCostSet = false;
		const FMassEntityHandle Source = Harness.CreateEntity(NoSet);
		const FMassEntityHandle Target = CreateWithMana(100.f);
		UArcEffectDefinition* Upkeep = ArcStatusTaskTestsPrivate::LoadStatus(TEXT("ED_Upkeep"));
		ASSERT_THAT(IsNotNull(Upkeep));
		const FArcEffectApplyResult Result = ArcEffects::TryApplyEffect(*Harness.EntityManager, Target,
			Upkeep, Source);

		Harness.TickDuration(1.f);
		ASSERT_THAT(IsNull(Harness.FindActive(Target, Result.Handle)));
	}
};
