// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "Components/ActorTestSpawner.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcMassAbilityCost.h"
#include "Abilities/ArcAbilityCooldown.h"
#include "Abilities/ArcAbilityStateTreeSubsystem.h"
#include "Abilities/ArcAbilityHandle.h"
#include "Processors/ArcAbilityInputProcessor.h"
#include "Processors/ArcAbilityStateTreeTickProcessor.h"
#include "Signals/ArcAbilitySignals.h"
#include "Abilities/Schema/ArcAbilityStateTreeSchema.h"
#include "Processors/ArcCooldownProcessor.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "Fragments/ArcAbilityCooldownFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Processors/ArcPendingAttributeOpsProcessor.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
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
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "Abilities/Conditions/ArcAbilityCondition_IsInputHeld.h"
#include "Abilities/Conditions/ArcAbilityCondition_InputState.h"
#include "Abilities/Tasks/ArcAbilityTask_ListenInputPressed.h"
#include "Abilities/Tasks/ArcAbilityTask_ListenInputReleased.h"
#include "Abilities/Tasks/ArcAbilityTask_ListenInputHeld.h"
#include "Abilities/Tasks/ArcAbilityTask_WaitDuration.h"
#include "Processors/ArcAbilityWaitProcessor.h"
#include "Processors/ArcAbilityHeldProcessor.h"
#include "Fragments/ArcAbilityWaitFragment.h"
#include "Fragments/ArcAbilityHeldFragment.h"
#include "StateTreeState.h"
#include "PropertyBindingPath.h"
#include "Abilities/Conditions/ArcAbilityCondition_InputState.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AbilTest_Input_Fire, "InputTag.Ability.Fire");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AbilTest_Input_Dodge, "InputTag.Ability.Dodge");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AbilTest_Ability_Fire, "Ability.Fire");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AbilTest_Ability_Dodge, "Ability.Dodge");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AbilTest_Ability_Melee, "Ability.Melee");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AbilTest_State_Firing, "State.Firing");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AbilTest_State_Blocking, "State.Blocking");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AbilTest_Req_Grounded, "State.Grounded");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AbilTest_Block_Stunned, "State.Stunned");

namespace ArcAbilityTestHelpers
{

FMassEntityHandle CreateAbilityTestEntity(FMassEntityManager& EntityManager, float HealthBase = 100.f)
{
	FArcTestStatsFragment Stats;
	Stats.Health.BaseValue = HealthBase;
	Stats.Health.CurrentValue = HealthBase;

	TArray<FInstancedStruct> Instances;
	Instances.Add(FInstancedStruct::Make(Stats));
	Instances.Add(FInstancedStruct::Make(FArcAbilityCollectionFragment()));
	Instances.Add(FInstancedStruct::Make(FArcAbilityInputFragment()));
	Instances.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));
	Instances.Add(FInstancedStruct::Make(FArcAbilityCooldownFragment()));
	Instances.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
	Instances.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
	Instances.Add(FInstancedStruct::Make(FArcPendingAttributeOpsFragment()));

	return EntityManager.CreateEntity(Instances);
}

UArcAbilityDefinition* CreateSimpleAbilityDef(FGameplayTagContainer AbilityTags = FGameplayTagContainer())
{
	UArcAbilityDefinition* Def = NewObject<UArcAbilityDefinition>();
	Def->AbilityTags = AbilityTags;
	return Def;
}

UStateTree* CreateEmptyStateTree(UObject* Outer)
{
	UStateTree* StateTree = NewObject<UStateTree>(Outer);
	UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(StateTree);
	StateTree->EditorData = EditorData;
	EditorData->Schema = NewObject<UArcAbilityStateTreeSchema>();

	UStateTreeState& Root = EditorData->AddSubTree(FName(TEXT("Root")));
	Root.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

	FStateTreeCompilerLog Log;
	FStateTreeCompiler Compiler(Log);
	Compiler.Compile(*StateTree);
	return StateTree;
}

UStateTree* CreateInputPressedStateTree(UObject* Outer, FGameplayTag InputTag)
{
	UStateTree* StateTree = NewObject<UStateTree>(Outer);
	UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(StateTree);
	StateTree->EditorData = EditorData;
	EditorData->Schema = NewObject<UArcAbilityStateTreeSchema>();

	UStateTreeState& Root = EditorData->AddSubTree(FName(TEXT("Root")));
	Root.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

	UStateTreeState& WaitState = Root.AddChildState(FName(TEXT("WaitForInput")));
	UStateTreeState& DoneState = Root.AddChildState(FName(TEXT("Done")));

	TStateTreeEditorNode<FArcAbilityTask_ListenInputPressed>& ListenTask =
		WaitState.AddTask<FArcAbilityTask_ListenInputPressed>();
	ListenTask.GetInstanceData().InputTag = InputTag;

	FStateTreeTransition& Transition = WaitState.AddTransition(
		EStateTreeTransitionTrigger::OnDelegate, EStateTreeTransitionType::GotoState, &DoneState);

	EditorData->AddPropertyBinding(
		FPropertyBindingPath(ListenTask.ID,
			GET_MEMBER_NAME_CHECKED(FArcAbilityTask_ListenInputPressedInstanceData, OnInputPressed)),
		FPropertyBindingPath(Transition.ID,
			GET_MEMBER_NAME_CHECKED(FStateTreeTransition, DelegateListener)));

	DoneState.AddTask<FArcTestInstantTask>();
	DoneState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

	FStateTreeCompilerLog Log;
	FStateTreeCompiler Compiler(Log);
	bool bCompiled = Compiler.Compile(*StateTree);
	ensureAlwaysMsgf(bCompiled, TEXT("InputReleasedStateTree failed to compile"));
	return StateTree;
}

