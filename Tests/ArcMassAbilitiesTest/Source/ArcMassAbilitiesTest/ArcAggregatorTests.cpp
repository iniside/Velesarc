// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "Attributes/ArcAggregator.h"
#include "Attributes/ArcAttributeAggregatorProcessor.h"
#include "Effects/ArcEffectTypes.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Modifiers/ArcModifierFunctions.h"
#include "Modifiers/ArcDirectModifier.h"
#include "Signals/ArcAttributeRecalculateSignal.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "MassSignalSubsystem.h"
#include "GameplayTagContainer.h"
#include "Tags/ArcGameplayTagRequirements.h"
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

	float EvalWithTags(const FArcAggregator& Aggregator, float BaseValue, const FGameplayTagContainer& SourceTags, const FGameplayTagContainer& TargetTags)
	{
		FGameplayTagContainer EmptyOwnerTags;
		FArcAggregationContext Ctx{FMassEntityHandle(), EmptyOwnerTags, *EntityManager, BaseValue};
		return Aggregator.EvaluateWithTags(Ctx, SourceTags, TargetTags);
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
		float Result = EvalWithTags(Aggregator, 100.f, SourceTags, TargetTags);
		ASSERT_THAT(IsNear(100.f, Result, 0.001f));
	}

	TEST_METHOD(EvaluateWithTags_AddMod_AppliesWhenNoTagReqs)
	{
		FArcAggregator Aggregator;
		Aggregator.AddMod(EArcModifierOp::Add, 25.f, FMassEntityHandle(), nullptr, nullptr);

		FGameplayTagContainer SourceTags;
		FGameplayTagContainer TargetTags;
		float Result = EvalWithTags(Aggregator, 100.f, SourceTags, TargetTags);
		ASSERT_THAT(IsNear(125.f, Result, 0.001f));
	}

	TEST_METHOD(EvaluateWithTags_FiltersModBySourceTag)
	{
		FArcGameplayTagRequirements FireReq;
		FireReq.RequireTags.AddTag(TAG_AggTest_Fire);

		FArcAggregator Aggregator;
		Aggregator.AddMod(EArcModifierOp::Add, 50.f, FMassEntityHandle(), &FireReq, nullptr);

		FGameplayTagContainer EmptySource;
		FGameplayTagContainer TargetTags;
		float WithoutTag = EvalWithTags(Aggregator, 100.f, EmptySource, TargetTags);
		ASSERT_THAT(IsNear(100.f, WithoutTag, 0.001f));

		FGameplayTagContainer FireSource;
		FireSource.AddTag(TAG_AggTest_Fire);
		float WithTag = EvalWithTags(Aggregator, 100.f, FireSource, TargetTags);
		ASSERT_THAT(IsNear(150.f, WithTag, 0.001f));
	}

	TEST_METHOD(EvaluateWithTags_DoesNotMutateBQualified)
	{
		FArcGameplayTagRequirements FireReq;
		FireReq.RequireTags.AddTag(TAG_AggTest_Fire);

		FArcAggregator Aggregator;
		Aggregator.AddMod(EArcModifierOp::Add, 50.f, FMassEntityHandle(), &FireReq, nullptr);

		const FArcAggregatorMod& Mod = Aggregator.Mods[static_cast<uint8>(EArcModifierOp::Add)][0];
		Mod.bQualified = true;

		FGameplayTagContainer EmptySource;
		FGameplayTagContainer TargetTags;
		EvalWithTags(Aggregator, 100.f, EmptySource, TargetTags);

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

		FArcGameplayTagRequirements TargetReqs;
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

		FArcGameplayTagRequirements TargetReqs;
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

		FArcGameplayTagRequirements SourceReqs;
		SourceReqs.RequireTags.AddTag(SourceTag);

		FArcOwnedTagsFragment TagsFrag;
		TagsFrag.Tags.AddTag(SourceTag);

		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(TagsFrag));
		FMassEntityHandle SourceEntity = ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(*EntityManager, Fragments);

		Aggregator.AddMod(EArcModifierOp::Add, 25.f, SourceEntity, &SourceReqs, nullptr);

		FGameplayTagContainer EmptyTags;
		FArcAggregationContext Ctx{FMassEntityHandle(), EmptyTags, *EntityManager, 100.f};
		float ResultWithTag = Aggregator.Evaluate(Ctx);
		ASSERT_THAT(IsNear(125.f, ResultWithTag, 0.001f));

		FMassEntityView SourceView(*EntityManager, SourceEntity);
		FArcOwnedTagsFragment* LiveFrag = SourceView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		check(LiveFrag);
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

