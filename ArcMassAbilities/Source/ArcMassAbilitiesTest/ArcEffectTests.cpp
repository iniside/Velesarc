// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcActiveEffectHandle.h"
#include "Effects/ArcEffectExecution.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcEffectSpec.h"
#include "Effects/ArcEffectTypes.h"
#include "Effects/ArcAttributeCapture.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Attributes/ArcAttribute.h"
#include "Processors/ArcPendingAttributeOpsProcessor.h"
#include "Effects/ArcEffectDurationProcessor.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "MassExecutor.h"
#include "MassProcessingContext.h"
#include "MassSignalSubsystem.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "ScalableFloat.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EffTest_Fire, "Test.Fire");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EffTest_Buff, "Test.Buff");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EffTest_BuffFire, "Buff.Fire");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EffTest_BuffParent, "Buff");

// ============================================================================
// ArcEffect_Instant
// ============================================================================

TEST_CLASS(ArcEffect_Instant, "ArcMassAbilities.Effects.Instant")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcPendingAttributeOpsProcessor* PendingProcessor = nullptr;

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
		PendingProcessor = NewObject<UArcPendingAttributeOpsProcessor>();
		PendingProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);
	}

	void DrainPendingOps(FMassEntityHandle Entity)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::PendingAttributeOps, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*PendingProcessor, ProcessingContext);
	}

	TEST_METHOD(Add_ModifiesBaseValue)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;

		FArcTestAddExecution Exec;
		Exec.TargetAttribute = FArcTestStatsFragment::GetHealthAttribute();
		Exec.Value = 50.f;
		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		DrainPendingOps(Entity);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcTestStatsFragment* Stats = EntityView.GetFragmentDataPtr<FArcTestStatsFragment>();
		ASSERT_THAT(IsNear(150.f, Stats->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(Override_SetsBaseValue)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;

		FArcTestOverrideExecution Exec;
		Exec.TargetAttribute = FArcTestStatsFragment::GetHealthAttribute();
		Exec.Value = 42.f;
		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		DrainPendingOps(Entity);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcTestStatsFragment* Stats = EntityView.GetFragmentDataPtr<FArcTestStatsFragment>();
		ASSERT_THAT(IsNear(42.f, Stats->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(NoEffectStackEntry)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;

		FArcTestAddExecution Exec;
		Exec.TargetAttribute = FArcTestStatsFragment::GetHealthAttribute();
		Exec.Value = 10.f;
		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(0, Stack->ActiveEffects.Num()));
	}
};

// ============================================================================
// ArcEffect_Duration
// ============================================================================

TEST_CLASS(ArcEffect_Duration, "ArcMassAbilities.Effects.Duration")
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

	TEST_METHOD(AddsToStack)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(25.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(1, Stack->ActiveEffects.Num()));
		ASSERT_THAT(IsTrue(Stack->ActiveEffects[0].ModifierHandles.Num() > 0));
	}

	TEST_METHOD(RemoveEffect_CleansUp)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(25.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		ArcEffects::RemoveEffect(*EntityManager, Entity, Effect);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		FArcAggregatorFragment* Agg = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();
		ASSERT_THAT(AreEqual(0, Stack->ActiveEffects.Num()));
		for (TPair<FArcAttributeRef, FArcAggregator>& Pair : Agg->Aggregators)
		{
			ASSERT_THAT(IsTrue(Pair.Value.IsEmpty()));
		}
	}

	TEST_METHOD(RemoveAllEffects_ClearsEverything)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* EffectA = NewObject<UArcEffectDefinition>();
		EffectA->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		EffectA->StackingPolicy.Duration = 5.f;
		FArcEffectModifier ModA;
		ModA.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		ModA.Operation = EArcModifierOp::Add;
		ModA.Magnitude = FScalableFloat(10.f);
		EffectA->Modifiers.Add(ModA);

		UArcEffectDefinition* EffectB = NewObject<UArcEffectDefinition>();
		EffectB->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		EffectB->StackingPolicy.Duration = 10.f;
		FArcEffectModifier ModB;
		ModB.Attribute = FArcTestStatsFragment::GetArmorAttribute();
		ModB.Operation = EArcModifierOp::Add;
		ModB.Magnitude = FScalableFloat(20.f);
		EffectB->Modifiers.Add(ModB);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, EffectA, FMassEntityHandle());
		ArcEffects::TryApplyEffect(*EntityManager, Entity, EffectB, FMassEntityHandle());

		ArcEffects::RemoveAllEffects(*EntityManager, Entity);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(0, Stack->ActiveEffects.Num()));
	}
};

// ============================================================================
// ArcEffect_Stacking
// ============================================================================