UStateTree* CreateInputReleasedStateTree(UObject* Outer, FGameplayTag InputTag)
{
	UStateTree* StateTree = NewObject<UStateTree>(Outer);
	UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(StateTree);
	StateTree->EditorData = EditorData;
	EditorData->Schema = NewObject<UArcAbilityStateTreeSchema>();

	UStateTreeState& Root = EditorData->AddSubTree(FName(TEXT("Root")));
	Root.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

	UStateTreeState& WaitState = Root.AddChildState(FName(TEXT("WaitForRelease")));
	UStateTreeState& DoneState = Root.AddChildState(FName(TEXT("Done")));

	TStateTreeEditorNode<FArcAbilityTask_ListenInputReleased>& ListenTask =
		WaitState.AddTask<FArcAbilityTask_ListenInputReleased>();
	ListenTask.GetInstanceData().InputTag = InputTag;

	FStateTreeTransition& Transition = WaitState.AddTransition(
		EStateTreeTransitionTrigger::OnDelegate, EStateTreeTransitionType::GotoState, &DoneState);

	EditorData->AddPropertyBinding(
		FPropertyBindingPath(ListenTask.ID,
			GET_MEMBER_NAME_CHECKED(FArcAbilityTask_ListenInputReleasedInstanceData, OnInputReleased)),
		FPropertyBindingPath(Transition.ID,
			GET_MEMBER_NAME_CHECKED(FStateTreeTransition, DelegateListener)));

	DoneState.AddTask<FArcTestInstantTask>();
	DoneState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

	FStateTreeCompilerLog Log;
	FStateTreeCompiler Compiler(Log);
	Compiler.Compile(*StateTree);
	return StateTree;
}


UStateTree* CreateWaitDurationStateTree(UObject* Outer, float Duration)
{
	UStateTree* StateTree = NewObject<UStateTree>(Outer);
	UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(StateTree);
	StateTree->EditorData = EditorData;
	EditorData->Schema = NewObject<UArcAbilityStateTreeSchema>();

	UStateTreeState& Root = EditorData->AddSubTree(FName(TEXT("Root")));
	Root.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

	TStateTreeEditorNode<FArcAbilityTask_WaitDuration>& WaitTask =
		Root.AddTask<FArcAbilityTask_WaitDuration>();
	WaitTask.GetInstanceData().Duration = Duration;

	FStateTreeCompilerLog Log;
	FStateTreeCompiler Compiler(Log);
	check(Compiler.Compile(*StateTree));
	return StateTree;
}

UStateTree* CreateHeldDurationStateTree(UObject* Outer, FGameplayTag InputTag, float MinHeldDuration)
{
	UStateTree* StateTree = NewObject<UStateTree>(Outer);
	UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(StateTree);
	StateTree->EditorData = EditorData;
	EditorData->Schema = NewObject<UArcAbilityStateTreeSchema>();

	UStateTreeState& Root = EditorData->AddSubTree(FName(TEXT("Root")));
	Root.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

	TStateTreeEditorNode<FArcAbilityTask_ListenInputHeld>& HeldTask =
		Root.AddTask<FArcAbilityTask_ListenInputHeld>();
	HeldTask.GetInstanceData().InputTag = InputTag;
	HeldTask.GetInstanceData().MinHeldDuration = MinHeldDuration;

	FStateTreeCompilerLog Log;
	FStateTreeCompiler Compiler(Log);
	check(Compiler.Compile(*StateTree));
	return StateTree;
}

} // namespace ArcAbilityTestHelpers

// ============================================================================
// ArcAbility_Functions — Grant, Find, Remove, Input
// ============================================================================

TEST_CLASS(ArcAbility_Functions, "ArcMassAbilities.Abilities.Functions")
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

	TEST_METHOD(GrantAbility_ReturnsValidHandle)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();

		FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ASSERT_THAT(IsTrue(Handle.IsValid()));
	}

	TEST_METHOD(GrantAbility_AddsToCollection)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(AreEqual(1, Collection.Abilities.Num()));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].Definition == Def));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].InputTag == TAG_AbilTest_Input_Fire));
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
	}

	TEST_METHOD(FindAbility_ReturnsGrantedAbility)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();

		FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		const FArcActiveAbility* Found = ArcAbilities::FindAbility(*EntityManager, Entity, Handle);
		ASSERT_THAT(IsNotNull(Found));
		ASSERT_THAT(IsTrue(Found->Definition == Def));
	}

	TEST_METHOD(FindAbility_InvalidHandle_ReturnsNull)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		FArcAbilityHandle BadHandle = FArcAbilityHandle::Make(99, 99);
		const FArcActiveAbility* Found = ArcAbilities::FindAbility(*EntityManager, Entity, BadHandle);
		ASSERT_THAT(IsNull(Found));
	}

	TEST_METHOD(PushInput_AddsToPressedThisFrame)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);

		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityInputFragment& Input = View.GetFragmentData<FArcAbilityInputFragment>();
		ASSERT_THAT(AreEqual(1, Input.PressedThisFrame.Num()));
		ASSERT_THAT(IsTrue(Input.PressedThisFrame[0] == TAG_AbilTest_Input_Fire));
		ASSERT_THAT(IsTrue(Input.HeldInputs.HasTag(TAG_AbilTest_Input_Fire)));
	}

	TEST_METHOD(ReleaseInput_AddsToReleasedThisFrame)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);

		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);
		ArcAbilities::ReleaseInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityInputFragment& Input = View.GetFragmentData<FArcAbilityInputFragment>();
		ASSERT_THAT(AreEqual(1, Input.ReleasedThisFrame.Num()));
		ASSERT_THAT(IsFalse(Input.HeldInputs.HasTag(TAG_AbilTest_Input_Fire)));
	}

	TEST_METHOD(TryActivateAbility_PushesInputForAbility)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();

		FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		bool bResult = ArcAbilities::TryActivateAbility(*EntityManager, Entity, Handle);
		ASSERT_THAT(IsTrue(bResult));

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityInputFragment& Input = View.GetFragmentData<FArcAbilityInputFragment>();
		ASSERT_THAT(AreEqual(1, Input.PressedThisFrame.Num()));
	}

	TEST_METHOD(IsAbilityActive_FalseBeforeActivation)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();

		FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ASSERT_THAT(IsFalse(ArcAbilities::IsAbilityActive(*EntityManager, Entity, Handle)));
	}

	TEST_METHOD(RemoveAbility_RemovesFromCollection)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();

		UArcAbilityStateTreeSubsystem* Subsystem = Spawner.GetWorld().GetSubsystem<UArcAbilityStateTreeSubsystem>();
		check(Subsystem);

		FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::RemoveAbility(*EntityManager, Entity, Handle, *Subsystem);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(AreEqual(0, Collection.Abilities.Num()));
	}
};

