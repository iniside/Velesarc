#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

#include "ArcMass/ArcMassFragments.h"
#include "ArcTQSQueryInstance.h"
#include "Actions/ArcTacticsSequenceAction.h"
#include "Debugger/ArcTacticsDebug.h"
#include "Decision/ArcTacticsCompiler.h"
#include "Decision/ArcTacticsConfig.h"
#include "Fixtures/ArcTacticsPackageTestTypes.h"
#include "Fixtures/ArcTacticsPoolTestTypes.h"
#include "GameplayTagContainer.h"
#include "Mass/ArcTacticsFragments.h"
#include "Mass/ArcTacticsInstanceSubsystem.h"
#include "Mass/ArcTacticsLifecycleProcessor.h"
#include "Mass/ArcTacticsTrait.h"
#include "Mass/ArcTacticsUtilityProcessor.h"
#include "Mass/EntityBuilder.h"
#include "Mass/EntityView.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "MassCommonFragments.h"
#include "MassEntityConfigAsset.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTemplate.h"
#include "MassEntityTemplateRegistry.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassNavMeshNavigationFragments.h"
#include "MassSpawnerSubsystem.h"
#include "Navigation/ArcPathFollow.h"
#include "SmoothOrientation/MassSmoothOrientationFragments.h"
#include "Steps/ArcTacticsFocusActiveStep.h"
#include "Steering/MassSteeringFragments.h"

namespace ArcTacticsPackageTestsPrivate
{
	struct FActionSpec
	{
		uint8 Mode = 2;
		int32 Priority = 0;
		float Weight = 0.5f;
		float Cooldown = 0.0f;
		EArcTacticsCooldownStart CooldownStart = EArcTacticsCooldownStart::OnEnd;
		FGameplayTag HistoryTag;
	};

	FPropertyBindingPath MakePath(const FGuid ID, const TCHAR* Path)
	{
		FPropertyBindingPath Result(ID);
		Result.FromString(Path);
		return Result;
	}

	bool Compile(UArcTacticsConfig& Config, const uint32 Revision)
	{
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		const bool bSucceeded = FArcTacticsCompiler::Compile(Config, Revision, Config.CompiledData, Diagnostics);
		if (!bSucceeded)
		{
			for (const FArcTacticsCompilerDiagnostic& Diagnostic : Diagnostics)
			{
				UE_LOGF(LogTemp, Warning, "ArcTacticsPackageTests compile diagnostic: NodeID=%ls Message=%ls",
					*Diagnostic.NodeID.ToString(), *Diagnostic.Message.ToString());
			}
		}
		return bSucceeded;
	}

