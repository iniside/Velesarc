// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "Components/ActorTestSpawner.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Attributes/ArcAggregator.h"
#include "Attributes/ArcAttributeAggregatorProcessor.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Effects/ArcActiveEffectHandle.h"
#include "Effects/ArcEffectComp_Inhibition.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectModifier.h"
#include "Effects/ArcEffectSpec.h"
#include "Effects/ArcEffectTypes.h"
#include "Effects/ArcMagnitudeCalculation.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Processors/ArcEffectInhibitionProcessor.h"
#include "Processors/ArcPendingAttributeOpsProcessor.h"
#include "Signals/ArcAttributeRecalculateSignal.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "MassSignalSubsystem.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "ArcScalableFloat.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/SharedStruct.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_LiveMag_Inhibit, "Test.LiveMag.Inhibit");

namespace ArcLiveMagnitudeTestsPrivate
{
	FArcAttributeRef Source()
	{
		return FArcAttributeSet_TestStats::GetIntelligenceAttribute();
	}

	FArcAttributeRef Middle()
	{
		return FArcAttributeSet_TestStats::GetArmorAttribute();
	}

	FArcAttributeRef Last()
	{
		return FArcAttributeSet_TestStats::GetAttackPowerAttribute();
	}

	UArcEffectDefinition* MakeLiveEffect(
		const FArcAttributeRef& Target, const FArcAttributeRef& SourceAttribute, EArcModifierOp Operation,
		float Coefficient, float PreAdd, float PostAdd)
	{
		FArcLiveMagnitude_AttributeLinear Live;
		Live.Attribute = SourceAttribute;
		Live.Coefficient = Coefficient;
		Live.PreAdd = PreAdd;
		Live.PostAdd = PostAdd;

		FArcEffectModifier Modifier;
		Modifier.Attribute = Target;
		Modifier.Operation = Operation;
		Modifier.Type = EArcModifierType::Custom;
		Modifier.CustomMagnitude = FInstancedStruct::Make(Live);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		Effect->Modifiers.Add(Modifier);
		return Effect;
	}

	UArcEffectDefinition* MakeLinearAddEffect(const FArcAttributeRef& Target, const FArcAttributeRef& SourceAttribute, float Coefficient, float PostAdd = 0.f)
	{
		return MakeLiveEffect(Target, SourceAttribute, EArcModifierOp::Add, Coefficient, 0.f, PostAdd);
	}

	UArcEffectDefinition* MakeFlatAddEffect(const FArcAttributeRef& Target, float Amount)
	{
		FArcEffectModifier Modifier;
		Modifier.Attribute = Target;
		Modifier.Operation = EArcModifierOp::Add;
		Modifier.Magnitude = FArcScalableFloat(Amount);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		Effect->Modifiers.Add(Modifier);
		return Effect;
	}

	UArcEffectDefinition* MakeInstantAddToSourceEffect(float Amount)
	{
		FInstancedStruct ExecStruct = FInstancedStruct::Make<FArcTestAddExecution>();
		FArcTestAddExecution& Exec = ExecStruct.GetMutable<FArcTestAddExecution>();
		Exec.TargetAttribute = Source();
		Exec.Value = Amount;

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;
		Effect->Executions.Add(ArcMassAbilitiesTestHelpers::MakeExecutionEntry(MoveTemp(ExecStruct)));
		return Effect;
	}
}