TEST_CLASS(ArcEffect_Stacking, "ArcMassAbilities.Effects.Stacking")
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

	TEST_METHOD(DenyNew_SecondApplication_ReturnsFalse)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;
		Effect->StackingPolicy.StackPolicy = EArcStackPolicy::DenyNew;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		FArcActiveEffectHandle hFirst = ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		FArcActiveEffectHandle hSecond = ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		ASSERT_THAT(IsTrue(hFirst.IsValid()));
		ASSERT_THAT(IsFalse(hSecond.IsValid()));

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(1, Stack->ActiveEffects.Num()));
		const FArcEffectSpec* Spec0 = Stack->ActiveEffects[0].Spec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(Spec0));
		ASSERT_THAT(AreEqual(1, Spec0->Context.StackCount));
	}

	TEST_METHOD(Replace_ResetsStack)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;
		Effect->StackingPolicy.StackPolicy = EArcStackPolicy::Replace;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(1, Stack->ActiveEffects.Num()));
		const FArcEffectSpec* SpecReplace = Stack->ActiveEffects[0].Spec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(SpecReplace));
		ASSERT_THAT(AreEqual(1, SpecReplace->Context.StackCount));
		ASSERT_THAT(IsNear(5.f, Stack->ActiveEffects[0].RemainingDuration, 0.001f));
	}

	TEST_METHOD(Aggregate_StacksUp)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;
		Effect->StackingPolicy.StackPolicy = EArcStackPolicy::Aggregate;
		Effect->StackingPolicy.MaxStacks = 5;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(1, Stack->ActiveEffects.Num()));
		const FArcEffectSpec* SpecAgg = Stack->ActiveEffects[0].Spec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(SpecAgg));
		ASSERT_THAT(AreEqual(2, SpecAgg->Context.StackCount));
	}

	TEST_METHOD(Aggregate_MaxStacks_Caps)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;
		Effect->StackingPolicy.StackPolicy = EArcStackPolicy::Aggregate;
		Effect->StackingPolicy.MaxStacks = 3;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(1, Stack->ActiveEffects.Num()));
		const FArcEffectSpec* SpecCapped = Stack->ActiveEffects[0].Spec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(SpecCapped));
		ASSERT_THAT(AreEqual(3, SpecCapped->Context.StackCount));
	}

	TEST_METHOD(Aggregate_Refresh_ResetsDuration)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;
		Effect->StackingPolicy.StackPolicy = EArcStackPolicy::Aggregate;
		Effect->StackingPolicy.MaxStacks = 5;
		Effect->StackingPolicy.RefreshPolicy = EArcStackDurationRefresh::Refresh;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView ViewBefore(*EntityManager, Entity);
		FArcEffectStackFragment* StackBefore = ViewBefore.GetFragmentDataPtr<FArcEffectStackFragment>();
		StackBefore->ActiveEffects[0].RemainingDuration = 2.f;

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView ViewAfter(*EntityManager, Entity);
		FArcEffectStackFragment* StackAfter = ViewAfter.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(IsNear(5.f, StackAfter->ActiveEffects[0].RemainingDuration, 0.001f));
	}

	TEST_METHOD(Aggregate_Extend_AddsDuration)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;
		Effect->StackingPolicy.StackPolicy = EArcStackPolicy::Aggregate;
		Effect->StackingPolicy.MaxStacks = 5;
		Effect->StackingPolicy.RefreshPolicy = EArcStackDurationRefresh::Extend;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(IsNear(10.f, Stack->ActiveEffects[0].RemainingDuration, 0.001f));
	}

	TEST_METHOD(Aggregate_Independent_NoDurationChange)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;
		Effect->StackingPolicy.StackPolicy = EArcStackPolicy::Aggregate;
		Effect->StackingPolicy.MaxStacks = 5;
		Effect->StackingPolicy.RefreshPolicy = EArcStackDurationRefresh::Independent;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView ViewBefore(*EntityManager, Entity);
		FArcEffectStackFragment* StackBefore = ViewBefore.GetFragmentDataPtr<FArcEffectStackFragment>();
		float DurationAfterFirst = StackBefore->ActiveEffects[0].RemainingDuration;

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView ViewAfter(*EntityManager, Entity);
		FArcEffectStackFragment* StackAfter = ViewAfter.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(IsNear(DurationAfterFirst, StackAfter->ActiveEffects[0].RemainingDuration, 0.001f));
	}

	TEST_METHOD(Aggregate_PeriodPolicy_Reset)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Periodicity = EArcEffectPeriodicity::Periodic;
		Effect->StackingPolicy.Duration = 10.f;
		Effect->StackingPolicy.Period = 2.f;
		Effect->StackingPolicy.StackType = EArcStackType::Counter;
		Effect->StackingPolicy.StackPolicy = EArcStackPolicy::Aggregate;
		Effect->StackingPolicy.MaxStacks = 5;
		Effect->StackingPolicy.PeriodPolicy = EArcStackPeriodPolicy::Reset;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView ViewBefore(*EntityManager, Entity);
		FArcEffectStackFragment* StackBefore = ViewBefore.GetFragmentDataPtr<FArcEffectStackFragment>();
		StackBefore->ActiveEffects[0].PeriodTimer = 0.5f;

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView ViewAfter(*EntityManager, Entity);
		FArcEffectStackFragment* StackAfter = ViewAfter.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(IsNear(2.f, StackAfter->ActiveEffects[0].PeriodTimer, 0.001f));
	}

	TEST_METHOD(Aggregate_PeriodPolicy_Unchanged)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Periodicity = EArcEffectPeriodicity::Periodic;
		Effect->StackingPolicy.Duration = 10.f;
		Effect->StackingPolicy.Period = 2.f;
		Effect->StackingPolicy.StackType = EArcStackType::Counter;
		Effect->StackingPolicy.StackPolicy = EArcStackPolicy::Aggregate;
		Effect->StackingPolicy.MaxStacks = 5;
		Effect->StackingPolicy.PeriodPolicy = EArcStackPeriodPolicy::Unchanged;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView ViewBefore(*EntityManager, Entity);
		FArcEffectStackFragment* StackBefore = ViewBefore.GetFragmentDataPtr<FArcEffectStackFragment>();
		StackBefore->ActiveEffects[0].PeriodTimer = 0.5f;

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView ViewAfter(*EntityManager, Entity);
		FArcEffectStackFragment* StackAfter = ViewAfter.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(IsNear(0.5f, StackAfter->ActiveEffects[0].PeriodTimer, 0.001f));
	}
};

// ============================================================================
// ArcEffect_Components
// ============================================================================

