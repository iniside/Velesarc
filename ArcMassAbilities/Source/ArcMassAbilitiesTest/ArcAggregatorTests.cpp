// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "Attributes/ArcAggregator.h"
#include "Effects/ArcEffectTypes.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "NativeGameplayTags.h"
#include "StructUtils/InstancedStruct.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AggTest_Fire, "Test.Fire");

// ============================================================================
// ArcAggregator_Core — pure math tests (minimal EntityManager for Evaluate)
// ============================================================================

TEST_CLASS(ArcAggregator_Core, "ArcMassAbilities.Aggregator")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();
	}

	float EvalDefault(FArcAggregator& Aggregator, float BaseValue)
	{
		FGameplayTagContainer EmptyTags;
		FArcAggregationContext Ctx{FMassEntityHandle(), EmptyTags, *EntityManager, BaseValue};
		return Aggregator.Evaluate(Ctx);
	}

	TEST_METHOD(AddMod_ReturnsValidHandle)
	{
		FArcAggregator Aggregator;
		FArcModifierHandle Handle = Aggregator.AddMod(
			EArcModifierOp::Add, 10.f, FMassEntityHandle(), nullptr, nullptr);

		ASSERT_THAT(IsTrue(Handle.IsValid()));
		ASSERT_THAT(IsTrue(Handle.Id != 0));
	}

	TEST_METHOD(RemoveMod_EmptiesAggregator)
	{
		FArcAggregator Aggregator;
		FArcModifierHandle Handle = Aggregator.AddMod(
			EArcModifierOp::Add, 10.f, FMassEntityHandle(), nullptr, nullptr);

		ASSERT_THAT(IsFalse(Aggregator.IsEmpty()));

		Aggregator.RemoveMod(Handle);

		ASSERT_THAT(IsTrue(Aggregator.IsEmpty()));
	}

	TEST_METHOD(RemoveMod_InvalidHandle_NoOp)
	{
		FArcAggregator Aggregator;
		FArcModifierHandle Handle = Aggregator.AddMod(
			EArcModifierOp::Add, 10.f, FMassEntityHandle(), nullptr, nullptr);

		FArcModifierHandle InvalidHandle;
		InvalidHandle.Id = 99999;
		Aggregator.RemoveMod(InvalidHandle);

		ASSERT_THAT(IsFalse(Aggregator.IsEmpty()));
	}

	TEST_METHOD(Evaluate_Add_Single)
	{
		FArcAggregator Aggregator;
		Aggregator.AddMod(EArcModifierOp::Add, 20.f, FMassEntityHandle(), nullptr, nullptr);

		float Result = EvalDefault(Aggregator, 100.f);

		ASSERT_THAT(IsNear(120.f, Result, 0.001f));
	}

	TEST_METHOD(Evaluate_Add_Multiple)
	{
		FArcAggregator Aggregator;
		Aggregator.AddMod(EArcModifierOp::Add, 10.f, FMassEntityHandle(), nullptr, nullptr);
		Aggregator.AddMod(EArcModifierOp::Add, 5.f, FMassEntityHandle(), nullptr, nullptr);

		float Result = EvalDefault(Aggregator, 100.f);

		ASSERT_THAT(IsNear(115.f, Result, 0.001f));
	}

	TEST_METHOD(Evaluate_MultiplyAdditive)
	{
		FArcAggregator Aggregator;
		Aggregator.AddMod(EArcModifierOp::MultiplyAdditive, 0.5f, FMassEntityHandle(), nullptr, nullptr);

		float Result = EvalDefault(Aggregator, 100.f);

		// 100 * (1 + 0.5) = 150
		ASSERT_THAT(IsNear(150.f, Result, 0.001f));
	}

	TEST_METHOD(Evaluate_DivideAdditive)
	{
		FArcAggregator Aggregator;
		Aggregator.AddMod(EArcModifierOp::DivideAdditive, 1.f, FMassEntityHandle(), nullptr, nullptr);

		float Result = EvalDefault(Aggregator, 100.f);

		// 100 / (1 + 1) = 50
		ASSERT_THAT(IsNear(50.f, Result, 0.001f));
	}

	TEST_METHOD(Evaluate_DivideAdditive_ByNegativeOne_Skipped)
	{
		FArcAggregator Aggregator;
		Aggregator.AddMod(EArcModifierOp::DivideAdditive, -1.f, FMassEntityHandle(), nullptr, nullptr);

		float Result = EvalDefault(Aggregator, 100.f);

		// 1 + (-1) = 0, division skipped
		ASSERT_THAT(IsNear(100.f, Result, 0.001f));
	}

	TEST_METHOD(Evaluate_Override)
	{
		FArcAggregator Aggregator;
		Aggregator.AddMod(EArcModifierOp::Override, 42.f, FMassEntityHandle(), nullptr, nullptr);

		float Result = EvalDefault(Aggregator, 100.f);

		ASSERT_THAT(IsNear(42.f, Result, 0.001f));
	}

	TEST_METHOD(Evaluate_OperationOrder)
	{
		FArcAggregator Aggregator;
		Aggregator.AddMod(EArcModifierOp::Add, 10.f, FMassEntityHandle(), nullptr, nullptr);
		Aggregator.AddMod(EArcModifierOp::MultiplyAdditive, 1.f, FMassEntityHandle(), nullptr, nullptr);

		float Result = EvalDefault(Aggregator, 100.f);

		// (100 + 10) * (1 + 1) = 220
		ASSERT_THAT(IsNear(220.f, Result, 0.001f));
	}

	TEST_METHOD(EvaluateWithTags_NoMods_ReturnsBaseValue)
	{
		FArcAggregator Aggregator;
		FGameplayTagContainer SourceTags;
		FGameplayTagContainer TargetTags;
		float Result = Aggregator.EvaluateWithTags(100.f, SourceTags, TargetTags);
		ASSERT_THAT(IsNear(100.f, Result, 0.001f));
	}

	TEST_METHOD(EvaluateWithTags_AddMod_AppliesWhenNoTagReqs)
	{
		FArcAggregator Aggregator;
		Aggregator.AddMod(EArcModifierOp::Add, 25.f, FMassEntityHandle(), nullptr, nullptr);

		FGameplayTagContainer SourceTags;
		FGameplayTagContainer TargetTags;
		float Result = Aggregator.EvaluateWithTags(100.f, SourceTags, TargetTags);
		ASSERT_THAT(IsNear(125.f, Result, 0.001f));
	}

	TEST_METHOD(EvaluateWithTags_FiltersModBySourceTag)
	{
		FGameplayTagRequirements FireReq;
		FireReq.RequireTags.AddTag(TAG_AggTest_Fire);

		FArcAggregator Aggregator;
		Aggregator.AddMod(EArcModifierOp::Add, 50.f, FMassEntityHandle(), &FireReq, nullptr);

		FGameplayTagContainer EmptySource;
		FGameplayTagContainer TargetTags;
		float WithoutTag = Aggregator.EvaluateWithTags(100.f, EmptySource, TargetTags);
		ASSERT_THAT(IsNear(100.f, WithoutTag, 0.001f));

		FGameplayTagContainer FireSource;
		FireSource.AddTag(TAG_AggTest_Fire);
		float WithTag = Aggregator.EvaluateWithTags(100.f, FireSource, TargetTags);
		ASSERT_THAT(IsNear(150.f, WithTag, 0.001f));
	}

	TEST_METHOD(EvaluateWithTags_DoesNotMutateBQualified)
	{
		FGameplayTagRequirements FireReq;
		FireReq.RequireTags.AddTag(TAG_AggTest_Fire);

		FArcAggregator Aggregator;
		Aggregator.AddMod(EArcModifierOp::Add, 50.f, FMassEntityHandle(), &FireReq, nullptr);

		const FArcAggregatorMod& Mod = Aggregator.Mods[static_cast<uint8>(EArcModifierOp::Add)][0];
		Mod.bQualified = true;

		FGameplayTagContainer EmptySource;
		FGameplayTagContainer TargetTags;
		Aggregator.EvaluateWithTags(100.f, EmptySource, TargetTags);

		ASSERT_THAT(IsTrue(Mod.bQualified));
	}
};

