// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "ArcAbilityCostTestTypes.h"
#include "ArcCooldownExtensibilityTestTypes.h"
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
#include "Events/ArcAbilityEventPayloads.h"
#include "Events/ArcGameEvent.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Events/ArcGameEventTags.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
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
#include "Abilities/Tasks/ArcAbilityTask_InputPressed.h"
#include "Abilities/Tasks/ArcAbilityTask_InputReleased.h"
#include "Abilities/Tasks/ArcAbilityTask_InputReleasedOnce.h"
#include "Abilities/Tasks/ArcAbilityTask_WaitDuration.h"
#include "StateTreeState.h"
#include "PropertyBindingPath.h"
#include "Abilities/Conditions/ArcAbilityCondition_InputState.h"

int32 FArcTestNonBlockingCooldown::OnExpiredCallCount = 0;

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AbilTest_Input_Fire, "Test.InputTag.Ability.Fire");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AbilTest_Input_Dodge, "Test.InputTag.Ability.Dodge");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AbilTest_Ability_Fire, "Test.Ability.Fire");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AbilTest_Ability_Dodge, "Test.Ability.Dodge");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AbilTest_Ability_Melee, "Test.Ability.Melee");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AbilTest_State_Firing, "Test.State.Firing");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AbilTest_State_Blocking, "Test.State.Blocking");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AbilTest_Req_Grounded, "Test.State.Grounded");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AbilTest_Block_Stunned, "Test.State.Stunned");

namespace ArcAbilityTestHelpers
{

FMassEntityHandle CreateAbilityTestEntity(FMassEntityManager& EntityManager, float HealthBase = 100.f)
{
	FArcAttributeSet_TestStats Stats;
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

	return ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(EntityManager, Instances);
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

UStateTree* CreateInstantStateTree(UObject* Outer)
{
	UStateTree* StateTree = NewObject<UStateTree>(Outer);
	UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(StateTree);
	StateTree->EditorData = EditorData;
	EditorData->Schema = NewObject<UArcAbilityStateTreeSchema>();

	UStateTreeState& Root = EditorData->AddSubTree(FName(TEXT("Root")));
	Root.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

	UStateTreeState& Done = Root.AddChildState(FName(TEXT("Done")));
	Done.AddTask<FArcTestInstantTask>();
	Done.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

	FStateTreeCompilerLog Log;
	FStateTreeCompiler Compiler(Log);
	const bool bCompiled = Compiler.Compile(*StateTree);
	ensureAlwaysMsgf(bCompiled, TEXT("CreateInstantStateTree failed to compile"));
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

	TStateTreeEditorNode<FArcAbilityTask_InputPressed>& ListenTask =
		WaitState.AddTask<FArcAbilityTask_InputPressed>();
	ListenTask.GetInstanceData().InputTag = InputTag;

	FStateTreeTransition& Transition = WaitState.AddTransition(
		EStateTreeTransitionTrigger::OnDelegate, EStateTreeTransitionType::GotoState, &DoneState);

	EditorData->AddPropertyBinding(
		FPropertyBindingPath(ListenTask.ID,
			GET_MEMBER_NAME_CHECKED(FArcAbilityTask_InputPressedInstanceData, OnInputPressed)),
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

	TStateTreeEditorNode<FArcAbilityTask_InputReleased>& ListenTask =
		WaitState.AddTask<FArcAbilityTask_InputReleased>();
	ListenTask.GetInstanceData().InputTag = InputTag;

	FStateTreeTransition& Transition = WaitState.AddTransition(
		EStateTreeTransitionTrigger::OnDelegate, EStateTreeTransitionType::GotoState, &DoneState);

	EditorData->AddPropertyBinding(
		FPropertyBindingPath(ListenTask.ID,
			GET_MEMBER_NAME_CHECKED(FArcAbilityTask_InputReleasedInstanceData, OnInputReleased)),
		FPropertyBindingPath(Transition.ID,
			GET_MEMBER_NAME_CHECKED(FStateTreeTransition, DelegateListener)));

	DoneState.AddTask<FArcTestInstantTask>();
	DoneState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

	FStateTreeCompilerLog Log;
	FStateTreeCompiler Compiler(Log);
	Compiler.Compile(*StateTree);
	return StateTree;
}

UStateTree* CreateInputReleasedOnceStateTree(UObject* Outer, FGameplayTag InputTag)
{
	UStateTree* StateTree = NewObject<UStateTree>(Outer);
	UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(StateTree);
	StateTree->EditorData = EditorData;
	EditorData->Schema = NewObject<UArcAbilityStateTreeSchema>();

	UStateTreeState& Root = EditorData->AddSubTree(FName(TEXT("Root")));
	Root.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

	UStateTreeState& WaitState = Root.AddChildState(FName(TEXT("WaitForRelease")));
	UStateTreeState& DoneState = Root.AddChildState(FName(TEXT("Done")));

	TStateTreeEditorNode<FArcAbilityTask_InputReleasedOnce>& ListenTask =
		WaitState.AddTask<FArcAbilityTask_InputReleasedOnce>();
	ListenTask.GetInstanceData().InputTag = InputTag;

	FStateTreeTransition& Transition = WaitState.AddTransition(
		EStateTreeTransitionTrigger::OnDelegate, EStateTreeTransitionType::GotoState, &DoneState);

	EditorData->AddPropertyBinding(
		FPropertyBindingPath(ListenTask.ID,
			GET_MEMBER_NAME_CHECKED(FArcAbilityTask_InputReleasedOnceInstanceData, OnInputReleased)),
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

// Tree whose single task runs forever and bumps FArcTestExitFlagTask::ExitStateCount in
// ExitState — used to assert cancel/remove paths Stop() the tree.
UStateTree* CreateExitFlagStateTree(UObject* Outer)
{
	UStateTree* StateTree = NewObject<UStateTree>(Outer);
	UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(StateTree);
	StateTree->EditorData = EditorData;
	EditorData->Schema = NewObject<UArcAbilityStateTreeSchema>();

	UStateTreeState& Root = EditorData->AddSubTree(FName(TEXT("Root")));
	Root.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);
	Root.AddTask<FArcTestExitFlagTask>();

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
		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
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
		const FArcAbilityInputFragment& Input = (*View.GetSparseFragmentDataPtr<FArcAbilityInputFragment>());
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
		const FArcAbilityInputFragment& Input = (*View.GetSparseFragmentDataPtr<FArcAbilityInputFragment>());
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
		const FArcAbilityInputFragment& Input = (*View.GetSparseFragmentDataPtr<FArcAbilityInputFragment>());
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
		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
		ASSERT_THAT(AreEqual(0, Collection.Abilities.Num()));
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
		Cooldown.CooldownDuration = FArcScalableFloat(5.f);

		ASSERT_THAT(IsTrue(Cooldown.CheckCooldown(*EntityManager, Entity, Handle)));
	}

	TEST_METHOD(ApplyCooldown_AddsCooldownEntry)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		FArcAbilityHandle Handle = FArcAbilityHandle::Make(0, 0);

		FArcAbilityCooldown_Duration Cooldown;
		Cooldown.CooldownDuration = FArcScalableFloat(5.f);

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
		Cooldown.CooldownDuration = FArcScalableFloat(5.f);

		Cooldown.ApplyCooldown(*EntityManager, Entity, Handle);
		ASSERT_THAT(IsFalse(Cooldown.CheckCooldown(*EntityManager, Entity, Handle)));
	}

	TEST_METHOD(CheckCooldown_DifferentHandle_ReturnsTrue)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		FArcAbilityHandle Handle0 = FArcAbilityHandle::Make(0, 0);
		FArcAbilityHandle Handle1 = FArcAbilityHandle::Make(1, 0);

		FArcAbilityCooldown_Duration Cooldown;
		Cooldown.CooldownDuration = FArcScalableFloat(5.f);

		Cooldown.ApplyCooldown(*EntityManager, Entity, Handle0);
		ASSERT_THAT(IsTrue(Cooldown.CheckCooldown(*EntityManager, Entity, Handle1)));
	}

	TEST_METHOD(CooldownExpires_ManualDecrement)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		FArcAbilityHandle Handle = FArcAbilityHandle::Make(0, 0);

		FArcAbilityCooldown_Duration Cooldown;
		Cooldown.CooldownDuration = FArcScalableFloat(2.f);

		Cooldown.ApplyCooldown(*EntityManager, Entity, Handle);

		FMassEntityView View(*EntityManager, Entity);
		FArcAbilityCooldownFragment* Frag = View.GetFragmentDataPtr<FArcAbilityCooldownFragment>();
		Frag->ActiveCooldowns[0].RemainingTime = 0.f;
		Frag->ActiveCooldowns.RemoveAtSwap(0);

		ASSERT_THAT(IsTrue(Cooldown.CheckCooldown(*EntityManager, Entity, Handle)));
	}