TEST_CLASS(ArcLiveMagnitude, "ArcMassAbilities.Aggregator.LiveMagnitude")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcAttributeAggregatorProcessor* AggregatorProcessor = nullptr;
	UArcPendingAttributeOpsProcessor* PendingProcessor = nullptr;
	UArcEffectInhibitionProcessor* InhibitionProcessor = nullptr;

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

		AggregatorProcessor = NewObject<UArcAttributeAggregatorProcessor>();
		AggregatorProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);

		PendingProcessor = NewObject<UArcPendingAttributeOpsProcessor>();
		PendingProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);

		InhibitionProcessor = NewObject<UArcEffectInhibitionProcessor>();
		InhibitionProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);
	}

	void RunAggregator(FMassEntityHandle Entity)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AttributeRecalculate, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*AggregatorProcessor, ProcessingContext);
	}

	void RunInhibition(FMassEntityHandle Entity)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AttributeRecalculate, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*InhibitionProcessor, ProcessingContext);
	}

	void DrainPendingOps(FMassEntityHandle Entity)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::PendingAttributeOps, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*PendingProcessor, ProcessingContext);
	}

	FArcAttributeSet_TestStats* GetStats(FMassEntityHandle Entity)
	{
		FMassEntityView View(*EntityManager, Entity);
		return View.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
	}

	FArcAggregatorFragment* GetAggregatorFragment(FMassEntityHandle Entity)
	{
		FMassEntityView View(*EntityManager, Entity);
		return View.GetFragmentDataPtr<FArcAggregatorFragment>();
	}

	FArcEffectStackFragment* GetEffectStack(FMassEntityHandle Entity)
	{
		FMassEntityView View(*EntityManager, Entity);
		return View.GetFragmentDataPtr<FArcEffectStackFragment>();
	}

	void SetIntelligenceAndNotify(FMassEntityHandle Entity, float Value)
	{
		FArcAttributeSet_TestStats* Stats = GetStats(Entity);
		FArcAttributeHandlerContext Context(*EntityManager, nullptr, Entity, static_cast<uint8*>(static_cast<void*>(Stats)), nullptr, FArcAttributeSet_TestStats::GetIntelligenceAttribute());
		Stats->SetIntelligenceDirectAndNotify(Context, Value);
	}

	FArcActiveEffectHandle Apply(FMassEntityHandle Entity, UArcEffectDefinition* Effect)
	{
		return ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle()).Handle;
	}

	void RemoveByHandle(FMassEntityHandle Entity, FArcActiveEffectHandle Handle)
	{
		FArcEffectRemovalQuery Query;
		Query.Handle = Handle;
		ArcEffects::RemoveEffects(*EntityManager, Entity, Query);
	}

	TEST_METHOD(LiveMod_AfterAggregatorPass_UsesSourceCurrentValue)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		Apply(Entity, ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Middle(), ArcLiveMagnitudeTestsPrivate::Source(), 2.f));

		RunAggregator(Entity);

		ASSERT_THAT(IsNear(20.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));
		ASSERT_THAT(IsTrue(GetAggregatorFragment(Entity)->HasLiveDependents(ArcLiveMagnitudeTestsPrivate::Source())));
	}

	TEST_METHOD(LiveMod_RecalculatesTargetAfterSetAttributeDirectAndNotify)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		Apply(Entity, ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Middle(), ArcLiveMagnitudeTestsPrivate::Source(), 2.f));
		RunAggregator(Entity);
		ASSERT_THAT(IsNear(20.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));

		SetIntelligenceAndNotify(Entity, 30.f);
		ASSERT_THAT(IsNear(60.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));

		SetIntelligenceAndNotify(Entity, 5.f);
		ASSERT_THAT(IsNear(10.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));
	}

	TEST_METHOD(LiveMod_RecalculatesTargetInSamePassAsPendingOp)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		Apply(Entity, ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Middle(), ArcLiveMagnitudeTestsPrivate::Source(), 2.f));
		RunAggregator(Entity);
		ASSERT_THAT(IsNear(20.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));

		ArcEffects::TryApplyEffect(*EntityManager, Entity, ArcLiveMagnitudeTestsPrivate::MakeInstantAddToSourceEffect(5.f), FMassEntityHandle());
		DrainPendingOps(Entity);

		ASSERT_THAT(IsNear(15.f, GetStats(Entity)->Intelligence.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(30.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));
	}

	TEST_METHOD(LiveMod_AggregatorPassEvaluatesAggregatedSourceBeforeItsDependent)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		Apply(Entity, ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Middle(), ArcLiveMagnitudeTestsPrivate::Source(), 2.f));
		Apply(Entity, ArcLiveMagnitudeTestsPrivate::MakeFlatAddEffect(ArcLiveMagnitudeTestsPrivate::Source(), 5.f));

		RunAggregator(Entity);

		ASSERT_THAT(IsNear(15.f, GetStats(Entity)->Intelligence.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(30.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));
	}

	TEST_METHOD(Chain_AggregatorPassEvaluatesInTopologicalOrder_WhenLastLinkWasAppliedFirst)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		Apply(Entity, ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Last(), ArcLiveMagnitudeTestsPrivate::Middle(), 1.f, 1.f));
		Apply(Entity, ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Middle(), ArcLiveMagnitudeTestsPrivate::Source(), 2.f));

		RunAggregator(Entity);

		ASSERT_THAT(IsNear(20.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(21.f, GetStats(Entity)->AttackPower.CurrentValue, 0.001f));
	}

	TEST_METHOD(Chain_SetAttributeDirectAndNotifyPropagatesThroughEveryLink)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		Apply(Entity, ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Last(), ArcLiveMagnitudeTestsPrivate::Middle(), 1.f, 1.f));
		Apply(Entity, ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Middle(), ArcLiveMagnitudeTestsPrivate::Source(), 2.f));
		RunAggregator(Entity);

		SetIntelligenceAndNotify(Entity, 20.f);

		ASSERT_THAT(IsNear(40.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(41.f, GetStats(Entity)->AttackPower.CurrentValue, 0.001f));
	}

	TEST_METHOD(Chain_PendingOpPropagatesThroughEveryLinkInOnePass)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		Apply(Entity, ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Last(), ArcLiveMagnitudeTestsPrivate::Middle(), 1.f, 1.f));
		Apply(Entity, ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Middle(), ArcLiveMagnitudeTestsPrivate::Source(), 2.f));
		RunAggregator(Entity);

		ArcEffects::TryApplyEffect(*EntityManager, Entity, ArcLiveMagnitudeTestsPrivate::MakeInstantAddToSourceEffect(5.f), FMassEntityHandle());
		DrainPendingOps(Entity);

		ASSERT_THAT(IsNear(30.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(31.f, GetStats(Entity)->AttackPower.CurrentValue, 0.001f));
	}

	TEST_METHOD(Cycle_TwoAttributes_SecondEdgeFallsBackToSnapshotAndIsNotRegistered)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		Apply(Entity, ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Middle(), ArcLiveMagnitudeTestsPrivate::Source(), 2.f));
		RunAggregator(Entity);
		ASSERT_THAT(IsNear(20.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));

		const FArcActiveEffectHandle CycleHandle = Apply(Entity, ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Source(), ArcLiveMagnitudeTestsPrivate::Middle(), 1.f, 0.f));
		ASSERT_THAT(IsTrue(CycleHandle.IsValid()));

		const FArcAggregatorFragment* AggFrag = GetAggregatorFragment(Entity);
		ASSERT_THAT(IsFalse(AggFrag->HasLiveDependents(ArcLiveMagnitudeTestsPrivate::Middle())));
		ASSERT_THAT(IsTrue(AggFrag->HasLiveDependents(ArcLiveMagnitudeTestsPrivate::Source())));

		const FArcAggregator* SourceAggregator = AggFrag->FindAggregator(ArcLiveMagnitudeTestsPrivate::Source());
		ASSERT_THAT(IsNotNull(SourceAggregator));
		const TArray<FArcAggregatorMod>& AddMods = SourceAggregator->Mods[static_cast<uint8>(EArcModifierOp::Add)];
		ASSERT_THAT(AreEqual(1, AddMods.Num()));
		ASSERT_THAT(IsFalse(AddMods[0].IsLive()));
		ASSERT_THAT(IsNear(20.f, AddMods[0].Magnitude, 0.001f));
	}

	TEST_METHOD(Cycle_TwoAttributes_ValuesSettleAndStayStableAcrossPasses)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		Apply(Entity, ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Middle(), ArcLiveMagnitudeTestsPrivate::Source(), 2.f));
		RunAggregator(Entity);
		Apply(Entity, ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Source(), ArcLiveMagnitudeTestsPrivate::Middle(), 1.f, 0.f));

		RunAggregator(Entity);
		const float SourceAfterFirst = GetStats(Entity)->Intelligence.CurrentValue;
		const float MiddleAfterFirst = GetStats(Entity)->Armor.CurrentValue;
		RunAggregator(Entity);
		RunAggregator(Entity);

		ASSERT_THAT(IsNear(30.f, SourceAfterFirst, 0.001f));
		ASSERT_THAT(IsNear(60.f, MiddleAfterFirst, 0.001f));
		ASSERT_THAT(IsNear(SourceAfterFirst, GetStats(Entity)->Intelligence.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(MiddleAfterFirst, GetStats(Entity)->Armor.CurrentValue, 0.001f));

		SetIntelligenceAndNotify(Entity, 40.f);
		ASSERT_THAT(IsNear(80.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));
	}

	TEST_METHOD(Cycle_SelfReference_FallsBackToSnapshotWithoutEdge)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager, 100.f, 5.f);
		const FArcActiveEffectHandle Handle = Apply(Entity, ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Middle(), ArcLiveMagnitudeTestsPrivate::Middle(), 3.f));
		ASSERT_THAT(IsTrue(Handle.IsValid()));

		ASSERT_THAT(IsFalse(GetAggregatorFragment(Entity)->HasLiveDependents(ArcLiveMagnitudeTestsPrivate::Middle())));

		RunAggregator(Entity);
		const float AfterFirst = GetStats(Entity)->Armor.CurrentValue;
		RunAggregator(Entity);

		ASSERT_THAT(IsNear(20.f, AfterFirst, 0.001f));
		ASSERT_THAT(IsNear(AfterFirst, GetStats(Entity)->Armor.CurrentValue, 0.001f));
	}

	TEST_METHOD(StackScaling_AddMultipliesLiveValueByStackCount)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		UArcEffectDefinition* Effect = ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Middle(), ArcLiveMagnitudeTestsPrivate::Source(), 1.f);
		Effect->StackingPolicy.StackPolicy = EArcStackPolicy::Aggregate;
		Effect->StackingPolicy.MaxStacks = 3;

		Apply(Entity, Effect);
		Apply(Entity, Effect);
		Apply(Entity, Effect);
		RunAggregator(Entity);

		const FArcEffectSpec* Spec = GetEffectStack(Entity)->ActiveEffects[0].Spec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(Spec));
		ASSERT_THAT(AreEqual(3, Spec->Context.StackCount));
		ASSERT_THAT(AreEqual(1, GetAggregatorFragment(Entity)->FindAggregator(ArcLiveMagnitudeTestsPrivate::Middle())->Mods[static_cast<uint8>(EArcModifierOp::Add)].Num()));
		ASSERT_THAT(IsNear(30.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));

		SetIntelligenceAndNotify(Entity, 20.f);
		ASSERT_THAT(IsNear(60.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));
	}

	TEST_METHOD(StackScaling_MultiplyCompoundRaisesLiveValueToStackCount)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager, 100.f, 5.f);
		UArcEffectDefinition* Effect = ArcLiveMagnitudeTestsPrivate::MakeLiveEffect(
			ArcLiveMagnitudeTestsPrivate::Middle(), ArcLiveMagnitudeTestsPrivate::Source(), EArcModifierOp::MultiplyCompound, 0.1f, 0.f, 1.f);
		Effect->StackingPolicy.StackPolicy = EArcStackPolicy::Aggregate;
		Effect->StackingPolicy.MaxStacks = 3;

		Apply(Entity, Effect);
		Apply(Entity, Effect);
		Apply(Entity, Effect);
		RunAggregator(Entity);

		ASSERT_THAT(IsNear(40.f, GetStats(Entity)->Armor.CurrentValue, 0.01f));

		SetIntelligenceAndNotify(Entity, 20.f);
		ASSERT_THAT(IsNear(135.f, GetStats(Entity)->Armor.CurrentValue, 0.01f));
	}

	TEST_METHOD(Inhibit_RemovesLiveModAndEdge_AndRestoreReAddsThem)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		UArcEffectDefinition* Effect = ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Middle(), ArcLiveMagnitudeTestsPrivate::Source(), 2.f);
		FArcEffectComp_Inhibition Inhibition;
		Inhibition.InhibitedByTags.AddTag(TAG_LiveMag_Inhibit);
		Effect->Components.Add(FInstancedStruct::Make(Inhibition));
		Apply(Entity, Effect);
		RunAggregator(Entity);
		ASSERT_THAT(IsNear(20.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));
		ASSERT_THAT(IsTrue(GetAggregatorFragment(Entity)->HasLiveDependents(ArcLiveMagnitudeTestsPrivate::Source())));

		FGameplayTagContainer InhibitTags;
		InhibitTags.AddTag(TAG_LiveMag_Inhibit);
		ArcAbilities::AddOwnedTags(*EntityManager, Entity, InhibitTags);
		RunInhibition(Entity);
		RunAggregator(Entity);

		ASSERT_THAT(IsTrue(GetEffectStack(Entity)->ActiveEffects[0].bInhibited));
		ASSERT_THAT(IsFalse(GetAggregatorFragment(Entity)->HasLiveDependents(ArcLiveMagnitudeTestsPrivate::Source())));
		ASSERT_THAT(IsTrue(GetAggregatorFragment(Entity)->LiveDependencyOrder.IsEmpty()));
		ASSERT_THAT(IsNear(0.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));

		SetIntelligenceAndNotify(Entity, 30.f);
		ASSERT_THAT(IsNear(0.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));

		ArcAbilities::RemoveOwnedTags(*EntityManager, Entity, InhibitTags);
		RunInhibition(Entity);
		RunAggregator(Entity);

		ASSERT_THAT(IsFalse(GetEffectStack(Entity)->ActiveEffects[0].bInhibited));
		ASSERT_THAT(IsTrue(GetAggregatorFragment(Entity)->HasLiveDependents(ArcLiveMagnitudeTestsPrivate::Source())));
		ASSERT_THAT(IsNear(60.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));

		SetIntelligenceAndNotify(Entity, 40.f);
		ASSERT_THAT(IsNear(80.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));
	}

	TEST_METHOD(RemoveEffect_RemovesLiveModAndEdge)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		const FArcActiveEffectHandle Handle = Apply(Entity, ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Middle(), ArcLiveMagnitudeTestsPrivate::Source(), 2.f));
		RunAggregator(Entity);
		ASSERT_THAT(IsNear(20.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));

		RemoveByHandle(Entity, Handle);
		RunAggregator(Entity);

		ASSERT_THAT(IsFalse(GetAggregatorFragment(Entity)->HasLiveDependents(ArcLiveMagnitudeTestsPrivate::Source())));
		ASSERT_THAT(IsTrue(GetAggregatorFragment(Entity)->LiveDependencyOrder.IsEmpty()));
		ASSERT_THAT(IsNear(0.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));
	}

	TEST_METHOD(TwoLiveModsOnSameEdge_EdgeSurvivesRemovingOne)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		const FArcActiveEffectHandle DoubleHandle = Apply(Entity, ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Middle(), ArcLiveMagnitudeTestsPrivate::Source(), 2.f));
		const FArcActiveEffectHandle SingleHandle = Apply(Entity, ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Middle(), ArcLiveMagnitudeTestsPrivate::Source(), 1.f));
		RunAggregator(Entity);
		ASSERT_THAT(IsNear(30.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));

		RemoveByHandle(Entity, DoubleHandle);
		RunAggregator(Entity);
		ASSERT_THAT(IsTrue(GetAggregatorFragment(Entity)->HasLiveDependents(ArcLiveMagnitudeTestsPrivate::Source())));
		ASSERT_THAT(IsNear(10.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));

		SetIntelligenceAndNotify(Entity, 20.f);
		ASSERT_THAT(IsNear(20.f, GetStats(Entity)->Armor.CurrentValue, 0.001f));

		RemoveByHandle(Entity, SingleHandle);
		ASSERT_THAT(IsFalse(GetAggregatorFragment(Entity)->HasLiveDependents(ArcLiveMagnitudeTestsPrivate::Source())));
	}

	TEST_METHOD(Evaluate_RefreshesLiveMagnitudeWithoutPolicy)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		Apply(Entity, ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Middle(), ArcLiveMagnitudeTestsPrivate::Source(), 2.f));
		GetStats(Entity)->Intelligence.CurrentValue = 30.f;

		const FArcAggregator* Aggregator = GetAggregatorFragment(Entity)->FindAggregator(ArcLiveMagnitudeTestsPrivate::Middle());
		ASSERT_THAT(IsNotNull(Aggregator));
		const FGameplayTagContainer NoTags;
		const FArcAggregationContext Context{Entity, NoTags, *EntityManager, 0.f};

		ASSERT_THAT(IsNear(60.f, Aggregator->Evaluate(Context), 0.001f));
	}

	TEST_METHOD(Evaluate_RefreshesLiveMagnitudeBeforeThePolicyRuns)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		Apply(Entity, ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Middle(), ArcLiveMagnitudeTestsPrivate::Source(), 2.f));
		GetStats(Entity)->Intelligence.CurrentValue = 30.f;

		FArcAggregator* Aggregator = GetAggregatorFragment(Entity)->FindAggregator(ArcLiveMagnitudeTestsPrivate::Middle());
		ASSERT_THAT(IsNotNull(Aggregator));
		Aggregator->AggregationPolicy = FConstSharedStruct::Make(FArcTestHighestAdditivePolicy());
		const FGameplayTagContainer NoTags;
		const FArcAggregationContext Context{Entity, NoTags, *EntityManager, 0.f};

		ASSERT_THAT(IsNear(60.f, Aggregator->Evaluate(Context), 0.001f));
	}

	TEST_METHOD(EvaluateWithTags_RefreshesLiveMagnitudeWithoutPolicy)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		Apply(Entity, ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Middle(), ArcLiveMagnitudeTestsPrivate::Source(), 2.f));
		GetStats(Entity)->Intelligence.CurrentValue = 30.f;

		const FArcAggregator* Aggregator = GetAggregatorFragment(Entity)->FindAggregator(ArcLiveMagnitudeTestsPrivate::Middle());
		ASSERT_THAT(IsNotNull(Aggregator));
		const FGameplayTagContainer NoTags;
		const FArcAggregationContext Context{Entity, NoTags, *EntityManager, 0.f};

		ASSERT_THAT(IsNear(60.f, Aggregator->EvaluateWithTags(Context, NoTags, NoTags), 0.001f));
	}

	TEST_METHOD(EvaluateWithTags_RefreshesLiveMagnitudeBeforeThePolicyRuns)
	{
		const FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);
		Apply(Entity, ArcLiveMagnitudeTestsPrivate::MakeLinearAddEffect(ArcLiveMagnitudeTestsPrivate::Middle(), ArcLiveMagnitudeTestsPrivate::Source(), 2.f));
		GetStats(Entity)->Intelligence.CurrentValue = 30.f;

		FArcAggregator* Aggregator = GetAggregatorFragment(Entity)->FindAggregator(ArcLiveMagnitudeTestsPrivate::Middle());
		ASSERT_THAT(IsNotNull(Aggregator));
		Aggregator->AggregationPolicy = FConstSharedStruct::Make(FArcTestHighestAdditivePolicy());
		const FGameplayTagContainer NoTags;
		const FArcAggregationContext Context{Entity, NoTags, *EntityManager, 0.f};

		ASSERT_THAT(IsNear(60.f, Aggregator->EvaluateWithTags(Context, NoTags, NoTags), 0.001f));
	}
};
