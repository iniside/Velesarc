// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcEnviroEffectsTestHarness.h"
#include "ArcEnviroEffects/ArcEnviroAttributes.h"
#include "Components/ActorTestSpawner.h"

TEST_CLASS(ArcEnviroModifiers, "ArcEnviroEffects.Modifiers")
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

	TEST_METHOD(ProtectionHandler_ClampsProtectionToZeroThroughThree)
	{
		ASSERT_THAT(IsTrue(Harness.SetWeatherLevels(0, 3, 0)));
		const FMassEntityHandle Entity = Harness.CreateEntity();

		Harness.OverrideAttribute(Entity, FArcEnviroAttributes::GetColdProtectionAttribute(), 9.f);
		ASSERT_THAT(IsNear(3.f, Harness.GetAttributes(Entity)->GetColdProtection(), 0.001f));

		Harness.OverrideAttribute(Entity, FArcEnviroAttributes::GetColdProtectionAttribute(), -2.f);
		ASSERT_THAT(IsNear(0.f, Harness.GetAttributes(Entity)->GetColdProtection(), 0.001f));

		Harness.OverrideAttribute(Entity, FArcEnviroAttributes::GetHeatProtectionAttribute(), 7.f);
		ASSERT_THAT(IsNear(3.f, Harness.GetAttributes(Entity)->GetHeatProtection(), 0.001f));

		Harness.OverrideAttribute(Entity, FArcEnviroAttributes::GetRainProtectionAttribute(), 5.f);
		ASSERT_THAT(IsNear(3.f, Harness.GetAttributes(Entity)->GetRainProtection(), 0.001f));

		Harness.OverrideAttribute(Entity, FArcEnviroAttributes::GetRainProtectionAttribute(), 2.f);
		ASSERT_THAT(IsNear(2.f, Harness.GetAttributes(Entity)->GetRainProtection(), 0.001f));
	}
};