	// -------------------------------------------------------------------------
	// CheckCooldown out-param: active cooldown surfaces remaining / max seconds
	// -------------------------------------------------------------------------

	TEST_METHOD(CheckCooldown_ActiveCooldown_OutResultPopulated)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		FArcAbilityHandle Handle = FArcAbilityHandle::Make(0, 0);

		FArcAbilityCooldown_Duration Cooldown;
		Cooldown.CooldownDuration = FArcScalableFloat(5.f);
		Cooldown.ApplyCooldown(*EntityManager, Entity, Handle);

		FArcCooldownCheckResult CdResult;
		const bool bNotOnCooldown = Cooldown.CheckCooldown(*EntityManager, Entity, Handle, &CdResult);

		ASSERT_THAT(IsFalse(bNotOnCooldown));
		ASSERT_THAT(IsTrue(CdResult.bOnCooldown));
		ASSERT_THAT(IsNear(5.f, CdResult.RemainingSeconds, 0.001f));
		ASSERT_THAT(IsNear(5.f, CdResult.MaxCooldownSeconds, 0.001f));
	}

	TEST_METHOD(CheckCooldown_NoCooldownActive_OutResultZero)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		FArcAbilityHandle Handle = FArcAbilityHandle::Make(0, 0);

		FArcAbilityCooldown_Duration Cooldown;
		Cooldown.CooldownDuration = FArcScalableFloat(5.f);

		FArcCooldownCheckResult CdResult;
		const bool bNotOnCooldown = Cooldown.CheckCooldown(*EntityManager, Entity, Handle, &CdResult);

		ASSERT_THAT(IsTrue(bNotOnCooldown));
		ASSERT_THAT(IsFalse(CdResult.bOnCooldown));
		ASSERT_THAT(IsNear(0.f, CdResult.RemainingSeconds, 0.001f));
	}

	// -------------------------------------------------------------------------
	// ApplyCooldown out-param: surfaces the resolved duration
	// -------------------------------------------------------------------------

	TEST_METHOD(ApplyCooldown_OutDuration_MatchesResolvedDuration)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		FArcAbilityHandle Handle = FArcAbilityHandle::Make(0, 0);

		FArcAbilityCooldown_Duration Cooldown;
		Cooldown.CooldownDuration = FArcScalableFloat(7.f);

		float OutDuration = 0.f;
		Cooldown.ApplyCooldown(*EntityManager, Entity, Handle, &OutDuration);

		ASSERT_THAT(IsNear(7.f, OutDuration, 0.001f));
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
		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
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
		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
	}

	TEST_METHOD(ActivationRequiredTags_Present_Activates)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->ActivationRequiredTags.AddTag(TAG_AbilTest_Req_Grounded);

		FMassEntityView View(*EntityManager, Entity);
		FArcOwnedTagsFragment& OwnedTags = *View.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		OwnedTags.Tags.AddTag(TAG_AbilTest_Req_Grounded);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		ExecuteInputProcessor(Entity);

		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
		ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));
	}

	TEST_METHOD(ActivationBlockedTags_Present_DoesNotActivate)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->ActivationBlockedTags.AddTag(TAG_AbilTest_Block_Stunned);

		FMassEntityView View(*EntityManager, Entity);
		FArcOwnedTagsFragment& OwnedTags = *View.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		OwnedTags.Tags.AddTag(TAG_AbilTest_Block_Stunned);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		ExecuteInputProcessor(Entity);

		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
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
		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
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
		const FArcOwnedTagsFragment& OwnedTags = *View.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
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
		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
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
		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[1].bIsActive));

		const FArcOwnedTagsFragment& OwnedTags = *View.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		ASSERT_THAT(IsFalse(OwnedTags.Tags.HasTag(TAG_AbilTest_State_Firing)));
	}

	TEST_METHOD(CostCheck_FailsActivation_WhenInsufficientResource)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager, 10.f);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();

		FArcTestAttributeCost CostData;
		CostData.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		CostData.Amount = 50.f;
		Def->Cost = FInstancedStruct::Make(CostData);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
	}

	TEST_METHOD(CostCheck_PassesActivation_WhenSufficientResource)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager, 100.f);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();

		FArcTestAttributeCost CostData;
		CostData.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		CostData.Amount = 30.f;
		Def->Cost = FInstancedStruct::Make(CostData);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
		ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));
	}

	TEST_METHOD(CooldownCheck_FailsActivation_WhenOnCooldown)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();

		FArcAbilityCooldown_Duration CooldownData;
		CooldownData.CooldownDuration = FArcScalableFloat(5.f);
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

		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
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
		const FArcAbilityInputFragment& Input = (*View.GetSparseFragmentDataPtr<FArcAbilityInputFragment>());
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
		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
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
		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
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

	void ExecuteInputAndTickProcessors(FMassEntityHandle Entity, float DeltaTime = 0.016f)
	{
		ExecuteInputProcessor(Entity, DeltaTime);

		UArcAbilityStateTreeTickProcessor* TickProcessor = NewObject<UArcAbilityStateTreeTickProcessor>();
		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		TickProcessor->CallInitialize(&Spawner.GetWorld(), SharedEM);

		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity);

		UE::Mass::FProcessingContext Ctx(*EntityManager, DeltaTime);
		TickProcessor->CallExecute(*EntityManager, Ctx.GetExecutionContext());
	}

	TEST_METHOD(CancelAbility_StopsTree_RunsExitState)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->StateTree = ArcAbilityTestHelpers::CreateExitFlagStateTree(&Spawner.GetWorld());
		Def->GrantTagsWhileActive.AddTag(TAG_AbilTest_State_Firing);

		FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		FArcTestExitFlagTask::ExitStateCount = 0;
		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
		ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(AreEqual(0, FArcTestExitFlagTask::ExitStateCount));

		ArcAbilities::CancelAbility(*EntityManager, Entity, Handle, *AbilitySubsystem, EArcAbilityEndReason::Cancelled, FMassEntityHandle());

		// Cancel must Stop() the tree (running ExitState), free the instance data, and deactivate.
		ASSERT_THAT(AreEqual(1, FArcTestExitFlagTask::ExitStateCount));
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsFalse(Collection.Abilities[0].InstanceHandle.IsValid()));
		ASSERT_THAT(IsNull(AbilitySubsystem->GetInstanceData(Collection.Abilities[0].InstanceHandle)));
	}

	TEST_METHOD(RemoveAbility_ActiveAbility_RunsExitState)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->StateTree = ArcAbilityTestHelpers::CreateExitFlagStateTree(&Spawner.GetWorld());

		FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		FArcTestExitFlagTask::ExitStateCount = 0;
		ExecuteInputProcessor(Entity);

		ArcAbilities::RemoveAbility(*EntityManager, Entity, Handle, *AbilitySubsystem);

		ASSERT_THAT(AreEqual(1, FArcTestExitFlagTask::ExitStateCount));

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
		ASSERT_THAT(AreEqual(0, Collection.Abilities.Num()));
	}

	TEST_METHOD(InstantStateTree_CompletesInTwoPasses)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->StateTree = ArcAbilityTestHelpers::CreateInstantStateTree(&Spawner.GetWorld());
		Def->GrantTagsWhileActive.AddTag(TAG_AbilTest_State_Firing);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		// Instant tree: Start() returns Succeeded synchronously; the input processor tears down
		// the ability in the same pass (no tags granted, instance freed, bIsActive=false).
		// The tick processor call is a no-op — entity is already inactive.
		ExecuteInputAndTickProcessors(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Succeeded));

		const FArcOwnedTagsFragment& OwnedTags = *View.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		ASSERT_THAT(IsFalse(OwnedTags.Tags.HasTag(TAG_AbilTest_State_Firing)));
	}

	TEST_METHOD(StateTree_CostGateBlocksActivation)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager, 10.f);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->StateTree = ArcAbilityTestHelpers::CreateEmptyStateTree(&Spawner.GetWorld());

		FArcTestAttributeCost CostData;
		CostData.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		CostData.Amount = 50.f;
		Def->Cost = FInstancedStruct::Make(CostData);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));

		FArcAttributeSet_TestStats* Stats = View.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNear(10.f, Stats->Health.BaseValue, 0.001f));
	}

	TEST_METHOD(StateTree_CooldownBlocksReactivation)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->StateTree = ArcAbilityTestHelpers::CreateInstantStateTree(&Spawner.GetWorld());

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		// Activate and complete across both passes
		ExecuteInputAndTickProcessors(Entity);

		// Manually add cooldown (simulating what Commit task would do)
		FMassEntityView View(*EntityManager, Entity);
		FArcAbilityCooldownFragment& CdFrag = View.GetFragmentData<FArcAbilityCooldownFragment>();
		FArcCooldownEntry& Entry = CdFrag.ActiveCooldowns.AddDefaulted_GetRef();
		Entry.AbilityHandle = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>()).Abilities[0].Handle;
		Entry.RemainingTime = 5.f;
		Entry.MaxCooldownTime = 5.f;

		// Try to reactivate while on cooldown
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);
		ExecuteInputAndTickProcessors(Entity);

		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
	}

	TEST_METHOD(FullLifecycle_ActivateTickCompleteRevokeTags)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->StateTree = ArcAbilityTestHelpers::CreateInstantStateTree(&Spawner.GetWorld());
		Def->GrantTagsWhileActive.AddTag(TAG_AbilTest_State_Firing);
		Def->GrantTagsWhileActive.AddTag(TAG_AbilTest_State_Blocking);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		// Instant tree: input processor activates + starts, tick processor completes and revokes tags
		ExecuteInputAndTickProcessors(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcOwnedTagsFragment& Tags = *View.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		ASSERT_THAT(IsFalse(Tags.Tags.HasTag(TAG_AbilTest_State_Firing)));
		ASSERT_THAT(IsFalse(Tags.Tags.HasTag(TAG_AbilTest_State_Blocking)));

		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Succeeded));
	}

	TEST_METHOD(SynchronousSucceededStart_TornDownInInputPassAlone)
	{
		// The input processor has a Succeeded branch that tears down the ability when Start()
		// returns Succeeded synchronously (no Tick() needed). In the current engine, the base
		// FStateTreeExecutionContext::Start() always returns Running — transitions are only
		// evaluated during Tick() — so no standard tree triggers this branch today. This test
		// documents the expected observable behavior for a Running tree started via input
		// processor: ability is active and Running after the input pass, and completes (with
		// tags revoked) only after the subsequent tick pass.
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->StateTree = ArcAbilityTestHelpers::CreateInstantStateTree(&Spawner.GetWorld());
		Def->GrantTagsWhileActive.AddTag(TAG_AbilTest_State_Firing);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		// Input processor only: Start() returns Running → ability is active, tags granted.
		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
		ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Running));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].InstanceHandle.IsValid()));

		// Tags ARE granted (Running path grants tags).
		const FArcOwnedTagsFragment& OwnedTagsMid = *View.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		ASSERT_THAT(IsTrue(OwnedTagsMid.Tags.HasTag(TAG_AbilTest_State_Firing)));

		// Now run tick: tree completes, tags revoked.
		UArcAbilityStateTreeTickProcessor* TickProcessor = NewObject<UArcAbilityStateTreeTickProcessor>();
		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		TickProcessor->CallInitialize(&Spawner.GetWorld(), SharedEM);
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity);
		UE::Mass::FProcessingContext TickCtx(*EntityManager, 0.016f);
		TickProcessor->CallExecute(*EntityManager, TickCtx.GetExecutionContext());

		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Succeeded));

		const FArcOwnedTagsFragment& OwnedTagsPost = *View.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		ASSERT_THAT(IsFalse(OwnedTagsPost.Tags.HasTag(TAG_AbilTest_State_Firing)));
	}
};