// ============================================================================
// ArcAbility_Cost — InstantModifier cost checking and application
// ============================================================================

TEST_CLASS(ArcAbility_Cost, "ArcMassAbilities.Abilities.Cost")
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

	TEST_METHOD(CheckCost_EnoughResource_ReturnsTrue)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager, 100.f);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);

		FArcMassAbilityCost_InstantModifier Cost;
		Cost.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Cost.Amount = FScalableFloat(30.f);

		FArcMassAbilityCostContext Ctx{*EntityManager, Entity, Handle, Def};
		ASSERT_THAT(IsTrue(Cost.CheckCost(Ctx)));
	}

	TEST_METHOD(CheckCost_NotEnoughResource_ReturnsFalse)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager, 20.f);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);

		FArcMassAbilityCost_InstantModifier Cost;
		Cost.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Cost.Amount = FScalableFloat(30.f);

		FArcMassAbilityCostContext Ctx{*EntityManager, Entity, Handle, Def};
		ASSERT_THAT(IsFalse(Cost.CheckCost(Ctx)));
	}

	TEST_METHOD(ApplyCost_DeductsFromAttribute)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager, 100.f);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);

		FArcMassAbilityCost_InstantModifier Cost;
		Cost.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Cost.Amount = FScalableFloat(25.f);

		FArcMassAbilityCostContext Ctx{*EntityManager, Entity, Handle, Def};
		Cost.ApplyCost(Ctx);
		DrainPendingOps(Entity);

		FMassEntityView View(*EntityManager, Entity);
		FArcTestStatsFragment* Stats = View.GetFragmentDataPtr<FArcTestStatsFragment>();
		ASSERT_THAT(IsNear(75.f, Stats->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(CheckCost_ExactAmount_ReturnsTrue)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager, 50.f);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);

		FArcMassAbilityCost_InstantModifier Cost;
		Cost.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		Cost.Amount = FScalableFloat(50.f);

		FArcMassAbilityCostContext Ctx{*EntityManager, Entity, Handle, Def};
		ASSERT_THAT(IsTrue(Cost.CheckCost(Ctx)));
	}
};

// ============================================================================
// ArcAbility_Cooldown — Duration cooldown check and application
// ============================================================================

TEST_CLASS(ArcAbility_Cooldown, "ArcMassAbilities.Abilities.Cooldown")
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

	TEST_METHOD(CheckCooldown_NoCooldownActive_ReturnsTrue)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		FArcAbilityHandle Handle = FArcAbilityHandle::Make(0, 0);

		FArcAbilityCooldown_Duration Cooldown;
		Cooldown.CooldownDuration = FScalableFloat(5.f);

		ASSERT_THAT(IsTrue(Cooldown.CheckCooldown(*EntityManager, Entity, Handle)));
	}

	TEST_METHOD(ApplyCooldown_AddsCooldownEntry)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		FArcAbilityHandle Handle = FArcAbilityHandle::Make(0, 0);

		FArcAbilityCooldown_Duration Cooldown;
		Cooldown.CooldownDuration = FScalableFloat(5.f);

		Cooldown.ApplyCooldown(*EntityManager, Entity, Handle);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCooldownFragment* Frag = View.GetFragmentDataPtr<FArcAbilityCooldownFragment>();
		ASSERT_THAT(AreEqual(1, Frag->ActiveCooldowns.Num()));
		ASSERT_THAT(IsNear(5.f, Frag->ActiveCooldowns[0].RemainingTime, 0.001f));
		ASSERT_THAT(IsNear(5.f, Frag->ActiveCooldowns[0].MaxCooldownTime, 0.001f));
	}

	TEST_METHOD(CheckCooldown_AfterApply_ReturnsFalse)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		FArcAbilityHandle Handle = FArcAbilityHandle::Make(0, 0);

		FArcAbilityCooldown_Duration Cooldown;
		Cooldown.CooldownDuration = FScalableFloat(5.f);

		Cooldown.ApplyCooldown(*EntityManager, Entity, Handle);
		ASSERT_THAT(IsFalse(Cooldown.CheckCooldown(*EntityManager, Entity, Handle)));
	}

	TEST_METHOD(CheckCooldown_DifferentHandle_ReturnsTrue)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		FArcAbilityHandle Handle0 = FArcAbilityHandle::Make(0, 0);
		FArcAbilityHandle Handle1 = FArcAbilityHandle::Make(1, 0);

		FArcAbilityCooldown_Duration Cooldown;
		Cooldown.CooldownDuration = FScalableFloat(5.f);

		Cooldown.ApplyCooldown(*EntityManager, Entity, Handle0);
		ASSERT_THAT(IsTrue(Cooldown.CheckCooldown(*EntityManager, Entity, Handle1)));
	}

	TEST_METHOD(CooldownExpires_ManualDecrement)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		FArcAbilityHandle Handle = FArcAbilityHandle::Make(0, 0);

		FArcAbilityCooldown_Duration Cooldown;
		Cooldown.CooldownDuration = FScalableFloat(2.f);

		Cooldown.ApplyCooldown(*EntityManager, Entity, Handle);

		FMassEntityView View(*EntityManager, Entity);
		FArcAbilityCooldownFragment* Frag = View.GetFragmentDataPtr<FArcAbilityCooldownFragment>();
		Frag->ActiveCooldowns[0].RemainingTime = 0.f;
		Frag->ActiveCooldowns.RemoveAtSwap(0);

		ASSERT_THAT(IsTrue(Cooldown.CheckCooldown(*EntityManager, Entity, Handle)));
	}
};