// ============================================================================
// ArcAggregator_TMapRehashRegression — regression for dangling pointer bug.
// ApplyInfinite used to take &StoredModifier.SourceTagReqs from the TMap value;
// a TMap rehash (triggered at ~8 and ~16 entries) would invalidate those pointers
// before the aggregator read them. Mods now store by value so rehash is safe.
// ============================================================================

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RehashTest_Fire, "Test.Rehash.Fire");

TEST_CLASS(ArcAggregator_TMapRehashRegression, "ArcMassAbilities.Aggregator")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcAttributeAggregatorProcessor* Processor = nullptr;

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

		SignalSubsystem = Spawner.GetWorld().GetSubsystem<UMassSignalSubsystem>();
		check(SignalSubsystem);

		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		Processor = NewObject<UArcAttributeAggregatorProcessor>();
		Processor->CallInitialize(EntityManager->GetOwner(), SharedEM);
	}

	void RecalculateAttributes(FMassEntityHandle Entity)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AttributeRecalculate, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*Processor, ProcessingContext);
	}

	// Apply 64 infinite modifiers to one entity, forcing multiple TMap rehashes.
	// The first modifier has non-empty SourceTagReqs/TargetTagReqs so a dangling
	// read would produce wrong qualification results (or crash). After rehash,
	// evaluate and assert the tag-gated qualifier still behaves correctly.
	TEST_METHOD(ApplyInfinite_64Times_TagGatedModQualifiesCorrectly)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		FMassEntityHandle NoSource;

		// First modifier: +50 Health, gated on target having TAG_RehashTest_Fire.
		// All subsequent 63 modifiers: +1 Health each, no tag reqs.
		// Without the tag the gated mod should not contribute; the 63 untagged mods add 63.
		// After adding the tag the gated mod contributes; total add = 63 + 50 = 113.

		FArcGameplayTagRequirements TargetReqs;
		TargetReqs.RequireTags.AddTag(TAG_RehashTest_Fire);

		FArcDirectModifier GatedMod;
		GatedMod.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		GatedMod.Operation = EArcModifierOp::Add;
		GatedMod.Magnitude = 50.f;
		GatedMod.TargetTagReqs = TargetReqs;

		ArcModifiers::ApplyInfinite(*EntityManager, Entity, NoSource, GatedMod);

		// Add 63 more untagged +1 mods to force multiple TMap rehashes (threshold ~8, ~16, ~32, ~64)
		FArcDirectModifier PaddingMod;
		PaddingMod.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		PaddingMod.Operation = EArcModifierOp::Add;
		PaddingMod.Magnitude = 1.f;

		for (int32 i = 0; i < 63; ++i)
		{
			ArcModifiers::ApplyInfinite(*EntityManager, Entity, NoSource, PaddingMod);
		}

		// Without fire tag: gated mod skipped, only 63 untagged mods apply → Health = 100 + 63 = 163
		RecalculateAttributes(Entity);
		{
			FMassEntityView EntityView(*EntityManager, Entity);
			FArcAttributeSet_TestStats* Stats = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
			ASSERT_THAT(IsNear(163.f, Stats->Health.CurrentValue, 0.001f));
		}

		// Add the fire tag to the entity so the gated mod qualifies on next recalc
		FMassEntityView EntityView(*EntityManager, Entity);
		FArcOwnedTagsFragment* TagsFrag = EntityView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		check(TagsFrag);
		TagsFrag->Tags.AddTag(TAG_RehashTest_Fire);

		// With fire tag: gated mod applies → Health = 100 + 50 + 63 = 213
		RecalculateAttributes(Entity);
		{
			FMassEntityView EntityView2(*EntityManager, Entity);
			FArcAttributeSet_TestStats* Stats = EntityView2.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
			ASSERT_THAT(IsNear(213.f, Stats->Health.CurrentValue, 0.001f));
		}
	}
};
