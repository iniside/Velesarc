// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcStatusTestHarness.h"

#include "ArcMassMechanicsTestTypes.h"
#include "ArcStatusTestTypes.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcAbilityStateTreeSubsystem.h"
#include "Abilities/Schema/ArcAbilityStateTreeSchema.h"
#include "Attributes/ArcAttributeAggregatorProcessor.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectDurationProcessor.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectSpec.h"
#include "Events/ArcGameEventSubsystem.h"
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
#include "Misc/AutomationTest.h"
#include "Processors/ArcAbilityInputProcessor.h"
#include "Processors/ArcAbilityStateTreeTickProcessor.h"
#include "Processors/ArcPendingAttributeOpsProcessor.h"
#include "Signals/ArcAbilitySignals.h"
#include "Signals/ArcAttributeRecalculateSignal.h"
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"

void ArcStatusTest::SuppressTestLogs()
{
	FAutomationTestBase::bSuppressLogWarnings = true;
	FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
	FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
	FAutomationTestBase::SuppressedLogCategories.AddUnique("LogStateTree");
	FAutomationTestBase::SuppressedLogCategories.AddUnique("LogMass");
}

UStateTree* ArcStatusTest::MakeRunForeverTree(UObject* Outer)
{
	UStateTree* StateTree = NewObject<UStateTree>(Outer);
	UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(StateTree);
	StateTree->EditorData = EditorData;
	EditorData->Schema = NewObject<UArcAbilityStateTreeSchema>();

	UStateTreeState& Root = EditorData->AddSubTree(FName(TEXT("Root")));
	Root.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);
	Root.AddTask<FArcMechTest_RunForeverTask>();

	FStateTreeCompilerLog Log;
	FStateTreeCompiler Compiler(Log);
	const bool bCompiled = Compiler.Compile(*StateTree);
	ensureAlwaysMsgf(bCompiled, TEXT("MakeRunForeverTree failed to compile"));
	return StateTree;
}

bool ArcStatusTest::FStatusHarness::Initialize(FActorTestSpawner& Spawner)
{
	LogScope.Emplace();
	CapturedEvents.Reset();

	Spawner.GetWorld();
	Spawner.InitializeGameSubsystems();
	World = &Spawner.GetWorld();

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
	EventBus = World->GetSubsystem<UArcGameEventSubsystem>();
	AbilitySubsystem = World->GetSubsystem<UArcAbilityStateTreeSubsystem>();
	if (!EntitySubsystem || !SignalSubsystem || !EventBus || !AbilitySubsystem)
	{
		return false;
	}
	EntityManager = &EntitySubsystem->GetMutableEntityManager();
	HandlerConfig = ArcMechTest::LoadTestAsset<UArcAttributeHandlerConfig>(TEXT("Handlers/AHC_Cost_Test"));
	if (!HandlerConfig)
	{
		return false;
	}

	const TSharedRef<FMassEntityManager> SharedManager = EntityManager->AsShared();
	DurationProcessor = NewObject<UArcEffectDurationProcessor>();
	DurationProcessor->CallInitialize(EntityManager->GetOwner(), SharedManager);
	PendingProcessor = NewObject<UArcPendingAttributeOpsProcessor>();
	PendingProcessor->CallInitialize(EntityManager->GetOwner(), SharedManager);
	AggregatorProcessor = NewObject<UArcAttributeAggregatorProcessor>();
	AggregatorProcessor->CallInitialize(EntityManager->GetOwner(), SharedManager);
	return true;
}

FMassEntityHandle ArcStatusTest::FStatusHarness::CreateEntity(const FStatusEntityOptions& Options)
{
	UE::Mass::FEntityBuilder Builder(*EntityManager);
	if (Options.bCostSet)
	{
		FArcMechTest_CostSet Set;
		Set.InitMana(Options.Mana);
		Set.InitManaMeta(0.f);
		Set.InitStamina(100.f);
		Set.InitStaminaMeta(0.f);
		Set.InitTierValue(Options.TierValue);
		Set.InitTierThreshold(0.f);
		Builder.Add<FArcMechTest_CostSet>(Set);
	}

	Builder.Add<FArcAbilityCollectionFragment>()
		.Add<FArcAbilityInputFragment>()
		.Add<FArcOwnedTagsFragment>()
		.Add<FArcAbilityCooldownFragment>()
		.Add<FArcEffectStackFragment>()
		.Add<FArcAggregatorFragment>()
		.Add<FArcPendingAttributeOpsFragment>();

	FArcAttributeHandlerSharedFragment HandlerShared;
	HandlerShared.Config = Options.HandlerConfigOverride ? Options.HandlerConfigOverride : HandlerConfig;
	Builder.Add<FArcAttributeHandlerSharedFragment>(HandlerShared);

	return Builder.Commit();
}