// ============================================================================
// ArcAbility_Processor — Full activation pipeline
// ============================================================================

TEST_CLASS(ArcAbility_Processor, "ArcMassAbilities.Abilities.Processor")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcAbilityStateTreeSubsystem* AbilitySubsystem = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogStateTree");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogMass");
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();
		SignalSubsystem = Spawner.GetWorld().GetSubsystem<UMassSignalSubsystem>();
		check(SignalSubsystem);
		AbilitySubsystem = Spawner.GetWorld().GetSubsystem<UArcAbilityStateTreeSubsystem>();
	}

	void ExecuteInputProcessor(FMassEntityHandle Entity, float DeltaTime = 0.016f)
	{
		UArcAbilityInputProcessor* Processor = NewObject<UArcAbilityInputProcessor>();
		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		Processor->CallInitialize(&Spawner.GetWorld(), SharedEM);

		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityInputReceived, Entity);

		UE::Mass::FProcessingContext ProcessingContext(*EntityManager, DeltaTime);
		FMassExecutionContext& Context = ProcessingContext.GetExecutionContext();
		Processor->CallExecute(*EntityManager, Context);
	}

	TEST_METHOD(InputActivation_NoStateTree_ActivatesAbility)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));
	}

	TEST_METHOD(ActivationRequiredTags_Missing_DoesNotActivate)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->ActivationRequiredTags.AddTag(TAG_AbilTest_Req_Grounded);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
	}

	TEST_METHOD(ActivationRequiredTags_Present_Activates)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->ActivationRequiredTags.AddTag(TAG_AbilTest_Req_Grounded);

		FMassEntityView View(*EntityManager, Entity);
		FArcOwnedTagsFragment& OwnedTags = View.GetFragmentData<FArcOwnedTagsFragment>();
		OwnedTags.Tags.AddTag(TAG_AbilTest_Req_Grounded);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		ExecuteInputProcessor(Entity);

		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));
	}

	TEST_METHOD(ActivationBlockedTags_Present_DoesNotActivate)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->ActivationBlockedTags.AddTag(TAG_AbilTest_Block_Stunned);

		FMassEntityView View(*EntityManager, Entity);
		FArcOwnedTagsFragment& OwnedTags = View.GetFragmentData<FArcOwnedTagsFragment>();
		OwnedTags.Tags.AddTag(TAG_AbilTest_Block_Stunned);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		ExecuteInputProcessor(Entity);

		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
	}

	TEST_METHOD(ActivationBlockedTags_NotPresent_Activates)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->ActivationBlockedTags.AddTag(TAG_AbilTest_Block_Stunned);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));
	}

	TEST_METHOD(GrantTagsWhileActive_AppliedOnActivation)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->GrantTagsWhileActive.AddTag(TAG_AbilTest_State_Firing);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcOwnedTagsFragment& OwnedTags = View.GetFragmentData<FArcOwnedTagsFragment>();
		ASSERT_THAT(IsTrue(OwnedTags.Tags.HasTag(TAG_AbilTest_State_Firing)));
	}

	TEST_METHOD(BlockAbilitiesWithTags_BlocksOtherAbility)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);

		FGameplayTagContainer FireTags;
		FireTags.AddTag(TAG_AbilTest_Ability_Fire);
		UArcAbilityDefinition* FireDef = ArcAbilityTestHelpers::CreateSimpleAbilityDef(FireTags);
		FireDef->BlockAbilitiesWithTags.AddTag(TAG_AbilTest_Ability_Dodge);

		FGameplayTagContainer DodgeTags;
		DodgeTags.AddTag(TAG_AbilTest_Ability_Dodge);
		UArcAbilityDefinition* DodgeDef = ArcAbilityTestHelpers::CreateSimpleAbilityDef(DodgeTags);

		ArcAbilities::GrantAbility(*EntityManager, Entity, FireDef, TAG_AbilTest_Input_Fire);
		ArcAbilities::GrantAbility(*EntityManager, Entity, DodgeDef, TAG_AbilTest_Input_Dodge);

		// Activate fire first
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);
		ExecuteInputProcessor(Entity);

		// Try to activate dodge while fire blocks it
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Dodge);
		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsFalse(Collection.Abilities[1].bIsActive));
	}

	TEST_METHOD(CancelAbilitiesWithTags_CancelsActiveAbility)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);

		FGameplayTagContainer FireTags;
		FireTags.AddTag(TAG_AbilTest_Ability_Fire);
		UArcAbilityDefinition* FireDef = ArcAbilityTestHelpers::CreateSimpleAbilityDef(FireTags);
		FireDef->GrantTagsWhileActive.AddTag(TAG_AbilTest_State_Firing);

		FGameplayTagContainer DodgeTags;
		DodgeTags.AddTag(TAG_AbilTest_Ability_Dodge);
		UArcAbilityDefinition* DodgeDef = ArcAbilityTestHelpers::CreateSimpleAbilityDef(DodgeTags);
		DodgeDef->CancelAbilitiesWithTags.AddTag(TAG_AbilTest_Ability_Fire);

		ArcAbilities::GrantAbility(*EntityManager, Entity, FireDef, TAG_AbilTest_Input_Fire);
		ArcAbilities::GrantAbility(*EntityManager, Entity, DodgeDef, TAG_AbilTest_Input_Dodge);

		// Activate fire first
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);
		ExecuteInputProcessor(Entity);

		// Dodge cancels fire
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Dodge);
		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[1].bIsActive));

		const FArcOwnedTagsFragment& OwnedTags = View.GetFragmentData<FArcOwnedTagsFragment>();
		ASSERT_THAT(IsFalse(OwnedTags.Tags.HasTag(TAG_AbilTest_State_Firing)));
	}

	TEST_METHOD(CostCheck_FailsActivation_WhenInsufficientResource)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager, 10.f);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();

		FArcMassAbilityCost_InstantModifier CostData;
		CostData.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		CostData.Amount = FScalableFloat(50.f);
		Def->Cost = FInstancedStruct::Make(CostData);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
	}

	TEST_METHOD(CostCheck_PassesActivation_WhenSufficientResource)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager, 100.f);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();

		FArcMassAbilityCost_InstantModifier CostData;
		CostData.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		CostData.Amount = FScalableFloat(30.f);
		Def->Cost = FInstancedStruct::Make(CostData);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));
	}

	TEST_METHOD(CooldownCheck_FailsActivation_WhenOnCooldown)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();

		FArcAbilityCooldown_Duration CooldownData;
		CooldownData.CooldownDuration = FScalableFloat(5.f);
		Def->Cooldown = FInstancedStruct::Make(CooldownData);

		FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);

		// Manually add cooldown entry
		FMassEntityView View(*EntityManager, Entity);
		FArcAbilityCooldownFragment& CdFrag = View.GetFragmentData<FArcAbilityCooldownFragment>();
		FArcCooldownEntry& Entry = CdFrag.ActiveCooldowns.AddDefaulted_GetRef();
		Entry.AbilityHandle = Handle;
		Entry.RemainingTime = 3.f;
		Entry.MaxCooldownTime = 5.f;

		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		ExecuteInputProcessor(Entity);

		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
	}

	TEST_METHOD(InputCleanup_ClearsPressedAfterExecution)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityInputFragment& Input = View.GetFragmentData<FArcAbilityInputFragment>();
		ASSERT_THAT(AreEqual(0, Input.PressedThisFrame.Num()));
		ASSERT_THAT(AreEqual(0, Input.ReleasedThisFrame.Num()));
	}

	TEST_METHOD(AlreadyActiveAbility_DoesNotReactivate)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);
		ExecuteInputProcessor(Entity);

		// Push same input again
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);
		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));
	}

	TEST_METHOD(NoMatchingInput_DoesNotActivate)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Dodge);

		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
	}
};

