// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMassMechanicsTestHarness.h"
#include "ArcMassMechanicsTestTypes.h"
#include "ArcMechTestSupport.h"
#include "ArcMassMechanics/Resources/ArcResourceTypes.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Components/ActorTestSpawner.h"
#include "MassEntityTemplate.h"
#include "StructUtils/InstancedStruct.h"

namespace ArcResourceProcessorTestsPrivate
{
	UArcResourceConfig* LoadResourceConfig(const TCHAR* Name)
	{
		return ArcMechTest::LoadTestAsset<UArcResourceConfig>(*FString::Printf(TEXT("Resources/%s"), Name));
	}

	UArcAttributeHandlerConfig* LoadHandlerConfig(const TCHAR* Name)
	{
		return ArcMechTest::LoadTestAsset<UArcAttributeHandlerConfig>(*FString::Printf(TEXT("Handlers/%s"), Name));
	}
}

TEST_CLASS(ArcResourceMovement, "ArcMassMechanics.Resources.Movement")
{
	FActorTestSpawner Spawner;
	ArcMassMechanicsTest::FHarness Harness;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");

		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
	}

	TEST_METHOD(Resource_ApproachesTarget_CappedAtRateTimesDeltaTime)
	{
		UArcResourceConfig* Config = Harness.MakeConfig();
		Config->Resources.Add(ArcMassMechanicsTest::MakeResourceA(0.f, 100.f, 80.f, 10.f));
		const FMassEntityHandle Entity = Harness.CreateEntity(Config);

		const float Expected[] = {10.f, 20.f, 30.f, 40.f, 50.f, 60.f, 70.f, 80.f, 80.f};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Expected); ++Index)
		{
			Harness.Tick();
			ASSERT_THAT(IsNear(Expected[Index], Harness.GetSet(Entity)->GetResourceA(), 0.001f));
		}

		ArcMassMechanicsTest::SetFixedTarget(*Config, 0, 0.f);
		Harness.Tick(0.5f);
		ASSERT_THAT(IsNear(75.f, Harness.GetSet(Entity)->GetResourceA(), 0.001f));
		Harness.Tick(2.5f);
		ASSERT_THAT(IsNear(50.f, Harness.GetSet(Entity)->GetResourceA(), 0.001f));
	}

	TEST_METHOD(Resource_WritesThroughTheDeltaHandler_AndLeavesTheDeltaAtZero)
	{
		ArcMassMechanicsTest::FEntityOptions Options;
		Options.HandlerConfigOverride = ArcResourceProcessorTestsPrivate::LoadHandlerConfig(TEXT("AHC_Resource_Clamp40"));
		ASSERT_THAT(IsNotNull(Options.HandlerConfigOverride));
		UArcResourceConfig* ResourceConfig = ArcResourceProcessorTestsPrivate::LoadResourceConfig(TEXT("DA_ResourceConfig_Test"));
		ASSERT_THAT(IsNotNull(ResourceConfig));
		const FMassEntityHandle Entity = Harness.CreateEntity(ResourceConfig, Options);

		Harness.Tick(1.f, 2);

		const FArcMechTest_ResourceSet* Set = Harness.GetSet(Entity);
		ASSERT_THAT(IsNear(40.f, Set->GetResourceA(), 0.001f));
		ASSERT_THAT(IsNear(40.f, Set->ResourceA.BaseValue, 0.001f));
		ASSERT_THAT(IsNear(0.f, Set->GetResourceADelta(), 0.001f));
		ASSERT_THAT(IsNear(0.f, Set->ResourceADelta.BaseValue, 0.001f));
	}

	TEST_METHOD(Resource_UpdateInterval_AccumulatesElapsedTimeIntoOneStep)
	{
		UArcResourceConfig* ResourceConfig = ArcResourceProcessorTestsPrivate::LoadResourceConfig(TEXT("DA_ResourceConfig_Interval"));
		ASSERT_THAT(IsNotNull(ResourceConfig));
		const FMassEntityHandle Entity = Harness.CreateEntity(ResourceConfig);

		const float Expected[] = {0.f, 0.f, 12.f, 12.f, 12.f, 24.f};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Expected); ++Index)
		{
			Harness.Tick(0.4f);
			ASSERT_THAT(IsNear(Expected[Index], Harness.GetSet(Entity)->GetResourceA(), 0.01f));
		}
	}

	TEST_METHOD(Resource_TargetIsClampedToMinAndMax)
	{
		UArcResourceConfig* Config = Harness.MakeConfig();
		Config->Resources.Add(ArcMassMechanicsTest::MakeResourceA(0.f, 100.f, 500.f, 1000.f));
		Config->Resources.Add(ArcMassMechanicsTest::MakeResourceB(-100.f, 100.f, -500.f, 1000.f));
		const FMassEntityHandle Entity = Harness.CreateEntity(Config);

		Harness.Tick();
		ASSERT_THAT(IsNear(100.f, Harness.GetSet(Entity)->GetResourceA(), 0.001f));
		ASSERT_THAT(IsNear(-100.f, Harness.GetSet(Entity)->GetResourceB(), 0.001f));

		ArcMassMechanicsTest::SetFixedTarget(*Config, 0, -500.f);
		ArcMassMechanicsTest::SetFixedTarget(*Config, 1, 500.f);
		Harness.Tick();
		ASSERT_THAT(IsNear(0.f, Harness.GetSet(Entity)->GetResourceA(), 0.001f));
		ASSERT_THAT(IsNear(100.f, Harness.GetSet(Entity)->GetResourceB(), 0.001f));
	}

	TEST_METHOD(Resource_NegativeRate_MovesAwayFromTarget_DownToMin_AndStops)
	{
		ArcMassMechanicsTest::FEntityOptions Options;
		Options.ResourceA = 30.f;
		UArcResourceConfig* ResourceConfig = ArcResourceProcessorTestsPrivate::LoadResourceConfig(TEXT("DA_ResourceConfig_NegativeRate"));
		ASSERT_THAT(IsNotNull(ResourceConfig));
		const FMassEntityHandle Entity = Harness.CreateEntity(ResourceConfig, Options);

		const float Expected[] = {20.f, 10.f, 0.f, 0.f};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Expected); ++Index)
		{
			Harness.Tick();
			ASSERT_THAT(IsNear(Expected[Index], Harness.GetSet(Entity)->GetResourceA(), 0.001f));
		}
	}

	TEST_METHOD(Resource_WithoutDeltaHandlerConfig_DoesNotMove_AndWarnsOnce)
	{
		UArcResourceConfig* Config = Harness.MakeConfig();
		Config->Resources.Add(ArcMassMechanicsTest::MakeResourceA(0.f, 100.f, 80.f, 1000.f));

		ArcMassMechanicsTest::FEntityOptions Options;
		Options.ResourceA = 30.f;
		Options.bHandlerConfig = false;
		const FMassEntityHandle First = Harness.CreateEntity(Config, Options);
		const FMassEntityHandle Second = Harness.CreateEntity(Config, Options);

		TestRunner->AddExpectedMessage(TEXT("which has no handler on the entity; the write is dropped"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		Harness.Tick(1.f, 3);

		ASSERT_THAT(IsNear(30.f, Harness.GetSet(First)->GetResourceA(), 0.001f));
		ASSERT_THAT(IsNear(30.f, Harness.GetSet(Second)->GetResourceA(), 0.001f));
	}

	TEST_METHOD(Resource_DeltaMissingFromHandlerConfig_DoesNotMove_WhileAWiredResourceDoes)
	{
		ArcMassMechanicsTest::FEntityOptions Options;
		Options.HandlerConfigOverride = ArcResourceProcessorTestsPrivate::LoadHandlerConfig(TEXT("AHC_Resource_OnlyB"));
		ASSERT_THAT(IsNotNull(Options.HandlerConfigOverride));
		UArcResourceConfig* ResourceConfig = ArcResourceProcessorTestsPrivate::LoadResourceConfig(TEXT("DA_ResourceConfig_Test"));
		ASSERT_THAT(IsNotNull(ResourceConfig));
		const FMassEntityHandle Entity = Harness.CreateEntity(ResourceConfig, Options);

		TestRunner->AddExpectedMessage(TEXT("which has no handler on the entity; the write is dropped"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		Harness.Tick(1.f, 2);

		ASSERT_THAT(IsNear(0.f, Harness.GetSet(Entity)->GetResourceA(), 0.001f));
		ASSERT_THAT(IsNear(80.f, Harness.GetSet(Entity)->GetResourceB(), 0.001f));
	}
};

TEST_CLASS(ArcResourceTrait, "ArcMassMechanics.Resources.Trait")
{
	FActorTestSpawner Spawner;
	ArcMassMechanicsTest::FHarness Harness;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");

		ASSERT_THAT(IsTrue(Harness.Initialize(Spawner)));
	}

	TEST_METHOD(BuildTemplate_AddsTheState_AndTheSharedConfig)
	{
		UArcResourceConfig* Config = ArcResourceProcessorTestsPrivate::LoadResourceConfig(TEXT("DA_ResourceConfig_TwoResources"));
		ASSERT_THAT(IsNotNull(Config));

		FMassEntityTemplateData TemplateData;
		ArcMassMechanicsTest::BuildTraitTemplate(Config, *Harness.World, TemplateData);

		const FArcResourceStateFragment* State = nullptr;
		for (const FInstancedStruct& Initial : TemplateData.GetInitialFragmentValues())
		{
			if (const FArcResourceStateFragment* Candidate = Initial.GetPtr<FArcResourceStateFragment>())
			{
				State = Candidate;
			}
		}
		ASSERT_THAT(IsNotNull(State));
		ASSERT_THAT(IsNear(0.f, State->UpdateAccumulator, 0.0001f));

		const FArcResourceConfigSharedFragment* Shared = nullptr;
		for (const FConstSharedStruct& Candidate : TemplateData.GetSharedFragmentValues().GetConstSharedFragments())
		{
			if (Candidate.GetScriptStruct() == FArcResourceConfigSharedFragment::StaticStruct())
			{
				Shared = &Candidate.Get<FArcResourceConfigSharedFragment>();
			}
		}
		ASSERT_THAT(IsNotNull(Shared));
		ASSERT_THAT(IsTrue(Shared->Config == Config));
	}

	TEST_METHOD(BuildTemplate_WithoutConfig_AddsNothing)
	{
		TestRunner->AddExpectedError(TEXT("Config is null"), EAutomationExpectedErrorFlags::Contains, 1);

		FMassEntityTemplateData TemplateData;
		ArcMassMechanicsTest::BuildTraitTemplate(nullptr, *Harness.World, TemplateData);

		ASSERT_THAT(AreEqual(0, TemplateData.GetInitialFragmentValues().Num()));
		ASSERT_THAT(AreEqual(0, TemplateData.GetSharedFragmentValues().GetConstSharedFragments().Num()));
	}
};