TEST_CLASS(ArcEffect_Components, "ArcMassAbilities.Effects.Components")
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

	TEST_METHOD(TagFilter_Blocks_WhenReqsNotMet)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		FArcEffectComp_TagFilter TagFilter;
		TagFilter.RequirementFilter.RequireTags.AddTag(TAG_EffTest_Fire);
		Effect->Components.Add(FInstancedStruct::Make(TagFilter));

		FArcActiveEffectHandle hResult = ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		ASSERT_THAT(IsFalse(hResult.IsValid()));
	}

	TEST_METHOD(TagFilter_Passes_WhenReqsMet)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		FMassEntityView SetupView(*EntityManager, Entity);
		FArcOwnedTagsFragment* Tags = SetupView.GetFragmentDataPtr<FArcOwnedTagsFragment>();
		Tags->Tags.AddTag(TAG_EffTest_Fire);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		FArcEffectComp_TagFilter TagFilter;
		TagFilter.RequirementFilter.RequireTags.AddTag(TAG_EffTest_Fire);
		Effect->Components.Add(FInstancedStruct::Make(TagFilter));

		FArcActiveEffectHandle hResult = ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		ASSERT_THAT(IsTrue(hResult.IsValid()));

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(1, Stack->ActiveEffects.Num()));
	}

	TEST_METHOD(GrantTags_AddsOnApply_RemovesOnRemove)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		FGameplayTag BuffTag = TAG_EffTest_Buff;

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;

		FArcEffectComp_GrantTags GrantTags;
		GrantTags.GrantedTags.AddTag(BuffTag);
		Effect->Components.Add(FInstancedStruct::Make(GrantTags));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView ViewApplied(*EntityManager, Entity);
		FArcOwnedTagsFragment* TagsApplied = ViewApplied.GetFragmentDataPtr<FArcOwnedTagsFragment>();
		ASSERT_THAT(IsTrue(TagsApplied->Tags.HasTag(BuffTag)));

		ArcEffects::RemoveEffect(*EntityManager, Entity, Effect);

		FMassEntityView ViewRemoved(*EntityManager, Entity);
		FArcOwnedTagsFragment* TagsRemoved = ViewRemoved.GetFragmentDataPtr<FArcOwnedTagsFragment>();
		ASSERT_THAT(IsFalse(TagsRemoved->Tags.HasTag(BuffTag)));
	}

	TEST_METHOD(RemoveEffectByTag_CleansMatchingEffects)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* EffectA = NewObject<UArcEffectDefinition>();
		FArcEffectComp_EffectTags EffectATags;
		EffectATags.OwnedTags.AddTag(TAG_EffTest_BuffFire);
		EffectA->Components.Add(FInstancedStruct::Make(EffectATags));
		EffectA->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		EffectA->StackingPolicy.Duration = 10.f;
		FArcEffectModifier ModA;
		ModA.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		ModA.Operation = EArcModifierOp::Add;
		ModA.Magnitude = FScalableFloat(10.f);
		EffectA->Modifiers.Add(ModA);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, EffectA, FMassEntityHandle());

		UArcEffectDefinition* EffectB = NewObject<UArcEffectDefinition>();
		EffectB->StackingPolicy.DurationType = EArcEffectDuration::Instant;

		FArcEffectComp_RemoveEffectByTag RemoveByTag;
		RemoveByTag.EffectTag = TAG_EffTest_BuffParent;
		EffectB->Components.Add(FInstancedStruct::Make(RemoveByTag));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, EffectB, FMassEntityHandle());

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(0, Stack->ActiveEffects.Num()));
	}
};

// ============================================================================
// ArcEffect_PeriodicExecution
// ============================================================================

