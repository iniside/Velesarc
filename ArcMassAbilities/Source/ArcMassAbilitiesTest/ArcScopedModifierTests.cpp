// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectExecution.h"
#include "Effects/ArcEffectSpec.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectTypes.h"
#include "Attributes/ArcAttribute.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Processors/ArcPendingAttributeOpsProcessor.h"
#include "Signals/ArcAttributeRecalculateSignal.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "MassExecutor.h"
#include "MassProcessingContext.h"
#include "MassSignalSubsystem.h"
#include "NativeGameplayTags.h"
#include "ScalableFloat.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/SharedStruct.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ScopedModTest_SetByCaller, "Test.ScopedMod.SetByCaller");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ScopedModTest_Fire, "Test.ScopedMod.Fire");

TEST_CLASS(ArcScopedModifiers, "ArcMassAbilities.Effects.ScopedModifiers")
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

	struct FTestEntities
	{
		FMassEntityHandle Source;
		FMassEntityHandle Target;
	};

	FTestEntities CreateEntities(float AttackPower, float Armor)
	{
		FArcTestStatsFragment SourceStats;
		SourceStats.Health = { 100.f, 100.f };
		SourceStats.Armor = { 0.f, 0.f };
		SourceStats.Intelligence = { 10.f, 10.f };
		SourceStats.AttackPower = { AttackPower, AttackPower };

		TArray<FInstancedStruct> SourceFragments;
		SourceFragments.Add(FInstancedStruct::Make(SourceStats));
		SourceFragments.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		SourceFragments.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		SourceFragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));

		FMassEntityHandle SourceEntity = EntityManager->CreateEntity(SourceFragments);

		FArcTestStatsFragment TargetStats;
		TargetStats.Health = { 100.f, 100.f };
		TargetStats.Armor = { Armor, Armor };
		TargetStats.Intelligence = { 10.f, 10.f };
		TargetStats.AttackPower = { 0.f, 0.f };

		FArcTestDerivedStatsFragment TargetDerived;

		TArray<FInstancedStruct> TargetFragments;
		TargetFragments.Add(FInstancedStruct::Make(TargetStats));
		TargetFragments.Add(FInstancedStruct::Make(TargetDerived));
		TargetFragments.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		TargetFragments.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		TargetFragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));
		TargetFragments.Add(FInstancedStruct::Make(FArcPendingAttributeOpsFragment()));

		FMassEntityHandle TargetEntity = EntityManager->CreateEntity(TargetFragments);

		return { SourceEntity, TargetEntity };
	}

	UArcEffectDefinition* CreateDamageEffect(FArcTestDamageCalcExecution& OutExec)
	{
		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;

		OutExec = FArcTestDamageCalcExecution();
		OutExec.OutputAttribute = FArcTestDerivedStatsFragment::GetDamageAttribute();

		return Effect;
	}

	TEST_METHOD(SimpleScopedModifier_AddsToCapture)
	{
		FTestEntities Entities = CreateEntities(50.f, 10.f);

		FArcTestDamageCalcExecution Exec;
		UArcEffectDefinition* Effect = CreateDamageEffect(Exec);

		FArcScopedModifier ScopedMod;
		ScopedMod.CaptureIndex = 0;
		ScopedMod.Operation = EArcModifierOp::Add;
		ScopedMod.Type = EArcModifierType::Simple;
		ScopedMod.Magnitude = FScalableFloat(20.f);
		Exec.ScopedModifiers.Add(ScopedMod);

		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));

		ArcEffects::TryApplyEffect(*EntityManager, Entities.Target, Effect, Entities.Source);
		DrainPendingOps(Entities.Target);

		FMassEntityView TargetView(*EntityManager, Entities.Target);
		FArcTestDerivedStatsFragment* Derived = TargetView.GetFragmentDataPtr<FArcTestDerivedStatsFragment>();
		// AttackPower=50+20=70, Armor=10, Damage=70-10=60
		ASSERT_THAT(IsNear(60.f, Derived->Damage.BaseValue, 0.001f));
	}

	TEST_METHOD(SetByCallerScopedModifier_ResolvesCorrectly)
	{
		FTestEntities Entities = CreateEntities(50.f, 10.f);

		FArcTestDamageCalcExecution Exec;
		UArcEffectDefinition* Effect = CreateDamageEffect(Exec);

		FArcScopedModifier ScopedMod;
		ScopedMod.CaptureIndex = 0;
		ScopedMod.Operation = EArcModifierOp::Add;
		ScopedMod.Type = EArcModifierType::SetByCaller;
		ScopedMod.SetByCallerTag = TAG_ScopedModTest_SetByCaller;
		Exec.ScopedModifiers.Add(ScopedMod);

		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));

		FArcEffectContext Ctx;
		Ctx.EntityManager = EntityManager;
		Ctx.SourceEntity = Entities.Source;
		Ctx.TargetEntity = Entities.Target;
		Ctx.EffectDefinition = Effect;
		Ctx.StackCount = 1;

		FSharedStruct SharedSpec = ArcEffects::MakeSpec(Effect, Ctx);
		FArcEffectSpec& Spec = SharedSpec.Get<FArcEffectSpec>();
		Spec.SetByCallerMagnitudes.Add(TAG_ScopedModTest_SetByCaller, 25.f);

		Spec.ResolvedScopedModifiers.Empty();
		ArcEffects::ResolveScopedModifiers(Spec);

		ASSERT_THAT(AreEqual(Spec.ResolvedScopedModifiers.Num(), 1));
		ASSERT_THAT(IsNear(Spec.ResolvedScopedModifiers[0].Magnitude, 25.f, 0.001f));
	}

	TEST_METHOD(MultipleScopedModifiers_ComposeCorrectly)
	{
		FTestEntities Entities = CreateEntities(50.f, 10.f);

		FArcTestDamageCalcExecution Exec;
		UArcEffectDefinition* Effect = CreateDamageEffect(Exec);

		FArcScopedModifier AddMod;
		AddMod.CaptureIndex = 0;
		AddMod.Operation = EArcModifierOp::Add;
		AddMod.Type = EArcModifierType::Simple;
		AddMod.Magnitude = FScalableFloat(10.f);
		Exec.ScopedModifiers.Add(AddMod);

		FArcScopedModifier MulMod;
		MulMod.CaptureIndex = 0;
		MulMod.Operation = EArcModifierOp::MultiplyCompound;
		MulMod.Type = EArcModifierType::Simple;
		MulMod.Magnitude = FScalableFloat(2.f);
		Exec.ScopedModifiers.Add(MulMod);

		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));

		ArcEffects::TryApplyEffect(*EntityManager, Entities.Target, Effect, Entities.Source);
		DrainPendingOps(Entities.Target);

		FMassEntityView TargetView(*EntityManager, Entities.Target);
		FArcTestDerivedStatsFragment* Derived = TargetView.GetFragmentDataPtr<FArcTestDerivedStatsFragment>();
		// AttackPower = (50+10)*2 = 120, Armor=10, Damage=120-10=110
		ASSERT_THAT(IsNear(110.f, Derived->Damage.BaseValue, 0.001f));
	}

	TEST_METHOD(ScopedModifier_TagFilter_SkipsWhenNotMet)
	{
		FTestEntities Entities = CreateEntities(50.f, 10.f);

		FArcTestDamageCalcExecution Exec;
		UArcEffectDefinition* Effect = CreateDamageEffect(Exec);

		FArcScopedModifier ScopedMod;
		ScopedMod.CaptureIndex = 0;
		ScopedMod.Operation = EArcModifierOp::Add;
		ScopedMod.Type = EArcModifierType::Simple;
		ScopedMod.Magnitude = FScalableFloat(100.f);
		ScopedMod.TagFilter.RequireTags.AddTag(TAG_ScopedModTest_Fire);
		Exec.ScopedModifiers.Add(ScopedMod);

		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));

		ArcEffects::TryApplyEffect(*EntityManager, Entities.Target, Effect, Entities.Source);
		DrainPendingOps(Entities.Target);

		FMassEntityView TargetView(*EntityManager, Entities.Target);
		FArcTestDerivedStatsFragment* Derived = TargetView.GetFragmentDataPtr<FArcTestDerivedStatsFragment>();
		// Tag not met, scoped mod skipped: Damage = 50-10 = 40
		ASSERT_THAT(IsNear(40.f, Derived->Damage.BaseValue, 0.001f));
	}

	TEST_METHOD(NoScopedModifiers_BehaviorUnchanged)
	{
		FTestEntities Entities = CreateEntities(80.f, 30.f);

		FArcTestDamageCalcExecution Exec;
		UArcEffectDefinition* Effect = CreateDamageEffect(Exec);
		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));

		ArcEffects::TryApplyEffect(*EntityManager, Entities.Target, Effect, Entities.Source);
		DrainPendingOps(Entities.Target);

		FMassEntityView TargetView(*EntityManager, Entities.Target);
		FArcTestDerivedStatsFragment* Derived = TargetView.GetFragmentDataPtr<FArcTestDerivedStatsFragment>();
		// No scoped mods: Damage = 80-30 = 50
		ASSERT_THAT(IsNear(50.f, Derived->Damage.BaseValue, 0.001f));
	}

	TEST_METHOD(ScopedModifier_OnSnapshotCapture)
	{
		FTestEntities Entities = CreateEntities(50.f, 10.f);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;

		FArcTestDamageCalcExecution Exec;
		Exec.OutputAttribute = FArcTestDerivedStatsFragment::GetDamageAttribute();
		Exec.Captures.Empty();
		FArcAttributeCaptureDef SourceCap;
		SourceCap.Attribute = FArcTestStatsFragment::GetAttackPowerAttribute();
		SourceCap.CaptureSource = EArcCaptureSource::Source;
		SourceCap.bSnapshot = true;
		Exec.Captures.Add(SourceCap);
		FArcAttributeCaptureDef TargetCap;
		TargetCap.Attribute = FArcTestStatsFragment::GetArmorAttribute();
		TargetCap.CaptureSource = EArcCaptureSource::Target;
		TargetCap.bSnapshot = true;
		Exec.Captures.Add(TargetCap);

		FArcScopedModifier ScopedMod;
		ScopedMod.CaptureIndex = 0;
		ScopedMod.Operation = EArcModifierOp::Add;
		ScopedMod.Type = EArcModifierType::Simple;
		ScopedMod.Magnitude = FScalableFloat(20.f);
		Exec.ScopedModifiers.Add(ScopedMod);

		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));

		ArcEffects::TryApplyEffect(*EntityManager, Entities.Target, Effect, Entities.Source);
		DrainPendingOps(Entities.Target);

		FMassEntityView TargetView(*EntityManager, Entities.Target);
		FArcTestDerivedStatsFragment* Derived = TargetView.GetFragmentDataPtr<FArcTestDerivedStatsFragment>();
		// Snapshot=50, scoped adds 20 -> 70, minus armor 10 -> 60
		ASSERT_THAT(IsNear(60.f, Derived->Damage.BaseValue, 0.001f));
	}

	TEST_METHOD(ScopedModifier_OnDifferentCapture)
	{
		FTestEntities Entities = CreateEntities(80.f, 30.f);

		FArcTestDamageCalcExecution Exec;
		UArcEffectDefinition* Effect = CreateDamageEffect(Exec);

		FArcScopedModifier ScopedMod;
		ScopedMod.CaptureIndex = 1; // Target.Armor
		ScopedMod.Operation = EArcModifierOp::Add;
		ScopedMod.Type = EArcModifierType::Simple;
		ScopedMod.Magnitude = FScalableFloat(20.f);
		Exec.ScopedModifiers.Add(ScopedMod);

		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));

		ArcEffects::TryApplyEffect(*EntityManager, Entities.Target, Effect, Entities.Source);
		DrainPendingOps(Entities.Target);

		FMassEntityView TargetView(*EntityManager, Entities.Target);
		FArcTestDerivedStatsFragment* Derived = TargetView.GetFragmentDataPtr<FArcTestDerivedStatsFragment>();
		// AttackPower=80, Armor=30+20=50, Damage=80-50=30
		ASSERT_THAT(IsNear(30.f, Derived->Damage.BaseValue, 0.001f));
	}

	TEST_METHOD(OutOfRange_CaptureIndex_Skipped)
	{
		FTestEntities Entities = CreateEntities(50.f, 10.f);

		FArcTestDamageCalcExecution Exec;
		UArcEffectDefinition* Effect = CreateDamageEffect(Exec);

		FArcScopedModifier ScopedMod;
		ScopedMod.CaptureIndex = 99;
		ScopedMod.Operation = EArcModifierOp::Add;
		ScopedMod.Type = EArcModifierType::Simple;
		ScopedMod.Magnitude = FScalableFloat(1000.f);
		Exec.ScopedModifiers.Add(ScopedMod);

		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));

		ArcEffects::TryApplyEffect(*EntityManager, Entities.Target, Effect, Entities.Source);
		DrainPendingOps(Entities.Target);

		FMassEntityView TargetView(*EntityManager, Entities.Target);
		FArcTestDerivedStatsFragment* Derived = TargetView.GetFragmentDataPtr<FArcTestDerivedStatsFragment>();
		// Out-of-range index skipped: Damage = 50-10 = 40
		ASSERT_THAT(IsNear(40.f, Derived->Damage.BaseValue, 0.001f));
	}
};
