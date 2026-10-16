// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcEnviroEffectsTestHarness.h"
#include "ArcEnviroEffects/ArcEnviroAttributes.h"
#include "Components/ActorTestSpawner.h"

TEST_CLASS(ArcEnviroMetaAttributes, "ArcEnviroEffects.Chunks")
{
	FActorTestSpawner Spawner;
	ArcEnviroTest::FHarness Harness;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");

		const bool bInitialized = Harness.Initialize(Spawner);
		check(bInitialized);
	}

	TEST_METHOD(MetaAttributes_AreZeroedAfterTheChunkIsApplied)
	{
		const FMassEntityHandle Entity = Harness.CreateEntity();

		Harness.ApplyDelta(Entity, FArcEnviroAttributes::GetTemperatureDeltaAttribute(), -20.f);
		Harness.ApplyDelta(Entity, FArcEnviroAttributes::GetWetDeltaAttribute(), 40.f);

		const FArcEnviroAttributes* Attributes = Harness.GetAttributes(Entity);
		ASSERT_THAT(IsNotNull(Attributes));
		ASSERT_THAT(IsNear(0.f, Attributes->GetTemperatureDelta(), 0.001f));
		ASSERT_THAT(IsNear(0.f, Attributes->TemperatureDelta.BaseValue, 0.001f));
		ASSERT_THAT(IsNear(0.f, Attributes->GetWetDelta(), 0.001f));
		ASSERT_THAT(IsNear(0.f, Attributes->WetDelta.BaseValue, 0.001f));
	}

	TEST_METHOD(RepeatedChunks_AddUp)
	{
		const FMassEntityHandle Entity = Harness.CreateEntity();

		Harness.ApplyDelta(Entity, FArcEnviroAttributes::GetWetDeltaAttribute(), 40.f);
		Harness.ApplyDelta(Entity, FArcEnviroAttributes::GetWetDeltaAttribute(), 30.f);
		ASSERT_THAT(IsNear(70.f, Harness.GetAttributes(Entity)->GetWet(), 0.001f));

		Harness.ApplyDelta(Entity, FArcEnviroAttributes::GetTemperatureDeltaAttribute(), 30.f);
		Harness.ApplyDelta(Entity, FArcEnviroAttributes::GetTemperatureDeltaAttribute(), -10.f);
		ASSERT_THAT(IsNear(20.f, Harness.GetAttributes(Entity)->GetTemperature(), 0.001f));
	}

	TEST_METHOD(Chunks_AreClampedToTheConditionRange)
	{
		const FMassEntityHandle Entity = Harness.CreateEntity();

		Harness.ApplyDelta(Entity, FArcEnviroAttributes::GetWetDeltaAttribute(), 500.f);
		ASSERT_THAT(IsNear(100.f, Harness.GetAttributes(Entity)->GetWet(), 0.001f));
		Harness.ApplyDelta(Entity, FArcEnviroAttributes::GetWetDeltaAttribute(), -500.f);
		ASSERT_THAT(IsNear(0.f, Harness.GetAttributes(Entity)->GetWet(), 0.001f));

		Harness.ApplyDelta(Entity, FArcEnviroAttributes::GetTemperatureDeltaAttribute(), -500.f);
		ASSERT_THAT(IsNear(-100.f, Harness.GetAttributes(Entity)->GetTemperature(), 0.001f));
		Harness.ApplyDelta(Entity, FArcEnviroAttributes::GetTemperatureDeltaAttribute(), 500.f);
		ASSERT_THAT(IsNear(100.f, Harness.GetAttributes(Entity)->GetTemperature(), 0.001f));
	}
};