TEST_CLASS(ArcEffect_PeriodicExecution, "ArcMassAbilities.Effects.PeriodicExecution")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcPendingAttributeOpsProcessor* PendingProcessor = nullptr;
	UArcEffectDurationProcessor* DurationProcessor = nullptr;

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
		PendingProcessor = NewObject<UArcPendingAttributeOpsProcessor>();
		PendingProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);

		DurationProcessor = NewObject<UArcEffectDurationProcessor>();
		DurationProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);
	}

	void DrainPendingOps(FMassEntityHandle Entity)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::PendingAttributeOps, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*PendingProcessor, ProcessingContext);
	}

	void TickDuration(float DeltaTime)
	{
		FMassProcessingContext ProcessingContext(*EntityManager, DeltaTime);
		UE::Mass::Executor::Run(*DurationProcessor, ProcessingContext);
	}

	TEST_METHOD(PeriodAndApplication_FiresAtApply)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Periodicity = EArcEffectPeriodicity::Periodic;
		Effect->StackingPolicy.PeriodicExecPolicy = EArcPeriodicExecutionPolicy::PeriodAndApplication;
		Effect->StackingPolicy.Duration = 10.f;
		Effect->StackingPolicy.Period = 2.f;

		FArcTestAddExecution Exec;
		Exec.TargetAttribute = FArcTestStatsFragment::GetHealthAttribute();
		Exec.Value = -10.f;
		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		DrainPendingOps(Entity);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcTestStatsFragment* Stats = EntityView.GetFragmentDataPtr<FArcTestStatsFragment>();
		ASSERT_THAT(IsNear(90.f, Stats->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(PeriodOnly_DoesNotFireAtApply)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Periodicity = EArcEffectPeriodicity::Periodic;
		Effect->StackingPolicy.PeriodicExecPolicy = EArcPeriodicExecutionPolicy::PeriodOnly;
		Effect->StackingPolicy.Duration = 10.f;
		Effect->StackingPolicy.Period = 2.f;

		FArcTestAddExecution Exec;
		Exec.TargetAttribute = FArcTestStatsFragment::GetHealthAttribute();
		Exec.Value = -10.f;
		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcTestStatsFragment* Stats = EntityView.GetFragmentDataPtr<FArcTestStatsFragment>();
		ASSERT_THAT(IsNear(100.f, Stats->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(PeriodicWithModifiers_ModifierInAggregator_ExecutionInstant)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Periodicity = EArcEffectPeriodicity::Periodic;
		Effect->StackingPolicy.PeriodicExecPolicy = EArcPeriodicExecutionPolicy::PeriodAndApplication;
		Effect->StackingPolicy.Duration = 10.f;
		Effect->StackingPolicy.Period = 2.f;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetArmorAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(-5.f);
		Effect->Modifiers.Add(Mod);

		FArcTestAddExecution Exec;
		Exec.TargetAttribute = FArcTestStatsFragment::GetHealthAttribute();
		Exec.Value = -10.f;
		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		DrainPendingOps(Entity);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcTestStatsFragment* Stats = EntityView.GetFragmentDataPtr<FArcTestStatsFragment>();
		ASSERT_THAT(IsNear(90.f, Stats->Health.BaseValue, 0.001f));

		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(1, Stack->ActiveEffects.Num()));
		ASSERT_THAT(IsTrue(Stack->ActiveEffects[0].ModifierHandles.Num() > 0));
	}

	TEST_METHOD(SimpleModifierEntry_AppliesAsPendingOp)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;

		FArcEffectExecutionEntry Entry;
		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(-25.f);
		Entry.Modifiers.Add(Mod);
		Effect->Executions.Add(Entry);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		DrainPendingOps(Entity);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcTestStatsFragment* Stats = EntityView.GetFragmentDataPtr<FArcTestStatsFragment>();
		ASSERT_THAT(IsNear(75.f, Stats->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(InfinitePeriodic_StaysActive)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		Effect->StackingPolicy.Periodicity = EArcEffectPeriodicity::Periodic;
		Effect->StackingPolicy.Period = 1.f;
		Effect->StackingPolicy.PeriodicExecPolicy = EArcPeriodicExecutionPolicy::PeriodOnly;

		FArcEffectExecutionEntry Entry;
		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(-5.f);
		Entry.Modifiers.Add(Mod);
		Effect->Executions.Add(Entry);

		FArcActiveEffectHandle Handle = ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		ASSERT_THAT(IsTrue(Handle.IsValid()));

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(1, Stack->ActiveEffects.Num()));
		ASSERT_THAT(IsNear(1.f, Stack->ActiveEffects[0].PeriodTimer, 0.001f));

		FArcTestStatsFragment* Stats = EntityView.GetFragmentDataPtr<FArcTestStatsFragment>();
		ASSERT_THAT(IsNear(100.f, Stats->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(PeriodicTick_AppliesSimpleModifierExecution)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 10.f;
		Effect->StackingPolicy.Periodicity = EArcEffectPeriodicity::Periodic;
		Effect->StackingPolicy.Period = 1.f;
		Effect->StackingPolicy.PeriodicExecPolicy = EArcPeriodicExecutionPolicy::PeriodAndApplication;

		FArcEffectExecutionEntry Entry;
		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(-10.f);
		Entry.Modifiers.Add(Mod);
		Effect->Executions.Add(Entry);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		DrainPendingOps(Entity);

		FMassEntityView View1(*EntityManager, Entity);
		FArcTestStatsFragment* Stats1 = View1.GetFragmentDataPtr<FArcTestStatsFragment>();
		ASSERT_THAT(IsNear(90.f, Stats1->Health.BaseValue, 0.001f));

		TickDuration(1.1f);
		DrainPendingOps(Entity);

		FMassEntityView View2(*EntityManager, Entity);
		FArcTestStatsFragment* Stats2 = View2.GetFragmentDataPtr<FArcTestStatsFragment>();
		ASSERT_THAT(IsNear(80.f, Stats2->Health.BaseValue, 0.001f));

		TickDuration(1.1f);
		DrainPendingOps(Entity);

		FMassEntityView View3(*EntityManager, Entity);
		FArcTestStatsFragment* Stats3 = View3.GetFragmentDataPtr<FArcTestStatsFragment>();
		ASSERT_THAT(IsNear(70.f, Stats3->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(PeriodOnly_DoesNotFireAtApply_ButFiresOnTick)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 10.f;
		Effect->StackingPolicy.Periodicity = EArcEffectPeriodicity::Periodic;
		Effect->StackingPolicy.Period = 1.f;
		Effect->StackingPolicy.PeriodicExecPolicy = EArcPeriodicExecutionPolicy::PeriodOnly;

		FArcEffectExecutionEntry Entry;
		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(-10.f);
		Entry.Modifiers.Add(Mod);
		Effect->Executions.Add(Entry);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		DrainPendingOps(Entity);

		FMassEntityView View1(*EntityManager, Entity);
		FArcTestStatsFragment* Stats1 = View1.GetFragmentDataPtr<FArcTestStatsFragment>();
		ASSERT_THAT(IsNear(100.f, Stats1->Health.BaseValue, 0.001f));

		TickDuration(1.1f);
		DrainPendingOps(Entity);

		FMassEntityView View2(*EntityManager, Entity);
		FArcTestStatsFragment* Stats2 = View2.GetFragmentDataPtr<FArcTestStatsFragment>();
		ASSERT_THAT(IsNear(90.f, Stats2->Health.BaseValue, 0.001f));
	}
};

// ============================================================================
// ArcEffect_BySource_Counter
// ============================================================================

TEST_CLASS(ArcEffect_BySource_Counter, "ArcMassAbilities.Effects.Stacking.BySource.Counter")
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

	TEST_METHOD(DifferentSources_CreateSeparateStacks)
	{
		FMassEntityHandle Target = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		FMassEntityHandle SourceA = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		FMassEntityHandle SourceB = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;
		Effect->StackingPolicy.StackType = EArcStackType::Counter;
		Effect->StackingPolicy.Grouping = EArcStackGrouping::BySource;
		Effect->StackingPolicy.StackPolicy = EArcStackPolicy::Aggregate;
		Effect->StackingPolicy.MaxStacks = 5;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceA);
		ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceB);

		FMassEntityView EntityView(*EntityManager, Target);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(2, Stack->ActiveEffects.Num()));
		const FArcEffectSpec* SpecSrc0 = Stack->ActiveEffects[0].Spec.GetPtr<FArcEffectSpec>();
		const FArcEffectSpec* SpecSrc1 = Stack->ActiveEffects[1].Spec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(SpecSrc0));
		ASSERT_THAT(IsNotNull(SpecSrc1));
		ASSERT_THAT(AreEqual(1, SpecSrc0->Context.StackCount));
		ASSERT_THAT(AreEqual(1, SpecSrc1->Context.StackCount));
	}

	TEST_METHOD(SameSource_AggregatesSameStack)
	{
		FMassEntityHandle Target = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		FMassEntityHandle SourceA = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;
		Effect->StackingPolicy.StackType = EArcStackType::Counter;
		Effect->StackingPolicy.Grouping = EArcStackGrouping::BySource;
		Effect->StackingPolicy.StackPolicy = EArcStackPolicy::Aggregate;
		Effect->StackingPolicy.MaxStacks = 5;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceA);
		ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceA);

		FMassEntityView EntityView(*EntityManager, Target);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(1, Stack->ActiveEffects.Num()));
		const FArcEffectSpec* SpecSameAgg = Stack->ActiveEffects[0].Spec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(SpecSameAgg));
		ASSERT_THAT(AreEqual(2, SpecSameAgg->Context.StackCount));
	}

	TEST_METHOD(BySource_DenyNew_PerSource)
	{
		FMassEntityHandle Target = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		FMassEntityHandle SourceA = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		FMassEntityHandle SourceB = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;
		Effect->StackingPolicy.StackType = EArcStackType::Counter;
		Effect->StackingPolicy.Grouping = EArcStackGrouping::BySource;
		Effect->StackingPolicy.StackPolicy = EArcStackPolicy::DenyNew;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		FArcActiveEffectHandle hA1 = ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceA);
		FArcActiveEffectHandle hA2 = ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceA);
		FArcActiveEffectHandle hB1 = ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceB);

		ASSERT_THAT(IsTrue(hA1.IsValid()));
		ASSERT_THAT(IsFalse(hA2.IsValid()));
		ASSERT_THAT(IsTrue(hB1.IsValid()));

		FMassEntityView EntityView(*EntityManager, Target);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(2, Stack->ActiveEffects.Num()));
	}

	TEST_METHOD(BySource_Replace_ReplacesOnlyMatchingSource)
	{
		FMassEntityHandle Target = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		FMassEntityHandle SourceA = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		FMassEntityHandle SourceB = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;
		Effect->StackingPolicy.StackType = EArcStackType::Counter;
		Effect->StackingPolicy.Grouping = EArcStackGrouping::BySource;
		Effect->StackingPolicy.StackPolicy = EArcStackPolicy::Replace;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceA);
		ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceB);

		FMassEntityView ViewBefore(*EntityManager, Target);
		FArcEffectStackFragment* StackBefore = ViewBefore.GetFragmentDataPtr<FArcEffectStackFragment>();
		StackBefore->ActiveEffects[0].RemainingDuration = 1.f;

		ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceA);

		FMassEntityView EntityView(*EntityManager, Target);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(2, Stack->ActiveEffects.Num()));
		ASSERT_THAT(IsNear(5.f, Stack->ActiveEffects[0].RemainingDuration, 0.001f));
		ASSERT_THAT(IsNear(5.f, Stack->ActiveEffects[1].RemainingDuration, 0.001f));
	}

	TEST_METHOD(BySource_MaxStacks_PerSource)
	{
		FMassEntityHandle Target = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		FMassEntityHandle SourceA = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		FMassEntityHandle SourceB = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;
		Effect->StackingPolicy.StackType = EArcStackType::Counter;
		Effect->StackingPolicy.Grouping = EArcStackGrouping::BySource;
		Effect->StackingPolicy.StackPolicy = EArcStackPolicy::Aggregate;
		Effect->StackingPolicy.MaxStacks = 2;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceA);
		ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceA);
		ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceA);
		ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceB);
		ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceB);
		ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceB);

		FMassEntityView EntityView(*EntityManager, Target);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(2, Stack->ActiveEffects.Num()));
		const FArcEffectSpec* SpecMaxSrc0 = Stack->ActiveEffects[0].Spec.GetPtr<FArcEffectSpec>();
		const FArcEffectSpec* SpecMaxSrc1 = Stack->ActiveEffects[1].Spec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(SpecMaxSrc0));
		ASSERT_THAT(IsNotNull(SpecMaxSrc1));
		ASSERT_THAT(AreEqual(2, SpecMaxSrc0->Context.StackCount));
		ASSERT_THAT(AreEqual(2, SpecMaxSrc1->Context.StackCount));
	}
};

