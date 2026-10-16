// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanicsAbilityHarness.h"

#include "ArcMassMechanicsTestTypes.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/Schema/ArcAbilityStateTreeSchema.h"
#include "Abilities/Tasks/ArcAbilityTask_Commit.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Components/ActorTestSpawner.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcAbilityCooldownFragment.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcAttributeHandlerSharedFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Mass/EntityBuilder.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "Processors/ArcAbilityInputProcessor.h"
#include "Processors/ArcAbilityStateTreeTickProcessor.h"
#include "Processors/ArcPendingAttributeOpsProcessor.h"
#include "Signals/ArcAbilitySignals.h"
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "StructUtils/InstancedStruct.h"

UStateTree* ArcMassMechanicsAbilityTest::MakeCommitTree(UObject* Outer)
{
	UStateTree* StateTree = NewObject<UStateTree>(Outer);
	UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(StateTree);
	StateTree->EditorData = EditorData;
	EditorData->Schema = NewObject<UArcAbilityStateTreeSchema>();

	UStateTreeState& Root = EditorData->AddSubTree(FName(TEXT("Root")));
	Root.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

	UStateTreeState& Run = Root.AddChildState(FName(TEXT("Commit")));
	Run.AddTask<FArcAbilityTask_Commit>();
	Run.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

	FStateTreeCompilerLog Log;
	FStateTreeCompiler Compiler(Log);
	const bool bCompiled = Compiler.Compile(*StateTree);
	ensureAlwaysMsgf(bCompiled, TEXT("MakeCommitTree failed to compile"));
	return StateTree;
}

bool ArcMassMechanicsAbilityTest::FAbilityHarness::Initialize(FActorTestSpawner& Spawner)
{
	LogScope.Emplace();
	ArcMassMechanicsTest::ResetCounts();

	Spawner.GetWorld();
	Spawner.InitializeGameSubsystems();
	World = &Spawner.GetWorld();

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
	if (!EntitySubsystem || !SignalSubsystem)
	{
		return false;
	}
	EntityManager = &EntitySubsystem->GetMutableEntityManager();
	HandlerConfig = ArcMechTest::LoadTestAsset<UArcAttributeHandlerConfig>(TEXT("Handlers/AHC_Cost_Test"));
	if (!HandlerConfig)
	{
		return false;
	}

	PendingProcessor = NewObject<UArcPendingAttributeOpsProcessor>();
	PendingProcessor->CallInitialize(EntityManager->GetOwner(), EntityManager->AsShared());
	return true;
}

FMassEntityHandle ArcMassMechanicsAbilityTest::FAbilityHarness::CreateEntity(const FCostEntityOptions& Options)
{
	FArcMechTest_CostSet Set;
	Set.InitMana(Options.Mana);
	Set.InitManaMeta(0.f);
	Set.InitStamina(Options.Stamina);
	Set.InitStaminaMeta(0.f);
	Set.InitTierValue(Options.TierValue);
	Set.InitTierThreshold(Options.TierThreshold);

	UE::Mass::FEntityBuilder Builder(*EntityManager);
	Builder.Add<FArcMechTest_CostSet>(Set)
		.Add<FArcAbilityCollectionFragment>()
		.Add<FArcAbilityInputFragment>()
		.Add<FArcOwnedTagsFragment>()
		.Add<FArcAbilityCooldownFragment>()
		.Add<FArcEffectStackFragment>()
		.Add<FArcAggregatorFragment>()
		.Add<FArcPendingAttributeOpsFragment>();

	if (Options.bHandlerConfig)
	{
		FArcAttributeHandlerSharedFragment HandlerShared;
		HandlerShared.Config = HandlerConfig;
		Builder.Add<FArcAttributeHandlerSharedFragment>(HandlerShared);
	}

	return Builder.Commit();
}

void ArcMassMechanicsAbilityTest::FAbilityHarness::Activate(FMassEntityHandle Entity, FGameplayTag InputTag)
{
	ArcAbilities::PushInput(*EntityManager, Entity, InputTag);

	UArcAbilityInputProcessor* InputProcessor = NewObject<UArcAbilityInputProcessor>();
	InputProcessor->CallInitialize(World, EntityManager->AsShared());
	SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityInputReceived, Entity);
	UE::Mass::FProcessingContext InputContext(*EntityManager, 0.016f);
	InputProcessor->CallExecute(*EntityManager, InputContext.GetExecutionContext());

	UArcAbilityStateTreeTickProcessor* TickProcessor = NewObject<UArcAbilityStateTreeTickProcessor>();
	TickProcessor->CallInitialize(World, EntityManager->AsShared());
	SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity);
	UE::Mass::FProcessingContext TickContext(*EntityManager, 0.016f);
	TickProcessor->CallExecute(*EntityManager, TickContext.GetExecutionContext());
}

void ArcMassMechanicsAbilityTest::FAbilityHarness::DrainPendingOps(FMassEntityHandle Entity)
{
	SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::PendingAttributeOps, Entity);
	FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
	UE::Mass::Executor::Run(*PendingProcessor, ProcessingContext);
}

FArcMechTest_CostSet* ArcMassMechanicsAbilityTest::FAbilityHarness::GetSet(FMassEntityHandle Entity) const
{
	FMassEntityView EntityView(*EntityManager, Entity);
	return EntityView.GetSparseFragmentDataPtr<FArcMechTest_CostSet>();
}

int32 ArcMassMechanicsAbilityTest::FAbilityHarness::CountActiveEffects(FMassEntityHandle Entity) const
{
	FMassEntityView EntityView(*EntityManager, Entity);
	const FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
	return Stack ? Stack->ActiveEffects.Num() : -1;
}