// ============================================================================
// ArcAbility_Functions_NullSafe — Ability API on entities without FArcAbilityCollectionFragment
// ============================================================================

TEST_CLASS(ArcAbility_Functions_NullSafe, "ArcMassAbilities.Abilities.Functions.NullSafe")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UArcAbilityStateTreeSubsystem* AbilitySubsystem = nullptr;

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
		AbilitySubsystem = Spawner.GetWorld().GetSubsystem<UArcAbilityStateTreeSubsystem>();
		check(AbilitySubsystem);
	}

	// Entity with only FArcAttributeSet_TestStats — no FArcAbilityCollectionFragment.
	FMassEntityHandle CreateEntityWithoutCollection()
	{
		FArcAttributeSet_TestStats Stats;
		Stats.Health.BaseValue = 100.f;
		Stats.Health.CurrentValue = 100.f;

		TArray<FInstancedStruct> Instances;
		Instances.Add(FInstancedStruct::Make(Stats));
		return ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(*EntityManager, Instances);
	}

	TEST_METHOD(GrantAbility_NoCollection_ReturnsInvalidHandle)
	{
		FMassEntityHandle Entity = CreateEntityWithoutCollection();
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();

		FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ASSERT_THAT(IsFalse(Handle.IsValid()));
	}

	TEST_METHOD(FindAbility_NoCollection_ReturnsNull)
	{
		FMassEntityHandle Entity = CreateEntityWithoutCollection();
		FArcAbilityHandle Handle = FArcAbilityHandle::Make(0, 0);

		const FArcActiveAbility* Found = ArcAbilities::FindAbility(*EntityManager, Entity, Handle);
		ASSERT_THAT(IsNull(Found));
	}

	TEST_METHOD(TryActivateAbility_NoCollection_ReturnsFalse)
	{
		FMassEntityHandle Entity = CreateEntityWithoutCollection();
		FArcAbilityHandle Handle = FArcAbilityHandle::Make(0, 0);

		bool bResult = ArcAbilities::TryActivateAbility(*EntityManager, Entity, Handle);
		ASSERT_THAT(IsFalse(bResult));
	}

	TEST_METHOD(RemoveAbility_NoCollection_NoCrash)
	{
		FMassEntityHandle Entity = CreateEntityWithoutCollection();
		FArcAbilityHandle Handle = FArcAbilityHandle::Make(0, 0);

		// Must not crash — entity simply has no collection.
		ArcAbilities::RemoveAbility(*EntityManager, Entity, Handle, *AbilitySubsystem);
		ASSERT_THAT(IsTrue(EntityManager->IsEntityValid(Entity)));
	}

	TEST_METHOD(CancelAbility_NoCollection_NoCrash)
	{
		FMassEntityHandle Entity = CreateEntityWithoutCollection();
		FArcAbilityHandle Handle = FArcAbilityHandle::Make(0, 0);

		// Must not crash — entity simply has no collection.
		ArcAbilities::CancelAbility(*EntityManager, Entity, Handle, *AbilitySubsystem, EArcAbilityEndReason::Cancelled, FMassEntityHandle());
		ASSERT_THAT(IsTrue(EntityManager->IsEntityValid(Entity)));
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
		const FArcAbilityInputFragment& Input = (*View.GetSparseFragmentDataPtr<FArcAbilityInputFragment>());
		ASSERT_THAT(IsTrue(Input.HeldInputs.HasTag(TAG_AbilTest_Input_Fire)));
	}

	TEST_METHOD(IsInputHeld_ReturnsFalseWhenNotHeld)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityInputFragment& Input = (*View.GetSparseFragmentDataPtr<FArcAbilityInputFragment>());
		ASSERT_THAT(IsFalse(Input.HeldInputs.HasTag(TAG_AbilTest_Input_Fire)));
	}

	TEST_METHOD(IsInputHeld_ReturnsFalseAfterRelease)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);
		ArcAbilities::ReleaseInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityInputFragment& Input = (*View.GetSparseFragmentDataPtr<FArcAbilityInputFragment>());
		ASSERT_THAT(IsFalse(Input.HeldInputs.HasTag(TAG_AbilTest_Input_Fire)));
	}

	TEST_METHOD(InputState_PressedThisFrame)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityInputFragment& Input = (*View.GetSparseFragmentDataPtr<FArcAbilityInputFragment>());
		ASSERT_THAT(IsTrue(Input.PressedThisFrame.Contains(TAG_AbilTest_Input_Fire)));
		ASSERT_THAT(IsFalse(Input.ReleasedThisFrame.Contains(TAG_AbilTest_Input_Fire)));
	}

	TEST_METHOD(InputState_ReleasedThisFrame)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);
		ArcAbilities::ReleaseInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityInputFragment& Input = (*View.GetSparseFragmentDataPtr<FArcAbilityInputFragment>());
		ASSERT_THAT(IsTrue(Input.ReleasedThisFrame.Contains(TAG_AbilTest_Input_Fire)));
	}

	TEST_METHOD(InputState_NoneWhenNoInput)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityInputFragment& Input = (*View.GetSparseFragmentDataPtr<FArcAbilityInputFragment>());
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

	void ExecuteInputAndTickProcessors(FMassEntityHandle Entity, float DeltaTime = 0.016f)
	{
		ExecuteInputProcessor(Entity, DeltaTime);

		UArcAbilityStateTreeTickProcessor* TickProcessor = NewObject<UArcAbilityStateTreeTickProcessor>();
		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		TickProcessor->CallInitialize(&Spawner.GetWorld(), SharedEM);

		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity);

		UE::Mass::FProcessingContext Ctx(*EntityManager, DeltaTime);
		TickProcessor->CallExecute(*EntityManager, Ctx.GetExecutionContext());
	}

	TEST_METHOD(NoSignal_EntityNotProcessed)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);

		FMassEntityView View(*EntityManager, Entity);
		FArcAbilityInputFragment& Input = (*View.GetSparseFragmentDataPtr<FArcAbilityInputFragment>());
		Input.PressedThisFrame.Add(TAG_AbilTest_Input_Fire);

		UArcAbilityInputProcessor* Processor = NewObject<UArcAbilityInputProcessor>();
		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		Processor->CallInitialize(&Spawner.GetWorld(), SharedEM);

		UE::Mass::FProcessingContext ProcessingContext(*EntityManager, 0.016f);
		Processor->CallExecute(*EntityManager, ProcessingContext.GetExecutionContext());

		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
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
		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
		ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));

		UArcAbilityStateTreeTickProcessor* TickProcessor = NewObject<UArcAbilityStateTreeTickProcessor>();
		TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
		TickProcessor->CallInitialize(&Spawner.GetWorld(), SharedEM);

		UE::Mass::FProcessingContext ProcessingContext(*EntityManager, 0.016f);
		TickProcessor->CallExecute(*EntityManager, ProcessingContext.GetExecutionContext());

		ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));
	}

	TEST_METHOD(InstantTree_CompletesAcrossInputAndTickPass)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->StateTree = ArcAbilityTestHelpers::CreateInstantStateTree(&Spawner.GetWorld());
		Def->GrantTagsWhileActive.AddTag(TAG_AbilTest_State_Firing);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		// Instant tree: Start() returns Succeeded synchronously; the input processor tears down
		// the ability in the same pass (no tags granted, instance freed, bIsActive=false).
		// The tick processor call is a no-op — entity is already inactive.
		ExecuteInputAndTickProcessors(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Succeeded));

		const FArcOwnedTagsFragment& OwnedTags = *View.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
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
		// Use the same tag for both ability activation and task-listen.
		// First PushInput(Fire): inactive match fires AbilityInputReceived → InputProcessor activates
		//   ability → EnterState subscribes lambda to AbilityActiveInputReceived.
		// Second PushInput(Fire): active match fires AbilityActiveInputReceived → lambda fires
		//   OnInputPressed → ExecuteTickProcessor transitions to Done → Succeeded.
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->StateTree = ArcAbilityTestHelpers::CreateInputPressedStateTree(&Spawner.GetWorld(), TAG_AbilTest_Input_Fire);
		Def->GrantTagsWhileActive.AddTag(TAG_AbilTest_State_Firing);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);

		// First press: activates the ability and subscribes the lambda.
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);
		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
		ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Running));

		// Second press: ability is now active → AbilityActiveInputReceived fires → lambda
		// sees PressedThisFrame.Contains(Fire) → BroadcastDelegate(OnInputPressed) +
		// SignalEntity(AbilityStateTreeTickRequired).
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);
		ExecuteTickProcessor(Entity);

		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Succeeded));

		const FArcOwnedTagsFragment& OwnedTags = *View.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		ASSERT_THAT(IsFalse(OwnedTags.Tags.HasTag(TAG_AbilTest_State_Firing)));
	}

	TEST_METHOD(InputReleased_SignalDrivenTransition)
	{
		// Same-tag pattern: Fire tag is used for both activation and task-listen.
		// Phase 1 — activate: first PushInput(Fire) fires AbilityInputReceived (inactive match)
		//   → InputProcessor activates → EnterState subscribes lambda (resets bSawPress=false).
		// Phase 2 — press observed: second PushInput(Fire) fires AbilityActiveInputReceived
		//   → lambda sees PressedThisFrame.Contains(Fire) → sets bSawPress=true, PressTime=Now.
		// Phase 3 — release observed: ReleaseInput(Fire) fires AbilityActiveInputReceived
		//   → lambda sees ReleasedThisFrame.Contains(Fire) + bSawPress=true
		//   → BroadcastDelegate(OnInputReleased) + SignalEntity(AbilityStateTreeTickRequired).
		// Phase 4 — tick: ExecuteTickProcessor transitions to Done → Succeeded.
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->StateTree = ArcAbilityTestHelpers::CreateInputReleasedStateTree(&Spawner.GetWorld(), TAG_AbilTest_Input_Fire);
		Def->GrantTagsWhileActive.AddTag(TAG_AbilTest_State_Firing);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);

		// Phase 1: activate the ability and subscribe the lambda.
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);
		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
		ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Running));

		// Phase 2: second press while active — lambda observes press, sets bSawPress=true.
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		// Phase 3: release — lambda observes release with bSawPress=true,
		// broadcasts OnInputReleased, signals AbilityStateTreeTickRequired.
		ArcAbilities::ReleaseInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		// Phase 4: tick processes the pending AbilityStateTreeTickRequired signal.
		ExecuteTickProcessor(Entity);

		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Succeeded));

		const FArcOwnedTagsFragment& OwnedTags = *View.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		ASSERT_THAT(IsFalse(OwnedTags.Tags.HasTag(TAG_AbilTest_State_Firing)));
	}

	TEST_METHOD(InputReleasedOnce_FiresOnReleaseWithoutPriorPress)
	{
		// InputReleasedOnce fires on the *first* release after EnterState — no prior press
		// observed during the task's lifetime is required.
		//
		// Phase 1 — activate: PushInput(Fire) fires AbilityInputReceived (inactive match)
		//   → InputProcessor activates → EnterState subscribes lambda.
		// Phase 2 — release only (no second press): ReleaseInput(Fire) fires
		//   AbilityActiveInputReceived → lambda sees ReleasedThisFrame.Contains(Fire)
		//   → BroadcastDelegate(OnInputReleased) + SignalEntity(AbilityStateTreeTickRequired).
		// Phase 3 — tick: ExecuteTickProcessor transitions to Done → Succeeded.
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->StateTree = ArcAbilityTestHelpers::CreateInputReleasedOnceStateTree(&Spawner.GetWorld(), TAG_AbilTest_Input_Fire);
		Def->GrantTagsWhileActive.AddTag(TAG_AbilTest_State_Firing);

		ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);

		// Phase 1: activate the ability and subscribe the lambda.
		ArcAbilities::PushInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);
		ExecuteInputProcessor(Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment& Collection = (*View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>());
		ASSERT_THAT(IsTrue(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Running));

		// Phase 2: release with no second press — lambda fires immediately on release.
		ArcAbilities::ReleaseInput(*EntityManager, Entity, TAG_AbilTest_Input_Fire);

		// Phase 3: tick processes the pending AbilityStateTreeTickRequired signal.
		ExecuteTickProcessor(Entity);

		ASSERT_THAT(IsFalse(Collection.Abilities[0].bIsActive));
		ASSERT_THAT(IsTrue(Collection.Abilities[0].RunStatus == EStateTreeRunStatus::Succeeded));

		const FArcOwnedTagsFragment& OwnedTags = *View.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		ASSERT_THAT(IsFalse(OwnedTags.Tags.HasTag(TAG_AbilTest_State_Firing)));
	}

};