// ============================================================================
// ArcEffect_Instance_ByTarget
// ============================================================================

TEST_CLASS(ArcEffect_Instance_ByTarget, "ArcMassAbilities.Effects.Stacking.Instance.ByTarget")
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

	TEST_METHOD(EachApplication_CreatesNewEntry)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;
		Effect->StackingPolicy.StackType = EArcStackType::Instance;
		Effect->StackingPolicy.Grouping = EArcStackGrouping::ByTarget;
		Effect->StackingPolicy.MaxStacks = 5;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(3, Stack->ActiveEffects.Num()));
		const FArcEffectSpec* SpecInst0 = Stack->ActiveEffects[0].Spec.GetPtr<FArcEffectSpec>();
		const FArcEffectSpec* SpecInst1 = Stack->ActiveEffects[1].Spec.GetPtr<FArcEffectSpec>();
		const FArcEffectSpec* SpecInst2 = Stack->ActiveEffects[2].Spec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(SpecInst0));
		ASSERT_THAT(IsNotNull(SpecInst1));
		ASSERT_THAT(IsNotNull(SpecInst2));
		ASSERT_THAT(AreEqual(1, SpecInst0->Context.StackCount));
		ASSERT_THAT(AreEqual(1, SpecInst1->Context.StackCount));
		ASSERT_THAT(AreEqual(1, SpecInst2->Context.StackCount));
	}

	TEST_METHOD(MaxStacks_Deny_RejectsNew)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;
		Effect->StackingPolicy.StackType = EArcStackType::Instance;
		Effect->StackingPolicy.Grouping = EArcStackGrouping::ByTarget;
		Effect->StackingPolicy.MaxStacks = 2;
		Effect->StackingPolicy.OverflowPolicy = EArcStackOverflowPolicy::Deny;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		FArcActiveEffectHandle h1 = ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		FArcActiveEffectHandle h2 = ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		FArcActiveEffectHandle h3 = ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		ASSERT_THAT(IsTrue(h1.IsValid()));
		ASSERT_THAT(IsTrue(h2.IsValid()));
		ASSERT_THAT(IsFalse(h3.IsValid()));

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(2, Stack->ActiveEffects.Num()));
	}

	TEST_METHOD(MaxStacks_RemoveOldest_ReplacesFirst)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;
		Effect->StackingPolicy.StackType = EArcStackType::Instance;
		Effect->StackingPolicy.Grouping = EArcStackGrouping::ByTarget;
		Effect->StackingPolicy.MaxStacks = 2;
		Effect->StackingPolicy.OverflowPolicy = EArcStackOverflowPolicy::RemoveOldest;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView ViewFirst(*EntityManager, Entity);
		FArcEffectStackFragment* StackFirst = ViewFirst.GetFragmentDataPtr<FArcEffectStackFragment>();
		StackFirst->ActiveEffects[0].RemainingDuration = 2.f;

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		FArcActiveEffectHandle h3 = ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		ASSERT_THAT(IsTrue(h3.IsValid()));

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(2, Stack->ActiveEffects.Num()));
		ASSERT_THAT(IsNear(5.f, Stack->ActiveEffects[0].RemainingDuration, 0.001f));
		ASSERT_THAT(IsNear(5.f, Stack->ActiveEffects[1].RemainingDuration, 0.001f));
	}

	TEST_METHOD(RemoveEffect_RemovesAllInstances)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;
		Effect->StackingPolicy.StackType = EArcStackType::Instance;
		Effect->StackingPolicy.Grouping = EArcStackGrouping::ByTarget;
		Effect->StackingPolicy.MaxStacks = 5;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		ArcEffects::RemoveEffect(*EntityManager, Entity, Effect);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		FArcAggregatorFragment* Agg = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();
		ASSERT_THAT(AreEqual(0, Stack->ActiveEffects.Num()));
		for (TPair<FArcAttributeRef, FArcAggregator>& Pair : Agg->Aggregators)
		{
			ASSERT_THAT(IsTrue(Pair.Value.IsEmpty()));
		}
	}

	TEST_METHOD(IndependentDurations)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;
		Effect->StackingPolicy.StackType = EArcStackType::Instance;
		Effect->StackingPolicy.Grouping = EArcStackGrouping::ByTarget;
		Effect->StackingPolicy.MaxStacks = 5;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		Stack->ActiveEffects[0].RemainingDuration = 1.f;

		ASSERT_THAT(IsNear(1.f, Stack->ActiveEffects[0].RemainingDuration, 0.001f));
		ASSERT_THAT(IsNear(5.f, Stack->ActiveEffects[1].RemainingDuration, 0.001f));
	}
};