// ============================================================================
// ArcAggregator_TagQualification — needs EntityManager for source tag eval
// ============================================================================

TEST_CLASS(ArcAggregator_TagQualification, "ArcMassAbilities.Aggregator")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();
	}

	TEST_METHOD(Evaluate_TagReqs_Unqualified_Skipped)
	{
		FArcAggregator Aggregator;

		FGameplayTagRequirements TargetReqs;
		FGameplayTag FireTag = TAG_AggTest_Fire;
		TargetReqs.RequireTags.AddTag(FireTag);

		Aggregator.AddMod(EArcModifierOp::Add, 50.f, FMassEntityHandle(), nullptr, &TargetReqs);

		FGameplayTagContainer EmptyTags;
		FArcAggregationContext Ctx{FMassEntityHandle(), EmptyTags, *EntityManager, 100.f};
		float Result = Aggregator.Evaluate(Ctx);

		ASSERT_THAT(IsNear(100.f, Result, 0.001f));
	}

	TEST_METHOD(Evaluate_TagReqs_Qualified_Applied)
	{
		FArcAggregator Aggregator;

		FGameplayTagRequirements TargetReqs;
		FGameplayTag FireTag = TAG_AggTest_Fire;
		TargetReqs.RequireTags.AddTag(FireTag);

		Aggregator.AddMod(EArcModifierOp::Add, 50.f, FMassEntityHandle(), nullptr, &TargetReqs);

		FGameplayTagContainer TargetTags;
		TargetTags.AddTag(FireTag);
		FArcAggregationContext Ctx{FMassEntityHandle(), TargetTags, *EntityManager, 100.f};
		float Result = Aggregator.Evaluate(Ctx);

		ASSERT_THAT(IsNear(150.f, Result, 0.001f));
	}

	TEST_METHOD(Evaluate_SourceTags_FromEntity)
	{
		FArcAggregator Aggregator;

		FGameplayTag SourceTag = TAG_AggTest_Fire;

		FGameplayTagRequirements SourceReqs;
		SourceReqs.RequireTags.AddTag(SourceTag);

		FArcOwnedTagsFragment TagsFrag;
		TagsFrag.Tags.AddTag(SourceTag);

		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(TagsFrag));
		FMassEntityHandle SourceEntity = EntityManager->CreateEntity(Fragments);

		Aggregator.AddMod(EArcModifierOp::Add, 25.f, SourceEntity, &SourceReqs, nullptr);

		FGameplayTagContainer EmptyTags;
		FArcAggregationContext Ctx{FMassEntityHandle(), EmptyTags, *EntityManager, 100.f};
		float ResultWithTag = Aggregator.Evaluate(Ctx);
		ASSERT_THAT(IsNear(125.f, ResultWithTag, 0.001f));

		FArcOwnedTagsFragment* LiveFrag = EntityManager->GetFragmentDataPtr<FArcOwnedTagsFragment>(SourceEntity);
		LiveFrag->Tags.RemoveTag(SourceTag);

		float ResultWithoutTag = Aggregator.Evaluate(Ctx);
		ASSERT_THAT(IsNear(100.f, ResultWithoutTag, 0.001f));
	}
};