// ============================================================================
// ArcAbility_CooldownExtensibility — non-blocking flag, OnExpired dispatch,
//                                    event subsystem integration
// ============================================================================

TEST_CLASS(ArcAbility_CooldownExtensibility, "ArcMassAbilities.Abilities.CooldownExtensibility")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UArcGameEventSubsystem* EventSubsystem = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogMass");
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();
		EventSubsystem = Spawner.GetWorld().GetSubsystem<UArcGameEventSubsystem>();
		check(EventSubsystem);
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

	// -----------------------------------------------------------------------
	// Test 1: Non-blocking scan
	//
	// A fragment with two entries — one bBlocking=false for HandleA, one bBlocking=true
	// for HandleB — must leave HandleA unblocked and block HandleB.
	// Direct CheckCooldown call; no processor needed.
	// -----------------------------------------------------------------------

	TEST_METHOD(NonBlocking_EntryDoesNotBlockActivation)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		const FArcAbilityHandle HandleA = FArcAbilityHandle::Make(0, 0);
		const FArcAbilityHandle HandleB = FArcAbilityHandle::Make(1, 0);

		FMassEntityView View(*EntityManager, Entity);
		FArcAbilityCooldownFragment& CdFrag = View.GetFragmentData<FArcAbilityCooldownFragment>();

		// Non-blocking entry for HandleA.
		FArcCooldownEntry& EntryA = CdFrag.ActiveCooldowns.AddDefaulted_GetRef();
		EntryA.AbilityHandle = HandleA;
		EntryA.RemainingTime = 5.f;
		EntryA.MaxCooldownTime = 5.f;
		EntryA.bBlocking = false;

		// Blocking entry for HandleB.
		FArcCooldownEntry& EntryB = CdFrag.ActiveCooldowns.AddDefaulted_GetRef();
		EntryB.AbilityHandle = HandleB;
		EntryB.RemainingTime = 5.f;
		EntryB.MaxCooldownTime = 5.f;
		EntryB.bBlocking = true;

		FArcAbilityCooldown_Duration Cooldown;
		Cooldown.CooldownDuration = FArcScalableFloat(5.f);

		// HandleA has an active entry but it is non-blocking — must NOT be blocked.
		ASSERT_THAT(IsTrue(Cooldown.CheckCooldown(*EntityManager, Entity, HandleA)));

		// HandleB has a blocking entry — must be blocked.
		ASSERT_THAT(IsFalse(Cooldown.CheckCooldown(*EntityManager, Entity, HandleB)));
	}

	// -----------------------------------------------------------------------
	// Test 1b: ApplyCooldown stamps bBlocking from IsBlocking()
	//
	// FArcAbilityCooldown_Duration (IsBlocking()=true) must produce bBlocking=true.
	// FArcTestNonBlockingCooldown (IsBlocking()=false) must produce bBlocking=false.
	// -----------------------------------------------------------------------

	TEST_METHOD(ApplyCooldown_StampsBBlockingFromIsBlocking)
	{
		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);
		const FArcAbilityHandle HandleA = FArcAbilityHandle::Make(0, 0);
		const FArcAbilityHandle HandleB = FArcAbilityHandle::Make(1, 0);

		// Blocking subtype.
		FArcAbilityCooldown_Duration BlockingCd;
		BlockingCd.CooldownDuration = FArcScalableFloat(3.f);
		BlockingCd.ApplyCooldown(*EntityManager, Entity, HandleA);

		// Non-blocking subtype.
		FArcTestNonBlockingCooldown NonBlockingCd;
		NonBlockingCd.ApplyCooldown(*EntityManager, Entity, HandleB);

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCooldownFragment& Frag = View.GetFragmentData<FArcAbilityCooldownFragment>();
		ASSERT_THAT(AreEqual(2, Frag.ActiveCooldowns.Num()));

		// Find the entries by handle (RemoveAtSwap means order may vary).
		const FArcCooldownEntry* FoundA = nullptr;
		const FArcCooldownEntry* FoundB = nullptr;
		for (const FArcCooldownEntry& E : Frag.ActiveCooldowns)
		{
			if (E.AbilityHandle == HandleA) { FoundA = &E; }
			if (E.AbilityHandle == HandleB) { FoundB = &E; }
		}

		ASSERT_THAT(IsNotNull(FoundA));
		ASSERT_THAT(IsTrue(FoundA->bBlocking));
		ASSERT_THAT(IsNotNull(FoundB));
		ASSERT_THAT(IsFalse(FoundB->bBlocking));
	}

	// -----------------------------------------------------------------------
	// Test 2: Expiry dispatch — OnExpired called + CooldownExpired event fired +
	//         entry removed.
	//
	// Grant an ability whose Definition->Cooldown is FArcTestNonBlockingCooldown.
	// Seed a cooldown entry for that handle with tiny RemainingTime.
	// Subscribe to ArcEvent.Ability.CooldownExpired globally and for the entity.
	// Run UArcCooldownProcessor with DeltaTime > RemainingTime, then drain the bus.
	// Assert: OnExpiredCallCount == 1, global event fired once with correct args,
	//         per-entity event fired once with correct args, entry removed.
	// -----------------------------------------------------------------------

	TEST_METHOD(ExpiryDispatch_OnExpiredAndEventFiredEntryRemoved)
	{
		FArcTestNonBlockingCooldown::OnExpiredCallCount = 0;

		FMassEntityHandle Entity = ArcAbilityTestHelpers::CreateAbilityTestEntity(*EntityManager);

		// Grant ability whose Cooldown is our custom subclass.
		UArcAbilityDefinition* Def = ArcAbilityTestHelpers::CreateSimpleAbilityDef();
		Def->Cooldown = FInstancedStruct::Make(FArcTestNonBlockingCooldown());
		const FArcAbilityHandle Handle = ArcAbilities::GrantAbility(*EntityManager, Entity, Def, TAG_AbilTest_Input_Fire);

		// Seed a cooldown entry that will expire on the next processor tick.
		FMassEntityView View(*EntityManager, Entity);
		FArcAbilityCooldownFragment& CdFrag = View.GetFragmentData<FArcAbilityCooldownFragment>();
		FArcCooldownEntry& Entry = CdFrag.ActiveCooldowns.AddDefaulted_GetRef();
		Entry.AbilityHandle = Handle;
		Entry.RemainingTime = 0.1f;
		Entry.MaxCooldownTime = 5.f;
		Entry.bBlocking = false; // non-blocking timer type

		// Track global CooldownExpired event.
		int32 GlobalExpiredCount = 0;
		const UArcAbilityDefinition* GlobalExpiredDef = nullptr;
		FArcAbilityHandle GlobalExpiredHandle;
		EventSubsystem->Subscribe(ArcGameEventTags::AbilityCooldownExpired.GetTag(), FMassEntityHandle(), EArcEventRole::Any,
			FArcGameEventDelegate::CreateLambda([&GlobalExpiredCount, &GlobalExpiredDef, &GlobalExpiredHandle](const FArcGameEvent& Event)
			{
				const FArcAbilityCooldownExpiredPayload* Payload = Event.Payload.GetPtr<FArcAbilityCooldownExpiredPayload>();
				if (Payload)
				{
					++GlobalExpiredCount;
					GlobalExpiredDef = Payload->Definition.Get();
					GlobalExpiredHandle = Payload->Handle;
				}
			}));

		// Track per-entity CooldownExpired event.
		int32 PerEntityExpiredCount = 0;
		FArcAbilityHandle PerEntityExpiredHandle;
		EventSubsystem->Subscribe(ArcGameEventTags::AbilityCooldownExpired.GetTag(), Entity, EArcEventRole::Any,
			FArcGameEventDelegate::CreateLambda([&PerEntityExpiredCount, &PerEntityExpiredHandle](const FArcGameEvent& Event)
			{
				const FArcAbilityCooldownExpiredPayload* Payload = Event.Payload.GetPtr<FArcAbilityCooldownExpiredPayload>();
				if (Payload)
				{
					++PerEntityExpiredCount;
					PerEntityExpiredHandle = Payload->Handle;
				}
			}));

		// Run processor with enough delta to expire the 0.1s entry.
		ExecuteCooldownProcessor(1.f);

		// Drain the event queue so the GT delegates fire.
		EventSubsystem->DrainOnPostActorTick(&Spawner.GetWorld(), LEVELTICK_All, 0.f);

		// Entry must have been removed.
		ASSERT_THAT(AreEqual(0, CdFrag.ActiveCooldowns.Num()));

		// OnExpired on the custom cooldown must have fired.
		ASSERT_THAT(AreEqual(1, FArcTestNonBlockingCooldown::OnExpiredCallCount));

		// Global CooldownExpired delegate must have fired exactly once with the correct Def and Handle.
		ASSERT_THAT(AreEqual(1, GlobalExpiredCount));
		ASSERT_THAT(IsTrue(GlobalExpiredDef == Def));
		ASSERT_THAT(IsTrue(GlobalExpiredHandle == Handle));

		// Per-entity CooldownExpired delegate must have fired exactly once with the correct Handle.
		ASSERT_THAT(AreEqual(1, PerEntityExpiredCount));
		ASSERT_THAT(IsTrue(PerEntityExpiredHandle == Handle));
	}
};
