// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectComp_Inhibition.h"
#include "Effects/ArcEffectSpec.h"
#include "Effects/ArcEffectTypes.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Attributes/ArcAttribute.h"
#include "Processors/ArcEffectInhibitionProcessor.h"
#include "Processors/ArcPendingAttributeOpsProcessor.h"
#include "Attributes/ArcAttributeAggregatorProcessor.h"
#include "Signals/ArcAttributeRecalculateSignal.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "MassSignalSubsystem.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Inhibit_Fire, "Test.Inhibit.Fire");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Inhibit_Ice, "Test.Inhibit.Ice");

// ============================================================================
// ArcEffectInhibition
// ============================================================================

TEST_CLASS(ArcEffectInhibition, "ArcMassAbilities.Effects.Inhibition")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcEffectInhibitionProcessor* InhibitionProcessor = nullptr;
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

		InhibitionProcessor = NewObject<UArcEffectInhibitionProcessor>();
		InhibitionProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);

		AggregatorProcessor = NewObject<UArcAttributeAggregatorProcessor>();
		AggregatorProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);
	}

	void RunInhibition(FMassEntityHandle Entity)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AttributeRecalculate, Entity);
		FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*InhibitionProcessor, ProcessingContext);
	}

	void RunAggregator(FMassEntityHandle Entity)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AttributeRecalculate, Entity);
		FMassProcessingContext AggProcessingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*AggregatorProcessor, AggProcessingContext);
	}

	UArcEffectDefinition* MakeInfiniteDurationEffectWithInhibition(FGameplayTagContainer InhibitedByTags, float AddMagnitude)
	{
		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;

		FArcEffectModifier Modifier;
		Modifier.Attribute = FArcAttributeSet_TestStats::GetArmorAttribute();
		Modifier.Operation = EArcModifierOp::Add;
		Modifier.Magnitude = FArcScalableFloat(AddMagnitude);
		Effect->Modifiers.Add(Modifier);

		FArcEffectComp_Inhibition InhibComp;
		InhibComp.InhibitedByTags = InhibitedByTags;
		Effect->Components.Add(FInstancedStruct::Make(InhibComp));

		return Effect;
	}

	// Case 1: Effect with inhibition tag present on entity — modifier should be removed from aggregator.
	TEST_METHOD(TagPresent_InhibitsEffect)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		FGameplayTagContainer InhibTags;
		InhibTags.AddTag(TAG_Inhibit_Fire);

		UArcEffectDefinition* Effect = MakeInfiniteDurationEffectWithInhibition(InhibTags, 50.f);
		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		// Add the inhibiting tag to the entity's OwnedTags sparse fragment.
		FMassEntityView EntityView(*EntityManager, Entity);
		FArcOwnedTagsFragment* OwnedTagsPtr = EntityView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		check(OwnedTagsPtr);
		ArcAbilities::AddOwnedTags(*EntityManager, Entity, InhibTags);

		RunInhibition(Entity);

		// After inhibition, bInhibited flag should be set on the effect.
		const bool bEffectInhibited = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>()->ActiveEffects[0].bInhibited;
		ASSERT_THAT(IsTrue(bEffectInhibited));
	}

	// Case 2: Effect inhibited, then tag removed — modifier should be restored.
	TEST_METHOD(TagRemoved_DeinhibitsEffect)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		FGameplayTagContainer InhibTags;
		InhibTags.AddTag(TAG_Inhibit_Fire);

		UArcEffectDefinition* Effect = MakeInfiniteDurationEffectWithInhibition(InhibTags, 50.f);
		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcOwnedTagsFragment* OwnedTagsPtr = EntityView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		check(OwnedTagsPtr);

		// Inhibit.
		ArcAbilities::AddOwnedTags(*EntityManager, Entity, InhibTags);
		RunInhibition(Entity);
		ASSERT_THAT(IsTrue(EntityView.GetFragmentDataPtr<FArcEffectStackFragment>()->ActiveEffects[0].bInhibited));

		// De-inhibit.
		ArcAbilities::RemoveOwnedTags(*EntityManager, Entity, InhibTags);
		RunInhibition(Entity);
		ASSERT_THAT(IsFalse(EntityView.GetFragmentDataPtr<FArcEffectStackFragment>()->ActiveEffects[0].bInhibited));
	}

	// Case 3: Effect without inhibition component is unaffected by inhibition processor.
	TEST_METHOD(NoInhibitionComponent_Unaffected)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Infinite;
		FArcEffectModifier Modifier;
		Modifier.Attribute = FArcAttributeSet_TestStats::GetArmorAttribute();
		Modifier.Operation = EArcModifierOp::Add;
		Modifier.Magnitude = FArcScalableFloat(30.f);
		Effect->Modifiers.Add(Modifier);
		ArcEffects::TryApplyEffect(*EntityManager, Entity, Effect, FMassEntityHandle());

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcOwnedTagsFragment* OwnedTagsPtr = EntityView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		check(OwnedTagsPtr);

		FGameplayTagContainer SomeTags;
		SomeTags.AddTag(TAG_Inhibit_Fire);
		ArcAbilities::AddOwnedTags(*EntityManager, Entity, SomeTags);

		RunInhibition(Entity);

		ASSERT_THAT(IsFalse(EntityView.GetFragmentDataPtr<FArcEffectStackFragment>()->ActiveEffects[0].bInhibited));
	}

	// Case 4: Multiple effects — only the one with matching inhibition tag is inhibited.
	TEST_METHOD(MultipleEffects_OnlyMatchingIsInhibited)
	{
		FMassEntityHandle Entity = ArcMassAbilitiesTestHelpers::CreateAbilityEntity(*EntityManager);

		FGameplayTagContainer FireTags;
		FireTags.AddTag(TAG_Inhibit_Fire);

		// Effect A: inhibited by Fire tag.
		UArcEffectDefinition* EffectA = MakeInfiniteDurationEffectWithInhibition(FireTags, 20.f);
		ArcEffects::TryApplyEffect(*EntityManager, Entity, EffectA, FMassEntityHandle());

		// Effect B: inhibited by Ice tag (won't match).
		FGameplayTagContainer IceTags;
		IceTags.AddTag(TAG_Inhibit_Ice);
		UArcEffectDefinition* EffectB = MakeInfiniteDurationEffectWithInhibition(IceTags, 30.f);
		ArcEffects::TryApplyEffect(*EntityManager, Entity, EffectB, FMassEntityHandle());

		FMassEntityView EntityView(*EntityManager, Entity);
		FArcOwnedTagsFragment* OwnedTagsPtr = EntityView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		check(OwnedTagsPtr);

		ArcAbilities::AddOwnedTags(*EntityManager, Entity, FireTags);

		RunInhibition(Entity);

		const FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		check(Stack && Stack->ActiveEffects.Num() == 2);
		ASSERT_THAT(IsTrue(Stack->ActiveEffects[0].bInhibited));
		ASSERT_THAT(IsFalse(Stack->ActiveEffects[1].bInhibited));
	}

	// Case 5: InhibitionProcessor runs before AggregatorProcessor (ordering contract).
	TEST_METHOD(RunsBeforeAggregator)
	{
		// Verify the ordering dependency is registered by checking the processor's ExecuteBefore set.
		UArcEffectInhibitionProcessor* Proc = NewObject<UArcEffectInhibitionProcessor>();
		const FName AggregatorName = UArcAttributeAggregatorProcessor::StaticClass()->GetFName();
		const bool bOrderingSet = Proc->GetExecutionOrder().ExecuteBefore.Contains(AggregatorName);
		ASSERT_THAT(IsTrue(bOrderingSet));
	}
};