// ============================================================================
// ArcAbility_CooldownProcessor — Cooldown tick-down
// ============================================================================

TEST_CLASS(ArcAbility_CooldownProcessor, "ArcMassAbilities.Abilities.CooldownProcessor")
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

	void ExecuteCooldownProcessor(float DeltaTime)
	{
		UArcCooldownProcessor* Processor = NewObject<UArcCooldownProcessor>();
		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		Processor->CallInitialize(&Spawner.GetWorld(), SharedEM);

		UE::Mass::FProcessingContext ProcessingContext(*EntityManager, DeltaTime);
		FMassExecutionContext& Context = ProcessingContext.GetExecutionContext();
		Processor->CallExecute(*EntityManager, Context);
	}

	TEST_METHOD(DecrementsCooldownRemainingTime)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);

		FMassEntityView View(*EntityManager, Entity);
		FArcAbilityCooldownFragment& CdFrag = View.GetFragmentData<FArcAbilityCooldownFragment>();
		FArcCooldownEntry& Entry = CdFrag.ActiveCooldowns.AddDefaulted_GetRef();
		Entry.AbilityHandle = FArcAbilityHandle::Make(0, 0);
		Entry.RemainingTime = 5.f;
		Entry.MaxCooldownTime = 5.f;

		ExecuteCooldownProcessor(1.f);

		ASSERT_THAT(IsNear(4.f, CdFrag.ActiveCooldowns[0].RemainingTime, 0.001f));
	}

	TEST_METHOD(RemovesExpiredCooldowns)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);

		FMassEntityView View(*EntityManager, Entity);
		FArcAbilityCooldownFragment& CdFrag = View.GetFragmentData<FArcAbilityCooldownFragment>();
		FArcCooldownEntry& Entry = CdFrag.ActiveCooldowns.AddDefaulted_GetRef();
		Entry.AbilityHandle = FArcAbilityHandle::Make(0, 0);
		Entry.RemainingTime = 0.5f;
		Entry.MaxCooldownTime = 5.f;

		ExecuteCooldownProcessor(1.f);

		ASSERT_THAT(AreEqual(0, CdFrag.ActiveCooldowns.Num()));
	}

	TEST_METHOD(MultipleCooldowns_IndependentTracking)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);

		FMassEntityView View(*EntityManager, Entity);
		FArcAbilityCooldownFragment& CdFrag = View.GetFragmentData<FArcAbilityCooldownFragment>();

		FArcCooldownEntry& Entry0 = CdFrag.ActiveCooldowns.AddDefaulted_GetRef();
		Entry0.AbilityHandle = FArcAbilityHandle::Make(0, 0);
		Entry0.RemainingTime = 2.f;
		Entry0.MaxCooldownTime = 2.f;

		FArcCooldownEntry& Entry1 = CdFrag.ActiveCooldowns.AddDefaulted_GetRef();
		Entry1.AbilityHandle = FArcAbilityHandle::Make(1, 0);
		Entry1.RemainingTime = 5.f;
		Entry1.MaxCooldownTime = 5.f;

		ExecuteCooldownProcessor(3.f);

		ASSERT_THAT(AreEqual(1, CdFrag.ActiveCooldowns.Num()));
		ASSERT_THAT(IsNear(2.f, CdFrag.ActiveCooldowns[0].RemainingTime, 0.001f));
	}
};

