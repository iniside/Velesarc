// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "Components/ActorTestSpawner.h"
#include "Modifiers/ArcModifierFunctions.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectTypes.h"
#include "Attributes/ArcAttribute.h"
#include "Attributes/ArcAttributeAggregatorProcessor.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcAttributeHandlerSharedFragment.h"
#include "Processors/ArcPendingAttributeOpsProcessor.h"
#include "Signals/ArcAttributeRecalculateSignal.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "MassSignalSubsystem.h"
#include "ArcScalableFloat.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/SharedStruct.h"

TEST_CLASS(ArcPendingAttributeOps, "ArcMassAbilities.Attributes.PendingOps")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcPendingAttributeOpsProcessor* PendingProcessor = nullptr;
	UArcAttributeAggregatorProcessor* AggregatorProcessor = nullptr;

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

		AggregatorProcessor = NewObject<UArcAttributeAggregatorProcessor>();
		AggregatorProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);
	}

	void DrainPendingOps(FMassEntityHandle Entity)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::PendingAttributeOps, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*PendingProcessor, ProcessingContext);
	}

	void RecalculateAttributes(FMassEntityHandle Entity)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AttributeRecalculate, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*AggregatorProcessor, ProcessingContext);
	}

	TEST_METHOD(QueueInstant_Add_AppliedOnDrain)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		ArcModifiers::QueueInstant(*EntityManager, Entity,
			FArcAttributeSet_TestStats::GetHealthAttribute(), EArcModifierOp::Add, -25.f);

		FMassEntityView ViewBefore(*EntityManager, Entity);
		FArcAttributeSet_TestStats* StatsBefore = ViewBefore.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(100.f, StatsBefore->Health.BaseValue, 0.001f));

		DrainPendingOps(Entity);

		FMassEntityView ViewAfter(*EntityManager, Entity);
		FArcAttributeSet_TestStats* StatsAfter = ViewAfter.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(75.f, StatsAfter->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(QueueSetValue_Override_AppliedOnDrain)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		ArcAttributes::QueueSetValue(*EntityManager, Entity,
			FArcAttributeSet_TestStats::GetHealthAttribute(), 42.f);

		DrainPendingOps(Entity);

		FMassEntityView View(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = View.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(42.f, Stats->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(MultipleOps_AppliedInOrder)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		ArcModifiers::QueueInstant(*EntityManager, Entity,
			FArcAttributeSet_TestStats::GetHealthAttribute(), EArcModifierOp::Add, -20.f);
		ArcModifiers::QueueInstant(*EntityManager, Entity,
			FArcAttributeSet_TestStats::GetHealthAttribute(), EArcModifierOp::MultiplyCompound, 2.f);

		DrainPendingOps(Entity);

		FMassEntityView View(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = View.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(160.f, Stats->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(EmptyQueue_NoChange)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		DrainPendingOps(Entity);

		FMassEntityView View(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = View.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(100.f, Stats->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(EffectExecution_QueuesRatherThanDirectWrite)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;

		FInstancedStruct ExecStruct = FInstancedStruct::Make<FArcTestAddExecution>();
		FArcTestAddExecution& Exec = ExecStruct.GetMutable<FArcTestAddExecution>();
		Exec.TargetAttribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		Exec.Value = -30.f;
		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(MoveTemp(ExecStruct)));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView PendingView(*EntityManager, Entity);
		FArcPendingAttributeOpsFragment* Pending = PendingView.GetSparseFragmentDataPtr<FArcPendingAttributeOpsFragment>();
		ASSERT_THAT(IsTrue(Pending != nullptr && Pending->PendingOps.Num() > 0));

		FArcAttributeSet_TestStats* StatsBefore = PendingView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(100.f, StatsBefore->Health.BaseValue, 0.001f));

		DrainPendingOps(Entity);

		FMassEntityView ViewAfter(*EntityManager, Entity);
		FArcAttributeSet_TestStats* StatsAfter = ViewAfter.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(70.f, StatsAfter->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(MetaAttribute_DamageToHealth_WorksThroughPendingOps)
	{
		FArcAttributeSet_TestStats Stats;
		Stats.Health.BaseValue = 100.f;
		Stats.Health.CurrentValue = 100.f;

		FArcAttributeSet_TestDerivedStats Derived;
		Derived.Damage.BaseValue = 0.f;
		Derived.Damage.CurrentValue = 0.f;

		TArray<FInstancedStruct> Instances;
		Instances.Add(FInstancedStruct::Make(Stats));
		Instances.Add(FInstancedStruct::Make(Derived));
		Instances.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		Instances.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		Instances.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));
		Instances.Add(FInstancedStruct::Make(FArcPendingAttributeOpsFragment()));

		UArcAttributeHandlerConfig* HandlerConfig = NewObject<UArcAttributeHandlerConfig>();
		FInstancedStruct HandlerStruct = FInstancedStruct::Make<FArcAttributeHandler_Test_DamageToHealth>();
		FArcAttributeHandler_Test_DamageToHealth& Handler = HandlerStruct.GetMutable<FArcAttributeHandler_Test_DamageToHealth>();

		HandlerConfig->Handlers.Add(FArcAttributeSet_TestDerivedStats::GetDamageAttribute(), MoveTemp(HandlerStruct));

		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(*EntityManager, Instances);
		FArcAttributeHandlerSharedFragment SharedHandler;
		SharedHandler.Config = HandlerConfig;
		const FConstSharedStruct HandlerSharedFrag = EntityManager->GetOrCreateConstSharedFragment(SharedHandler);
		EntityManager->AddConstSharedFragmentToEntity(Entity, HandlerSharedFrag);

		UArcEffectDefinition* DamageEffect = NewObject<UArcEffectDefinition>();
		DamageEffect->StackingPolicy.DurationType = EArcEffectDuration::Instant;

		FInstancedStruct ExecStruct = FInstancedStruct::Make<FArcTestAddExecution>();
		FArcTestAddExecution& Exec = ExecStruct.GetMutable<FArcTestAddExecution>();
		Exec.TargetAttribute = FArcAttributeSet_TestDerivedStats::GetDamageAttribute();
		Exec.Value = 40.f;
		DamageEffect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(MoveTemp(ExecStruct)));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, DamageEffect, FMassEntityHandle());
		DrainPendingOps(Entity);

		FMassEntityView View(*EntityManager, Entity);
		FArcAttributeSet_TestStats* FinalStats = View.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		FArcAttributeSet_TestDerivedStats* FinalDerived = View.GetSparseFragmentDataPtr<FArcAttributeSet_TestDerivedStats>();

		ASSERT_THAT(IsNear(0.f, FinalDerived->Damage.BaseValue, 0.001f));
		ASSERT_THAT(IsNear(60.f, FinalStats->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(AbilityCost_InstantModifier_GoesThoughQueue)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		ArcAttributes::SetValue(*EntityManager, Entity,
			FArcAttributeSet_TestStats::GetHealthAttribute(), 50.f);

		DrainPendingOps(Entity);

		FMassEntityView View(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = View.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(50.f, Stats->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(QueueInstant_DoesNotTriggerPostExecute)
	{
		FArcAttributeHandler_Test_PostExecuteTracker Tracker;

		UArcAttributeHandlerConfig* HandlerConfig = NewObject<UArcAttributeHandlerConfig>();
		HandlerConfig->Handlers.Add(FArcAttributeSet_TestStats::GetHealthAttribute(), FInstancedStruct::Make(Tracker));

		FArcAttributeSet_TestStats InitStats;
		InitStats.Health = { 100.f, 100.f };

		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(InitStats));
		Fragments.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcPendingAttributeOpsFragment()));

		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(*EntityManager, Fragments);
		FArcAttributeHandlerSharedFragment SharedHandler;
		SharedHandler.Config = HandlerConfig;
		const FConstSharedStruct HandlerSharedFrag = EntityManager->GetOrCreateConstSharedFragment(SharedHandler);
		EntityManager->AddConstSharedFragmentToEntity(Entity, HandlerSharedFrag);

		ArcModifiers::QueueInstant(*EntityManager, Entity,
			FArcAttributeSet_TestStats::GetHealthAttribute(), EArcModifierOp::Add, -10.f);
		DrainPendingOps(Entity);

		const FArcAttributeHandler_Test_PostExecuteTracker* TrackerPtr =
			HandlerConfig->Handlers.Find(FArcAttributeSet_TestStats::GetHealthAttribute())->GetPtr<FArcAttributeHandler_Test_PostExecuteTracker>();

		ASSERT_THAT(AreEqual(1, TrackerPtr->PostChangeCount));
		ASSERT_THAT(AreEqual(0, TrackerPtr->PostExecuteCount));
	}

	TEST_METHOD(QueueExecute_TriggersPostExecute)
	{
		FArcAttributeHandler_Test_PostExecuteTracker Tracker;

		UArcAttributeHandlerConfig* HandlerConfig = NewObject<UArcAttributeHandlerConfig>();
		HandlerConfig->Handlers.Add(FArcAttributeSet_TestStats::GetHealthAttribute(), FInstancedStruct::Make(Tracker));

		FArcAttributeSet_TestStats InitStats;
		InitStats.Health = { 100.f, 100.f };

		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(InitStats));
		Fragments.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcPendingAttributeOpsFragment()));

		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(*EntityManager, Fragments);
		FArcAttributeHandlerSharedFragment SharedHandler;
		SharedHandler.Config = HandlerConfig;
		const FConstSharedStruct HandlerSharedFrag = EntityManager->GetOrCreateConstSharedFragment(SharedHandler);
		EntityManager->AddConstSharedFragmentToEntity(Entity, HandlerSharedFrag);

		ArcModifiers::QueueExecute(*EntityManager, Entity,
			FArcAttributeSet_TestStats::GetHealthAttribute(), EArcModifierOp::Add, -10.f);
		DrainPendingOps(Entity);

		const FArcAttributeHandler_Test_PostExecuteTracker* TrackerPtr =
			HandlerConfig->Handlers.Find(FArcAttributeSet_TestStats::GetHealthAttribute())->GetPtr<FArcAttributeHandler_Test_PostExecuteTracker>();

		ASSERT_THAT(AreEqual(1, TrackerPtr->PostChangeCount));
		ASSERT_THAT(AreEqual(1, TrackerPtr->PostExecuteCount));

		FMassEntityView View(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = View.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(90.f, Stats->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(PostExecute_CanQueueSecondaryOps)
	{
		FArcAttributeHandler_Test_DamageToHealth DamageHandler;

		UArcAttributeHandlerConfig* HandlerConfig = NewObject<UArcAttributeHandlerConfig>();
		HandlerConfig->Handlers.Add(FArcAttributeSet_TestDerivedStats::GetDamageAttribute(), FInstancedStruct::Make(DamageHandler));

		FArcAttributeSet_TestStats InitStats;
		InitStats.Health = { 100.f, 100.f };

		FArcAttributeSet_TestDerivedStats DerivedStats;

		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(InitStats));
		Fragments.Add(FInstancedStruct::Make(DerivedStats));
		Fragments.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcPendingAttributeOpsFragment()));

		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(*EntityManager, Fragments);
		FArcAttributeHandlerSharedFragment SharedHandler;
		SharedHandler.Config = HandlerConfig;
		const FConstSharedStruct HandlerSharedFrag = EntityManager->GetOrCreateConstSharedFragment(SharedHandler);
		EntityManager->AddConstSharedFragmentToEntity(Entity, HandlerSharedFrag);

		ArcModifiers::QueueExecute(*EntityManager, Entity,
			FArcAttributeSet_TestDerivedStats::GetDamageAttribute(), EArcModifierOp::Add, 30.f);
		DrainPendingOps(Entity);

		FMassEntityView View(*EntityManager, Entity);
		FArcAttributeSet_TestDerivedStats* Derived = View.GetSparseFragmentDataPtr<FArcAttributeSet_TestDerivedStats>();
		ASSERT_THAT(IsNear(0.f, Derived->Damage.BaseValue, 0.001f));

		FArcAttributeSet_TestStats* Stats = View.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(70.f, Stats->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(PreChange_Rejection_BlocksPostExecute)
	{
		FArcAttributeHandler_Test_Rejecting Rejector;

		UArcAttributeHandlerConfig* HandlerConfig = NewObject<UArcAttributeHandlerConfig>();
		HandlerConfig->Handlers.Add(FArcAttributeSet_TestStats::GetHealthAttribute(), FInstancedStruct::Make(Rejector));

		FArcAttributeSet_TestStats InitStats;
		InitStats.Health = { 100.f, 100.f };

		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(InitStats));
		Fragments.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcPendingAttributeOpsFragment()));

		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(*EntityManager, Fragments);
		FArcAttributeHandlerSharedFragment SharedHandler;
		SharedHandler.Config = HandlerConfig;
		const FConstSharedStruct HandlerSharedFrag = EntityManager->GetOrCreateConstSharedFragment(SharedHandler);
		EntityManager->AddConstSharedFragmentToEntity(Entity, HandlerSharedFrag);

		ArcModifiers::QueueExecute(*EntityManager, Entity,
			FArcAttributeSet_TestStats::GetHealthAttribute(), EArcModifierOp::Add, -10.f);
		DrainPendingOps(Entity);

		FMassEntityView View(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = View.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(100.f, Stats->Health.CurrentValue, 0.001f));

		const FArcAttributeHandler_Test_Rejecting* RejectorPtr =
			HandlerConfig->Handlers.Find(FArcAttributeSet_TestStats::GetHealthAttribute())->GetPtr<FArcAttributeHandler_Test_Rejecting>();
		ASSERT_THAT(AreEqual(0, RejectorPtr->PostExecuteCount));
	}
};