// ============================================================================
// ArcAggregator_CustomPolicy — custom aggregation policy tests
// ============================================================================

TEST_CLASS(ArcAggregator_CustomPolicy, "ArcMassAbilities.Aggregator")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();
	}

	TEST_METHOD(DefaultPolicy_SameAsNoPolicy)
	{
		FArcAggregator AggDefault;
		AggDefault.AddMod(EArcModifierOp::Add, 10.f, FMassEntityHandle(), nullptr, nullptr);
		AggDefault.AddMod(EArcModifierOp::Add, 5.f, FMassEntityHandle(), nullptr, nullptr);

		FArcAggregator AggWithPolicy;
		AggWithPolicy.AddMod(EArcModifierOp::Add, 10.f, FMassEntityHandle(), nullptr, nullptr);
		AggWithPolicy.AddMod(EArcModifierOp::Add, 5.f, FMassEntityHandle(), nullptr, nullptr);
		AggWithPolicy.AggregationPolicy = FConstSharedStruct::Make(FArcAggregationPolicy());

		FGameplayTagContainer EmptyTags;
		FArcAggregationContext Ctx1{FMassEntityHandle(), EmptyTags, *EntityManager, 100.f};
		FArcAggregationContext Ctx2{FMassEntityHandle(), EmptyTags, *EntityManager, 100.f};

		float ResultDefault = AggDefault.Evaluate(Ctx1);
		float ResultPolicy = AggWithPolicy.Evaluate(Ctx2);

		ASSERT_THAT(IsNear(ResultDefault, ResultPolicy, 0.001f));
	}

	TEST_METHOD(HighestAdditivePolicy_OnlyHighestAdd)
	{
		FArcAggregator Aggregator;
		Aggregator.AddMod(EArcModifierOp::Add, 10.f, FMassEntityHandle(), nullptr, nullptr);
		Aggregator.AddMod(EArcModifierOp::Add, 50.f, FMassEntityHandle(), nullptr, nullptr);
		Aggregator.AddMod(EArcModifierOp::Add, 30.f, FMassEntityHandle(), nullptr, nullptr);
		Aggregator.AggregationPolicy = FConstSharedStruct::Make(FArcTestHighestAdditivePolicy());

		FGameplayTagContainer EmptyTags;
		FArcAggregationContext Ctx{FMassEntityHandle(), EmptyTags, *EntityManager, 100.f};
		float Result = Aggregator.Evaluate(Ctx);

		ASSERT_THAT(IsNear(150.f, Result, 0.001f));
	}

	TEST_METHOD(HighestAdditivePolicy_MultiplyStillAggregatesAll)
	{
		FArcAggregator Aggregator;
		Aggregator.AddMod(EArcModifierOp::Add, 10.f, FMassEntityHandle(), nullptr, nullptr);
		Aggregator.AddMod(EArcModifierOp::Add, 50.f, FMassEntityHandle(), nullptr, nullptr);
		Aggregator.AddMod(EArcModifierOp::MultiplyAdditive, 1.f, FMassEntityHandle(), nullptr, nullptr);
		Aggregator.AggregationPolicy = FConstSharedStruct::Make(FArcTestHighestAdditivePolicy());

		FGameplayTagContainer EmptyTags;
		FArcAggregationContext Ctx{FMassEntityHandle(), EmptyTags, *EntityManager, 100.f};
		float Result = Aggregator.Evaluate(Ctx);

		// (100 + 50) * (1 + 1) = 300
		ASSERT_THAT(IsNear(300.f, Result, 0.001f));
	}

	TEST_METHOD(NoPolicy_FallsBackToDefault)
	{
		FArcAggregator Aggregator;
		Aggregator.AddMod(EArcModifierOp::Add, 10.f, FMassEntityHandle(), nullptr, nullptr);
		Aggregator.AddMod(EArcModifierOp::Add, 5.f, FMassEntityHandle(), nullptr, nullptr);

		FGameplayTagContainer EmptyTags;
		FArcAggregationContext Ctx{FMassEntityHandle(), EmptyTags, *EntityManager, 100.f};
		float Result = Aggregator.Evaluate(Ctx);

		ASSERT_THAT(IsNear(115.f, Result, 0.001f));
	}

	TEST_METHOD(EvaluateDefault_StaticHelper_Works)
	{
		TArray<FArcAggregatorMod> Mods[static_cast<uint8>(EArcModifierOp::Max)];

		FArcAggregatorMod AddMod;
		AddMod.Magnitude = 20.f;
		AddMod.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::Add)].Add(AddMod);

		FArcAggregatorMod MulMod;
		MulMod.Magnitude = 1.f;
		MulMod.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::MultiplyAdditive)].Add(MulMod);

		float Result = FArcAggregationPolicy::EvaluateDefault(100.f, Mods);

		// (100 + 20) * (1 + 1) = 240
		ASSERT_THAT(IsNear(240.f, Result, 0.001f));
	}
};
