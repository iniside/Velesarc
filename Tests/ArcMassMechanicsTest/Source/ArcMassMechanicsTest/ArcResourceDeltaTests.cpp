// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMassMechanicsTestHarness.h"
#include "ArcMassMechanicsTestTypes.h"
#include "ArcMechTestSupport.h"
#include "ArcMassMechanics/Resources/ArcResourceTypes.h"
#include "Components/ActorTestSpawner.h"
#include "StructUtils/InstancedStruct.h"

namespace ArcResourceDeltaTestsPrivate
{
	UArcResourceConfig* LoadResourceConfig(const TCHAR* Name)
	{
		return ArcMechTest::LoadTestAsset<UArcResourceConfig>(*FString::Printf(TEXT("Resources/%s"), Name));
	}
}

TEST_CLASS(ArcResourceDelta, "ArcMassMechanics.Resources.Delta")
{
	FActorTestSpawner Spawner;
	ArcMassMechanicsTest::FFlowHarness Harness;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");

		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
	}

	TEST_METHOD(UpdateInterval_AccumulatesPerEntity_AndAppliesTheFullElapsedTime)
	{
		UArcResourceConfig* Config = ArcResourceDeltaTestsPrivate::LoadResourceConfig(TEXT("DA_ResourceConfig_FlowInterval"));
		ASSERT_THAT(IsNotNull(Config));

		const FMassEntityHandle First = Harness.CreateEntity(Config);
		Harness.Tick(0.4f);
		const FMassEntityHandle Second = Harness.CreateEntity(Config);
		ASSERT_THAT(IsNear(50.f, Harness.GetResource(First), 0.001f));

		Harness.Tick(0.4f);
		ASSERT_THAT(IsNear(50.f, Harness.GetResource(First), 0.001f));
		ASSERT_THAT(IsNear(50.f, Harness.GetResource(Second), 0.001f));

		Harness.Tick(0.4f);
		ASSERT_THAT(IsNear(62.f, Harness.GetResource(First), 0.01f));
		ASSERT_THAT(IsNear(50.f, Harness.GetResource(Second), 0.001f));

		Harness.Tick(0.4f);
		ASSERT_THAT(IsNear(62.f, Harness.GetResource(First), 0.01f));
		ASSERT_THAT(IsNear(62.f, Harness.GetResource(Second), 0.01f));
	}

	TEST_METHOD(Regen_FlowsThroughTheDeltaMeta)
	{
		UArcResourceConfig* Config = ArcResourceDeltaTestsPrivate::LoadResourceConfig(TEXT("DA_ResourceConfig_FlowConstant"));
		ASSERT_THAT(IsNotNull(Config));
		const FMassEntityHandle Entity = Harness.CreateEntity(Config);

		Harness.Tick(0.5f);

		const FArcMechTest_FlowSet* Set = Harness.GetSet(Entity);
		ASSERT_THAT(IsNear(55.f, Set->GetResource(), 0.001f));
		ASSERT_THAT(IsNear(0.f, Set->GainMeta.BaseValue, 0.001f));
	}

	TEST_METHOD(Drain_FlowsThroughTheDeltaMetaAsANegativeStep)
	{
		UArcResourceConfig* Config = ArcResourceDeltaTestsPrivate::LoadResourceConfig(TEXT("DA_ResourceConfig_FlowDrain"));
		ASSERT_THAT(IsNotNull(Config));
		const FMassEntityHandle Entity = Harness.CreateEntity(Config);

		Harness.Tick(0.5f);

		const FArcMechTest_FlowSet* Set = Harness.GetSet(Entity);
		ASSERT_THAT(IsNear(45.f, Set->GetResource(), 0.001f));
		ASSERT_THAT(IsNear(0.f, Set->GainMeta.BaseValue, 0.001f));
	}

	TEST_METHOD(EmptyRate_SkipsTheResource)
	{
		UArcResourceConfig* Config = Harness.MakeConfig();
		ArcMassMechanicsTest::AddFlowRule(*Config, FInstancedStruct(), FInstancedStruct::Make(FArcResourceTarget_Max()));
		ArcMassMechanicsTest::AddConstantFlowRule(*Config, 5.f);
		const FMassEntityHandle Entity = Harness.CreateEntity(Config);

		Harness.Tick(1.f);

		ASSERT_THAT(IsNear(55.f, Harness.GetResource(Entity), 0.001f));
	}

	TEST_METHOD(MissingMetaHandler_LogsOnceForTheConfig_AndDoesNotWrite)
	{
		UArcResourceConfig* Config = Harness.MakeConfig();
		ArcMassMechanicsTest::AddConstantFlowRule(*Config, 10.f);
		Config->Resources.Last().Delta = FArcMechTest_FlowSet::GetUnhandledMetaAttribute();

		const FMassEntityHandle WithConfig = Harness.CreateEntity(Config);
		ArcMassMechanicsTest::FFlowEntityOptions NoConfig;
		NoConfig.bHandlerConfig = false;
		const FMassEntityHandle WithoutConfig = Harness.CreateEntity(Config, NoConfig);

		TestRunner->AddExpectedMessage(TEXT("has no handler on the entity"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		Harness.Tick(1.f, 3);

		ASSERT_THAT(IsNear(50.f, Harness.GetResource(WithConfig), 0.001f));
		ASSERT_THAT(IsNear(0.f, Harness.GetSet(WithConfig)->UnhandledMeta.BaseValue, 0.001f));
		ASSERT_THAT(IsNear(50.f, Harness.GetResource(WithoutConfig), 0.001f));
		ASSERT_THAT(IsNear(0.f, Harness.GetSet(WithoutConfig)->UnhandledMeta.BaseValue, 0.001f));
	}

	TEST_METHOD(DrainToZero_WritesTheResourceAfterThePendingOpsDrain_StopsAtMin)
	{
		ArcMassMechanicsTest::FFlowEntityOptions Options;
		Options.Resource = 5.f;
		UArcResourceConfig* Config = ArcResourceDeltaTestsPrivate::LoadResourceConfig(TEXT("DA_ResourceConfig_FlowDrain"));
		ASSERT_THAT(IsNotNull(Config));
		const FMassEntityHandle Entity = Harness.CreateEntity(Config, Options);

		Harness.Tick(1.f);

		const FArcMechTest_FlowSet* Set = Harness.GetSet(Entity);
		ASSERT_THAT(IsTrue(Set->GetResource() == 0.f));
		ASSERT_THAT(IsTrue(Set->Resource.BaseValue == 0.f));
		ASSERT_THAT(IsTrue(Set->GetGainMeta() == 0.f));
		ASSERT_THAT(IsTrue(Set->GainMeta.BaseValue == 0.f));

		Harness.Tick(1.f);
		ASSERT_THAT(IsTrue(Harness.GetResource(Entity) == 0.f));
	}
};