// ============================================================================
// ArcAbility_Integration — Full pipeline with compiled StateTree
// ============================================================================

TEST_CLASS(ArcAbility_Integration, "ArcMassAbilities.Abilities.Integration")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcAbilityStateTreeSubsystem* AbilitySubsystem = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogStateTree");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogMass");
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();
		SignalSubsystem = Spawner.GetWorld().GetSubsystem<UMassSignalSubsystem>();
		check(SignalSubsystem);
		AbilitySubsystem = Spawner.GetWorld().GetSubsystem<UArcAbilityStateTreeSubsystem>();
	}

	void ExecuteInputProcessor(FMassEntityHandle Entity, float DeltaTime = 0.016f)
	{
		UArcAbilityInputProcessor* Processor = NewObject<UArcAbilityInputProcessor>();
		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		Processor->CallInitialize(&Spawner.GetWorld(), SharedEM);

		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityInputReceived, Entity);

		UE::Mass::FProcessingContext ProcessingContext(*EntityManager, DeltaTime);
		FMassExecutionContext& Context = ProcessingContext.GetExecutionContext();
		Processor->CallExecute(*EntityManager, Context);
	}

	TEST_METHOD(EmptyStateTree_CompletesInOneFrame)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->StateTree = ArcAbilityTestHelpers::CreateEmptyStateTree(&Spawner.GetWorld());
		Def->GrantTagsWhileActive.AddTag(TAG_AbilTest_State_Firing);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		// Empty tree: Start → Running, Tick → Succeeded, Completion → deactivate, all in one frame
		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Succeeded));

		const FArcOwnedTagsFragment& OwnedTags = View.GetFragmentData<FArcOwnedTagsFragment>();
		ASSERT_THAT(IsFalse(OwnedTags.Tags.HasTag(TAG_AbilTest_State_Firing)));
	}

	TEST_METHOD(StateTree_CostGateBlocksActivation)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager, 10.f);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->StateTree = ArcAbilityTestHelpers::CreateEmptyStateTree(&Spawner.GetWorld());

		FArcMassAbilityCost_InstantModifier CostData;
		CostData.Attribute = FArcTestStatsFragment::GetHealthAttribute();
		CostData.Amount = FScalableFloat(50.f);
		Def->Cost = FInstancedStruct::Make(CostData);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));

		FArcTestStatsFragment* Stats = View.GetFragmentDataPtr<FArcTestStatsFragment>();
		ASSERT_THAT(IsNear(10.f, Stats->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(StateTree_CooldownBlocksReactivation)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->StateTree = ArcAbilityTestHelpers::CreateEmptyStateTree(&Spawner.GetWorld());

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		// Activate and complete in one frame
		ExecuteInputProcessor(Entity);

		// Manually add cooldown (simulating what Commit task would do)
		FMassEntityView View(*EntityManager, Entity);
		FArcAbilityCooldownFragment& CdFrag = View.GetFragmentData<FArcAbilityCooldownFragment>();
		FArcCooldownEntry& Entry = CdFrag.ActiveCooldowns.AddDefaulted_GetRef();
		Entry.AbilityHandle = View.GetFragmentData<FArcAbilityCollectionFragment>().Abilities[0].Handle;
		Entry.RemainingTime = 5.f;
		Entry.MaxCooldownTime = 5.f;

		// Try to reactivate while on cooldown
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);
		ExecuteInputProcessor(Entity);

		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
	}

	TEST_METHOD(FullLifecycle_ActivateTickCompleteRevokeTags)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->StateTree = ArcAbilityTestHelpers::CreateEmptyStateTree(&Spawner.GetWorld());
		Def->GrantTagsWhileActive.AddTag(TAG_AbilTest_State_Firing);
		Def->GrantTagsWhileActive.AddTag(TAG_AbilTest_State_Blocking);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		// Empty tree completes in one frame: activate → start → tick → succeed → complete → revoke
		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcOwnedTagsFragment& Tags = View.GetFragmentData<FArcOwnedTagsFragment>();
		ASSERT_THAT(IsFalse(Tags.Tags.HasTag(TAG_AbilTest_State_Firing)));
		ASSERT_THAT(IsFalse(Tags.Tags.HasTag(TAG_AbilTest_State_Blocking)));

		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Succeeded));
	}
};