	UArcTacticsConfig* NewConfig(const TCHAR* Name)
	{
		UArcTacticsConfig* Config = NewObject<UArcTacticsConfig>(
			GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UArcTacticsConfig::StaticClass(), FName(Name)));
		Config->ContextID = FGuid(10, 20, 30, 40);
		Config->GlobalParameters.AddProperty(TEXT("Captured"), EPropertyBagPropertyType::Int32);
		return Config;
	}

	void AddTask(
		UArcTacticsConfig& Config,
		const uint8 Mode,
		UArcTacticsPackageProbe* Probe,
		const bool bNeedsTeam = false)
	{
		FArcTacticsNodeDefinition Task;
		Task.ID = FGuid(100, 1, 2, 3 + Config.GlobalTasks.Num());
		Task.Name = TEXT("PackageTask");
		if (bNeedsTeam)
		{
			Task.Node.InitializeAs<FArcTacticsPackageTeamTask>();
		}
		else
		{
			Task.Node.InitializeAs<FArcTacticsPackageTask>();
		}
		FArcTacticsPackageTaskInstanceData Defaults;
		Defaults.Mode = Mode;
		Defaults.Probe = Probe;
		Task.InstanceData.InitializeAs<FArcTacticsPackageTaskInstanceData>(Defaults);
		Config.GlobalTasks.Add(Task);
	}

	void ApplySpec(FArcTacticsBranchParameters& Parameters, const FActionSpec& Spec)
	{
		Parameters.Priority = Spec.Priority;
		Parameters.Weight = Spec.Weight;
		Parameters.Cooldown = Spec.Cooldown;
		Parameters.CooldownStart = Spec.CooldownStart;
		Parameters.HistoryTag = Spec.HistoryTag;
	}

	void AddLifecycleAction(UArcTacticsConfig& Config, const FActionSpec& Spec)
	{
		const int32 ActionIndex = Config.Actions.Num();
		FArcTacticsActionDefinition Action;
		Action.Action.ID = FGuid(400 + ActionIndex, 1, 2, 3);
		Action.Action.Name = FName(*FString::Printf(TEXT("PackageBranch%d"), ActionIndex));
		Action.Action.Node.InitializeAs<FArcTacticsPoolLifecycleAction>();
		FArcTacticsPoolLifecycleActionInstanceData Defaults;
		Defaults.Mode = Spec.Mode;
		Action.Action.InstanceData.InitializeAs<FArcTacticsPoolLifecycleActionInstanceData>(Defaults);
		ApplySpec(Action.Parameters, Spec);
		Config.Actions.Add(Action);
	}

	void AddScriptAction(
		UArcTacticsConfig& Config,
		const FActionSpec& Spec,
		UArcTacticsPackageProbe& Probe,
		const bool bAcquireHold)
	{
		const int32 ActionIndex = Config.Actions.Num();
		FArcTacticsActionDefinition Action;
		Action.Action.ID = FGuid(600 + ActionIndex, 1, 2, 3);
		Action.Action.Name = TEXT("PackageScript");
		Action.Action.Node.InitializeAs<FArcTacticsPackageScriptAction>();
		FArcTacticsPackageScriptActionInstanceData Defaults;
		Defaults.bAcquireHold = bAcquireHold;
		Defaults.Probe = &Probe;
		Action.Action.InstanceData.InitializeAs<FArcTacticsPackageScriptActionInstanceData>(Defaults);
		ApplySpec(Action.Parameters, Spec);
		Config.Actions.Add(Action);
	}

	void AddFocusAction(UArcTacticsConfig& Config, const FActionSpec& Spec, const FVector& Location)
	{
		const int32 ActionIndex = Config.Actions.Num();
		FArcTacticsActionDefinition Action;
		Action.Action.ID = FGuid(700 + ActionIndex, 1, 2, 3);
		Action.Action.Name = TEXT("PackageFocus");
		Action.Action.Node.InitializeAs<FArcTacticsSequenceAction>();
		Action.Action.InstanceData.InitializeAs<FArcTacticsSequenceActionInstanceData>();
		FArcTacticsNodeDefinition Step;
		Step.ID = FGuid(710 + ActionIndex, 1, 2, 3);
		Step.Name = TEXT("PackageStep");
		Step.Node.InitializeAs<FArcTacticsPackageStep>();
		Step.InstanceData.InitializeAs<FArcTacticsPackageStepInstanceData>();
		Action.Steps.Add(Step);
		FArcTacticsNodeDefinition Focus;
		Focus.ID = FGuid(720 + ActionIndex, 1, 2, 3);
		Focus.Name = TEXT("PackageFocusStep");
		Focus.Node.InitializeAs<FArcTacticsFocusActiveStep>();
		FArcTacticsFocusActiveStepInstanceData FocusDefaults;
		FocusDefaults.bUseTargetLocation = true;
		FocusDefaults.TargetLocation = Location;
		Focus.InstanceData.InitializeAs<FArcTacticsFocusActiveStepInstanceData>(FocusDefaults);
		Action.ActiveSteps.Add(Focus);
		ApplySpec(Action.Parameters, Spec);
		Config.Actions.Add(Action);
	}

	bool Finish(UArcTacticsConfig& Config, const uint32 Revision = 9)
	{
		Config.EnsureValidNodeIDs();
		for (const FArcTacticsActionDefinition& Action : Config.Actions)
		{
			if (Action.Action.Node.GetScriptStruct() == FArcTacticsPoolLifecycleAction::StaticStruct())
			{
				Config.EditorBindings.AddBinding(
					MakePath(Config.GlobalParametersID, TEXT("Captured")),
					MakePath(Action.Action.ID, TEXT("CapturedGlobal")));
			}
		}
		return Compile(Config, Revision) && Config.CompiledData.IsExecutable();
	}

	UArcTacticsConfig* MakeConfig(const TCHAR* Name, const TConstArrayView<FActionSpec> Actions)
	{
		UArcTacticsConfig* Config = NewConfig(Name);
		for (const FActionSpec& Spec : Actions)
		{
			AddLifecycleAction(*Config, Spec);
		}
		return Finish(*Config) ? Config : nullptr;
	}

	UArcTacticsConfig* MakeTaskConfig(
		const TCHAR* Name,
		const uint8 TaskMode,
		UArcTacticsPackageProbe* Probe,
		const TConstArrayView<FActionSpec> Actions,
		const bool bNeedsTeam = false)
	{
		UArcTacticsConfig* Config = NewConfig(Name);
		AddTask(*Config, TaskMode, Probe, bNeedsTeam);
		for (const FActionSpec& Spec : Actions)
		{
			AddLifecycleAction(*Config, Spec);
		}
		return Finish(*Config) ? Config : nullptr;
	}

	FActionSpec Spec(const uint8 Mode, const int32 Priority, const float Weight)
	{
		FActionSpec Result;
		Result.Mode = Mode;
		Result.Priority = Priority;
		Result.Weight = Weight;
		return Result;
	}

	FArcTacticsDecisionSettings ZeroIntervalSettings()
	{
		FArcTacticsDecisionSettings Settings;
		Settings.DecisionInterval = 0.0f;
		return Settings;
	}

	FMassEntityHandle CreateEntity(
		FMassEntityManager& EntityManager,
		const TConstArrayView<const UArcTacticsConfig*> Configs,
		const FArcTacticsDecisionSettings& Settings,
		const bool bNavigation = false)
	{
		UE::Mass::FEntityBuilder Builder = EntityManager.MakeEntityBuilder();
		Builder.Add<FArcTacticsInstanceFragment>();
		Builder.Add<FArcTacticsTag>();
		FArcTacticsConfigFragment& ConfigFragment = Builder.Add_GetRef<FArcTacticsConfigFragment>();
		for (const UArcTacticsConfig* Config : Configs)
		{
			ConfigFragment.Configs.Add(Config);
		}
		ConfigFragment.Settings = Settings;
		if (bNavigation)
		{
			Builder.Add<FTransformFragment>();
			Builder.Add<FAgentRadiusFragment>();
			Builder.Add<FAgentHeightFragment>();
			Builder.Add<FMassDesiredMovementFragment>();
			Builder.Add<FMassMoveTargetFragment>();
			Builder.Add<FMassNavMeshShortPathFragment>();
			Builder.Add<FMassNavMeshCachedPathFragment>();
			Builder.Add<FMassSteeringFragment>();
			Builder.Add<FMassStandingSteeringFragment>();
			Builder.Add<FMassGhostLocationFragment>();
			Builder.Add<FMassMovementParameters>();
			Builder.Add<FMassSmoothOrientationParameters>();
		}
		return Builder.Commit();
	}

	const FArcTacticsConfigFragment* FindConfigFragment(const FMassEntityTemplateData& TemplateData)
	{
		for (const FConstSharedStruct& Shared : TemplateData.GetSharedFragmentValues().GetConstSharedFragments())
		{
			if (Shared.GetScriptStruct() == FArcTacticsConfigFragment::StaticStruct())
			{
				return &Shared.Get<FArcTacticsConfigFragment>();
			}
		}
		return nullptr;
	}

	FMassEntityHandle CreateEntityFromTemplate(FMassEntityManager& EntityManager, const FMassEntityTemplateData& TemplateData)
	{
		const FMassArchetypeHandle Archetype = EntityManager.CreateArchetype(TemplateData.GetCompositionDescriptor());
		FMassArchetypeSharedFragmentValues SharedValues = TemplateData.GetSharedFragmentValues();
		SharedValues.Sort();
		return EntityManager.CreateEntity(Archetype, SharedValues);
	}

	void RunProcessor(UMassProcessor& Processor, FMassEntityManager& EntityManager)
	{
		FMassProcessingContext ProcessingContext(EntityManager, 0.0f);
		UE::Mass::Executor::Run(Processor, ProcessingContext);
	}

	void RunLifecycle(UWorld& World, FMassEntityManager& EntityManager)
	{
		UArcTacticsLifecycleProcessor* Processor = NewObject<UArcTacticsLifecycleProcessor>(&World);
		Processor->CallInitialize(EntityManager.GetOwner(), EntityManager.AsShared());
		RunProcessor(*Processor, EntityManager);
	}

	void DrainDeferred(UWorld& World, FMassEntityManager& EntityManager)
	{
		UArcTacticsPoolDrainProcessor* Processor = NewObject<UArcTacticsPoolDrainProcessor>(&World);
		Processor->CallInitialize(EntityManager.GetOwner(), EntityManager.AsShared());
		RunProcessor(*Processor, EntityManager);
	}

	FArcTacticsInstanceHandle GetHandle(FMassEntityManager& EntityManager, const FMassEntityHandle Entity)
	{
		const FArcTacticsInstanceFragment* Fragment = EntityManager.GetFragmentDataPtr<FArcTacticsInstanceFragment>(Entity);
		return Fragment ? Fragment->Handle : FArcTacticsInstanceHandle();
	}

	void RunDecision(
		UArcTacticsInstanceSubsystem& Pool,
		FMassEntityManager& EntityManager,
		const FArcTacticsInstanceHandle Handle,
		const ArcPathFollow::FContext* PathContext = nullptr)
	{
		UArcTacticsUtilityProcessor::RunDecision(Pool, EntityManager, Handle, 0.0f, PathContext);
	}

	int32 CountCandidates(const FArcTacticsInstanceSlot& Slot, const int32 PackageIndex)
	{
		int32 Count = 0;
		for (const FArcTacticsCandidate& Candidate : Slot.Candidates)
		{
			Count += Candidate.Option.PackageIndex == PackageIndex ? 1 : 0;
		}
		return Count;
	}

	const FArcTacticsCandidate* FindCandidate(const FArcTacticsInstanceSlot& Slot, const int32 PackageIndex)
	{
		for (const FArcTacticsCandidate& Candidate : Slot.Candidates)
		{
			if (Candidate.Option.PackageIndex == PackageIndex)
			{
				return &Candidate;
			}
		}
		return nullptr;
	}

	bool HasOutcome(
		const FArcTacticsInstanceSlot& Slot,
		const int32 PackageIndex,
		const EArcTacticsCandidateOutcome Outcome,
		const TCHAR* Reason)
	{
		const FArcTacticsCandidate* Candidate = FindCandidate(Slot, PackageIndex);
		return Candidate && Candidate->Outcome == Outcome
			&& (!Reason || Candidate->RejectionReason == FName(Reason));
	}

	bool IsActiveFrom(const FArcTacticsInstanceSlot& Slot, const int32 PackageIndex)
	{
		return Slot.ActiveAction.IsSet()
			&& Slot.Packages.IsValidIndex(PackageIndex)
			&& Slot.ActiveAction.PackageIndex == PackageIndex
			&& Slot.ActiveAction.PackageSerial == Slot.Packages[PackageIndex]->Serial;
	}

	const FArcTacticsPackage* FindPackage(const FArcTacticsInstanceSlot& Slot, const UArcTacticsConfig& Config)
	{
		const int32 PackageIndex = Slot.FindPackageIndex(Config);
		return PackageIndex != INDEX_NONE ? Slot.Packages[PackageIndex].Get() : nullptr;
	}

	const FArcMassFocusFragment* FindFocus(FMassEntityManager& EntityManager, const FMassEntityHandle Entity)
	{
		return EntityManager.GetMutableSparseElementDataForEntity(FArcMassFocusFragment::StaticStruct(), Entity)
			.GetPtr<FArcMassFocusFragment>();
	}

	TOptional<ArcPathFollow::FContext> MakePathContext(
		UWorld& World,
		FMassEntityManager& EntityManager,
		const FMassEntityHandle Entity)
	{
		const FMassEntityView EntityView(EntityManager, Entity);
		FTransformFragment* Transform = EntityView.GetFragmentDataPtr<FTransformFragment>();
		const FAgentRadiusFragment* Radius = EntityView.GetFragmentDataPtr<FAgentRadiusFragment>();
		const FAgentHeightFragment* Height = EntityView.GetFragmentDataPtr<FAgentHeightFragment>();
		const FMassDesiredMovementFragment* Desired = EntityView.GetFragmentDataPtr<FMassDesiredMovementFragment>();
		FMassMoveTargetFragment* MoveTarget = EntityView.GetFragmentDataPtr<FMassMoveTargetFragment>();
		FMassNavMeshShortPathFragment* ShortPath = EntityView.GetFragmentDataPtr<FMassNavMeshShortPathFragment>();
		FMassNavMeshCachedPathFragment* CachedPath = EntityView.GetFragmentDataPtr<FMassNavMeshCachedPathFragment>();
		const FMassMovementParameters* Movement = EntityView.GetConstSharedFragmentDataPtr<FMassMovementParameters>();
		if (!Transform || !Radius || !Height || !Desired || !MoveTarget || !ShortPath || !CachedPath || !Movement)
		{
			return TOptional<ArcPathFollow::FContext>();
		}
		return TOptional<ArcPathFollow::FContext>(ArcPathFollow::FContext{
			World, Entity, *Transform, *Radius, *Height, *Desired, *Movement,
			*MoveTarget, *ShortPath, *CachedPath, INDEX_NONE});
	}
}