// ============================================================================
// ArcEffect_Instance_BySource
// ============================================================================

TEST_CLASS(ArcEffect_Instance_BySource, "ArcMassAbilities.Effects.Stacking.Instance.BySource")
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

	TEST_METHOD(PerSource_MaxStacks_Independent)
	{
		FMassEntityHandle Target = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		FMassEntityHandle SourceA = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		FMassEntityHandle SourceB = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;
		Effect->StackingPolicy.StackType = EArcStackType::Instance;
		Effect->StackingPolicy.Grouping = EArcStackGrouping::BySource;
		Effect->StackingPolicy.MaxStacks = 2;
		Effect->StackingPolicy.OverflowPolicy = EArcStackOverflowPolicy::Deny;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceA);
		ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceA);
		FArcActiveEffectHandle hDenied = ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceA);
		ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceB);
		ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceB);

		ASSERT_THAT(IsFalse(hDenied.IsValid()));

		FMassEntityView EntityView(*EntityManager, Target);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(4, Stack->ActiveEffects.Num()));
	}

	TEST_METHOD(RemoveOldest_PerSource)
	{
		FMassEntityHandle Target = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		FMassEntityHandle SourceA = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;
		Effect->StackingPolicy.StackType = EArcStackType::Instance;
		Effect->StackingPolicy.Grouping = EArcStackGrouping::BySource;
		Effect->StackingPolicy.MaxStacks = 2;
		Effect->StackingPolicy.OverflowPolicy = EArcStackOverflowPolicy::RemoveOldest;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceA);

		FMassEntityView ViewFirst(*EntityManager, Target);
		FArcEffectStackFragment* StackFirst = ViewFirst.GetFragmentDataPtr<FArcEffectStackFragment>();
		StackFirst->ActiveEffects[0].RemainingDuration = 1.f;

		ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceA);
		FArcActiveEffectHandle h3 = ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, SourceA);

		ASSERT_THAT(IsTrue(h3.IsValid()));

		FMassEntityView EntityView(*EntityManager, Target);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(2, Stack->ActiveEffects.Num()));
	}
};

// ============================================================================
// ArcEffect_SourceData
// ============================================================================