// ============================================================================
// ArcAbility_InputConditions — IsInputHeld, InputState
// ============================================================================

TEST_CLASS(ArcAbility_InputConditions, "ArcMassAbilities.Abilities.InputConditions")
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

	TEST_METHOD(IsInputHeld_ReturnsTrueWhenHeld)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityInputFragment& Input = View.GetFragmentData<FArcAbilityInputFragment>();
		ASSERT_THAT(IsTrue(Input.HeldInputs.HasTag(TAG_AbilTest_Input_Fire)));
	}

	TEST_METHOD(IsInputHeld_ReturnsFalseWhenNotHeld)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityInputFragment& Input = View.GetFragmentData<FArcAbilityInputFragment>();
		ASSERT_THAT(IsFalse(Input.HeldInputs.HasTag(TAG_AbilTest_Input_Fire)));
	}

	TEST_METHOD(IsInputHeld_ReturnsFalseAfterRelease)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);
		ArcAbilities::ReleaseInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityInputFragment& Input = View.GetFragmentData<FArcAbilityInputFragment>();
		ASSERT_THAT(IsFalse(Input.HeldInputs.HasTag(TAG_AbilTest_Input_Fire)));
	}

	TEST_METHOD(InputState_PressedThisFrame)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityInputFragment& Input = View.GetFragmentData<FArcAbilityInputFragment>();
		ASSERT_THAT(IsTrue(Input.PressedThisFrame.Contains(TAG_AbilTest_Input_Fire)));
		ASSERT_THAT(IsFalse(Input.ReleasedThisFrame.Contains(TAG_AbilTest_Input_Fire)));
	}

	TEST_METHOD(InputState_ReleasedThisFrame)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);
		ArcAbilities::ReleaseInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityInputFragment& Input = View.GetFragmentData<FArcAbilityInputFragment>();
		ASSERT_THAT(IsTrue(Input.ReleasedThisFrame.Contains(TAG_AbilTest_Input_Fire)));
	}

	TEST_METHOD(InputState_NoneWhenNoInput)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityInputFragment& Input = View.GetFragmentData<FArcAbilityInputFragment>();
		ASSERT_THAT(IsFalse(Input.PressedThisFrame.Contains(TAG_AbilTest_Input_Fire)));
		ASSERT_THAT(IsFalse(Input.ReleasedThisFrame.Contains(TAG_AbilTest_Input_Fire)));
		ASSERT_THAT(IsFalse(Input.HeldInputs.HasTag(TAG_AbilTest_Input_Fire)));
	}
};

// ============================================================================
// ArcAbility_SignalDriven — Signal-driven behavior validation
// ============================================================================