TEST_CLASS(ArcTacticsPackageTests, "ArcTactics.Packages")
{
	FActorTestSpawner Spawner;

	TEST_METHOD(StaticPackages_CandidatesShareOnePoolAndSelectAcrossPackages)
	{
		using namespace ArcTacticsPackageTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

		UArcTacticsConfig* LowScore = MakeConfig(TEXT("LowScore"), {Spec(2, 0, 0.3f)});
		UArcTacticsConfig* HighScore = MakeConfig(TEXT("HighScore"), {Spec(2, 0, 0.8f)});
		UArcTacticsConfig* HighBucket = MakeConfig(TEXT("HighBucket"), {Spec(2, 1, 0.2f)});
		UArcTacticsConfig* LowBucket = MakeConfig(TEXT("LowBucket"), {Spec(2, 0, 0.9f)});
		UArcTacticsConfig* TieFirst = MakeConfig(TEXT("TieFirst"), {Spec(2, 0, 0.5f)});
		UArcTacticsConfig* TieSecond = MakeConfig(TEXT("TieSecond"), {Spec(2, 0, 0.5f)});
		ASSERT_THAT(IsTrue(LowScore && HighScore && HighBucket && LowBucket && TieFirst && TieSecond));

		const FMassEntityHandle ScoreEntity = CreateEntity(EntityManager, {LowScore, HighScore}, ZeroIntervalSettings());
		const FMassEntityHandle BucketEntity = CreateEntity(EntityManager, {LowBucket, HighBucket}, ZeroIntervalSettings());
		const FMassEntityHandle TieEntity = CreateEntity(EntityManager, {TieFirst, TieSecond}, ZeroIntervalSettings());
		RunLifecycle(World, EntityManager);

		const FArcTacticsInstanceHandle ScoreHandle = GetHandle(EntityManager, ScoreEntity);
		FArcTacticsInstanceSlot* ScoreSlot = Pool->Resolve(ScoreHandle);
		ASSERT_THAT(IsNotNull(ScoreSlot));
		ASSERT_THAT(AreEqual(2, ScoreSlot->Packages.Num()));
		ASSERT_THAT(IsTrue(ScoreSlot->bDecisionRequested, TEXT("Allocation requests a decision")));
		RunDecision(*Pool, EntityManager, ScoreHandle);
		ASSERT_THAT(AreEqual(EArcTacticsDecisionStatus::Succeeded, ScoreSlot->DecisionStatus));
		ASSERT_THAT(AreEqual(2, ScoreSlot->Candidates.Num(), TEXT("Both packages contribute to one pool")));
		ASSERT_THAT(AreEqual(1, CountCandidates(*ScoreSlot, 0)));
		ASSERT_THAT(AreEqual(1, CountCandidates(*ScoreSlot, 1)));
		ASSERT_THAT(IsTrue(HasOutcome(*ScoreSlot, 0, EArcTacticsCandidateOutcome::Accepted, nullptr)));
		ASSERT_THAT(IsTrue(HasOutcome(*ScoreSlot, 1, EArcTacticsCandidateOutcome::Accepted, nullptr)));
		ASSERT_THAT(IsTrue(FindCandidate(*ScoreSlot, 0)->Order < FindCandidate(*ScoreSlot, 1)->Order,
			TEXT("Candidate order is global and follows package list order")));
		ASSERT_THAT(IsTrue(IsActiveFrom(*ScoreSlot, 1), TEXT("The higher score of the second package wins")));

		const FArcTacticsInstanceHandle BucketHandle = GetHandle(EntityManager, BucketEntity);
		FArcTacticsInstanceSlot* BucketSlot = Pool->Resolve(BucketHandle);
		ASSERT_THAT(IsNotNull(BucketSlot));
		RunDecision(*Pool, EntityManager, BucketHandle);
		ASSERT_THAT(AreEqual(2, BucketSlot->Candidates.Num()));
		ASSERT_THAT(IsTrue(IsActiveFrom(*BucketSlot, 1),
			TEXT("The higher priority bucket of the second package wins over a higher score in the first")));

		const FArcTacticsInstanceHandle TieHandle = GetHandle(EntityManager, TieEntity);
		FArcTacticsInstanceSlot* TieSlot = Pool->Resolve(TieHandle);
		ASSERT_THAT(IsNotNull(TieSlot));
		RunDecision(*Pool, EntityManager, TieHandle);
		ASSERT_THAT(IsTrue(IsActiveFrom(*TieSlot, 0), TEXT("Equal scores break ties by package list order")));
	}

	TEST_METHOD(AddConfig_LiveEntityActivatesPackageAndRequestsDecision)
	{
		using namespace ArcTacticsPackageTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		UArcTacticsConfig* Static = MakeConfig(TEXT("Static"), {Spec(2, 0, 0.3f)});
		UArcTacticsConfig* Granted = MakeConfig(TEXT("Granted"), {Spec(2, 1, 0.5f)});
		ASSERT_THAT(IsTrue(Static && Granted));
		const FMassEntityHandle Entity = CreateEntity(EntityManager, {Static}, ZeroIntervalSettings());
		RunLifecycle(World, EntityManager);
		const FArcTacticsInstanceHandle Handle = GetHandle(EntityManager, Entity);
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(IsTrue(IsActiveFrom(*Slot, 0)));
		ASSERT_THAT(IsFalse(Slot->bDecisionRequested));
		const uint32 Generation = Slot->DecisionGeneration;

		ASSERT_THAT(IsTrue(Pool->AddConfig(Entity, *Granted)));
		ASSERT_THAT(AreEqual(2, Slot->Packages.Num()));
		const FArcTacticsPackage& Package = *Slot->Packages[1];
		ASSERT_THAT(IsTrue(Package.Config == Granted));
		ASSERT_THAT(IsTrue(Package.bActive && Package.Definitions.IsValid()));
		ASSERT_THAT(AreEqual(1, Package.RuntimeCount));
		ASSERT_THAT(AreEqual(0, Package.StaticCount));
		ASSERT_THAT(IsTrue(Slot->bDecisionRequested, TEXT("Adding an active package requests a decision")));

		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(IsTrue(Slot->DecisionGeneration != Generation));
		ASSERT_THAT(AreEqual(1, CountCandidates(*Slot, 1), TEXT("The granted package's candidates appear")));
		ASSERT_THAT(IsTrue(HasOutcome(*Slot, 1, EArcTacticsCandidateOutcome::Accepted, nullptr)));
		ASSERT_THAT(IsTrue(IsActiveFrom(*Slot, 1), TEXT("The granted higher priority action replaces the static one")));
	}

	TEST_METHOD(AddConfig_DuringRunningDecisionCancelsAndRestartsIt)
	{
		using namespace ArcTacticsPackageTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		UArcTacticsPackageProbe* TaskProbe = NewObject<UArcTacticsPackageProbe>(GetTransientPackage());
		UArcTacticsConfig* Static = MakeTaskConfig(TEXT("RunningStatic"), 1, TaskProbe, {Spec(2, 0, 0.3f)});
		UArcTacticsConfig* Granted = MakeConfig(TEXT("Granted"), {Spec(2, 0, 0.8f)});
		ASSERT_THAT(IsTrue(Static && Granted));
		const FMassEntityHandle Entity = CreateEntity(EntityManager, {Static}, ZeroIntervalSettings());
		RunLifecycle(World, EntityManager);
		const FArcTacticsInstanceHandle Handle = GetHandle(EntityManager, Entity);
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(AreEqual(EArcTacticsDecisionStatus::Running, Slot->DecisionStatus));
		ASSERT_THAT(AreEqual(1, TaskProbe->EnterCount));
		const uint32 RunningGeneration = Slot->DecisionGeneration;

		ASSERT_THAT(IsTrue(Pool->AddConfig(Entity, *Granted)));
		ASSERT_THAT(AreEqual(1, TaskProbe->CancelCount, TEXT("The running global task is cancelled")));
		ASSERT_THAT(AreEqual(EArcTacticsDecisionStatus::Cancelled, Slot->DecisionStatus));
		ASSERT_THAT(IsTrue(Slot->bDecisionRequested));

		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(IsTrue(Slot->DecisionGeneration != RunningGeneration, TEXT("A new decision begins")));
		ASSERT_THAT(AreEqual(2, TaskProbe->EnterCount, TEXT("The restarted decision runs the global task again")));
		ASSERT_THAT(AreEqual(EArcTacticsDecisionStatus::Running, Slot->DecisionStatus));
		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(AreEqual(EArcTacticsDecisionStatus::Succeeded, Slot->DecisionStatus));
		ASSERT_THAT(AreEqual(1, CountCandidates(*Slot, 0)));
		ASSERT_THAT(AreEqual(1, CountCandidates(*Slot, 1), TEXT("The restarted decision includes the new package")));
		ASSERT_THAT(IsTrue(IsActiveFrom(*Slot, 1)));
	}

	TEST_METHOD(AddRemove_CountsAndRejections)
	{
		using namespace ArcTacticsPackageTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		UArcTacticsConfig* Static = MakeConfig(TEXT("Static"), {Spec(2, 0, 0.3f)});
		UArcTacticsConfig* Granted = MakeConfig(TEXT("Granted"), {Spec(2, 0, 0.5f)});
		UArcTacticsConfig* Never = MakeConfig(TEXT("Never"), {Spec(2, 0, 0.5f)});
		ASSERT_THAT(IsTrue(Static && Granted && Never));
		const FMassEntityHandle Entity = CreateEntity(EntityManager, {Static, Static}, ZeroIntervalSettings());
		RunLifecycle(World, EntityManager);
		const FArcTacticsInstanceHandle Handle = GetHandle(EntityManager, Entity);
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		ASSERT_THAT(AreEqual(1, Slot->Packages.Num(), TEXT("A config listed twice is one package")));
		ASSERT_THAT(AreEqual(2, Slot->Packages[0]->StaticCount));

		ASSERT_THAT(IsTrue(Pool->AddConfig(Entity, *Granted)));
		ASSERT_THAT(IsTrue(Pool->AddConfig(Entity, *Granted)));
		ASSERT_THAT(AreEqual(2, Slot->Packages.Num(), TEXT("The same config added twice is one package")));
		const FArcTacticsPackage* GrantedPackage = FindPackage(*Slot, *Granted);
		ASSERT_THAT(IsNotNull(GrantedPackage));
		ASSERT_THAT(AreEqual(2, GrantedPackage->RuntimeCount));
		const uint32 GrantedSerial = GrantedPackage->Serial;

		ASSERT_THAT(IsTrue(Pool->RemoveConfig(Entity, *Granted)));
		GrantedPackage = FindPackage(*Slot, *Granted);
		ASSERT_THAT(IsNotNull(GrantedPackage, TEXT("One removal of two grants keeps the package")));
		ASSERT_THAT(AreEqual(1, GrantedPackage->RuntimeCount));
		ASSERT_THAT(AreEqual(GrantedSerial, GrantedPackage->Serial));
		ASSERT_THAT(IsTrue(Pool->RemoveConfig(Entity, *Granted)));
		ASSERT_THAT(IsNull(FindPackage(*Slot, *Granted), TEXT("The second removal removes the package")));
		ASSERT_THAT(AreEqual(1, Slot->Packages.Num()));

		ASSERT_THAT(IsTrue(Pool->AddConfig(Entity, *Static)));
		ASSERT_THAT(AreEqual(1, Slot->Packages.Num()));
		ASSERT_THAT(AreEqual(1, Slot->Packages[0]->RuntimeCount));
		ASSERT_THAT(IsTrue(Pool->RemoveConfig(Entity, *Static)));
		ASSERT_THAT(AreEqual(1, Slot->Packages.Num(), TEXT("Removing the runtime grant of a static config keeps the package")));
		ASSERT_THAT(AreEqual(0, Slot->Packages[0]->RuntimeCount));
		ASSERT_THAT(AreEqual(2, Slot->Packages[0]->StaticCount));

		TestRunner->AddExpectedError(TEXT("was not added at runtime"), EAutomationExpectedErrorFlags::Contains, 3);
		ASSERT_THAT(IsFalse(Pool->RemoveConfig(Entity, *Static), TEXT("A trait-only config cannot be removed")));
		ASSERT_THAT(IsFalse(Pool->RemoveConfig(Entity, *Never), TEXT("A never-added config cannot be removed")));
		ASSERT_THAT(IsFalse(Pool->RemoveConfig(Entity, *Granted), TEXT("A fully removed config cannot be removed again")));
		ASSERT_THAT(AreEqual(1, Slot->Packages.Num()));
		ASSERT_THAT(AreEqual(2, Slot->Packages[0]->StaticCount));

		UE::Mass::FEntityBuilder Builder = EntityManager.MakeEntityBuilder();
		Builder.Add<FArcTacticsInstanceFragment>();
		const FMassEntityHandle Untracked = Builder.Commit();
		TestRunner->AddExpectedError(TEXT("has no tactics instance"), EAutomationExpectedErrorFlags::Contains, 2);
		ASSERT_THAT(IsFalse(Pool->AddConfig(Untracked, *Granted)));
		ASSERT_THAT(IsFalse(Pool->RemoveConfig(Untracked, *Granted)));
	}

	TEST_METHOD(RemoveConfig_ActivePackageCancelsActionReleasesHoldAndRequestsDecision)
	{
		using namespace ArcTacticsPackageTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		UArcTacticsPackageProbe* Probe = NewObject<UArcTacticsPackageProbe>(GetTransientPackage());
		UArcTacticsConfig* Static = MakeConfig(TEXT("Static"), {Spec(2, 0, 0.2f)});
		UArcTacticsConfig* Holding = NewConfig(TEXT("Holding"));
		FActionSpec HoldingSpec = Spec(2, 0, 0.9f);
		HoldingSpec.Cooldown = 50.0f;
		AddScriptAction(*Holding, HoldingSpec, *Probe, true);
		ASSERT_THAT(IsTrue(Static && Finish(*Holding)));
		const FMassEntityHandle Entity = CreateEntity(EntityManager, {Static}, ZeroIntervalSettings(), true);
		RunLifecycle(World, EntityManager);
		const FArcTacticsInstanceHandle Handle = GetHandle(EntityManager, Entity);
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		ASSERT_THAT(IsTrue(Pool->AddConfig(Entity, *Holding)));
		TOptional<ArcPathFollow::FContext> PathContext = MakePathContext(World, EntityManager, Entity);
		ASSERT_THAT(IsTrue(PathContext.IsSet()));
		RunDecision(*Pool, EntityManager, Handle, PathContext.GetPtrOrNull());
		ASSERT_THAT(IsTrue(IsActiveFrom(*Slot, 1)));
		ASSERT_THAT(AreEqual(1, Probe->EnterCount));
		ASSERT_THAT(IsTrue(Slot->MovementHold.bOwned, TEXT("The granted action holds movement")));
		const FMassMoveTargetFragment* MoveTarget = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(Entity);
		ASSERT_THAT(IsNotNull(MoveTarget));
		ASSERT_THAT(AreEqual(EMassMovementAction::Animate, MoveTarget->GetCurrentAction()));
		ASSERT_THAT(IsFalse(Slot->bDecisionRequested));
		const double RemoveTime = World.GetTimeSeconds();

		ASSERT_THAT(IsTrue(Pool->RemoveConfig(Entity, *Holding)));
		ASSERT_THAT(AreEqual(1, Probe->CancelCount, TEXT("Removing the package cancels its active action")));
		ASSERT_THAT(AreEqual(EArcTacticsActionCancelMode::ReleaseHold, Probe->LastCancelMode));
		ASSERT_THAT(IsFalse(Slot->ActiveAction.IsSet()));
		ASSERT_THAT(IsFalse(Slot->MovementHold.bOwned, TEXT("Removing the package releases the hold")));
		ASSERT_THAT(AreEqual(EMassMovementAction::Stand, MoveTarget->GetCurrentAction(),
			TEXT("The released hold returns the entity to Stand")));
		const double* Stamp = Slot->CooldownEndTimes.Find(FArcTacticsCooldownKey(Holding, 0));
		ASSERT_THAT(IsTrue(Stamp && FMath::IsNearlyEqual(RemoveTime + 50.0, *Stamp, 0.001),
			TEXT("The removal is recorded as an action end and stamps the OnEnd cooldown")));
		ASSERT_THAT(AreEqual(1, Slot->Packages.Num()));
		ASSERT_THAT(IsNull(FindPackage(*Slot, *Holding)));
		ASSERT_THAT(IsTrue(Slot->bDecisionRequested, TEXT("Removing a package requests a decision")));
		FArcTacticsDebugSnapshot Snapshot;
		ASSERT_THAT(IsTrue(ArcTacticsDebug::BuildEntitySnapshot(World, Entity, Snapshot)));
		const FArcTacticsDebugEndedAction& Ended = Snapshot.LastEndedAction;
		ASSERT_THAT(IsTrue(Ended.bSet, TEXT("The snapshot records the removed package's action end")));
		ASSERT_THAT(AreEqual(Holding->GetName(), Ended.Package));
		ASSERT_THAT(IsTrue(Ended.Status == EArcTacticsActionEndStatus::Cancelled));
		ASSERT_THAT(IsTrue(Ended.Reason == EArcTacticsActionEndReason::PackageRemoved));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(RemoveTime, Ended.Time, 0.001)));

		RunDecision(*Pool, EntityManager, Handle, PathContext.GetPtrOrNull());
		ASSERT_THAT(IsTrue(IsActiveFrom(*Slot, 0)));
		ASSERT_THAT(AreEqual(1, Probe->CancelCount));
	}

	TEST_METHOD(RemoveConfig_OtherPackageKeepsActiveActionAndReindexesIt)
	{
		using namespace ArcTacticsPackageTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		UArcTacticsConfig* First = MakeConfig(TEXT("First"), {Spec(2, 0, 0.3f)});
		UArcTacticsConfig* Second = MakeConfig(TEXT("Second"), {Spec(2, 0, 0.9f)});
		UArcTacticsConfig* Third = MakeConfig(TEXT("Third"), {Spec(2, 0, 0.2f)});
		ASSERT_THAT(IsTrue(First && Second && Third));
		const FMassEntityHandle Entity = CreateEntity(EntityManager, {}, ZeroIntervalSettings());
		RunLifecycle(World, EntityManager);
		const FArcTacticsInstanceHandle Handle = GetHandle(EntityManager, Entity);
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		ASSERT_THAT(IsTrue(Pool->AddConfig(Entity, *First)));
		ASSERT_THAT(IsTrue(Pool->AddConfig(Entity, *Second)));
		ASSERT_THAT(IsTrue(Pool->AddConfig(Entity, *Third)));
		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(IsTrue(IsActiveFrom(*Slot, 1)));
		const uint32 ActionSerial = Slot->ActiveAction.Serial;

		ASSERT_THAT(IsTrue(Pool->RemoveConfig(Entity, *First)));
		ASSERT_THAT(AreEqual(2, Slot->Packages.Num()));
		ASSERT_THAT(IsTrue(Slot->ActiveAction.IsSet(), TEXT("An action of another package keeps running")));
		ASSERT_THAT(AreEqual(ActionSerial, Slot->ActiveAction.Serial));
		ASSERT_THAT(IsTrue(IsActiveFrom(*Slot, 0), TEXT("The active action's package index follows the removal")));
		UArcTacticsUtilityProcessor::TickAction(
			*Pool, EntityManager, Handle, EArcTacticsActionTickLane::Utility, true, nullptr);
		const FArcTacticsPoolLifecycleActionInstanceData* Data =
			Slot->ActiveAction.ActionData.GetPtr<FArcTacticsPoolLifecycleActionInstanceData>();
		ASSERT_THAT(IsNotNull(Data));
		ASSERT_THAT(AreEqual(1, Data->TickCount, TEXT("The reindexed action still resolves and ticks")));

		ASSERT_THAT(IsTrue(Pool->RemoveConfig(Entity, *Third)));
		ASSERT_THAT(AreEqual(1, Slot->Packages.Num()));
		ASSERT_THAT(IsTrue(Slot->ActiveAction.IsSet()));
		ASSERT_THAT(AreEqual(ActionSerial, Slot->ActiveAction.Serial));
		ASSERT_THAT(AreEqual(0, Slot->ActiveAction.PackageIndex,
			TEXT("Removing a package after the active one leaves the active package index unchanged")));
		ASSERT_THAT(IsTrue(IsActiveFrom(*Slot, 0)));
		UArcTacticsUtilityProcessor::TickAction(
			*Pool, EntityManager, Handle, EArcTacticsActionTickLane::Utility, true, nullptr);
		Data = Slot->ActiveAction.ActionData.GetPtr<FArcTacticsPoolLifecycleActionInstanceData>();
		ASSERT_THAT(IsNotNull(Data));
		ASSERT_THAT(AreEqual(2, Data->TickCount, TEXT("The action still resolves and ticks after a later package is removed")));
	}

	TEST_METHOD(RemoveConfig_DuringRunningGlobalTaskCancelsDecisionAndIgnoresStaleCompletion)
	{
		using namespace ArcTacticsPackageTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		UArcTacticsPackageProbe* RemovedProbe = NewObject<UArcTacticsPackageProbe>(GetTransientPackage());
		UArcTacticsPackageProbe* ReplacementProbe = NewObject<UArcTacticsPackageProbe>(GetTransientPackage());
		UArcTacticsConfig* Static = MakeConfig(TEXT("Static"), {Spec(2, 0, 0.3f)});
		UArcTacticsConfig* Removed = MakeTaskConfig(TEXT("Removed"), 1, RemovedProbe, {Spec(2, 0, 0.5f)});
		UArcTacticsConfig* Replacement = MakeTaskConfig(TEXT("Replacement"), 1, ReplacementProbe, {Spec(2, 0, 0.5f)});
		ASSERT_THAT(IsTrue(Static && Removed && Replacement));
		const FMassEntityHandle Entity = CreateEntity(EntityManager, {Static}, ZeroIntervalSettings());
		RunLifecycle(World, EntityManager);
		const FArcTacticsInstanceHandle Handle = GetHandle(EntityManager, Entity);
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		ASSERT_THAT(IsTrue(Pool->AddConfig(Entity, *Removed)));
		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(AreEqual(EArcTacticsDecisionStatus::Running, Slot->DecisionStatus));
		ASSERT_THAT(AreEqual(1, Slot->PackageCursor, TEXT("The decision waits on the second package's task")));
		ASSERT_THAT(AreEqual(1, RemovedProbe->EnterCount));

		FArcTacticsAsyncRequestToken StaleToken;
		StaleToken.InstanceSubsystem = Pool;
		StaleToken.Entity = Entity;
		StaleToken.InstanceHandle = Handle;
		StaleToken.DecisionGeneration = Slot->DecisionGeneration;

		ASSERT_THAT(IsTrue(Pool->RemoveConfig(Entity, *Removed)));
		ASSERT_THAT(AreEqual(1, RemovedProbe->CancelCount, TEXT("The running task of the removed package is cancelled")));
		ASSERT_THAT(AreEqual(EArcTacticsDecisionStatus::Cancelled, Slot->DecisionStatus));
		ASSERT_THAT(AreEqual(1, Slot->Packages.Num()));
		ASSERT_THAT(IsTrue(Slot->bDecisionRequested));
		FArcTQSQueryInstance CompletedQuery;
		Pool->CompleteTaskQuery(StaleToken, CompletedQuery);

		ASSERT_THAT(IsTrue(Pool->AddConfig(Entity, *Replacement)));
		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(AreEqual(EArcTacticsDecisionStatus::Running, Slot->DecisionStatus));
		ASSERT_THAT(AreEqual(1, Slot->PackageCursor));
		ASSERT_THAT(AreEqual(1, ReplacementProbe->EnterCount));
		Pool->CompleteTaskQuery(StaleToken, CompletedQuery);
		ASSERT_THAT(AreEqual(0, ReplacementProbe->CallbackCount,
			TEXT("A completion from the cancelled decision does not reach the package now at its cursor")));

		FArcTacticsAsyncRequestToken FreshToken = StaleToken;
		FreshToken.DecisionGeneration = Slot->DecisionGeneration;
		Pool->CompleteTaskQuery(FreshToken, CompletedQuery);
		ASSERT_THAT(AreEqual(1, ReplacementProbe->CallbackCount, TEXT("A completion of the current decision is delivered")));
	}

	TEST_METHOD(Reentrancy_ChangesFromActionCallbacksAreQueuedAndAppliedInOrder)
	{
		using namespace ArcTacticsPackageTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		UArcTacticsPackageProbe* Probe = NewObject<UArcTacticsPackageProbe>(GetTransientPackage());
		UArcTacticsConfig* Static = MakeConfig(TEXT("Static"), {Spec(2, 0, 0.2f)});
		UArcTacticsConfig* QueuedAdd = MakeConfig(TEXT("QueuedAdd"), {Spec(2, 0, 0.2f)});
		UArcTacticsConfig* NestedAdd = MakeConfig(TEXT("NestedAdd"), {Spec(2, 0, 0.2f)});
		UArcTacticsConfig* Scripted = NewConfig(TEXT("Scripted"));
		AddScriptAction(*Scripted, Spec(2, 0, 0.9f), *Probe, false);
		ASSERT_THAT(IsTrue(Static && QueuedAdd && NestedAdd && Finish(*Scripted)));
		Probe->TickConfigs = {QueuedAdd, Scripted, Scripted};
		Probe->TickAdds = {true, false, false};
		Probe->CancelConfigs = {NestedAdd, QueuedAdd, QueuedAdd};
		Probe->CancelAdds = {true, false, false};

		const FMassEntityHandle Entity = CreateEntity(EntityManager, {Static}, ZeroIntervalSettings());
		RunLifecycle(World, EntityManager);
		const FArcTacticsInstanceHandle Handle = GetHandle(EntityManager, Entity);
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		ASSERT_THAT(IsTrue(Pool->AddConfig(Entity, *Scripted)));
		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(IsTrue(IsActiveFrom(*Slot, 1)));
		ASSERT_THAT(AreEqual(1, Probe->EnterCount));

		TestRunner->AddExpectedError(TEXT("was not added at runtime"), EAutomationExpectedErrorFlags::Contains, 2);
		UArcTacticsUtilityProcessor::TickAction(
			*Pool, EntityManager, Handle, EArcTacticsActionTickLane::Utility, true, nullptr);
		ASSERT_THAT(AreEqual(1, Probe->TickCount));
		ASSERT_THAT(AreEqual(3, Probe->TickResults.Num()));
		ASSERT_THAT(IsTrue(Probe->TickResults[0], TEXT("A queued add is accepted")));
		ASSERT_THAT(IsTrue(Probe->TickResults[1], TEXT("A queued removal of a count of one is accepted")));
		ASSERT_THAT(IsFalse(Probe->TickResults[2],
			TEXT("A second removal is validated against the pending removal and rejected at enqueue")));
		ASSERT_THAT(AreEqual(2, Probe->PackagesDuringTick, TEXT("The package list does not change inside the callback")));
		ASSERT_THAT(AreEqual(2, Probe->PendingDuringTick, TEXT("Only the accepted changes are queued")));

		ASSERT_THAT(AreEqual(1, Probe->CancelCount, TEXT("The queued removal cancels the active action after the tick")));
		ASSERT_THAT(AreEqual(EArcTacticsActionCancelMode::ReleaseHold, Probe->LastCancelMode));
		ASSERT_THAT(AreEqual(3, Probe->PackagesDuringCancel,
			TEXT("The queued add was applied before the queued removal ran its cancel")));
		ASSERT_THAT(AreEqual(3, Probe->CancelResults.Num()));
		ASSERT_THAT(IsTrue(Probe->CancelResults[0], TEXT("An add from inside the drain is queued")));
		ASSERT_THAT(IsTrue(Probe->CancelResults[1],
			TEXT("A removal of the config whose queued add was already applied is accepted against its count of one")));
		ASSERT_THAT(IsFalse(Probe->CancelResults[2],
			TEXT("A second removal is rejected: the already-applied queued add is not counted again")));

		ASSERT_THAT(AreEqual(2, Slot->Packages.Num()));
		ASSERT_THAT(IsTrue(Slot->Packages[0]->Config == Static));
		ASSERT_THAT(IsTrue(Slot->Packages[1]->Config == NestedAdd,
			TEXT("The add queued during the drain is applied in the same drain")));
		ASSERT_THAT(IsNull(FindPackage(*Slot, *Scripted)));
		ASSERT_THAT(IsNull(FindPackage(*Slot, *QueuedAdd),
			TEXT("The removal queued during the drain is applied in the same drain")));
		ASSERT_THAT(AreEqual(1, Slot->Packages[1]->RuntimeCount));
		ASSERT_THAT(IsFalse(Slot->ActiveAction.IsSet()));
		ASSERT_THAT(AreEqual(0, Slot->PendingPackageChanges.Num()));
		ASSERT_THAT(AreEqual(0, Slot->ExecutionDepth));
		ASSERT_THAT(IsTrue(Slot->bDecisionRequested));
	}

	TEST_METHOD(Reentrancy_RemovalsFromTheApplyingAddDoNotCountItTwice)
	{
		using namespace ArcTacticsPackageTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		UArcTacticsPackageProbe* TaskProbe = NewObject<UArcTacticsPackageProbe>(GetTransientPackage());
		UArcTacticsConfig* Static = MakeTaskConfig(TEXT("RunningStatic"), 1, TaskProbe, {Spec(2, 0, 0.3f)});
		UArcTacticsConfig* Granted = MakeConfig(TEXT("Granted"), {Spec(2, 0, 0.8f)});
		ASSERT_THAT(IsTrue(Static && Granted));
		TaskProbe->CallbackConfigs = {Granted};
		TaskProbe->CallbackAdds = {true};
		TaskProbe->CancelConfigs = {Granted, Granted};
		TaskProbe->CancelAdds = {false, false};

		const FMassEntityHandle Entity = CreateEntity(EntityManager, {Static}, ZeroIntervalSettings());
		RunLifecycle(World, EntityManager);
		const FArcTacticsInstanceHandle Handle = GetHandle(EntityManager, Entity);
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(AreEqual(EArcTacticsDecisionStatus::Running, Slot->DecisionStatus));

		FArcTacticsAsyncRequestToken Token;
		Token.InstanceSubsystem = Pool;
		Token.Entity = Entity;
		Token.InstanceHandle = Handle;
		Token.DecisionGeneration = Slot->DecisionGeneration;
		FArcTQSQueryInstance CompletedQuery;
		TestRunner->AddExpectedError(TEXT("was not added at runtime"), EAutomationExpectedErrorFlags::Contains, 1);
		Pool->CompleteTaskQuery(Token, CompletedQuery);

		ASSERT_THAT(AreEqual(1, TaskProbe->CallbackCount));
		ASSERT_THAT(AreEqual(1, TaskProbe->CallbackResults.Num()));
		ASSERT_THAT(IsTrue(TaskProbe->CallbackResults[0], TEXT("An add from inside the completion is queued")));
		ASSERT_THAT(AreEqual(1, TaskProbe->PackagesDuringCallback, TEXT("The queued add is not applied inside the callback")));
		ASSERT_THAT(AreEqual(1, TaskProbe->CancelCount, TEXT("Applying the queued add cancels the running global task")));
		ASSERT_THAT(AreEqual(2, TaskProbe->PackagesDuringCancel, TEXT("The cancel runs while the queued add is being applied")));
		ASSERT_THAT(AreEqual(2, TaskProbe->CancelResults.Num()));
		ASSERT_THAT(IsTrue(TaskProbe->CancelResults[0],
			TEXT("A removal from inside the applying add is accepted against its count of one")));
		ASSERT_THAT(IsFalse(TaskProbe->CancelResults[1],
			TEXT("A second removal is rejected: the add being applied is not counted again as pending")));

		ASSERT_THAT(AreEqual(1, Slot->Packages.Num()));
		ASSERT_THAT(IsNull(FindPackage(*Slot, *Granted), TEXT("The accepted removal is applied in the same drain")));
		ASSERT_THAT(AreEqual(0, Slot->PendingPackageChanges.Num()));
		ASSERT_THAT(AreEqual(0, Slot->ExecutionDepth));
		ASSERT_THAT(AreEqual(EArcTacticsDecisionStatus::Cancelled, Slot->DecisionStatus));
		ASSERT_THAT(IsTrue(Slot->bDecisionRequested));
	}

	TEST_METHOD(Exclusion_FailingPackageIsExcludedAndAllExcludedFails)
	{
		using namespace ArcTacticsPackageTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		UArcTacticsConfig* Failing = MakeTaskConfig(TEXT("Failing"), 2, nullptr, {Spec(2, 0, 0.9f)});
		UArcTacticsConfig* OtherFailing = MakeTaskConfig(TEXT("OtherFailing"), 2, nullptr, {Spec(2, 0, 0.9f)});
		UArcTacticsConfig* Healthy = MakeConfig(TEXT("Healthy"), {Spec(2, 0, 0.3f)});
		ASSERT_THAT(IsTrue(Failing && OtherFailing && Healthy));
		const FMassEntityHandle MixedEntity = CreateEntity(EntityManager, {Failing, Healthy}, ZeroIntervalSettings());
		const FMassEntityHandle FailedEntity = CreateEntity(EntityManager, {Failing, OtherFailing}, ZeroIntervalSettings());
		RunLifecycle(World, EntityManager);

		const FArcTacticsInstanceHandle MixedHandle = GetHandle(EntityManager, MixedEntity);
		FArcTacticsInstanceSlot* MixedSlot = Pool->Resolve(MixedHandle);
		ASSERT_THAT(IsNotNull(MixedSlot));
		RunDecision(*Pool, EntityManager, MixedHandle);
		ASSERT_THAT(AreEqual(EArcTacticsDecisionStatus::Succeeded, MixedSlot->DecisionStatus));
		ASSERT_THAT(IsTrue(MixedSlot->Packages[0]->bExcluded));
		ASSERT_THAT(IsTrue(MixedSlot->Packages[0]->Instance.Diagnostic == TEXT("GlobalTaskFailed"),
			TEXT("The excluded package records its own diagnostic")));
		ASSERT_THAT(IsFalse(MixedSlot->Packages[1]->bExcluded));
		ASSERT_THAT(AreEqual(0, CountCandidates(*MixedSlot, 0), TEXT("An excluded package contributes no candidates")));
		ASSERT_THAT(AreEqual(1, CountCandidates(*MixedSlot, 1)));
		ASSERT_THAT(IsTrue(IsActiveFrom(*MixedSlot, 1), TEXT("The other package still selects")));

		const FArcTacticsInstanceHandle FailedHandle = GetHandle(EntityManager, FailedEntity);
		FArcTacticsInstanceSlot* FailedSlot = Pool->Resolve(FailedHandle);
		ASSERT_THAT(IsNotNull(FailedSlot));
		RunDecision(*Pool, EntityManager, FailedHandle);
		ASSERT_THAT(AreEqual(EArcTacticsDecisionStatus::Failed, FailedSlot->DecisionStatus,
			TEXT("Excluding every package fails the decision")));
		ASSERT_THAT(IsTrue(FailedSlot->Diagnostic == TEXT("GlobalTaskFailed")));
		ASSERT_THAT(AreEqual(0, FailedSlot->Candidates.Num()));
		ASSERT_THAT(IsFalse(FailedSlot->ActiveAction.IsSet()));
		ASSERT_THAT(IsTrue(FailedSlot->bDecisionRequested, TEXT("A failed decision without an active action retries")));
	}

	TEST_METHOD(ZeroPackages_StaysIdleWithoutRetry)
	{
		using namespace ArcTacticsPackageTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		UArcTacticsConfig* Granted = MakeConfig(TEXT("Granted"), {Spec(2, 0, 0.5f)});
		ASSERT_THAT(IsTrue(Granted != nullptr));
		const FMassEntityHandle Entity = CreateEntity(EntityManager, {}, ZeroIntervalSettings());
		RunLifecycle(World, EntityManager);
		const FArcTacticsInstanceHandle Handle = GetHandle(EntityManager, Entity);
		ASSERT_THAT(IsTrue(Handle.IsValid(), TEXT("An empty Configs list still gets a slot")));
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		ASSERT_THAT(AreEqual(0, Slot->Packages.Num()));
		ASSERT_THAT(IsFalse(Slot->bDecisionRequested, TEXT("A slot with no active package records no request")));
		Pool->RequestDecision(Handle);
		ASSERT_THAT(IsFalse(Slot->bDecisionRequested));

		Slot->bDecisionRequested = true;
		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(AreEqual(EArcTacticsDecisionStatus::Idle, Slot->DecisionStatus));
		ASSERT_THAT(AreEqual(0u, Slot->DecisionGeneration, TEXT("No decision runs")));
		ASSERT_THAT(IsFalse(Slot->bDecisionRequested, TEXT("No retry is scheduled")));
		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(AreEqual(0u, Slot->DecisionGeneration));

		ASSERT_THAT(IsTrue(Pool->AddConfig(Entity, *Granted)));
		ASSERT_THAT(IsTrue(Slot->bDecisionRequested, TEXT("AddConfig requests the first decision")));
		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(IsTrue(IsActiveFrom(*Slot, 0)));
	}

	TEST_METHOD(Requirements_MissingFragmentLeavesPackageInactiveWithoutReinitLoop)
	{
		using namespace ArcTacticsPackageTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		UArcTacticsConfig* Healthy = MakeConfig(TEXT("Healthy"), {Spec(2, 0, 0.3f)});
		UArcTacticsConfig* NeedsTeam = MakeTaskConfig(TEXT("NeedsTeam"), 0, nullptr, {Spec(2, 0, 0.9f)}, true);
		UArcTacticsConfig* GrantedNeedsTeam = MakeTaskConfig(TEXT("GrantedNeedsTeam"), 0, nullptr, {Spec(2, 0, 0.9f)}, true);
		ASSERT_THAT(IsTrue(Healthy && NeedsTeam && GrantedNeedsTeam));
		TestRunner->AddExpectedError(TEXT("needs ArcMassTeamFragment; package not activated"),
			EAutomationExpectedErrorFlags::Contains, 2);
		const FMassEntityHandle Entity = CreateEntity(EntityManager, {NeedsTeam, Healthy}, ZeroIntervalSettings());
		RunLifecycle(World, EntityManager);
		const FArcTacticsInstanceHandle Handle = GetHandle(EntityManager, Entity);
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		ASSERT_THAT(AreEqual(2, Slot->Packages.Num(), TEXT("The inactive package stays listed")));
		ASSERT_THAT(IsFalse(Slot->Packages[0]->bActive));
		ASSERT_THAT(IsFalse(Slot->Packages[0]->Definitions.IsValid()));
		ASSERT_THAT(IsTrue(Slot->Packages[1]->bActive));
		ASSERT_THAT(IsTrue(Pool->IsCurrentRevision(Handle)));

		RunLifecycle(World, EntityManager);
		RunLifecycle(World, EntityManager);
		ASSERT_THAT(IsTrue(GetHandle(EntityManager, Entity) == Handle, TEXT("An inactive package does not reinitialize the slot")));
		ASSERT_THAT(IsTrue(Pool->Resolve(Handle) == Slot));

		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(AreEqual(0, CountCandidates(*Slot, 0)));
		ASSERT_THAT(IsTrue(IsActiveFrom(*Slot, 1)));
		ASSERT_THAT(IsFalse(Slot->bDecisionRequested));

		ASSERT_THAT(IsTrue(Pool->AddConfig(Entity, *GrantedNeedsTeam), TEXT("An inactive grant is still counted")));
		const FArcTacticsPackage* Granted = FindPackage(*Slot, *GrantedNeedsTeam);
		ASSERT_THAT(IsNotNull(Granted));
		ASSERT_THAT(IsFalse(Granted->bActive));
		ASSERT_THAT(AreEqual(1, Granted->RuntimeCount));
		ASSERT_THAT(IsFalse(Slot->bDecisionRequested, TEXT("An inactive grant requests no decision")));
		RunLifecycle(World, EntityManager);
		ASSERT_THAT(IsTrue(GetHandle(EntityManager, Entity) == Handle));
	}

	TEST_METHOD(Recompile_KeepsRuntimePackagesAndCounts)
	{
		using namespace ArcTacticsPackageTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		UArcTacticsConfig* Static = MakeConfig(TEXT("Static"), {Spec(2, 0, 0.3f)});
		UArcTacticsConfig* Granted = MakeConfig(TEXT("Granted"), {Spec(2, 0, 0.5f)});
		ASSERT_THAT(IsTrue(Static && Granted));
		const FMassEntityHandle Entity = CreateEntity(EntityManager, {Static}, ZeroIntervalSettings());
		RunLifecycle(World, EntityManager);
		const FArcTacticsInstanceHandle FirstHandle = GetHandle(EntityManager, Entity);
		ASSERT_THAT(IsTrue(Pool->AddConfig(Entity, *Granted)));
		ASSERT_THAT(IsTrue(Pool->AddConfig(Entity, *Granted)));

		ASSERT_THAT(IsTrue(Compile(*Static, 10)));
		ASSERT_THAT(IsFalse(Pool->IsCurrentRevision(FirstHandle)));
		RunLifecycle(World, EntityManager);
		const FArcTacticsInstanceHandle SecondHandle = GetHandle(EntityManager, Entity);
		ASSERT_THAT(IsFalse(SecondHandle == FirstHandle, TEXT("A static config recompile reinitializes the slot")));
		ASSERT_THAT(IsNull(Pool->Resolve(FirstHandle)));
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(SecondHandle);
		ASSERT_THAT(IsNotNull(Slot));
		ASSERT_THAT(AreEqual(2, Slot->Packages.Num()));
		ASSERT_THAT(IsTrue(Slot->Packages[0]->Config == Static));
		ASSERT_THAT(AreEqual(10u, Slot->Packages[0]->SeenRevision));
		const FArcTacticsPackage* GrantedPackage = FindPackage(*Slot, *Granted);
		ASSERT_THAT(IsNotNull(GrantedPackage, TEXT("The runtime package survives the recompile")));
		ASSERT_THAT(AreEqual(2, GrantedPackage->RuntimeCount));
		ASSERT_THAT(AreEqual(0, GrantedPackage->StaticCount));
		ASSERT_THAT(IsTrue(GrantedPackage->bActive));
		ASSERT_THAT(IsTrue(Slot->bDecisionRequested));

		ASSERT_THAT(IsTrue(Compile(*Granted, 11)));
		RunLifecycle(World, EntityManager);
		const FArcTacticsInstanceHandle ThirdHandle = GetHandle(EntityManager, Entity);
		ASSERT_THAT(IsFalse(ThirdHandle == SecondHandle, TEXT("A runtime config recompile reinitializes the slot")));
		Slot = Pool->Resolve(ThirdHandle);
		ASSERT_THAT(IsNotNull(Slot));
		GrantedPackage = FindPackage(*Slot, *Granted);
		ASSERT_THAT(IsNotNull(GrantedPackage));
		ASSERT_THAT(AreEqual(2, GrantedPackage->RuntimeCount));
		ASSERT_THAT(AreEqual(11u, GrantedPackage->SeenRevision));
		ASSERT_THAT(IsTrue(GrantedPackage->bActive));

		ASSERT_THAT(IsTrue(Pool->RemoveConfig(Entity, *Granted)));
		ASSERT_THAT(IsNotNull(FindPackage(*Slot, *Granted)));
		ASSERT_THAT(IsTrue(Pool->RemoveConfig(Entity, *Granted)));
		ASSERT_THAT(IsNull(FindPackage(*Slot, *Granted), TEXT("The carried count is a real count")));
	}

	TEST_METHOD(Cooldowns_SurviveRemoveAndReAdd)
	{
		using namespace ArcTacticsPackageTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		FActionSpec CooldownSpec = Spec(0, 0, 0.9f);
		CooldownSpec.Cooldown = 100.0f;
		UArcTacticsConfig* Granted = MakeConfig(TEXT("Granted"), {CooldownSpec});
		ASSERT_THAT(IsTrue(Granted != nullptr));
		const FMassEntityHandle Entity = CreateEntity(EntityManager, {}, ZeroIntervalSettings());
		RunLifecycle(World, EntityManager);
		const FArcTacticsInstanceHandle Handle = GetHandle(EntityManager, Entity);
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		const double StartTime = World.GetTimeSeconds();

		ASSERT_THAT(IsTrue(Pool->AddConfig(Entity, *Granted)));
		RunDecision(*Pool, EntityManager, Handle);
		const double* Stamp = Slot->CooldownEndTimes.Find(FArcTacticsCooldownKey(Granted, 0));
		ASSERT_THAT(IsTrue(Stamp && FMath::IsNearlyEqual(StartTime + 100.0, *Stamp, 0.001)));
		const uint32 FirstSerial = Slot->Packages[0]->Serial;

		ASSERT_THAT(IsTrue(Pool->RemoveConfig(Entity, *Granted)));
		ASSERT_THAT(AreEqual(0, Slot->Packages.Num()));
		ASSERT_THAT(IsNotNull(Slot->CooldownEndTimes.Find(FArcTacticsCooldownKey(Granted, 0)),
			TEXT("Removing the package keeps its cooldown")));
		World.TimeSeconds = StartTime + 10.0;
		ASSERT_THAT(IsTrue(Pool->AddConfig(Entity, *Granted)));
		ASSERT_THAT(IsTrue(Slot->Packages[0]->Serial != FirstSerial, TEXT("The re-added package is a new package")));
		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(IsTrue(HasOutcome(*Slot, 0, EArcTacticsCandidateOutcome::Vetoed, TEXT("ActionCooldown")),
			TEXT("The re-added package's branch is still on cooldown")));
		Stamp = Slot->CooldownEndTimes.Find(FArcTacticsCooldownKey(Granted, 0));
		ASSERT_THAT(IsTrue(Stamp && FMath::IsNearlyEqual(StartTime + 100.0, *Stamp, 0.001)));

		World.TimeSeconds = StartTime + 100.0;
		Pool->RequestDecision(Handle);
		RunDecision(*Pool, EntityManager, Handle);
		Stamp = Slot->CooldownEndTimes.Find(FArcTacticsCooldownKey(Granted, 0));
		ASSERT_THAT(IsTrue(Stamp && FMath::IsNearlyEqual(StartTime + 200.0, *Stamp, 0.001),
			TEXT("Once the cooldown ends the branch runs again")));
	}

	TEST_METHOD(SameBranchIndex_InTwoPackagesDoesNotCollideInVetoOrCooldown)
	{
		using namespace ArcTacticsPackageTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		FActionSpec CooldownSpec = Spec(0, 0, 0.9f);
		CooldownSpec.Cooldown = 100.0f;
		UArcTacticsConfig* Running = MakeConfig(TEXT("Running"), {Spec(2, 0, 0.5f)});
		UArcTacticsConfig* Cooling = MakeConfig(TEXT("Cooling"), {CooldownSpec});
		UArcTacticsConfig* Higher = MakeConfig(TEXT("Higher"), {Spec(2, 1, 0.4f)});
		ASSERT_THAT(IsTrue(Running && Cooling && Higher));
		const FMassEntityHandle Entity = CreateEntity(EntityManager, {Running, Cooling}, ZeroIntervalSettings());
		RunLifecycle(World, EntityManager);
		const FArcTacticsInstanceHandle Handle = GetHandle(EntityManager, Entity);
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));

		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(IsFalse(Slot->ActiveAction.IsSet()));
		ASSERT_THAT(IsNotNull(Slot->CooldownEndTimes.Find(FArcTacticsCooldownKey(Cooling, 0))));
		ASSERT_THAT(IsTrue(Slot->bDecisionRequested));

		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(IsTrue(HasOutcome(*Slot, 1, EArcTacticsCandidateOutcome::Vetoed, TEXT("ActionCooldown"))));
		ASSERT_THAT(IsTrue(HasOutcome(*Slot, 0, EArcTacticsCandidateOutcome::Accepted, nullptr),
			TEXT("Branch 0 of another package is not on the cooled branch's cooldown")));
		ASSERT_THAT(IsNull(Slot->CooldownEndTimes.Find(FArcTacticsCooldownKey(Running, 0))));
		ASSERT_THAT(IsTrue(IsActiveFrom(*Slot, 0)));

		ASSERT_THAT(IsTrue(Pool->AddConfig(Entity, *Higher)));
		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(IsTrue(HasOutcome(*Slot, 0, EArcTacticsCandidateOutcome::Vetoed, TEXT("BelowActivePriority"))));
		ASSERT_THAT(IsTrue(HasOutcome(*Slot, 2, EArcTacticsCandidateOutcome::Accepted, nullptr),
			TEXT("Branch 0 of another package is not vetoed as the active action's own branch")));
		ASSERT_THAT(IsTrue(IsActiveFrom(*Slot, 2)));
	}

	TEST_METHOD(Focus_ReplacementKeepsNewFocusAndExitRemovesFragment)
	{
		using namespace ArcTacticsPackageTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		const FVector FirstLocation(100.0, 0.0, 0.0);
		const FVector SecondLocation(0.0, 200.0, 0.0);
		UArcTacticsConfig* FirstFocus = NewConfig(TEXT("FirstFocus"));
		AddFocusAction(*FirstFocus, Spec(2, 0, 0.5f), FirstLocation);
		UArcTacticsConfig* SecondFocus = NewConfig(TEXT("SecondFocus"));
		AddFocusAction(*SecondFocus, Spec(2, 1, 0.5f), SecondLocation);
		UArcTacticsConfig* NoFocus = MakeConfig(TEXT("NoFocus"), {Spec(2, 2, 0.5f)});
		ASSERT_THAT(IsTrue(Finish(*FirstFocus) && Finish(*SecondFocus) && NoFocus));
		const FMassEntityHandle Entity = CreateEntity(EntityManager, {FirstFocus}, ZeroIntervalSettings(), true);
		RunLifecycle(World, EntityManager);
		const FArcTacticsInstanceHandle Handle = GetHandle(EntityManager, Entity);
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		ASSERT_THAT(IsTrue(Slot->Packages[0]->bActive));

		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(IsTrue(IsActiveFrom(*Slot, 0)));
		const uint32 FirstSerial = Slot->ActiveAction.Serial;
		ASSERT_THAT(IsNull(FindFocus(EntityManager, Entity), TEXT("Enter does not change composition immediately")));
		DrainDeferred(World, EntityManager);
		const FArcMassFocusFragment* Focus = FindFocus(EntityManager, Entity);
		ASSERT_THAT(IsNotNull(Focus));
		ASSERT_THAT(AreEqual(FirstSerial, Focus->OwnerSerial));
		ASSERT_THAT(IsTrue(Focus->TargetLocation.Equals(FirstLocation)));

		ASSERT_THAT(IsTrue(Pool->AddConfig(Entity, *SecondFocus)));
		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(IsTrue(IsActiveFrom(*Slot, 1), TEXT("The second Focus action replaces the first in one decision")));
		const uint32 SecondSerial = Slot->ActiveAction.Serial;
		ASSERT_THAT(IsTrue(SecondSerial != FirstSerial));
		DrainDeferred(World, EntityManager);
		Focus = FindFocus(EntityManager, Entity);
		ASSERT_THAT(IsNotNull(Focus, TEXT("The replaced action's exit does not remove the new focus")));
		ASSERT_THAT(AreEqual(SecondSerial, Focus->OwnerSerial));
		ASSERT_THAT(IsTrue(Focus->TargetLocation.Equals(SecondLocation)));

		ASSERT_THAT(IsTrue(Pool->AddConfig(Entity, *NoFocus)));
		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(IsTrue(IsActiveFrom(*Slot, 2)));
		ASSERT_THAT(IsNotNull(FindFocus(EntityManager, Entity), TEXT("Exit defers the removal")));
		DrainDeferred(World, EntityManager);
		ASSERT_THAT(IsNull(FindFocus(EntityManager, Entity), TEXT("The Focus action's exit removes the fragment")));
	}

	TEST_METHOD(Lifecycle_TraitSettingsReachTheSlot)
	{
		using namespace ArcTacticsPackageTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		UArcTacticsConfig* Config = MakeConfig(TEXT("Config"), {Spec(2, 0, 0.5f)});
		ASSERT_THAT(IsTrue(Config != nullptr));
		UArcTacticsTrait* Trait = NewObject<UArcTacticsTrait>(GetTransientPackage());
		Trait->Configs.Add(Config);
		Trait->Settings.DecisionInterval = 1.5f;
		Trait->Settings.Selection.Mode = EArcTacticsSelectionMode::RandomFromTopPercent;
		Trait->Settings.Selection.TopPercent = 0.5f;
		Trait->Settings.HistorySize = 3;
		ASSERT_THAT(IsNotNull(World.GetSubsystem<UMassSpawnerSubsystem>()));
		FMassEntityConfig EntityConfig;
		EntityConfig.AddTrait(*Trait);
		ASSERT_THAT(IsTrue(EntityConfig.ValidateEntityTemplate(World), TEXT("The production template build validates the trait")));
		const FMassEntityTemplateData& TemplateData = EntityConfig.GetOrCreateEntityTemplate(World).GetTemplateData();
		ASSERT_THAT(IsTrue(TemplateData.HasTag<FArcTacticsTag>()));
		ASSERT_THAT(IsTrue(TemplateData.HasFragment<FArcTacticsInstanceFragment>()));
		const FArcTacticsConfigFragment* BuiltFragment = FindConfigFragment(TemplateData);
		ASSERT_THAT(IsNotNull(BuiltFragment, TEXT("BuildTemplate adds the config fragment")));
		ASSERT_THAT(AreEqual(1, BuiltFragment->Configs.Num()));
		ASSERT_THAT(IsTrue(BuiltFragment->Configs[0] == Config));
		ASSERT_THAT(IsNear(1.5f, BuiltFragment->Settings.DecisionInterval, 0.0001f,
			TEXT("BuildTemplate copies the trait Settings into the config fragment")));
		ASSERT_THAT(AreEqual(EArcTacticsSelectionMode::RandomFromTopPercent, BuiltFragment->Settings.Selection.Mode));
		ASSERT_THAT(IsNear(0.5f, BuiltFragment->Settings.Selection.TopPercent, 0.0001f));
		ASSERT_THAT(AreEqual(3, BuiltFragment->Settings.HistorySize));

		const FMassEntityHandle Entity = CreateEntityFromTemplate(EntityManager, TemplateData);
		ASSERT_THAT(IsTrue(EntityManager.IsEntityActive(Entity)));
		RunLifecycle(World, EntityManager);
		const FArcTacticsInstanceHandle Handle = GetHandle(EntityManager, Entity);
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot, TEXT("The lifecycle processor allocates the slot")));
		ASSERT_THAT(IsNear(1.5f, Slot->Settings.DecisionInterval, 0.0001f));
		ASSERT_THAT(AreEqual(EArcTacticsSelectionMode::RandomFromTopPercent, Slot->Settings.Selection.Mode));
		ASSERT_THAT(IsNear(0.5f, Slot->Settings.Selection.TopPercent, 0.0001f));
		ASSERT_THAT(AreEqual(3, Slot->Settings.HistorySize));
		const double StartTime = World.GetTimeSeconds();
		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(IsTrue(Slot->ActiveAction.IsSet()));
		ASSERT_THAT(IsNear(StartTime + 1.5, Slot->NextDecisionTime, 0.001, TEXT("The trait interval schedules the next decision")));
	}

	TEST_METHOD(Focus_StaleExitKeepsFragmentAndOwningExitRemovesIt)
	{
		using namespace ArcTacticsPackageTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		UE::Mass::FEntityBuilder Builder = EntityManager.MakeEntityBuilder();
		Builder.Add<FTransformFragment>();
		const FMassEntityHandle Entity = Builder.Commit();
		ASSERT_THAT(IsTrue(EntityManager.IsEntityActive(Entity)));

		const FVector Location(50.0, 60.0, 0.0);
		FArcTacticsFocusActiveStepInstanceData StepData;
		StepData.bUseTargetLocation = true;
		StepData.TargetLocation = Location;
		const FArcTacticsFocusActiveStep Step;
		FArcTacticsActionContext Context;
		Context.World = &World;
		Context.EntityManager = &EntityManager;
		Context.Entity = Entity;
		Context.RequestToken.Entity = Entity;
		Context.RequestToken.ActionSerial = 7;
		ASSERT_THAT(IsTrue(Step.Enter(Context, FStructView::Make(StepData))));
		DrainDeferred(World, EntityManager);
		const FArcMassFocusFragment* Focus = FindFocus(EntityManager, Entity);
		ASSERT_THAT(IsNotNull(Focus));
		ASSERT_THAT(AreEqual(7u, Focus->OwnerSerial, TEXT("Enter stamps the action serial on the focus")));
		ASSERT_THAT(IsTrue(Focus->TargetLocation.Equals(Location)));

		Context.RequestToken.ActionSerial = 6;
		Step.Exit(Context, FStructView::Make(StepData));
		DrainDeferred(World, EntityManager);
		Focus = FindFocus(EntityManager, Entity);
		ASSERT_THAT(IsNotNull(Focus, TEXT("An exit with a stale action serial does not remove the focus")));
		ASSERT_THAT(AreEqual(7u, Focus->OwnerSerial));

		Context.RequestToken.ActionSerial = 7;
		Step.Exit(Context, FStructView::Make(StepData));
		DrainDeferred(World, EntityManager);
		ASSERT_THAT(IsNull(FindFocus(EntityManager, Entity), TEXT("An exit with the owning action serial removes the focus")));
	}

	TEST_METHOD(MinScore_ExcludedPackageShowsCompiledDefaultAfterBoundDecision)
	{
		using namespace ArcTacticsPackageTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		UArcTacticsPackageProbe* Probe = NewObject<UArcTacticsPackageProbe>(GetTransientPackage());
		UArcTacticsConfig* Bound = NewConfig(TEXT("Bound"));
		AddTask(*Bound, 0, Probe);
		AddLifecycleAction(*Bound, Spec(2, 0, 0.9f));
		Bound->DecisionSettings.MinScore = 0.05f;
		Bound->GlobalParameters.AddProperty(TEXT("Threshold"), EPropertyBagPropertyType::Float);
		Bound->GlobalParameters.SetValueFloat(TEXT("Threshold"), 0.25f);
		Bound->EnsureValidNodeIDs();
		Bound->EditorBindings.AddBinding(
			MakePath(Bound->GlobalParametersID, TEXT("Threshold")),
			MakePath(Bound->DecisionSettingsID, TEXT("MinScore")));
		UArcTacticsConfig* Healthy = MakeConfig(TEXT("Healthy"), {Spec(2, 0, 0.3f)});
		ASSERT_THAT(IsTrue(Finish(*Bound) && Healthy));
		ASSERT_THAT(IsNear(0.05f, Bound->CompiledData.DefaultDecisionSettings.MinScore, 0.0001f));
		const FMassEntityHandle Entity = CreateEntity(EntityManager, {Bound, Healthy}, ZeroIntervalSettings());
		RunLifecycle(World, EntityManager);
		const FArcTacticsInstanceHandle Handle = GetHandle(EntityManager, Entity);
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));

		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(AreEqual(1, Probe->EnterCount));
		const FArcTacticsPackage* BoundPackage = FindPackage(*Slot, *Bound);
		ASSERT_THAT(IsNotNull(BoundPackage));
		ASSERT_THAT(IsFalse(BoundPackage->bExcluded));
		ASSERT_THAT(IsNear(0.25f, BoundPackage->Instance.DecisionSettings.MinScore, 0.0001f,
			TEXT("The first decision binds the non-default MinScore")));

		Probe->bFailEnter = true;
		Pool->RequestDecision(Handle);
		RunDecision(*Pool, EntityManager, Handle);
		ASSERT_THAT(AreEqual(2, Probe->EnterCount, TEXT("The second decision runs the global task")));
		BoundPackage = FindPackage(*Slot, *Bound);
		ASSERT_THAT(IsNotNull(BoundPackage));
		ASSERT_THAT(IsTrue(BoundPackage->bExcluded));
		ASSERT_THAT(IsTrue(BoundPackage->Instance.Diagnostic == TEXT("GlobalTaskFailed")));
		ASSERT_THAT(IsNear(0.05f, BoundPackage->Instance.DecisionSettings.MinScore, 0.0001f,
			TEXT("An excluded package shows the compiled default MinScore, not the previous decision's bound value")));
	}

	TEST_METHOD(History_IsCappedByTraitHistorySize)
	{
		using namespace ArcTacticsPackageTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		const FGameplayTag HistoryTag = FGameplayTag::RequestGameplayTag(TEXT("AI.Memory.Anchor"), false);
		ASSERT_THAT(IsTrue(HistoryTag.IsValid()));
		FActionSpec TaggedSpec = Spec(0, 0, 0.5f);
		TaggedSpec.HistoryTag = HistoryTag;
		UArcTacticsConfig* Config = MakeConfig(TEXT("Tagged"), {TaggedSpec});
		ASSERT_THAT(IsTrue(Config != nullptr));
		FArcTacticsDecisionSettings Settings = ZeroIntervalSettings();
		Settings.HistorySize = 2;
		const FMassEntityHandle Entity = CreateEntity(EntityManager, {Config}, Settings);
		RunLifecycle(World, EntityManager);
		const FArcTacticsInstanceHandle Handle = GetHandle(EntityManager, Entity);
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		const double StartTime = World.GetTimeSeconds();
		for (int32 Run = 0; Run < 4; ++Run)
		{
			World.TimeSeconds = StartTime + Run;
			ASSERT_THAT(IsTrue(Slot->bDecisionRequested));
			RunDecision(*Pool, EntityManager, Handle);
			ASSERT_THAT(AreEqual(FMath::Min(Run + 1, 2), Slot->Context.History.Entries.Num()));
		}
		const TArray<FArcTacticsActionHistoryEntry>& Entries = Slot->Context.History.Entries;
		ASSERT_THAT(IsTrue(Entries[0].Tag == HistoryTag));
		ASSERT_THAT(IsNear(StartTime + 2.0, Entries[0].Time, 0.001, TEXT("The oldest entries are dropped")));
		ASSERT_THAT(IsNear(StartTime + 3.0, Entries[1].Time, 0.001));
	}
};