TEST_CLASS(ArcEffect_SourceData, "ArcMassAbilities.Effects.SourceData")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcPendingAttributeOpsProcessor* PendingProcessor = nullptr;

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
		PendingProcessor = NewObject<UArcPendingAttributeOpsProcessor>();
		PendingProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);
	}

	void DrainPendingOps(FMassEntityHandle Entity)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::PendingAttributeOps, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*PendingProcessor, ProcessingContext);
	}

	TEST_METHOD(Execution_ReceivesSourceData)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;

		FArcTestSourceDataExecution Exec;
		Exec.TargetAttribute = FArcTestStatsFragment::GetHealthAttribute();
		Exec.BaseValue = 10.f;
		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));

		FArcTestSourcePayload Payload;
		Payload.DamageMultiplier = 3.f;
		FInstancedStruct SourceData = FInstancedStruct::Make(Payload);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle(), SourceData);
		DrainPendingOps(Entity);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcTestStatsFragment* Stats = EntityView.GetFragmentDataPtr<FArcTestStatsFragment>();
		// Base 100 + (10 * 3) = 130
		ASSERT_THAT(IsNear(130.f, Stats->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(Execution_WithoutSourceData_UsesDefault)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;

		FArcTestSourceDataExecution Exec;
		Exec.TargetAttribute = FArcTestStatsFragment::GetHealthAttribute();
		Exec.BaseValue = 10.f;
		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		DrainPendingOps(Entity);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcTestStatsFragment* Stats = EntityView.GetFragmentDataPtr<FArcTestStatsFragment>();
		// Base 100 + (10 * 1) = 110
		ASSERT_THAT(IsNear(110.f, Stats->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(SourceData_StoredOnActiveEffect)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 5.f;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		FArcTestSourcePayload Payload;
		Payload.DamageMultiplier = 2.5f;
		FInstancedStruct SourceData = FInstancedStruct::Make(Payload);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle(), SourceData);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(1, Stack->ActiveEffects.Num()));

		const FArcEffectSpec* StoredSpec = Stack->ActiveEffects[0].Spec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(StoredSpec));
		const FArcTestSourcePayload* Stored = StoredSpec->Context.SourceData.GetPtr<FArcTestSourcePayload>();
		ASSERT_THAT(IsNotNull(Stored));
		ASSERT_THAT(IsNear(2.5f, Stored->DamageMultiplier, 0.001f));
	}

	TEST_METHOD(SourceData_PropagatesThroughApplyOtherEffect)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* ChildEffect = NewObject<UArcEffectDefinition>();
		ChildEffect->StackingPolicy.DurationType = EArcEffectDuration::Instant;

		FArcTestSourceDataExecution ChildExec;
		ChildExec.TargetAttribute = FArcTestStatsFragment::GetHealthAttribute();
		ChildExec.BaseValue = 20.f;
		ChildEffect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(ChildExec)));

		UArcEffectDefinition* ParentEffect = NewObject<UArcEffectDefinition>();
		ParentEffect->StackingPolicy.DurationType = EArcEffectDuration::Instant;

		FArcEffectComp_ApplyOtherEffect ApplyComp;
		ApplyComp.EffectToApply = ChildEffect;
		ParentEffect->Components.Add(FInstancedStruct::Make(ApplyComp));

		FArcTestSourcePayload Payload;
		Payload.DamageMultiplier = 2.f;
		FInstancedStruct SourceData = FInstancedStruct::Make(Payload);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, ParentEffect, FMassEntityHandle(), SourceData);
		DrainPendingOps(Entity);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcTestStatsFragment* Stats = EntityView.GetFragmentDataPtr<FArcTestStatsFragment>();
		// Base 100 + (20 * 2) = 140
		ASSERT_THAT(IsNear(140.f, Stats->Health.BaseValue, 0.001f));
	}
};

// ============================================================================
// ArcEffect_Capture
// ============================================================================

TEST_CLASS(ArcEffect_Capture, "ArcMassAbilities.Effects.Capture")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcPendingAttributeOpsProcessor* PendingProcessor = nullptr;

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
		PendingProcessor = NewObject<UArcPendingAttributeOpsProcessor>();
		PendingProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);
	}

	void DrainPendingOps(FMassEntityHandle Entity)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::PendingAttributeOps, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*PendingProcessor, ProcessingContext);
	}

	TEST_METHOD(SnapshotCapture_UsesValueAtApplicationTime)
	{
		FMassEntityHandle Source = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager, 100.f, 50.f);
		FMassEntityHandle Target = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager, 200.f, 30.f);

		FArcAttributeCaptureDef SourceHealthCap;
		SourceHealthCap.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		SourceHealthCap.CaptureSource = EArcCaptureSource::Source;
		SourceHealthCap.bSnapshot = true;

		FArcAttributeCaptureDef TargetArmorCap;
		TargetArmorCap.Attribute = FArcTestStatsFragment::GetArmorAttribute();
		TargetArmorCap.CaptureSource = EArcCaptureSource::Target;
		TargetArmorCap.bSnapshot = true;

		FArcTestCaptureExecution Exec;
		Exec.OutputAttribute = FArcTestStatsFragment::GetHealthAttribute();
		Exec.Captures.Empty();
		Exec.Captures.Add(SourceHealthCap);
		Exec.Captures.Add(TargetArmorCap);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;
		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));

		FArcEffectContext Context;
		Context.SourceEntity = Source;
		ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, Context);
		DrainPendingOps(Target);

		FMassEntityView TargetView(*EntityManager, Target);
		FArcTestStatsFragment* Stats = TargetView.GetFragmentDataPtr<FArcTestStatsFragment>();
		// Damage = max(0, SourceHealth(100) - TargetArmor(30)) = 70
		// Target health = 200 - 70 = 130
		ASSERT_THAT(IsNear(130.f, Stats->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(LiveCapture_ReadsCurrentValue)
	{
		FMassEntityHandle Source = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager, 80.f, 0.f);
		FMassEntityHandle Target = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager, 200.f, 20.f);

		FArcAttributeCaptureDef SourceHealthCap;
		SourceHealthCap.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		SourceHealthCap.CaptureSource = EArcCaptureSource::Source;
		SourceHealthCap.bSnapshot = false;

		FArcAttributeCaptureDef TargetArmorCap;
		TargetArmorCap.Attribute = FArcTestStatsFragment::GetArmorAttribute();
		TargetArmorCap.CaptureSource = EArcCaptureSource::Target;
		TargetArmorCap.bSnapshot = false;

		FArcTestCaptureExecution Exec;
		Exec.OutputAttribute = FArcTestStatsFragment::GetHealthAttribute();
		Exec.Captures.Empty();
		Exec.Captures.Add(SourceHealthCap);
		Exec.Captures.Add(TargetArmorCap);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;
		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));

		FArcEffectContext Context;
		Context.SourceEntity = Source;
		ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, Context);
		DrainPendingOps(Target);

		FMassEntityView TargetView(*EntityManager, Target);
		FArcTestStatsFragment* Stats = TargetView.GetFragmentDataPtr<FArcTestStatsFragment>();
		// Damage = max(0, 80 - 20) = 60
		// Target health = 200 - 60 = 140
		ASSERT_THAT(IsNear(140.f, Stats->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(TagFilteredCapture_ModIncludedWhenTagPresent)
	{
		FMassEntityHandle Source = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager, 100.f, 0.f);
		FMassEntityHandle Target = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager, 200.f, 10.f);

		// Add a tag-filtered modifier to source health: +50 if source has Test.Fire
		FMassEntityView SourceView(*EntityManager, Source);
		FArcAggregatorFragment* SourceAgg = SourceView.GetFragmentDataPtr<FArcAggregatorFragment>();

		FGameplayTagRequirements FireReq;
		FireReq.RequireTags.AddTag(TAG_EffTest_Fire);
		SourceAgg->FindOrAddAggregator(FArcTestStatsFragment::GetHealthAttribute())
			.AddMod(EArcModifierOp::Add, 50.f, FMassEntityHandle(), &FireReq, nullptr);

		// Create effect with Fire tag
		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;

		FArcEffectComp_EffectTags TagsComp;
		TagsComp.OwnedTags.AddTag(TAG_EffTest_Fire);
		Effect->Components.Add(FInstancedStruct::Make(TagsComp));

		FArcAttributeCaptureDef SourceHealthCap;
		SourceHealthCap.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		SourceHealthCap.CaptureSource = EArcCaptureSource::Source;
		SourceHealthCap.bSnapshot = true;

		FArcAttributeCaptureDef TargetArmorCap;
		TargetArmorCap.Attribute = FArcTestStatsFragment::GetArmorAttribute();
		TargetArmorCap.CaptureSource = EArcCaptureSource::Target;
		TargetArmorCap.bSnapshot = true;

		FArcTestCaptureExecution Exec;
		Exec.OutputAttribute = FArcTestStatsFragment::GetHealthAttribute();
		Exec.Captures.Empty();
		Exec.Captures.Add(SourceHealthCap);
		Exec.Captures.Add(TargetArmorCap);
		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));

		FArcEffectContext Context;
		Context.SourceEntity = Source;
		ArcEffects::TryApplyEffect(*EntityManager, Target, Effect, Context);
		DrainPendingOps(Target);

		FMassEntityView TargetView(*EntityManager, Target);
		FArcTestStatsFragment* Stats = TargetView.GetFragmentDataPtr<FArcTestStatsFragment>();
		// Source health evaluated with Fire tag: 100 + 50 = 150
		// Damage = max(0, 150 - 10) = 140
		// Target health = 200 - 140 = 60
		ASSERT_THAT(IsNear(60.f, Stats->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(OldOverload_StillWorks)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;

		FArcTestAddExecution Exec;
		Exec.TargetAttribute = FArcTestStatsFragment::GetHealthAttribute();
		Exec.Value = 50.f;
		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		DrainPendingOps(Entity);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcTestStatsFragment* Stats = EntityView.GetFragmentDataPtr<FArcTestStatsFragment>();
		ASSERT_THAT(IsNear(150.f, Stats->Health.BaseValue, 0.001f));
	}
};