TEST_CLASS(ArcAbility_SignalDriven, "ArcMassAbilities.Abilities.SignalDriven")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UMassSignalSubsystem* SignalSubsystem = nullptr;
	UArcAbilityStateTreeSubsystem* AbilitySubsystem = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogStateTree");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogMass");
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();
		SignalSubsystem = Spawner.GetWorld().GetSubsystem<UMassSignalSubsystem>();
		check(SignalSubsystem);
		AbilitySubsystem = Spawner.GetWorld().GetSubsystem<UArcAbilityStateTreeSubsystem>();
	}

	void ExecuteInputProcessor(FMassEntityHandle Entity, float DeltaTime = 0.016f)
	{
		UArcAbilityInputProcessor* Processor = NewObject<UArcAbilityInputProcessor>();
		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		Processor->CallInitialize(&Spawner.GetWorld(), SharedEM);

		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityInputReceived, Entity);

		UE::Mass::FProcessingContext ProcessingContext(*EntityManager, DeltaTime);
		FMassExecutionContext& Context = ProcessingContext.GetExecutionContext();
		Processor->CallExecute(*EntityManager, Context);
	}

	TEST_METHOD(NoSignal_EntityNotProcessed)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);

		FMassEntityView View(*EntityManager, Entity);
		FArcAbilityInputFragment& Input = View.GetFragmentData<FArcAbilityInputFragment>();
		Input.PressedThisFrame.Add(TAG_AbilTest_Input_Fire);

		UArcAbilityInputProcessor* Processor = NewObject<UArcAbilityInputProcessor>();
		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		Processor->CallInitialize(&Spawner.GetWorld(), SharedEM);

		UE::Mass::FProcessingContext ProcessingContext(*EntityManager, 0.016f);
		Processor->CallExecute(*EntityManager, ProcessingContext.GetExecutionContext());

		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
	}

	TEST_METHOD(TreeDormancy_NoSignal_ActiveTreeNotTicked)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));

		UArcAbilityStateTreeTickProcessor* TickProcessor = NewObject<UArcAbilityStateTreeTickProcessor>();
		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		TickProcessor->CallInitialize(&Spawner.GetWorld(), SharedEM);

		UE::Mass::FProcessingContext ProcessingContext(*EntityManager, 0.016f);
		TickProcessor->CallExecute(*EntityManager, ProcessingContext.GetExecutionContext());

		ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));
	}

	TEST_METHOD(InstantTree_CompletesInInputProcessor)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->StateTree = ArcAbilityTestHelpers::CreateEmptyStateTree(&Spawner.GetWorld());
		Def->GrantTagsWhileActive.AddTag(TAG_AbilTest_State_Firing);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Succeeded));

		const FArcOwnedTagsFragment& OwnedTags = View.GetFragmentData<FArcOwnedTagsFragment>();
		ASSERT_THAT(IsFalse(OwnedTags.Tags.HasTag(TAG_AbilTest_State_Firing)));
	}

	void ExecuteTickProcessor(FMassEntityHandle Entity, float DeltaTime = 0.016f)
	{
		UArcAbilityStateTreeTickProcessor* Processor = NewObject<UArcAbilityStateTreeTickProcessor>();
		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		Processor->CallInitialize(&Spawner.GetWorld(), SharedEM);

		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity);

		UE::Mass::FProcessingContext ProcessingContext(*EntityManager, DeltaTime);
		FMassExecutionContext& Context = ProcessingContext.GetExecutionContext();
		Processor->CallExecute(*EntityManager, Context);
	}

	TEST_METHOD(InputPressed_SignalDrivenTransition)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->StateTree = ArcAbilityTestHelpers::CreateInputPressedStateTree(&Spawner.GetWorld(), TAG_AbilTest_Input_Dodge);
		Def->GrantTagsWhileActive.AddTag(TAG_AbilTest_State_Firing);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);
		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Running));

		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Dodge);
		ExecuteTickProcessor(Entity);

		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Succeeded));

		const FArcOwnedTagsFragment& OwnedTags = View.GetFragmentData<FArcOwnedTagsFragment>();
		ASSERT_THAT(IsFalse(OwnedTags.Tags.HasTag(TAG_AbilTest_State_Firing)));
	}

	TEST_METHOD(InputReleased_SignalDrivenTransition)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->StateTree = ArcAbilityTestHelpers::CreateInputReleasedStateTree(&Spawner.GetWorld(), TAG_AbilTest_Input_Fire);
		Def->GrantTagsWhileActive.AddTag(TAG_AbilTest_State_Firing);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);
		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Running));

		ArcAbilities::ReleaseInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);
		ExecuteTickProcessor(Entity);

		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Succeeded));

		const FArcOwnedTagsFragment& OwnedTags = View.GetFragmentData<FArcOwnedTagsFragment>();
		ASSERT_THAT(IsFalse(OwnedTags.Tags.HasTag(TAG_AbilTest_State_Firing)));
	}

	void AdvanceWaitTime(FMassEntityHandle Entity, float DeltaTime)
	{
		EntityManager->FlushCommands();

		FStructView WaitView = EntityManager->GetMutableSparseElementDataForEntity(
			FArcAbilityWaitFragment::StaticStruct(), Entity);
		FArcAbilityWaitFragment* WaitFrag = WaitView.GetPtr<FArcAbilityWaitFragment>();
		if (WaitFrag)
		{
			WaitFrag->ElapsedTime += DeltaTime;
			if (WaitFrag->ElapsedTime >= WaitFrag->Duration)
			{
				SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity);
			}
		}
	}

	void AdvanceHeldTime(FMassEntityHandle Entity, float DeltaTime)
	{
		EntityManager->FlushCommands();

		FStructView HeldView = EntityManager->GetMutableSparseElementDataForEntity(
			FArcAbilityHeldFragment::StaticStruct(), Entity);
		FArcAbilityHeldFragment* HeldFrag = HeldView.GetPtr<FArcAbilityHeldFragment>();
		if (HeldFrag)
		{
			FMassEntityView EntityView(*EntityManager, Entity);
			const FArcAbilityInputFragment* InputFrag = EntityView.GetFragmentDataPtr<FArcAbilityInputFragment>();
			if (InputFrag)
			{
				HeldFrag->bIsHeld = InputFrag->HeldInputs.HasTag(HeldFrag->InputTag);
			}

			if (HeldFrag->bIsHeld)
			{
				HeldFrag->HeldDuration += DeltaTime;
			}

			if (!HeldFrag->bThresholdReached && HeldFrag->MinHeldDuration > 0.f
				&& HeldFrag->HeldDuration >= HeldFrag->MinHeldDuration)
			{
				HeldFrag->bThresholdReached = true;
				SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity);
			}
		}
	}

	TEST_METHOD(WaitDuration_CompletesAfterTime)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->StateTree = ArcAbilityTestHelpers::CreateWaitDurationStateTree(&Spawner.GetWorld(), 1.0f);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);
		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Running));

		AdvanceWaitTime(Entity, 0.5f);
		ExecuteTickProcessor(Entity);
		ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));

		AdvanceWaitTime(Entity, 0.6f);
		ExecuteTickProcessor(Entity);
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Succeeded));
	}

	TEST_METHOD(InputHeld_CompletesAfterMinDuration)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->StateTree = ArcAbilityTestHelpers::CreateHeldDurationStateTree(&Spawner.GetWorld(), TAG_AbilTest_Input_Fire, 1.0f);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);
		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = View.GetFragmentData<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Running));

		AdvanceHeldTime(Entity, 0.5f);
		ExecuteTickProcessor(Entity);
		ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));

		AdvanceHeldTime(Entity, 0.6f);
		ExecuteTickProcessor(Entity);
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Succeeded));
	}
};
