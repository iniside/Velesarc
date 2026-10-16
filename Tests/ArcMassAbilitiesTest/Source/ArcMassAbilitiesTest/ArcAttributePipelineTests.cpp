// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectTypes.h"
#include "Attributes/ArcAttribute.h"
#include "Attributes/ArcAggregator.h"
#include "Attributes/ArcAttributeAggregatorProcessor.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Processors/ArcPendingAttributeOpsProcessor.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcAttributeHandlerSharedFragment.h"
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
#include "ArcScalableFloat.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/SharedStruct.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PipelineTest_Fire, "Test.Fire");

TEST_CLASS(ArcAttributePipeline, "ArcMassAbilities.Attributes.Pipeline")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcAttributeAggregatorProcessor* Processor = nullptr;
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

		Processor = NewObject<UArcAttributeAggregatorProcessor>();
		Processor->CallInitialize(EntityManager->GetOwner(), SharedEM);

		PendingProcessor = NewObject<UArcPendingAttributeOpsProcessor>();
		PendingProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);
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
		UE::Mass::Executor::Run(*Processor, ProcessingContext);
	}

	TEST_METHOD(Add_UpdatesCurrentValue)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 10.f;

		FArcEffectModifier Mod;
		Mod.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FArcScalableFloat(25.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		RecalculateAttributes(Entity);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(125.f, Stats->Health.CurrentValue, 0.001f));
	}

	TEST_METHOD(MultipleOps_FormulaApplied)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* EffectAdd = NewObject<UArcEffectDefinition>();
		EffectAdd->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		EffectAdd->StackingPolicy.Duration = 10.f;
		FArcEffectModifier ModAdd;
		ModAdd.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		ModAdd.Operation = EArcModifierOp::Add;
		ModAdd.Magnitude = FArcScalableFloat(20.f);
		EffectAdd->Modifiers.Add(ModAdd);

		UArcEffectDefinition* EffectMul = NewObject<UArcEffectDefinition>();
		EffectMul->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		EffectMul->StackingPolicy.Duration = 10.f;
		FArcEffectModifier ModMul;
		ModMul.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		ModMul.Operation = EArcModifierOp::MultiplyAdditive;
		ModMul.Magnitude = FArcScalableFloat(0.5f);
		EffectMul->Modifiers.Add(ModMul);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, EffectAdd, FMassEntityHandle());
		ArcEffects::TryApplyEffect(*EntityManager, Entity, EffectMul, FMassEntityHandle());
		RecalculateAttributes(Entity);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		// (100 + 20) * (1 + 0.5) = 180
		ASSERT_THAT(IsNear(180.f, Stats->Health.CurrentValue, 0.001f));
	}

	TEST_METHOD(Override_SetsCurrentValue)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 10.f;
		FArcEffectModifier Mod;
		Mod.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Override;
		Mod.Magnitude = FArcScalableFloat(42.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		RecalculateAttributes(Entity);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(42.f, Stats->Health.CurrentValue, 0.001f));
	}

	TEST_METHOD(RemoveMod_RevertsToBase)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 10.f;
		FArcEffectModifier Mod;
		Mod.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FArcScalableFloat(25.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		RecalculateAttributes(Entity);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(125.f, Stats->Health.CurrentValue, 0.001f));

		FArcEffectRemovalQuery RemoveQuery;
		RemoveQuery.Definition = Effect;
		ArcEffects::RemoveEffects(*EntityManager, Entity, RemoveQuery);
		RecalculateAttributes(Entity);

		ASSERT_THAT(IsNear(100.f, Stats->Health.CurrentValue, 0.001f));
	}

	TEST_METHOD(DirectModifier_UpdatesCurrentValue)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcAggregatorFragment* AggFrag = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();

		FArcAggregator& Aggregator = AggFrag->FindOrAddAggregator(FArcAttributeSet_TestStats::GetHealthAttribute());
		Aggregator.AddMod(EArcModifierOp::Add, 30.f, FMassEntityHandle(), nullptr, nullptr);

		RecalculateAttributes(Entity);

		FArcAttributeSet_TestStats* Stats = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(130.f, Stats->Health.CurrentValue, 0.001f));
	}

	TEST_METHOD(CustomPolicy_HighestAdd)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcAggregatorFragment* AggFrag = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();

		FArcAggregator& Aggregator = AggFrag->FindOrAddAggregator(FArcAttributeSet_TestStats::GetHealthAttribute());

		FArcTestHighestAdditivePolicy Policy;
		Aggregator.AggregationPolicy = FConstSharedStruct::Make<FArcTestHighestAdditivePolicy>(Policy);

		Aggregator.AddMod(EArcModifierOp::Add, 10.f, FMassEntityHandle(), nullptr, nullptr);
		Aggregator.AddMod(EArcModifierOp::Add, 30.f, FMassEntityHandle(), nullptr, nullptr);

		RecalculateAttributes(Entity);

		FArcAttributeSet_TestStats* Stats = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		// Custom policy: only highest Add applies -> 100 + 30 = 130
		ASSERT_THAT(IsNear(130.f, Stats->Health.CurrentValue, 0.001f));
	}

	TEST_METHOD(TagQualification_ModSkipped)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcAggregatorFragment* AggFrag = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();
		FArcOwnedTagsFragment* Tags = EntityView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();

		FArcGameplayTagRequirements TargetReqs;
		TargetReqs.RequireTags.AddTag(TAG_PipelineTest_Fire);

		FArcAggregator& Aggregator = AggFrag->FindOrAddAggregator(FArcAttributeSet_TestStats::GetHealthAttribute());
		Aggregator.AddMod(EArcModifierOp::Add, 50.f, FMassEntityHandle(), nullptr, &TargetReqs);

		RecalculateAttributes(Entity);

		FArcAttributeSet_TestStats* Stats = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		// Entity lacks tag, mod is disqualified
		ASSERT_THAT(IsNear(100.f, Stats->Health.CurrentValue, 0.001f));

		// Add tag, recalculate - mod now qualifies
		Tags->Tags.AddTag(TAG_PipelineTest_Fire);
		RecalculateAttributes(Entity);

		ASSERT_THAT(IsNear(150.f, Stats->Health.CurrentValue, 0.001f));
	}

	TEST_METHOD(TwoAttributes_IndependentRecalc)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager, 100.f, 50.f);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 10.f;

		FArcEffectModifier HealthMod;
		HealthMod.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		HealthMod.Operation = EArcModifierOp::Add;
		HealthMod.Magnitude = FArcScalableFloat(25.f);
		Effect->Modifiers.Add(HealthMod);

		FArcEffectModifier ArmorMod;
		ArmorMod.Attribute = FArcAttributeSet_TestStats::GetArmorAttribute();
		ArmorMod.Operation = EArcModifierOp::Add;
		ArmorMod.Magnitude = FArcScalableFloat(10.f);
		Effect->Modifiers.Add(ArmorMod);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		RecalculateAttributes(Entity);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(125.f, Stats->Health.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(60.f, Stats->Armor.CurrentValue, 0.001f));
	}

	TEST_METHOD(InstantEffect_ThenRecalcWithActiveMods)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		// Apply duration mod: Add +25
		UArcEffectDefinition* DurationEffect = NewObject<UArcEffectDefinition>();
		DurationEffect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		DurationEffect->StackingPolicy.Duration = 10.f;
		FArcEffectModifier Mod;
		Mod.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FArcScalableFloat(25.f);
		DurationEffect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, DurationEffect, FMassEntityHandle());
		RecalculateAttributes(Entity);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(125.f, Stats->Health.CurrentValue, 0.001f));

		// Apply instant effect via execution: adds +10 to BaseValue
		UArcEffectDefinition* InstantEffect = NewObject<UArcEffectDefinition>();
		InstantEffect->StackingPolicy.DurationType = EArcEffectDuration::Instant;
		FArcTestAddExecution Exec;
		Exec.TargetAttribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		Exec.Value = 10.f;
		InstantEffect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, InstantEffect, FMassEntityHandle());
		DrainPendingOps(Entity);

		// BaseValue is now 110, recalculate with active +25 mod
		RecalculateAttributes(Entity);

		ASSERT_THAT(IsNear(135.f, Stats->Health.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(110.f, Stats->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(AttributeDependency_ChainedRecalc)
	{
		// Create entity with handler fragment included from the start
		FArcAttributeHandler_Test_ArmorFromHealth Handler;

		UArcAttributeHandlerConfig* HandlerConfig = NewObject<UArcAttributeHandlerConfig>();
		HandlerConfig->Handlers.Add(FArcAttributeSet_TestStats::GetHealthAttribute(), FInstancedStruct::Make(Handler));

		FArcAttributeSet_TestStats InitStats;
		InitStats.Health = { 100.f, 100.f };
		InitStats.Armor = { 0.f, 0.f };

		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(InitStats));
		Fragments.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));

		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(*EntityManager, Fragments);
		FArcAttributeHandlerSharedFragment SharedHandler;
		SharedHandler.Config = HandlerConfig;
		const FConstSharedStruct HandlerSharedFrag = EntityManager->GetOrCreateConstSharedFragment(SharedHandler);
		EntityManager->AddConstSharedFragmentToEntity(Entity, HandlerSharedFrag);

		// Apply duration Add +50 on Health
		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 10.f;
		FArcEffectModifier Mod;
		Mod.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FArcScalableFloat(50.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		// Ensure Armor aggregator exists so processor can recalculate it after handler modifies BaseValue
		FMassEntityView EntityView(*EntityManager, Entity);
		FArcAggregatorFragment* AggFrag = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();
		AggFrag->FindOrAddAggregator(FArcAttributeSet_TestStats::GetArmorAttribute());

		// First recalc: Health updates, handler sets Armor.BaseValue, re-signal queued
		RecalculateAttributes(Entity);
		// Second recalc: processes the re-signal for Armor
		RecalculateAttributes(Entity);

		FMassEntityView UpdatedView(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = UpdatedView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();

		// Health: base 100 + 50 = 150
		ASSERT_THAT(IsNear(150.f, Stats->Health.CurrentValue, 0.001f));
		// Handler set Armor.BaseValue = 150 * 0.5 = 75
		ASSERT_THAT(IsNear(75.f, Stats->Armor.BaseValue, 0.001f));
		ASSERT_THAT(IsNear(75.f, Stats->Armor.CurrentValue, 0.001f));
	}

	TEST_METHOD(CrossFragment_IntelligenceToEnergy)
	{
		FArcAttributeHandler_Test_EnergyFromIntelligence Handler;

		UArcAttributeHandlerConfig* HandlerConfig = NewObject<UArcAttributeHandlerConfig>();
		HandlerConfig->Handlers.Add(FArcAttributeSet_TestStats::GetIntelligenceAttribute(), FInstancedStruct::Make(Handler));

		FArcAttributeSet_TestStats InitStats;
		InitStats.Health = { 100.f, 100.f };
		InitStats.Armor = { 0.f, 0.f };
		InitStats.Intelligence = { 10.f, 10.f };
		InitStats.AttackPower = { 0.f, 0.f };

		FArcAttributeSet_TestDerivedStats DerivedStats;

		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(InitStats));
		Fragments.Add(FInstancedStruct::Make(DerivedStats));
		Fragments.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));

		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(*EntityManager, Fragments);
		FArcAttributeHandlerSharedFragment SharedHandler;
		SharedHandler.Config = HandlerConfig;
		const FConstSharedStruct HandlerSharedFrag = EntityManager->GetOrCreateConstSharedFragment(SharedHandler);
		EntityManager->AddConstSharedFragmentToEntity(Entity, HandlerSharedFrag);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcAggregatorFragment* AggFrag = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();
		AggFrag->FindOrAddAggregator(FArcAttributeSet_TestDerivedStats::GetMaxEnergyAttribute());

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 10.f;
		FArcEffectModifier Mod;
		Mod.Attribute = FArcAttributeSet_TestStats::GetIntelligenceAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FArcScalableFloat(5.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		RecalculateAttributes(Entity);
		RecalculateAttributes(Entity);

		FMassEntityView ResultView(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = ResultView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		FArcAttributeSet_TestDerivedStats* Derived = ResultView.GetSparseFragmentDataPtr<FArcAttributeSet_TestDerivedStats>();

		ASSERT_THAT(IsNear(15.f, Stats->Intelligence.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(30.f, Derived->MaxEnergy.BaseValue, 0.001f));
		ASSERT_THAT(IsNear(30.f, Derived->MaxEnergy.CurrentValue, 0.001f));
	}

	TEST_METHOD(MetaAttribute_DamageToHealth)
	{
		FArcAttributeHandler_Test_DamageToHealth Handler;

		UArcAttributeHandlerConfig* HandlerConfig = NewObject<UArcAttributeHandlerConfig>();
		HandlerConfig->Handlers.Add(FArcAttributeSet_TestDerivedStats::GetDamageAttribute(), FInstancedStruct::Make(Handler));

		FArcAttributeSet_TestStats InitStats;
		InitStats.Health = { 100.f, 100.f };
		InitStats.Armor = { 0.f, 0.f };
		InitStats.Intelligence = { 10.f, 10.f };
		InitStats.AttackPower = { 0.f, 0.f };

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

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;
		FArcTestAddExecution Exec;
		Exec.TargetAttribute = FArcAttributeSet_TestDerivedStats::GetDamageAttribute();
		Exec.Value = 25.f;
		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		DrainPendingOps(Entity);

		FMassEntityView ResultView(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = ResultView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		FArcAttributeSet_TestDerivedStats* Derived = ResultView.GetSparseFragmentDataPtr<FArcAttributeSet_TestDerivedStats>();

		ASSERT_THAT(IsNear(75.f, Stats->Health.BaseValue, 0.001f));
		ASSERT_THAT(IsNear(75.f, Stats->Health.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(0.f, Derived->Damage.BaseValue, 0.001f));
	}

	TEST_METHOD(CaptureExecution_SourceAndTarget)
	{
		FArcAttributeSet_TestStats SourceStats;
		SourceStats.Health = { 100.f, 100.f };
		SourceStats.Armor = { 0.f, 0.f };
		SourceStats.Intelligence = { 10.f, 10.f };
		SourceStats.AttackPower = { 80.f, 80.f };

		TArray<FInstancedStruct> SourceFragments;
		SourceFragments.Add(FInstancedStruct::Make(SourceStats));
		SourceFragments.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		SourceFragments.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		SourceFragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));

		FMassEntityHandle SourceEntity = ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(*EntityManager, SourceFragments);

		FArcAttributeSet_TestStats TargetStats;
		TargetStats.Health = { 100.f, 100.f };
		TargetStats.Armor = { 30.f, 30.f };
		TargetStats.Intelligence = { 10.f, 10.f };
		TargetStats.AttackPower = { 0.f, 0.f };

		FArcAttributeSet_TestDerivedStats TargetDerived;

		TArray<FInstancedStruct> TargetFragments;
		TargetFragments.Add(FInstancedStruct::Make(TargetStats));
		TargetFragments.Add(FInstancedStruct::Make(TargetDerived));
		TargetFragments.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		TargetFragments.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		TargetFragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));
		TargetFragments.Add(FInstancedStruct::Make(FArcPendingAttributeOpsFragment()));

		FMassEntityHandle TargetEntity = ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(*EntityManager, TargetFragments);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;

		FArcTestDamageCalcExecution Exec;
		Exec.OutputAttribute = FArcAttributeSet_TestDerivedStats::GetDamageAttribute();
		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));
		Effect->BuildCaptureLayout();

		ArcEffects::TryApplyEffect(*EntityManager, TargetEntity, Effect, SourceEntity);
		DrainPendingOps(TargetEntity);

		FMassEntityView TargetView(*EntityManager, TargetEntity);
		FArcAttributeSet_TestDerivedStats* Derived = TargetView.GetSparseFragmentDataPtr<FArcAttributeSet_TestDerivedStats>();
		ASSERT_THAT(IsNear(50.f, Derived->Damage.BaseValue, 0.001f));
	}

	TEST_METHOD(CaptureExecution_WithTagFiltering)
	{
		FArcAttributeSet_TestStats SourceStats;
		SourceStats.Health = { 100.f, 100.f };
		SourceStats.Armor = { 0.f, 0.f };
		SourceStats.Intelligence = { 10.f, 10.f };
		SourceStats.AttackPower = { 50.f, 50.f };

		TArray<FInstancedStruct> SourceFragments;
		SourceFragments.Add(FInstancedStruct::Make(SourceStats));
		SourceFragments.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		SourceFragments.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		SourceFragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));

		FMassEntityHandle SourceEntity = ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(*EntityManager, SourceFragments);

		FMassEntityView SourceView(*EntityManager, SourceEntity);
		FArcAggregatorFragment* SourceAgg = SourceView.GetFragmentDataPtr<FArcAggregatorFragment>();
		FArcAggregator& AttackAgg = SourceAgg->FindOrAddAggregator(FArcAttributeSet_TestStats::GetAttackPowerAttribute());

		FArcGameplayTagRequirements TargetReqs;
		TargetReqs.RequireTags.AddTag(TAG_PipelineTest_Fire);
		AttackAgg.AddMod(EArcModifierOp::Add, 30.f, FMassEntityHandle(), nullptr, &TargetReqs);

		FArcAttributeSet_TestStats TargetStats;
		TargetStats.Health = { 100.f, 100.f };
		TargetStats.Armor = { 10.f, 10.f };
		TargetStats.Intelligence = { 10.f, 10.f };
		TargetStats.AttackPower = { 0.f, 0.f };

		FArcAttributeSet_TestDerivedStats TargetDerived;

		TArray<FInstancedStruct> TargetFragments;
		TargetFragments.Add(FInstancedStruct::Make(TargetStats));
		TargetFragments.Add(FInstancedStruct::Make(TargetDerived));
		TargetFragments.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		TargetFragments.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		TargetFragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));
		TargetFragments.Add(FInstancedStruct::Make(FArcPendingAttributeOpsFragment()));

		FMassEntityHandle TargetEntity = ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(*EntityManager, TargetFragments);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;

		FArcTestDamageCalcExecution Exec;
		Exec.OutputAttribute = FArcAttributeSet_TestDerivedStats::GetDamageAttribute();
		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));
		Effect->BuildCaptureLayout();

		// Without buff tag — mod disqualified
		ArcEffects::TryApplyEffect(*EntityManager, TargetEntity, Effect, SourceEntity);
		DrainPendingOps(TargetEntity);

		FMassEntityView TargetView(*EntityManager, TargetEntity);
		FArcAttributeSet_TestDerivedStats* Derived = TargetView.GetSparseFragmentDataPtr<FArcAttributeSet_TestDerivedStats>();
		// AttackPower = 50 (mod disqualified), Damage = 50 - 10 = 40
		ASSERT_THAT(IsNear(40.f, Derived->Damage.BaseValue, 0.001f));

		// Add buff tag and re-apply
		FArcOwnedTagsFragment* TargetTags = TargetView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		TargetTags->Tags.AddTag(TAG_PipelineTest_Fire);
		Derived->Damage.BaseValue = 0.f;

		ArcEffects::TryApplyEffect(*EntityManager, TargetEntity, Effect, SourceEntity);
		DrainPendingOps(TargetEntity);

		// AttackPower = 80 (50+30 qualified), Damage = 80 - 10 = 70
		ASSERT_THAT(IsNear(70.f, Derived->Damage.BaseValue, 0.001f));
	}

	TEST_METHOD(CaptureExecution_DamageToHealth)
	{
		// Handler: Damage -> Health via PostExecute
		FArcAttributeHandler_Test_DamageToHealth Handler;

		UArcAttributeHandlerConfig* HandlerConfig = NewObject<UArcAttributeHandlerConfig>();
		HandlerConfig->Handlers.Add(FArcAttributeSet_TestDerivedStats::GetDamageAttribute(), FInstancedStruct::Make(Handler));

		// Source: AttackPower = 80
		FArcAttributeSet_TestStats SourceStats;
		SourceStats.Health = { 100.f, 100.f };
		SourceStats.Armor = { 0.f, 0.f };
		SourceStats.Intelligence = { 10.f, 10.f };
		SourceStats.AttackPower = { 80.f, 80.f };

		TArray<FInstancedStruct> SourceFragments;
		SourceFragments.Add(FInstancedStruct::Make(SourceStats));
		SourceFragments.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		SourceFragments.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		SourceFragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));

		FMassEntityHandle SourceEntity = ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(*EntityManager, SourceFragments);

		// Target: Health=100, Armor=30, has DamageToHealth handler
		FArcAttributeSet_TestStats TargetStats;
		TargetStats.Health = { 100.f, 100.f };
		TargetStats.Armor = { 30.f, 30.f };
		TargetStats.Intelligence = { 10.f, 10.f };
		TargetStats.AttackPower = { 0.f, 0.f };

		FArcAttributeSet_TestDerivedStats TargetDerived;

		TArray<FInstancedStruct> TargetFragments;
		TargetFragments.Add(FInstancedStruct::Make(TargetStats));
		TargetFragments.Add(FInstancedStruct::Make(TargetDerived));
		TargetFragments.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		TargetFragments.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		TargetFragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));
		TargetFragments.Add(FInstancedStruct::Make(FArcPendingAttributeOpsFragment()));

		FMassEntityHandle TargetEntity = ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(*EntityManager, TargetFragments);
		FArcAttributeHandlerSharedFragment SharedHandler;
		SharedHandler.Config = HandlerConfig;
		const FConstSharedStruct HandlerSharedFrag = EntityManager->GetOrCreateConstSharedFragment(SharedHandler);
		EntityManager->AddConstSharedFragmentToEntity(TargetEntity, HandlerSharedFrag);

		// Execution: captures source AttackPower, target Armor, writes Damage
		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;

		FArcTestDamageCalcExecution Exec;
		Exec.OutputAttribute = FArcAttributeSet_TestDerivedStats::GetDamageAttribute();
		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));
		Effect->BuildCaptureLayout();

		ArcEffects::TryApplyEffect(*EntityManager, TargetEntity, Effect, SourceEntity);
		DrainPendingOps(TargetEntity);

		// Damage = max(0, 80 - 30) = 50
		// PostExecute: Health -= 50, Damage reset to 0
		FMassEntityView TargetView(*EntityManager, TargetEntity);
		FArcAttributeSet_TestStats* Stats = TargetView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		FArcAttributeSet_TestDerivedStats* Derived = TargetView.GetSparseFragmentDataPtr<FArcAttributeSet_TestDerivedStats>();

		ASSERT_THAT(IsNear(50.f, Stats->GetHealth(), 0.001f));
		ASSERT_THAT(IsNear(50.f, Stats->Health.BaseValue, 0.001f));
		ASSERT_THAT(IsNear(0.f, Derived->Damage.BaseValue, 0.001f));
	}

	TEST_METHOD(CaptureExecution_DamageToHealth_WithTagFiltering)
	{
		FArcAttributeHandler_Test_DamageToHealth Handler;

		UArcAttributeHandlerConfig* HandlerConfig = NewObject<UArcAttributeHandlerConfig>();
		HandlerConfig->Handlers.Add(FArcAttributeSet_TestDerivedStats::GetDamageAttribute(), FInstancedStruct::Make(Handler));

		// Source: AttackPower=50, conditional +30 requiring "Test.Fire" on target
		FArcAttributeSet_TestStats SourceStats;
		SourceStats.Health = { 100.f, 100.f };
		SourceStats.Armor = { 0.f, 0.f };
		SourceStats.Intelligence = { 10.f, 10.f };
		SourceStats.AttackPower = { 50.f, 50.f };

		TArray<FInstancedStruct> SourceFragments;
		SourceFragments.Add(FInstancedStruct::Make(SourceStats));
		SourceFragments.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		SourceFragments.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		SourceFragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));

		FMassEntityHandle SourceEntity = ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(*EntityManager, SourceFragments);

		FMassEntityView SourceView(*EntityManager, SourceEntity);
		FArcAggregatorFragment* SourceAgg = SourceView.GetFragmentDataPtr<FArcAggregatorFragment>();
		FArcAggregator& AttackAgg = SourceAgg->FindOrAddAggregator(FArcAttributeSet_TestStats::GetAttackPowerAttribute());

		FArcGameplayTagRequirements TargetReqs;
		TargetReqs.RequireTags.AddTag(TAG_PipelineTest_Fire);
		AttackAgg.AddMod(EArcModifierOp::Add, 30.f, FMassEntityHandle(), nullptr, &TargetReqs);

		// Target: Health=100, Armor=10, has DamageToHealth handler
		FArcAttributeSet_TestStats TargetStats;
		TargetStats.Health = { 100.f, 100.f };
		TargetStats.Armor = { 10.f, 10.f };
		TargetStats.Intelligence = { 10.f, 10.f };
		TargetStats.AttackPower = { 0.f, 0.f };

		FArcAttributeSet_TestDerivedStats TargetDerived;

		TArray<FInstancedStruct> TargetFragments;
		TargetFragments.Add(FInstancedStruct::Make(TargetStats));
		TargetFragments.Add(FInstancedStruct::Make(TargetDerived));
		TargetFragments.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		TargetFragments.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		TargetFragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));
		TargetFragments.Add(FInstancedStruct::Make(FArcPendingAttributeOpsFragment()));

		FMassEntityHandle TargetEntity = ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(*EntityManager, TargetFragments);
		FArcAttributeHandlerSharedFragment SharedHandler;
		SharedHandler.Config = HandlerConfig;
		const FConstSharedStruct HandlerSharedFrag = EntityManager->GetOrCreateConstSharedFragment(SharedHandler);
		EntityManager->AddConstSharedFragmentToEntity(TargetEntity, HandlerSharedFrag);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;

		FArcTestDamageCalcExecution Exec;
		Exec.OutputAttribute = FArcAttributeSet_TestDerivedStats::GetDamageAttribute();
		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(FInstancedStruct::Make(Exec)));
		Effect->BuildCaptureLayout();

		// Without buff tag: AttackPower=50, Damage=50-10=40, Health=100-40=60
		ArcEffects::TryApplyEffect(*EntityManager, TargetEntity, Effect, SourceEntity);
		DrainPendingOps(TargetEntity);

		FMassEntityView TargetView(*EntityManager, TargetEntity);
		FArcAttributeSet_TestStats* Stats = TargetView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(60.f, Stats->GetHealth(), 0.001f));

		// Add buff tag: AttackPower=80, Damage=80-10=70, Health=60-70=-10 (clamped? no, raw)
		FArcOwnedTagsFragment* TargetTags = TargetView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		TargetTags->Tags.AddTag(TAG_PipelineTest_Fire);

		ArcEffects::TryApplyEffect(*EntityManager, TargetEntity, Effect, SourceEntity);
		DrainPendingOps(TargetEntity);

		// Health = 60 - 70 = -10
		ASSERT_THAT(IsNear(-10.f, Stats->GetHealth(), 0.001f));
	}

	TEST_METHOD(PostExecute_NotFiredFromProcessorRecalc)
	{
		FArcAttributeHandler_Test_DamageToHealth Handler;

		UArcAttributeHandlerConfig* HandlerConfig = NewObject<UArcAttributeHandlerConfig>();
		HandlerConfig->Handlers.Add(FArcAttributeSet_TestDerivedStats::GetDamageAttribute(), FInstancedStruct::Make(Handler));

		FArcAttributeSet_TestStats InitStats;
		InitStats.Health = { 100.f, 100.f };
		InitStats.Armor = { 0.f, 0.f };
		InitStats.Intelligence = { 10.f, 10.f };
		InitStats.AttackPower = { 0.f, 0.f };

		FArcAttributeSet_TestDerivedStats DerivedStats;

		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(InitStats));
		Fragments.Add(FInstancedStruct::Make(DerivedStats));
		Fragments.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		Fragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));

		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(*EntityManager, Fragments);
		FArcAttributeHandlerSharedFragment SharedHandler;
		SharedHandler.Config = HandlerConfig;
		const FConstSharedStruct HandlerSharedFrag = EntityManager->GetOrCreateConstSharedFragment(SharedHandler);
		EntityManager->AddConstSharedFragmentToEntity(Entity, HandlerSharedFrag);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 10.f;
		FArcEffectModifier Mod;
		Mod.Attribute = FArcAttributeSet_TestDerivedStats::GetDamageAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FArcScalableFloat(20.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		RecalculateAttributes(Entity);

		FMassEntityView ResultView(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = ResultView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		FArcAttributeSet_TestDerivedStats* Derived = ResultView.GetSparseFragmentDataPtr<FArcAttributeSet_TestDerivedStats>();

		ASSERT_THAT(IsNear(20.f, Derived->Damage.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(100.f, Stats->Health.BaseValue, 0.001f));
	}

	// SetBaseValue writes BaseValue only; CurrentValue is not updated until an
	// AttributeRecalculate signal is processed for an attribute with an active aggregator mod.
	TEST_METHOD(SetBaseValue_WritesBaseOnly_CurrentUnchangedUntilRecalc)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		// Apply a duration Add +10 mod so the aggregator has something to re-evaluate.
		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Duration;
		Effect->StackingPolicy.Duration = 10.f;
		FArcEffectModifier Mod;
		Mod.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Magnitude = FArcScalableFloat(10.f);
		Effect->Modifiers.Add(Mod);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());
		// First recalc seeds CurrentValue: base 100 + Add 10 = 110.
		RecalculateAttributes(Entity);

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcAttributeSet_TestStats* Stats = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(110.f, Stats->Health.CurrentValue, 0.001f));

		// SetBaseValue writes BaseValue only — CurrentValue must remain at 110 immediately after.
		Stats->Health.SetBaseValue(200.f);
		ASSERT_THAT(IsNear(200.f, Stats->Health.BaseValue, 0.001f));
		ASSERT_THAT(IsNear(110.f, Stats->Health.CurrentValue, 0.001f));

		// After an AttributeRecalculate signal, CurrentValue picks up the new base: 200 + Add 10 = 210.
		RecalculateAttributes(Entity);
		ASSERT_THAT(IsNear(210.f, Stats->Health.CurrentValue, 0.001f));
	}
};