// ============================================================================
// ArcEffect_Handle
// ============================================================================

TEST_CLASS(ArcEffect_Handle, "ArcMassAbilities.Effects.Handle")
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

	TEST_METHOD(TryApplyEffect_ReturnsValidHandle)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;

		FArcActiveEffectHandle Handle = ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		ASSERT_THAT(IsTrue(Handle.IsValid()));
	}

	TEST_METHOD(TryApplyEffect_FailReturnsInvalidHandle)
	{
		FMassEntityHandle InvalidEntity;

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;

		FArcActiveEffectHandle Handle = ArcEffects::TryApplyEffect(*EntityManager, InvalidEntity, Effect, FMassEntityHandle());
		ASSERT_THAT(IsFalse(Handle.IsValid()));
	}

	TEST_METHOD(TwoEffects_DifferentHandles)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* EffectA = NewObject<UArcEffectDefinition>();
		EffectA->StackingPolicy.DurationType = EArcEffectDuration::Infinite;

		UArcEffectDefinition* EffectB = NewObject<UArcEffectDefinition>();
		EffectB->StackingPolicy.DurationType = EArcEffectDuration::Infinite;

		FArcActiveEffectHandle HandleA = ArcEffects::TryApplyEffect(*EntityManager, Entity, EffectA, FMassEntityHandle());
		FArcActiveEffectHandle HandleB = ArcEffects::TryApplyEffect(*EntityManager, Entity, EffectB, FMassEntityHandle());

		ASSERT_THAT(IsTrue(HandleA != HandleB));
	}

	TEST_METHOD(RemoveEffectByHandle_RemovesCorrectInstance)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		Effect->StackingPolicy.StackType = EArcStackType::Instance;
		Effect->StackingPolicy.MaxStacks = 3;

		FArcActiveEffectHandle Handle1 = ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		FArcActiveEffectHandle Handle2 = ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		FArcActiveEffectHandle Handle3 = ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		ASSERT_THAT(AreEqual(3, Stack->ActiveEffects.Num()));

		bool bRemoved = ArcEffects::RemoveEffectByHandle(*EntityManager, Entity, Handle2);
		ASSERT_THAT(IsTrue(bRemoved));
		ASSERT_THAT(AreEqual(2, Stack->ActiveEffects.Num()));

		ASSERT_THAT(IsTrue(ArcEffects::FindActiveEffect(*Stack, Handle1) != nullptr));
		ASSERT_THAT(IsTrue(ArcEffects::FindActiveEffect(*Stack, Handle2) == nullptr));
		ASSERT_THAT(IsTrue(ArcEffects::FindActiveEffect(*Stack, Handle3) != nullptr));
	}

	TEST_METHOD(RemoveEffectByHandle_InvalidHandle_ReturnsFalse)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		bool bRemoved = ArcEffects::RemoveEffectByHandle(*EntityManager, Entity, FArcActiveEffectHandle());
		ASSERT_THAT(IsFalse(bRemoved));
	}

	TEST_METHOD(FindActiveEffect_ReturnsNullForMissingHandle)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();

		FArcActiveEffectHandle BogusHandle = FArcActiveEffectHandle::Generate();
		ASSERT_THAT(IsTrue(ArcEffects::FindActiveEffect(*Stack, BogusHandle) == nullptr));
	}
};