void ArcStatusTest::FStatusHarness::CaptureEvents(FGameplayTag Tag)
{
	EventBus->Subscribe(Tag, FMassEntityHandle(), EArcEventRole::Any, FArcGameEventDelegate::CreateRaw(this, &FStatusHarness::HandleEvent));
}

void ArcStatusTest::FStatusHarness::DrainEvents()
{
	EventBus->DrainOnPostActorTick(World, LEVELTICK_All, 0.f);
}

int32 ArcStatusTest::FStatusHarness::CountCaptured(FGameplayTag Tag) const
{
	int32 Count = 0;
	for (const FArcGameEvent& Event : CapturedEvents)
	{
		if (Event.Tag == Tag)
		{
			++Count;
		}
	}
	return Count;
}

void ArcStatusTest::FStatusHarness::HandleEvent(const FArcGameEvent& Event)
{
	CapturedEvents.Add(Event);
}

void ArcStatusTest::FStatusHarness::TickDuration(float DeltaSeconds)
{
	FMassProcessingContext ProcessingContext(*EntityManager, DeltaSeconds);
	UE::Mass::Executor::Run(*DurationProcessor, ProcessingContext);
}

void ArcStatusTest::FStatusHarness::DrainPendingOps(FMassEntityHandle Entity)
{
	SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::PendingAttributeOps, Entity);
	FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
	UE::Mass::Executor::Run(*PendingProcessor, ProcessingContext);
}

void ArcStatusTest::FStatusHarness::Recalculate(FMassEntityHandle Entity)
{
	SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AttributeRecalculate, Entity);
	FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
	UE::Mass::Executor::Run(*AggregatorProcessor, ProcessingContext);
}

void ArcStatusTest::FStatusHarness::Activate(FMassEntityHandle Entity, FGameplayTag InputTag)
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

void ArcStatusTest::FStatusHarness::AdvanceWorldTime(double Seconds)
{
	World->TimeSeconds += Seconds;
}

FArcActiveEffect* ArcStatusTest::FStatusHarness::FindActive(FMassEntityHandle Entity, FArcActiveEffectHandle Handle) const
{
	FMassEntityView EntityView(*EntityManager, Entity);
	FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
	return Stack ? ArcEffects::FindActiveEffect(*Stack, Handle) : nullptr;
}

const FArcActiveEffect* ArcStatusTest::FStatusHarness::FindActiveByDefinition(FMassEntityHandle Entity, const UArcEffectDefinition* Definition) const
{
	FMassEntityView EntityView(*EntityManager, Entity);
	const FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
	if (!Stack)
	{
		return nullptr;
	}
	return Stack->ActiveEffects.FindByPredicate([Definition](const FArcActiveEffect& Active)
	{
		const FArcEffectSpec* Spec = Active.Spec.GetPtr<FArcEffectSpec>();
		return Spec && Spec->Definition == Definition;
	});
}

int32 ArcStatusTest::FStatusHarness::NumActive(FMassEntityHandle Entity) const
{
	FMassEntityView EntityView(*EntityManager, Entity);
	const FArcEffectStackFragment* Stack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
	return Stack ? Stack->ActiveEffects.Num() : -1;
}

FArcMechTest_CostSet* ArcStatusTest::FStatusHarness::GetSet(FMassEntityHandle Entity) const
{
	FMassEntityView EntityView(*EntityManager, Entity);
	return EntityView.GetSparseFragmentDataPtr<FArcMechTest_CostSet>();
}

float ArcStatusTest::FStatusHarness::GetTierValue(FMassEntityHandle Entity) const
{
	const FArcMechTest_CostSet* Set = GetSet(Entity);
	return Set ? Set->GetTierValue() : -1.f;
}

float ArcStatusTest::FStatusHarness::GetMana(FMassEntityHandle Entity) const
{
	const FArcMechTest_CostSet* Set = GetSet(Entity);
	return Set ? Set->GetMana() : -1.f;
}
