#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

#include "Fixtures/ArcTacticsPoolTestTypes.h"
#include "Fixtures/ArcTacticsSlotTestHelpers.h"
#include "Decision/ArcTacticsCompiler.h"
#include "Decision/ArcTacticsConfig.h"
#include "Mass/ArcTacticsInstanceSubsystem.h"
#include "Mass/ArcTacticsFragments.h"
#include "Mass/ArcTacticsUtilityProcessor.h"
#include "Mass/EntityBuilder.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "ArcTacticsSignals.h"
#include "ArcTQSQueryInstance.h"

namespace ArcTacticsInstancePoolTestsPrivate
{
	FPropertyBindingPath MakePath(const FGuid ID, const TCHAR* Path)
	{
		FPropertyBindingPath Result(ID);
		Result.FromString(Path);
		return Result;
	}

	bool PathMatches(const FPropertyBindingPath& Path, const FGuid& ExpectedStructID, const TCHAR* ExpectedPath)
	{
		if (Path.GetStructID() != ExpectedStructID)
		{
			return false;
		}
		const FPropertyBindingPath Expected = MakePath(ExpectedStructID, ExpectedPath);
		if (Path.NumSegments() != Expected.NumSegments())
		{
			return false;
		}
		for (int32 SegmentIndex = 0; SegmentIndex < Path.NumSegments(); ++SegmentIndex)
		{
			const FPropertyBindingPathSegment& ActualSegment = Path.GetSegment(SegmentIndex);
			const FPropertyBindingPathSegment& ExpectedSegment = Expected.GetSegment(SegmentIndex);
			if (ActualSegment.GetName() != ExpectedSegment.GetName()
				|| ActualSegment.GetArrayIndex() != ExpectedSegment.GetArrayIndex())
			{
				return false;
			}
		}
		return true;
	}

	bool CompileOrLogDiagnostics(UArcTacticsConfig& Config, const uint32 Revision)
	{
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		const bool bSucceeded = FArcTacticsCompiler::Compile(Config, Revision, Config.CompiledData, Diagnostics);
		if (!bSucceeded)
		{
			for (const FArcTacticsCompilerDiagnostic& Diagnostic : Diagnostics)
			{
				UE_LOGF(LogTemp, Warning, "ArcTacticsInstancePoolTests compile diagnostic: NodeID=%ls Message=%ls",
					*Diagnostic.NodeID.ToString(), *Diagnostic.Message.ToString());
			}
		}
		return bSucceeded;
	}

	UArcTacticsConfig* MakeConfig(const bool bCallbacks, const bool bAction = false)
	{
		UArcTacticsConfig* Config = NewObject<UArcTacticsConfig>(GetTransientPackage());
		Config->ContextID = FGuid(10, 20, 30, 40);
		if (bCallbacks)
		{
			FArcTacticsNodeDefinition Task;
			Task.ID = FGuid(100, 1, 2, 3);
			Task.Name = TEXT("PoolCallbackTask");
			Task.Node.InitializeAs<FArcTacticsPoolCallbackTask>();
			Task.InstanceData.InitializeAs<FArcTacticsPoolCallbackTaskInstanceData>();
			Config->GlobalTasks.Add(Task);
		}
		if (bAction)
		{
			FArcTacticsActionDefinition Action;
			Action.Action.ID = FGuid(200, 1, 2, 3);
			Action.Action.Name = TEXT("PoolCallbackAction");
			Action.Action.Node.InitializeAs<FArcTacticsPoolCallbackAction>();
			Action.Action.InstanceData.InitializeAs<FArcTacticsPoolCallbackActionInstanceData>();
			Config->Actions.Add(Action);
		}
		Config->EnsureValidNodeIDs();
		CompileOrLogDiagnostics(*Config, 9);
		return Config;
	}

	FArcTacticsDecisionSettings MakeSettings(const float DecisionInterval)
	{
		FArcTacticsDecisionSettings Settings;
		Settings.DecisionInterval = DecisionInterval;
		return Settings;
	}

	UArcTacticsConfig* MakeMinScoreBoundConfig()
	{
		UArcTacticsConfig* Config = MakeConfig(false);
		Config->EditorBindings.AddBinding(
			MakePath(Config->ContextID, TEXT("Self.Index")),
			MakePath(Config->DecisionSettingsID, TEXT("MinScore")));
		CompileOrLogDiagnostics(*Config, 9);
		return Config;
	}

	UArcTacticsConfig* MakeLifecycleConfig(const uint8 Mode, UArcTacticsPoolOutputSink& Sink)
	{
		UArcTacticsConfig* Config = MakeConfig(false);
		FArcTacticsActionDefinition Action;
		Action.Action.ID = FGuid(300, 1, 2, 3);
		Action.Action.Name = TEXT("PoolLifecycleAction");
		Action.Action.Node.InitializeAs<FArcTacticsPoolLifecycleAction>();
		FArcTacticsPoolLifecycleActionInstanceData Defaults;
		Defaults.Mode = Mode;
		Action.Action.InstanceData.InitializeAs<FArcTacticsPoolLifecycleActionInstanceData>(Defaults);
		Config->GlobalParameters.AddProperty(TEXT("Captured"), EPropertyBagPropertyType::Int32);
		Config->GlobalParameters.SetValueInt32(TEXT("Captured"), 77);
		Action.ParameterBag.AddProperty(
			TEXT("Sink"), EPropertyBagPropertyType::Object, UArcTacticsPoolOutputSink::StaticClass());
		Action.ParameterBag.SetValueObject(TEXT("Sink"), &Sink);
		Config->Actions.Add(Action);
		Config->EnsureValidNodeIDs();
		const FGuid ActionParameterBagID = Config->Actions.Last().ParameterBagID;
		Config->EditorBindings.AddBinding(
			MakePath(ActionParameterBagID, TEXT("Sink.Value")),
			MakePath(Action.Action.ID, TEXT("Output")));
		Config->EditorBindings.AddBinding(
			MakePath(Config->GlobalParametersID, TEXT("Captured")),
			MakePath(Action.Action.ID, TEXT("CapturedGlobal")));
		Config->EditorBindings.ForEachMutableBinding(
			[ActionID = Action.Action.ID](FPropertyBindingBinding& Binding)
			{
				FArcTacticsBinding* TacticsBinding = static_cast<FArcTacticsBinding*>(&Binding);
				if (PathMatches(TacticsBinding->GetTargetPath(), ActionID, TEXT("Output")))
				{
					TacticsBinding->SetIsOutputBinding(true);
				}
			});
		CompileOrLogDiagnostics(*Config, 9);
		return Config;
	}

	struct FBranchSpec
	{
		uint8 Mode = 2;
		int32 Priority = 0;
		float Cooldown = 0.0f;
		EArcTacticsCooldownStart CooldownStart = EArcTacticsCooldownStart::OnEnd;
		UArcTacticsPoolTestObject* CancelTarget = nullptr;
	};

	UArcTacticsConfig* MakeBranchConfig(const TConstArrayView<FBranchSpec> Branches)
	{
		UArcTacticsConfig* Config = MakeConfig(false);
		Config->GlobalParameters.AddProperty(TEXT("Captured"), EPropertyBagPropertyType::Int32);
		for (int32 BranchIndex = 0; BranchIndex < Branches.Num(); ++BranchIndex)
		{
			const FBranchSpec& Spec = Branches[BranchIndex];
			FArcTacticsActionDefinition Action;
			Action.Action.ID = FGuid(400 + BranchIndex, 1, 2, 3);
			Action.Action.Name = FName(*FString::Printf(TEXT("PoolBranch%d"), BranchIndex));
			Action.Action.Node.InitializeAs<FArcTacticsPoolLifecycleAction>();
			FArcTacticsPoolLifecycleActionInstanceData Defaults;
			Defaults.Mode = Spec.Mode;
			Defaults.CancelTarget = Spec.CancelTarget;
			Action.Action.InstanceData.InitializeAs<FArcTacticsPoolLifecycleActionInstanceData>(Defaults);
			Action.Parameters.Priority = Spec.Priority;
			Action.Parameters.Cooldown = Spec.Cooldown;
			Action.Parameters.CooldownStart = Spec.CooldownStart;
			Config->Actions.Add(Action);
		}
		Config->EnsureValidNodeIDs();
		for (const FArcTacticsActionDefinition& Action : Config->Actions)
		{
			Config->EditorBindings.AddBinding(
				MakePath(Config->GlobalParametersID, TEXT("Captured")),
				MakePath(Action.Action.ID, TEXT("CapturedGlobal")));
		}
		CompileOrLogDiagnostics(*Config, 9);
		return Config;
	}

	const FArcTacticsCandidate* FindCandidate(const FArcTacticsInstanceSlot& Instance, const int32 ActionIndex)
	{
		for (const FArcTacticsCandidate& Candidate : Instance.Candidates)
		{
			if (!Candidate.Option.bFromSelector && Candidate.Option.SourceIndex == ActionIndex)
			{
				return &Candidate;
			}
		}
		return nullptr;
	}

	bool HasOutcome(
		const FArcTacticsInstanceSlot& Instance,
		const int32 ActionIndex,
		const EArcTacticsCandidateOutcome Outcome,
		const TCHAR* Reason)
	{
		const FArcTacticsCandidate* Candidate = FindCandidate(Instance, ActionIndex);
		return Candidate && Candidate->Outcome == Outcome
			&& (!Reason || Candidate->RejectionReason == FName(Reason));
	}

	FMassEntityHandle CreateTacticsEntity(
		FMassEntityManager& EntityManager,
		const UArcTacticsConfig& Config,
		const FArcTacticsDecisionSettings& Settings)
	{
		UE::Mass::FEntityBuilder Builder = EntityManager.MakeEntityBuilder();
		Builder.Add<FArcTacticsInstanceFragment>();
		Builder.Add<FArcTacticsTag>();
		FArcTacticsConfigFragment& ConfigFragment = Builder.Add_GetRef<FArcTacticsConfigFragment>();
		ConfigFragment.Configs.Add(&Config);
		ConfigFragment.Settings = Settings;
		return Builder.Commit();
	}

	UArcTacticsUtilityProcessor* MakeUtilityProcessor(UWorld& World, FMassEntityManager& EntityManager)
	{
		UArcTacticsUtilityProcessor* Processor = NewObject<UArcTacticsUtilityProcessor>(&World);
		Processor->CallInitialize(EntityManager.GetOwner(), EntityManager.AsShared());
		return Processor;
	}

	void RunUtilityProcessor(UArcTacticsUtilityProcessor& Processor, FMassEntityManager& EntityManager)
	{
		FMassProcessingContext ProcessingContext(EntityManager, 0.0f);
		UE::Mass::Executor::Run(Processor, ProcessingContext);
	}

	void DrainDeferred(UWorld& World, FMassEntityManager& EntityManager)
	{
		UArcTacticsPoolDrainProcessor* Processor = NewObject<UArcTacticsPoolDrainProcessor>(&World);
		Processor->CallInitialize(EntityManager.GetOwner(), EntityManager.AsShared());
		FMassProcessingContext ProcessingContext(EntityManager, 0.0f);
		UE::Mass::Executor::Run(*Processor, ProcessingContext);
	}

	void DestroyEntity(UWorld& World, FMassEntityManager& EntityManager, const FMassEntityHandle Entity)
	{
		if (EntityManager.IsEntityActive(Entity))
		{
			EntityManager.Defer().DestroyEntity(Entity);
			DrainDeferred(World, EntityManager);
		}
	}

	FMassEntityHandle CreateTestEntity(FMassEntityManager& EntityManager, const bool bWithTacticsTag = false)
	{
		UE::Mass::FEntityBuilder Builder = EntityManager.MakeEntityBuilder();
		Builder.Add<FArcTacticsInstanceFragment>();
		if (bWithTacticsTag)
		{
			Builder.Add<FArcTacticsTag>();
		}
		return Builder.Commit();
	}
}

TEST_CLASS(ArcTacticsInstancePoolTests, "ArcTactics.InstancePool")
{
	FActorTestSpawner Spawner;

	TEST_METHOD(WorldPool_AllocatesDistinctContextAndImmutableDefaults)
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* EntitySubsystem = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = Spawner.GetWorld().GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		UArcTacticsConfig* Config = ArcTacticsInstancePoolTestsPrivate::MakeConfig(true);
		ASSERT_THAT(IsTrue(Config->CompiledData.IsExecutable()));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		const FMassEntityHandle FirstEntity = ArcTacticsInstancePoolTestsPrivate::CreateTestEntity(EntityManager);
		const FMassEntityHandle SecondEntity = ArcTacticsInstancePoolTestsPrivate::CreateTestEntity(EntityManager);
		const FArcTacticsInstanceHandle First = Pool->AllocateInstance(EntityManager, FirstEntity, ArcTacticsSlotTestHelpers::Configs(*Config), FArcTacticsDecisionSettings());
		const FArcTacticsInstanceHandle Second = Pool->AllocateInstance(EntityManager, SecondEntity, ArcTacticsSlotTestHelpers::Configs(*Config), FArcTacticsDecisionSettings());
		ASSERT_THAT(IsTrue(First.IsValid()));
		ASSERT_THAT(IsTrue(Second.IsValid()));
		ASSERT_THAT(IsFalse(First == Second));
		FArcTacticsInstanceSlot* FirstSlot = Pool->Resolve(First);
		FArcTacticsInstanceSlot* SecondSlot = Pool->Resolve(Second);
		ASSERT_THAT(IsNotNull(FirstSlot));
		ASSERT_THAT(IsNotNull(SecondSlot));
		ASSERT_THAT(IsTrue(FirstSlot->Context.Self.Index == FirstEntity.Index
			&& FirstSlot->Context.Self.SerialNumber == FirstEntity.SerialNumber));
		ASSERT_THAT(IsTrue(SecondSlot->Context.Self.Index == SecondEntity.Index
			&& SecondSlot->Context.Self.SerialNumber == SecondEntity.SerialNumber));
		FArcTacticsPoolCallbackTaskInstanceData* FirstData = ArcTacticsSlotTestHelpers::PackageInstance(*FirstSlot).NodeInstanceData[0].GetPtr<FArcTacticsPoolCallbackTaskInstanceData>();
		FArcTacticsPoolCallbackTaskInstanceData* SecondData = ArcTacticsSlotTestHelpers::PackageInstance(*SecondSlot).NodeInstanceData[0].GetPtr<FArcTacticsPoolCallbackTaskInstanceData>();
		ASSERT_THAT(IsNotNull(FirstData));
		ASSERT_THAT(IsNotNull(SecondData));
		FirstData->CallbackCount = 99;
		ASSERT_THAT(AreEqual(0, SecondData->CallbackCount));
		FirstSlot->Context.Self = FMassEntityHandle(99, 1);
		ASSERT_THAT(IsTrue(SecondSlot->Context.Self.Index == SecondEntity.Index
			&& SecondSlot->Context.Self.SerialNumber == SecondEntity.SerialNumber));
		ArcTacticsInstancePoolTestsPrivate::DestroyEntity(Spawner.GetWorld(), EntityManager, FirstEntity);
		ArcTacticsInstancePoolTestsPrivate::DestroyEntity(Spawner.GetWorld(), EntityManager, SecondEntity);
	}

	TEST_METHOD(WorldPool_FreeRejectsStaleHandleAndReuseAdvancesGeneration)
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* EntitySubsystem = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = Spawner.GetWorld().GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		UArcTacticsConfig* Config = ArcTacticsInstancePoolTestsPrivate::MakeConfig(true, true);
		ASSERT_THAT(IsTrue(Config->CompiledData.IsExecutable()));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		const FMassEntityHandle Entity = ArcTacticsInstancePoolTestsPrivate::CreateTestEntity(EntityManager);
		const FArcTacticsInstanceHandle First = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*Config), FArcTacticsDecisionSettings());
		ASSERT_THAT(IsTrue(First.IsValid()));
		Pool->CancelAndFreeInstance(EntityManager, Entity, First, nullptr);
		ASSERT_THAT(IsNull(Pool->Resolve(First)));
		const FArcTacticsInstanceHandle Reused = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*Config), FArcTacticsDecisionSettings());
		ASSERT_THAT(IsTrue(Reused.IsValid()));
		ASSERT_THAT(AreEqual(First.Index, Reused.Index));
		ASSERT_THAT(IsFalse(First == Reused));
		ArcTacticsInstancePoolTestsPrivate::DestroyEntity(Spawner.GetWorld(), EntityManager, Entity);
	}

	TEST_METHOD(WorldPool_GrowthPreservesResolvedValues)
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* EntitySubsystem = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = Spawner.GetWorld().GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		UArcTacticsConfig* Config = ArcTacticsInstancePoolTestsPrivate::MakeConfig(false);
		ASSERT_THAT(IsTrue(Config->CompiledData.IsExecutable()));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		const FMassEntityHandle FirstEntity = ArcTacticsInstancePoolTestsPrivate::CreateTestEntity(EntityManager);
		const FArcTacticsInstanceHandle First = Pool->AllocateInstance(EntityManager, FirstEntity, ArcTacticsSlotTestHelpers::Configs(*Config), FArcTacticsDecisionSettings());
		ASSERT_THAT(IsTrue(First.IsValid()));
		for (int32 Index = 0; Index < 64; ++Index)
		{
			const FMassEntityHandle Entity = ArcTacticsInstancePoolTestsPrivate::CreateTestEntity(EntityManager);
			const FArcTacticsInstanceHandle Handle = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*Config), FArcTacticsDecisionSettings());
			ASSERT_THAT(IsTrue(Handle.IsValid()));
		}
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(First);
		ASSERT_THAT(IsNotNull(Slot));
		ASSERT_THAT(IsTrue(Slot->Entity.Index == FirstEntity.Index
			&& Slot->Entity.SerialNumber == FirstEntity.SerialNumber));
		ASSERT_THAT(IsTrue(Slot->Context.Self.Index == FirstEntity.Index
			&& Slot->Context.Self.SerialNumber == FirstEntity.SerialNumber));
	}

	TEST_METHOD(WorldPool_ReinitializeInvalidatesOldRevisionAndGeneration)
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* EntitySubsystem = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = Spawner.GetWorld().GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		UArcTacticsConfig* Config = ArcTacticsInstancePoolTestsPrivate::MakeConfig(true, true);
		ASSERT_THAT(IsTrue(Config->CompiledData.IsExecutable()));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		const FMassEntityHandle Entity = ArcTacticsInstancePoolTestsPrivate::CreateTestEntity(EntityManager);
		const FArcTacticsInstanceHandle Old = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*Config), FArcTacticsDecisionSettings());
		ASSERT_THAT(IsTrue(Pool->IsCurrentRevision(Old)));
		UArcTacticsPoolTestObject* CancelTarget = NewObject<UArcTacticsPoolTestObject>(GetTransientPackage());
		CancelTarget->AddToRoot();
		FArcTacticsInstanceSlot* OldSlot = Pool->Resolve(Old);
		ASSERT_THAT(IsNotNull(OldSlot));
		OldSlot->ResetForDecision(FArcTQSFunctionContext());
		ArcTacticsSlotTestHelpers::PackageInstance(*OldSlot).bGlobalTaskRunning = true;
		OldSlot->ActiveAction.Action = FArcTacticsNodeHandle(1);
		ArcTacticsSlotTestHelpers::BindActiveActionToPackage(*OldSlot);
		OldSlot->ActiveAction.ActionData.InitializeAs<FArcTacticsPoolCallbackActionInstanceData>();
		ArcTacticsSlotTestHelpers::PackageInstance(*OldSlot).NodeInstanceData[0].GetPtr<FArcTacticsPoolCallbackTaskInstanceData>()->CancelTarget = CancelTarget;
		OldSlot->ActiveAction.ActionData.GetMutablePtr<FArcTacticsPoolCallbackActionInstanceData>()->CancelTarget = CancelTarget;
		Config->CompiledData.Revision = 10;
		const FArcTacticsInstanceHandle Fresh = Pool->ReinitializeInstance(
			EntityManager, Entity, Old, ArcTacticsSlotTestHelpers::Configs(*Config), FArcTacticsDecisionSettings(), nullptr);
		ASSERT_THAT(AreEqual(2, CancelTarget->CancelCount));
		ASSERT_THAT(IsNull(Pool->Resolve(Old)));
		ASSERT_THAT(IsTrue(Fresh.IsValid()));
		ASSERT_THAT(IsFalse(Old == Fresh));
		ASSERT_THAT(IsTrue(Pool->IsCurrentRevision(Fresh)));
		CancelTarget->RemoveFromRoot();
		ArcTacticsInstancePoolTestsPrivate::DestroyEntity(Spawner.GetWorld(), EntityManager, Entity);
	}

	TEST_METHOD(WorldPool_LiveSlotRetainsStrongFixtureReferenceUntilFree)
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* EntitySubsystem = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = Spawner.GetWorld().GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		UArcTacticsPoolTestObject* NodeObject = NewObject<UArcTacticsPoolTestObject>(GetTransientPackage());
		UArcTacticsPoolTestObject* CandidateObject = NewObject<UArcTacticsPoolTestObject>(GetTransientPackage());
		UArcTacticsPoolTestObject* ActiveObject = NewObject<UArcTacticsPoolTestObject>(GetTransientPackage());
		TWeakObjectPtr<UArcTacticsPoolTestObject> WeakNode(NodeObject);
		TWeakObjectPtr<UArcTacticsPoolTestObject> WeakCandidate(CandidateObject);
		TWeakObjectPtr<UArcTacticsPoolTestObject> WeakActive(ActiveObject);
		NodeObject->AddToRoot();
		CandidateObject->AddToRoot();
		ActiveObject->AddToRoot();
		UArcTacticsConfig* Config = ArcTacticsInstancePoolTestsPrivate::MakeConfig(true, true);
		ASSERT_THAT(IsTrue(Config->CompiledData.IsExecutable()));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		const FMassEntityHandle Entity = ArcTacticsInstancePoolTestsPrivate::CreateTestEntity(EntityManager);
		const FArcTacticsInstanceHandle Handle = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*Config), FArcTacticsDecisionSettings());
		ASSERT_THAT(IsTrue(Handle.IsValid()));
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		ArcTacticsSlotTestHelpers::PackageInstance(*Slot).NodeInstanceData[0].GetPtr<FArcTacticsPoolCallbackTaskInstanceData>()->StrongReference = NodeObject;
		Slot->ActiveAction.Action = FArcTacticsNodeHandle(1);
		ArcTacticsSlotTestHelpers::BindActiveActionToPackage(*Slot);
		Slot->ActiveAction.ActionData.InitializeAs<FArcTacticsPoolCallbackActionInstanceData>();
		Slot->ActiveAction.ActionData.GetMutablePtr<FArcTacticsPoolCallbackActionInstanceData>()->StrongReference = ActiveObject;
		FArcTacticsCandidate Candidate;
		Candidate.Option.Action = FArcTacticsNodeHandle(1);
		Candidate.Option.ActionData.InitializeAs<FArcTacticsPoolCallbackActionInstanceData>();
		Candidate.Option.ActionData.GetMutablePtr<FArcTacticsPoolCallbackActionInstanceData>()->StrongReference = CandidateObject;
		Slot->Candidates.Add(Candidate);
		NodeObject->RemoveFromRoot();
		CandidateObject->RemoveFromRoot();
		ActiveObject->RemoveFromRoot();
		CollectGarbage(RF_NoFlags);
		ASSERT_THAT(IsTrue(WeakNode.IsValid()));
		ASSERT_THAT(IsTrue(WeakCandidate.IsValid()));
		ASSERT_THAT(IsTrue(WeakActive.IsValid()));
		Pool->CancelAndFreeInstance(EntityManager, Entity, Handle, nullptr);
		CollectGarbage(RF_NoFlags);
		ASSERT_THAT(IsFalse(WeakNode.IsValid()));
		ASSERT_THAT(IsFalse(WeakCandidate.IsValid()));
		ASSERT_THAT(IsFalse(WeakActive.IsValid()));
		ArcTacticsInstancePoolTestsPrivate::DestroyEntity(Spawner.GetWorld(), EntityManager, Entity);
	}

	TEST_METHOD(WorldPool_StaleTaskCompletionCannotWriteRecycledSlot)
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* EntitySubsystem = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = Spawner.GetWorld().GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		UArcTacticsConfig* Config = ArcTacticsInstancePoolTestsPrivate::MakeConfig(true);
		ASSERT_THAT(IsTrue(Config->CompiledData.IsExecutable()));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		const FMassEntityHandle Entity = ArcTacticsInstancePoolTestsPrivate::CreateTestEntity(EntityManager);
		const FArcTacticsInstanceHandle Old = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*Config), FArcTacticsDecisionSettings());
		FArcTacticsInstanceSlot* OldSlot = Pool->Resolve(Old);
		ASSERT_THAT(IsNotNull(OldSlot));
		OldSlot->ResetForDecision(FArcTQSFunctionContext());
		ArcTacticsSlotTestHelpers::PackageInstance(*OldSlot).bGlobalTaskRunning = true;
		FArcTacticsAsyncRequestToken Token;
		Token.InstanceSubsystem = Pool;
		Token.Entity = Entity;
		Token.InstanceHandle = Old;
		Token.DecisionGeneration = OldSlot->DecisionGeneration;
		FArcTQSQueryInstance CompletedQuery;
		Pool->CompleteTaskQuery(Token, CompletedQuery);
		FArcTacticsPoolCallbackTaskInstanceData* Data = ArcTacticsSlotTestHelpers::PackageInstance(*OldSlot).NodeInstanceData[0].GetPtr<FArcTacticsPoolCallbackTaskInstanceData>();
		ASSERT_THAT(IsNotNull(Data));
		ASSERT_THAT(AreEqual(1, Data->CallbackCount));
		const uint32 StaleDecisionGeneration = OldSlot->DecisionGeneration;
		OldSlot->ResetForDecision(FArcTQSFunctionContext());
		ArcTacticsSlotTestHelpers::PackageInstance(*OldSlot).bGlobalTaskRunning = true;
		FArcTacticsPoolCallbackTaskInstanceData* FreshDecisionData =
			ArcTacticsSlotTestHelpers::PackageInstance(*OldSlot).NodeInstanceData[0].GetPtr<FArcTacticsPoolCallbackTaskInstanceData>();
		ASSERT_THAT(IsNotNull(FreshDecisionData));
		FreshDecisionData->CallbackCount = 7;
		Token.DecisionGeneration = StaleDecisionGeneration;
		Pool->CompleteTaskQuery(Token, CompletedQuery);
		ASSERT_THAT(AreEqual(7, FreshDecisionData->CallbackCount));
		Pool->CancelAndFreeInstance(EntityManager, Entity, Old, nullptr);
		const FArcTacticsInstanceHandle Fresh = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*Config), FArcTacticsDecisionSettings());
		ASSERT_THAT(IsTrue(Fresh.IsValid()));
		FArcTacticsInstanceSlot* FreshSlot = Pool->Resolve(Fresh);
		ASSERT_THAT(IsNotNull(FreshSlot));
		FreshSlot->ResetForDecision(FArcTQSFunctionContext());
		ArcTacticsSlotTestHelpers::PackageInstance(*FreshSlot).bGlobalTaskRunning = true;
		Pool->CompleteTaskQuery(Token, CompletedQuery);
		FArcTacticsPoolCallbackTaskInstanceData* FreshData = ArcTacticsSlotTestHelpers::PackageInstance(*FreshSlot).NodeInstanceData[0].GetPtr<FArcTacticsPoolCallbackTaskInstanceData>();
		ASSERT_THAT(IsNotNull(FreshData));
		ASSERT_THAT(AreEqual(0, FreshData->CallbackCount));
		ArcTacticsInstancePoolTestsPrivate::DestroyEntity(Spawner.GetWorld(), EntityManager, Entity);
	}

	TEST_METHOD(WorldPool_ObservedTagRemovalCancelsAndFrees)
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* EntitySubsystem = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = Spawner.GetWorld().GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		if (!EntitySubsystem || !Pool)
		{
			return;
		}
		UArcTacticsConfig* Config = ArcTacticsInstancePoolTestsPrivate::MakeConfig(true, true);
		ASSERT_THAT(IsTrue(Config->CompiledData.IsExecutable()));
		UArcTacticsPoolTestObject* CancelTarget = NewObject<UArcTacticsPoolTestObject>(GetTransientPackage());
		CancelTarget->AddToRoot();
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		const FMassEntityHandle Entity = ArcTacticsInstancePoolTestsPrivate::CreateTestEntity(EntityManager, true);
		const FArcTacticsInstanceHandle Handle = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*Config), FArcTacticsDecisionSettings());
		ASSERT_THAT(IsTrue(Handle.IsValid()));
		FArcTacticsInstanceFragment* InstanceFragment = EntityManager.GetFragmentDataPtr<FArcTacticsInstanceFragment>(Entity);
		ASSERT_THAT(IsNotNull(InstanceFragment));
		InstanceFragment->Handle = Handle;
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		Slot->ResetForDecision(FArcTQSFunctionContext());
		ArcTacticsSlotTestHelpers::PackageInstance(*Slot).bGlobalTaskRunning = true;
		Slot->ActiveAction.Action = FArcTacticsNodeHandle(1);
		ArcTacticsSlotTestHelpers::BindActiveActionToPackage(*Slot);
		Slot->ActiveAction.ActionData.InitializeAs<FArcTacticsPoolCallbackActionInstanceData>();
		ArcTacticsSlotTestHelpers::PackageInstance(*Slot).NodeInstanceData[0].GetPtr<FArcTacticsPoolCallbackTaskInstanceData>()->CancelTarget = CancelTarget;
		Slot->ActiveAction.ActionData.GetMutablePtr<FArcTacticsPoolCallbackActionInstanceData>()->CancelTarget = CancelTarget;
		ASSERT_THAT(AreEqual(0, CancelTarget->CancelCount));
		EntityManager.Defer().RemoveTag<FArcTacticsTag>(Entity);
		ArcTacticsInstancePoolTestsPrivate::DrainDeferred(Spawner.GetWorld(), EntityManager);
		ASSERT_THAT(AreEqual(2, CancelTarget->CancelCount));
		ASSERT_THAT(IsNull(Pool->Resolve(Handle)));
		InstanceFragment = EntityManager.GetFragmentDataPtr<FArcTacticsInstanceFragment>(Entity);
		ASSERT_THAT(IsNotNull(InstanceFragment));
		ASSERT_THAT(IsFalse(InstanceFragment->Handle.IsValid()));
		CancelTarget->RemoveFromRoot();
		ArcTacticsInstancePoolTestsPrivate::DestroyEntity(Spawner.GetWorld(), EntityManager, Entity);
	}

	TEST_METHOD(WorldPool_StaleActionCompletionCannotWriteRecycledSlot)
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* EntitySubsystem = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = Spawner.GetWorld().GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		UArcTacticsConfig* Config = ArcTacticsInstancePoolTestsPrivate::MakeConfig(false, true);
		ASSERT_THAT(IsTrue(Config->CompiledData.IsExecutable()));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		const FMassEntityHandle Entity = ArcTacticsInstancePoolTestsPrivate::CreateTestEntity(EntityManager);
		const FArcTacticsInstanceHandle Old = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*Config), FArcTacticsDecisionSettings());
		FArcTacticsInstanceSlot* OldSlot = Pool->Resolve(Old);
		ASSERT_THAT(IsNotNull(OldSlot));
		OldSlot->ActiveAction.Action = FArcTacticsNodeHandle(0);
		ArcTacticsSlotTestHelpers::BindActiveActionToPackage(*OldSlot);
		OldSlot->ActiveAction.Serial = 5;
		OldSlot->ActiveAction.ActionData.InitializeAs<FArcTacticsPoolCallbackActionInstanceData>();
		FArcTacticsAsyncRequestToken Token;
		Token.InstanceSubsystem = Pool;
		Token.Entity = Entity;
		Token.InstanceHandle = Old;
		Token.ActionSerial = 5;
		FArcTQSQueryInstance CompletedQuery;
		Pool->CompleteActionQuery(Token, CompletedQuery);
		FArcTacticsPoolCallbackActionInstanceData* Data = OldSlot->ActiveAction.ActionData.GetMutablePtr<FArcTacticsPoolCallbackActionInstanceData>();
		ASSERT_THAT(IsNotNull(Data));
		ASSERT_THAT(AreEqual(1, Data->CallbackCount));
		OldSlot->ActiveAction.Serial = 6;
		Token.ActionSerial = 5;
		Pool->CompleteActionQuery(Token, CompletedQuery);
		ASSERT_THAT(AreEqual(1, Data->CallbackCount));
		Pool->CancelAndFreeInstance(EntityManager, Entity, Old, nullptr);
		const FArcTacticsInstanceHandle Fresh = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*Config), FArcTacticsDecisionSettings());
		ASSERT_THAT(IsTrue(Fresh.IsValid()));
		FArcTacticsInstanceSlot* FreshSlot = Pool->Resolve(Fresh);
		ASSERT_THAT(IsNotNull(FreshSlot));
		FreshSlot->ActiveAction.Action = FArcTacticsNodeHandle(0);
		ArcTacticsSlotTestHelpers::BindActiveActionToPackage(*FreshSlot);
		FreshSlot->ActiveAction.Serial = 5;
		FreshSlot->ActiveAction.ActionData.InitializeAs<FArcTacticsPoolCallbackActionInstanceData>();
		Pool->CompleteActionQuery(Token, CompletedQuery);
		FArcTacticsPoolCallbackActionInstanceData* FreshData = FreshSlot->ActiveAction.ActionData.GetMutablePtr<FArcTacticsPoolCallbackActionInstanceData>();
		ASSERT_THAT(IsNotNull(FreshData));
		ASSERT_THAT(AreEqual(0, FreshData->CallbackCount));
		ArcTacticsInstancePoolTestsPrivate::DestroyEntity(Spawner.GetWorld(), EntityManager, Entity);
	}

	TEST_METHOD(WorldPool_WorldTeardownCancelsLiveTaskAndAction)
	{
		UArcTacticsPoolTestObject* CancelTarget = NewObject<UArcTacticsPoolTestObject>(GetTransientPackage());
		CancelTarget->AddToRoot();
		{
			FActorTestSpawner LocalSpawner;
			LocalSpawner.GetWorld();
			LocalSpawner.InitializeGameSubsystems();
			UMassEntitySubsystem* EntitySubsystem = LocalSpawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
			UArcTacticsInstanceSubsystem* Pool = LocalSpawner.GetWorld().GetSubsystem<UArcTacticsInstanceSubsystem>();
			ASSERT_THAT(IsNotNull(EntitySubsystem));
			ASSERT_THAT(IsNotNull(Pool));
			UArcTacticsConfig* Config = ArcTacticsInstancePoolTestsPrivate::MakeConfig(true, true);
			ASSERT_THAT(IsTrue(Config->CompiledData.IsExecutable()));
			FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
			const FMassEntityHandle Entity = ArcTacticsInstancePoolTestsPrivate::CreateTestEntity(EntityManager);
			const FArcTacticsInstanceHandle Handle = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*Config), FArcTacticsDecisionSettings());
			ASSERT_THAT(IsTrue(Handle.IsValid()));
			FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
			ASSERT_THAT(IsNotNull(Slot));
			Slot->ResetForDecision(FArcTQSFunctionContext());
			ArcTacticsSlotTestHelpers::PackageInstance(*Slot).bGlobalTaskRunning = true;
			Slot->ActiveAction.Action = FArcTacticsNodeHandle(1);
			ArcTacticsSlotTestHelpers::BindActiveActionToPackage(*Slot);
			Slot->ActiveAction.ActionData.InitializeAs<FArcTacticsPoolCallbackActionInstanceData>();
			ArcTacticsSlotTestHelpers::PackageInstance(*Slot).NodeInstanceData[0].GetPtr<FArcTacticsPoolCallbackTaskInstanceData>()->CancelTarget = CancelTarget;
			Slot->ActiveAction.ActionData.GetMutablePtr<FArcTacticsPoolCallbackActionInstanceData>()->CancelTarget = CancelTarget;
			ASSERT_THAT(AreEqual(0, CancelTarget->CancelCount));
		}
		ASSERT_THAT(AreEqual(2, CancelTarget->CancelCount));
		CancelTarget->RemoveFromRoot();
	}

	TEST_METHOD(WorldPool_DecisionRequestGatesIntervalAndFailedDecisionRetries)
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* EntitySubsystem = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = Spawner.GetWorld().GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		if (!EntitySubsystem || !Pool)
		{
			return;
		}
		UArcTacticsConfig* Config = ArcTacticsInstancePoolTestsPrivate::MakeMinScoreBoundConfig();
		ASSERT_THAT(IsTrue(Config->CompiledData.IsExecutable()));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		const FMassEntityHandle Entity = ArcTacticsInstancePoolTestsPrivate::CreateTestEntity(EntityManager);
		const FArcTacticsInstanceHandle Handle = Pool->AllocateInstance(
			EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*Config), ArcTacticsInstancePoolTestsPrivate::MakeSettings(3.0f));
		ASSERT_THAT(IsTrue(Handle.IsValid()));
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		if (!Slot)
		{
			return;
		}
		ASSERT_THAT(IsNear(3.0f, Slot->Settings.DecisionInterval, 0.001f));
		Slot->Context.Self = FMassEntityHandle(3, 1);
		ASSERT_THAT(IsTrue(Slot->ResetForDecision(FArcTQSFunctionContext())));
		ASSERT_THAT(IsNear(3.0f, Slot->Settings.DecisionInterval, 0.001f));
		UWorld& World = Spawner.GetWorld();
		const double StartTime = World.GetTimeSeconds();
		ASSERT_THAT(IsFalse(Slot->bDecisionRequested));
		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Handle, 0.0f, nullptr);
		ASSERT_THAT(AreEqual(EArcTacticsDecisionStatus::Succeeded, Slot->DecisionStatus,
			TEXT("The pending decision begun by ResetForDecision completes without a request")));
		ASSERT_THAT(IsFalse(Slot->bDecisionRequested,
			TEXT("A successful decision without a selection schedules nothing")));
		ASSERT_THAT(IsNear(StartTime + 3.0, Slot->NextDecisionTime, 0.001,
			TEXT("DecisionInterval is the minimum time to the next decision")));

		const uint32 FirstGeneration = Slot->DecisionGeneration;
		Pool->RequestDecision(Handle);
		ASSERT_THAT(IsTrue(Slot->bDecisionRequested));
		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Handle, 0.0f, nullptr);
		ASSERT_THAT(AreEqual(FirstGeneration, Slot->DecisionGeneration,
			TEXT("A request inside the interval does not run a decision")));
		ASSERT_THAT(IsTrue(Slot->bDecisionRequested, TEXT("A deferred request stays set")));

		Slot->Context.Self = FMassEntityHandle(5, 1);
		World.TimeSeconds = StartTime + 3.0;
		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Handle, 0.0f, nullptr);
		ASSERT_THAT(IsTrue(Slot->DecisionGeneration != FirstGeneration,
			TEXT("The deferred request runs once the interval has elapsed")));
		ASSERT_THAT(IsFalse(Slot->bDecisionRequested, TEXT("Beginning a decision consumes the request")));
		ASSERT_THAT(IsNear(5.0f, ArcTacticsSlotTestHelpers::PackageInstance(*Slot).DecisionSettings.MinScore, 0.001f));
		ASSERT_THAT(IsNear(StartTime + 6.0, Slot->NextDecisionTime, 0.001));

		const uint32 SecondGeneration = Slot->DecisionGeneration;
		World.TimeSeconds = StartTime + 6.0;
		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Handle, 0.0f, nullptr);
		ASSERT_THAT(AreEqual(SecondGeneration, Slot->DecisionGeneration,
			TEXT("No decision runs without a request even once the interval has elapsed")));
		Slot->Context.Self = FMassEntityHandle(INDEX_NONE, 1);
		Pool->RequestDecision(Handle);
		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Handle, 0.0f, nullptr);
		ASSERT_THAT(AreEqual(EArcTacticsDecisionStatus::Failed, Slot->DecisionStatus));
		ASSERT_THAT(IsTrue(Slot->Diagnostic == TEXT("InvalidDecisionSettings")));
		ASSERT_THAT(IsTrue(Slot->bDecisionRequested,
			TEXT("A failed decision with no active action requests a retry")));
		ASSERT_THAT(IsNear(StartTime + 9.0, Slot->NextDecisionTime, 0.001,
			TEXT("The retry waits the trait DecisionInterval")));
		FArcTacticsInstanceSlot Direct;
		const bool bDirectInitialized = ArcTacticsSlotTestHelpers::InitializeSlot(
			Direct, Config->CompiledData, ArcTacticsInstancePoolTestsPrivate::MakeSettings(-1.0f), FMassEntityHandle(-1, 1), 19);
		ASSERT_THAT(IsFalse(bDirectInitialized));
		ASSERT_THAT(AreEqual(EArcTacticsDecisionStatus::Failed, Direct.DecisionStatus));
		ASSERT_THAT(IsTrue(Direct.Diagnostic == TEXT("InvalidTraitDecisionSettings")));
		ArcTacticsInstancePoolTestsPrivate::DestroyEntity(Spawner.GetWorld(), EntityManager, Entity);
	}

	TEST_METHOD(WorldPool_FailedInitializationReturnsSlotForReuse)
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* EntitySubsystem = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = Spawner.GetWorld().GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		if (!EntitySubsystem || !Pool)
		{
			return;
		}
		UArcTacticsConfig* Config = ArcTacticsInstancePoolTestsPrivate::MakeConfig(false);
		ASSERT_THAT(IsTrue(Config->CompiledData.IsExecutable()));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		const FMassEntityHandle SyntheticEntity(-1, 1);
		ASSERT_THAT(IsTrue(SyntheticEntity.IsSet()));
		TestRunner->AddExpectedError(TEXT("invalid decision settings"), EAutomationExpectedErrorFlags::Contains, 1);
		const FArcTacticsInstanceHandle Failed = Pool->AllocateInstance(
			EntityManager, SyntheticEntity, ArcTacticsSlotTestHelpers::Configs(*Config), ArcTacticsInstancePoolTestsPrivate::MakeSettings(-1.0f));
		ASSERT_THAT(IsFalse(Failed.IsValid()));
		const FMassEntityHandle Entity = ArcTacticsInstancePoolTestsPrivate::CreateTestEntity(EntityManager);
		const FArcTacticsInstanceHandle Reused = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*Config), FArcTacticsDecisionSettings());
		ASSERT_THAT(IsTrue(Reused.IsValid()));
		ASSERT_THAT(AreEqual(0, Reused.Index));
		ArcTacticsInstancePoolTestsPrivate::DestroyEntity(Spawner.GetWorld(), EntityManager, Entity);
	}

	TEST_METHOD(WorldPool_ActionLifecyclePublishesOnlySuccessfulRunAndTick)
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* EntitySubsystem = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = Spawner.GetWorld().GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		if (!EntitySubsystem || !Pool)
		{
			return;
		}
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		UArcTacticsPoolOutputSink* Sink = NewObject<UArcTacticsPoolOutputSink>(GetTransientPackage());
		const FMassEntityHandle Entity = ArcTacticsInstancePoolTestsPrivate::CreateTestEntity(EntityManager);
		UArcTacticsConfig* RunningConfig = ArcTacticsInstancePoolTestsPrivate::MakeLifecycleConfig(1, *Sink);
		ASSERT_THAT(IsTrue(RunningConfig->CompiledData.IsExecutable()));
		const FArcTacticsInstanceHandle Running = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*RunningConfig), ArcTacticsInstancePoolTestsPrivate::MakeSettings(0.0f));
		ASSERT_THAT(IsTrue(Running.IsValid()));
		Pool->RequestDecision(Running);
		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Running, 0.0f, nullptr);
		FArcTacticsInstanceSlot* RunningSlot = Pool->Resolve(Running);
		ASSERT_THAT(IsNotNull(RunningSlot));
		ASSERT_THAT(IsTrue(RunningSlot && RunningSlot->ActiveAction.IsSet()));
		ASSERT_THAT(AreEqual(101, Sink->Value));
		UArcTacticsUtilityProcessor::TickAction(
			*Pool, EntityManager, Running, EArcTacticsActionTickLane::Utility, true, nullptr);
		RunningSlot = Pool->Resolve(Running);
		ASSERT_THAT(IsNotNull(RunningSlot));
		ASSERT_THAT(IsTrue(RunningSlot && !RunningSlot->ActiveAction.IsSet()));
		ASSERT_THAT(AreEqual(201, Sink->Value));
		Pool->CancelAndFreeInstance(EntityManager, Entity, Running, nullptr);

		UArcTacticsPoolOutputSink* TickRunningSink = NewObject<UArcTacticsPoolOutputSink>(GetTransientPackage());
		TickRunningSink->Value = Sink->Value;
		UArcTacticsConfig* TickRunningConfig = ArcTacticsInstancePoolTestsPrivate::MakeLifecycleConfig(2, *TickRunningSink);
		ASSERT_THAT(IsTrue(TickRunningConfig->CompiledData.IsExecutable()));
		const FArcTacticsInstanceHandle TickRunning = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*TickRunningConfig), ArcTacticsInstancePoolTestsPrivate::MakeSettings(0.0f));
		ASSERT_THAT(IsTrue(TickRunning.IsValid()));
		Pool->RequestDecision(TickRunning);
		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, TickRunning, 0.0f, nullptr);
		ASSERT_THAT(AreEqual(102, TickRunningSink->Value));
		FArcTacticsInstanceSlot* TickRunningSlot = Pool->Resolve(TickRunning);
		ASSERT_THAT(IsNotNull(TickRunningSlot));
		if (!TickRunningSlot)
		{
			Pool->CancelAndFreeInstance(EntityManager, Entity, TickRunning, nullptr);
			ArcTacticsInstancePoolTestsPrivate::DestroyEntity(Spawner.GetWorld(), EntityManager, Entity);
			return;
		}
		ArcTacticsSlotTestHelpers::PackageInstance(*TickRunningSlot).GlobalParameters.SetValueInt32(TEXT("Captured"), 999);
		UArcTacticsUtilityProcessor::TickAction(
			*Pool, EntityManager, TickRunning, EArcTacticsActionTickLane::Utility, true, nullptr);
		TickRunningSlot = Pool->Resolve(TickRunning);
		ASSERT_THAT(IsNotNull(TickRunningSlot));
		ASSERT_THAT(IsTrue(TickRunningSlot && TickRunningSlot->ActiveAction.IsSet()));
		ASSERT_THAT(AreEqual(202, TickRunningSink->Value));
		FArcTacticsPoolLifecycleActionInstanceData* TickRunningData =
			TickRunningSlot->ActiveAction.ActionData.GetMutablePtr<FArcTacticsPoolLifecycleActionInstanceData>();
		ASSERT_THAT(IsNotNull(TickRunningData));
		if (TickRunningData)
		{
			const uint32 ActiveSerial = TickRunningSlot->ActiveAction.Serial;
			const uint32 DecisionGeneration = TickRunningSlot->DecisionGeneration;
			ASSERT_THAT(AreEqual(1, TickRunningData->EnterCount));
			ASSERT_THAT(AreEqual(1, TickRunningData->TickCount));
			ASSERT_THAT(AreEqual(77, TickRunningData->CapturedGlobal));
			Pool->RequestDecision(TickRunning);
			UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, TickRunning, 0.0f, nullptr);
			TickRunningSlot = Pool->Resolve(TickRunning);
			ASSERT_THAT(IsNotNull(TickRunningSlot));
			if (TickRunningSlot)
			{
				ASSERT_THAT(IsTrue(TickRunningSlot->DecisionGeneration > DecisionGeneration));
				ASSERT_THAT(AreEqual(1, TickRunningSlot->Candidates.Num()));
				if (TickRunningSlot->Candidates.Num() == 1)
				{
					ASSERT_THAT(IsTrue(TickRunningSlot->Candidates[0].RejectionReason == TEXT("BelowActivePriority"),
						TEXT("The running action's own branch is vetoed")));
				}
				ASSERT_THAT(IsTrue(TickRunningSlot->ActiveAction.IsSet(),
					TEXT("A decision with no selection keeps the active action running")));
				FArcTacticsPoolLifecycleActionInstanceData* KeptData =
					TickRunningSlot->ActiveAction.ActionData.GetMutablePtr<FArcTacticsPoolLifecycleActionInstanceData>();
				ASSERT_THAT(AreEqual(ActiveSerial, TickRunningSlot->ActiveAction.Serial));
				ASSERT_THAT(IsNotNull(KeptData));
				if (KeptData)
				{
					ASSERT_THAT(AreEqual(1, KeptData->EnterCount));
					ASSERT_THAT(AreEqual(1, KeptData->TickCount));
					ASSERT_THAT(AreEqual(77, KeptData->CapturedGlobal));
				}
			}
		}
		Pool->CancelAndFreeInstance(EntityManager, Entity, TickRunning, nullptr);
		ASSERT_THAT(AreEqual(202, TickRunningSink->Value));

		UArcTacticsPoolOutputSink* SucceededSink = NewObject<UArcTacticsPoolOutputSink>(GetTransientPackage());
		UArcTacticsConfig* SucceededConfig = ArcTacticsInstancePoolTestsPrivate::MakeLifecycleConfig(0, *SucceededSink);
		ASSERT_THAT(IsTrue(SucceededConfig->CompiledData.IsExecutable()));
		const FArcTacticsInstanceHandle Succeeded = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*SucceededConfig), ArcTacticsInstancePoolTestsPrivate::MakeSettings(0.0f));
		ASSERT_THAT(IsTrue(Succeeded.IsValid()));
		Pool->RequestDecision(Succeeded);
		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Succeeded, 0.0f, nullptr);
		FArcTacticsInstanceSlot* SucceededSlot = Pool->Resolve(Succeeded);
		ASSERT_THAT(IsNotNull(SucceededSlot));
		ASSERT_THAT(IsTrue(SucceededSlot && !SucceededSlot->ActiveAction.IsSet()));
		ASSERT_THAT(AreEqual(100, SucceededSink->Value));
		Pool->CancelAndFreeInstance(EntityManager, Entity, Succeeded, nullptr);

		UArcTacticsPoolOutputSink* TickFailedSink = NewObject<UArcTacticsPoolOutputSink>(GetTransientPackage());
		TickFailedSink->Value = Sink->Value;
		UArcTacticsConfig* TickFailedConfig = ArcTacticsInstancePoolTestsPrivate::MakeLifecycleConfig(4, *TickFailedSink);
		ASSERT_THAT(IsTrue(TickFailedConfig->CompiledData.IsExecutable()));
		const FArcTacticsInstanceHandle TickFailed = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*TickFailedConfig), ArcTacticsInstancePoolTestsPrivate::MakeSettings(0.0f));
		ASSERT_THAT(IsTrue(TickFailed.IsValid()));
		Pool->RequestDecision(TickFailed);
		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, TickFailed, 0.0f, nullptr);
		ASSERT_THAT(AreEqual(104, TickFailedSink->Value));
		UArcTacticsUtilityProcessor::TickAction(
			*Pool, EntityManager, TickFailed, EArcTacticsActionTickLane::Utility, true, nullptr);
		FArcTacticsInstanceSlot* TickFailedSlot = Pool->Resolve(TickFailed);
		ASSERT_THAT(IsNotNull(TickFailedSlot));
		ASSERT_THAT(IsTrue(TickFailedSlot && !TickFailedSlot->ActiveAction.IsSet()));
		ASSERT_THAT(AreEqual(104, TickFailedSink->Value));
		Pool->CancelAndFreeInstance(EntityManager, Entity, TickFailed, nullptr);

		UArcTacticsPoolOutputSink* FailedSink = NewObject<UArcTacticsPoolOutputSink>(GetTransientPackage());
		FailedSink->Value = Sink->Value;
		UArcTacticsConfig* FailedConfig = ArcTacticsInstancePoolTestsPrivate::MakeLifecycleConfig(3, *FailedSink);
		ASSERT_THAT(IsTrue(FailedConfig->CompiledData.IsExecutable()));
		const FArcTacticsInstanceHandle Failed = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*FailedConfig), ArcTacticsInstancePoolTestsPrivate::MakeSettings(0.0f));
		ASSERT_THAT(IsTrue(Failed.IsValid()));
		Pool->RequestDecision(Failed);
		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Failed, 0.0f, nullptr);
		FArcTacticsInstanceSlot* FailedSlot = Pool->Resolve(Failed);
		ASSERT_THAT(IsNotNull(FailedSlot));
		ASSERT_THAT(IsTrue(FailedSlot && !FailedSlot->ActiveAction.IsSet()));
		ASSERT_THAT(AreEqual(201, FailedSink->Value));
		Pool->CancelAndFreeInstance(EntityManager, Entity, Failed, nullptr);
		const FArcTacticsInstanceHandle Reused = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*RunningConfig), ArcTacticsInstancePoolTestsPrivate::MakeSettings(0.0f));
		ASSERT_THAT(IsTrue(Reused.IsValid()));
		Pool->RequestDecision(Reused);
		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Reused, 0.0f, nullptr);
		FArcTacticsInstanceSlot* ReusedSlot = Pool->Resolve(Reused);
		ASSERT_THAT(IsNotNull(ReusedSlot));
		if (ReusedSlot)
		{
			ASSERT_THAT(IsTrue(ReusedSlot->ActiveAction.IsSet()));
			UArcTacticsUtilityProcessor::TickAction(
				*Pool, EntityManager, Reused, EArcTacticsActionTickLane::Utility, true, nullptr);
			ASSERT_THAT(IsTrue(!ReusedSlot->ActiveAction.IsSet()));
		}
		Pool->CancelAndFreeInstance(EntityManager, Entity, Reused, nullptr);
		ArcTacticsInstancePoolTestsPrivate::DestroyEntity(Spawner.GetWorld(), EntityManager, Entity);
	}

	TEST_METHOD(WorldPool_CooldownOnEnterStampsOnlyWhenActionStartsAndEndDoesNotRestamp)
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		if (!EntitySubsystem || !Pool)
		{
			return;
		}
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		const FMassEntityHandle Entity = ArcTacticsInstancePoolTestsPrivate::CreateTestEntity(EntityManager);
		ArcTacticsInstancePoolTestsPrivate::FBranchSpec Spec;
		Spec.Mode = 1;
		Spec.Cooldown = 10.0f;
		Spec.CooldownStart = EArcTacticsCooldownStart::OnEnter;
		UArcTacticsConfig* Config = ArcTacticsInstancePoolTestsPrivate::MakeBranchConfig({Spec});
		ASSERT_THAT(IsTrue(Config->CompiledData.IsExecutable()));
		const FArcTacticsInstanceHandle Handle = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*Config), ArcTacticsInstancePoolTestsPrivate::MakeSettings(0.0f));
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		if (!Slot)
		{
			return;
		}
		const double StartTime = World.GetTimeSeconds();
		Pool->RequestDecision(Handle);
		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Handle, 0.0f, nullptr);
		ASSERT_THAT(IsTrue(Slot->ActiveAction.IsSet()));
		const double* Stamp = Slot->CooldownEndTimes.Find(ArcTacticsSlotTestHelpers::CooldownKey(*Config, 0));
		ASSERT_THAT(IsNotNull(Stamp, TEXT("OnEnter stamps when the action becomes active")));
		ASSERT_THAT(IsTrue(Stamp && FMath::IsNearlyEqual(StartTime + 10.0, *Stamp, 0.001)));

		World.TimeSeconds = StartTime + 3.0;
		UArcTacticsUtilityProcessor::TickAction(
			*Pool, EntityManager, Handle, EArcTacticsActionTickLane::Utility, true, nullptr);
		ASSERT_THAT(IsFalse(Slot->ActiveAction.IsSet()));
		ASSERT_THAT(IsTrue(Slot->bDecisionRequested, TEXT("An action end requests a decision")));
		Stamp = Slot->CooldownEndTimes.Find(ArcTacticsSlotTestHelpers::CooldownKey(*Config, 0));
		ASSERT_THAT(IsTrue(Stamp && FMath::IsNearlyEqual(StartTime + 10.0, *Stamp, 0.001),
			TEXT("An OnEnter action's end does not stamp again")));

		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Handle, 0.0f, nullptr);
		ASSERT_THAT(IsFalse(Slot->ActiveAction.IsSet()));
		ASSERT_THAT(IsTrue(ArcTacticsInstancePoolTestsPrivate::HasOutcome(
			*Slot, 0, EArcTacticsCandidateOutcome::Vetoed, TEXT("ActionCooldown"))));

		World.TimeSeconds = StartTime + 10.0;
		Pool->RequestDecision(Handle);
		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Handle, 0.0f, nullptr);
		ASSERT_THAT(IsTrue(Slot->ActiveAction.IsSet(), TEXT("The branch is selectable at its cooldown end")));
		Stamp = Slot->CooldownEndTimes.Find(ArcTacticsSlotTestHelpers::CooldownKey(*Config, 0));
		ASSERT_THAT(IsTrue(Stamp && FMath::IsNearlyEqual(StartTime + 20.0, *Stamp, 0.001)));
		Pool->CancelAndFreeInstance(EntityManager, Entity, Handle, nullptr);

		ArcTacticsInstancePoolTestsPrivate::FBranchSpec FailingSpec;
		FailingSpec.Mode = 3;
		FailingSpec.Cooldown = 4.0f;
		FailingSpec.CooldownStart = EArcTacticsCooldownStart::OnEnter;
		UArcTacticsConfig* FailingConfig = ArcTacticsInstancePoolTestsPrivate::MakeBranchConfig({FailingSpec});
		ASSERT_THAT(IsTrue(FailingConfig->CompiledData.IsExecutable()));
		const FArcTacticsInstanceHandle FailingHandle = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*FailingConfig), ArcTacticsInstancePoolTestsPrivate::MakeSettings(0.0f));
		FArcTacticsInstanceSlot* FailingSlot = Pool->Resolve(FailingHandle);
		ASSERT_THAT(IsNotNull(FailingSlot));
		if (FailingSlot)
		{
			Pool->RequestDecision(FailingHandle);
			UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, FailingHandle, 0.0f, nullptr);
			ASSERT_THAT(IsFalse(FailingSlot->ActiveAction.IsSet()));
			ASSERT_THAT(IsNull(FailingSlot->CooldownEndTimes.Find(ArcTacticsSlotTestHelpers::CooldownKey(*FailingConfig, 0)),
				TEXT("OnEnter does not stamp when Enter fails")));
			ASSERT_THAT(IsTrue(FailingSlot->bDecisionRequested, TEXT("An Enter failure requests a decision")));
			const uint32 FailedEnterGeneration = FailingSlot->DecisionGeneration;
			UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, FailingHandle, 0.0f, nullptr);
			ASSERT_THAT(IsTrue(FailingSlot->DecisionGeneration != FailedEnterGeneration,
				TEXT("The follow-up decision ran, so its candidate outcome is fresh")));
			ASSERT_THAT(IsTrue(ArcTacticsInstancePoolTestsPrivate::HasOutcome(
				*FailingSlot, 0, EArcTacticsCandidateOutcome::Accepted, nullptr),
				TEXT("A branch whose Enter failed is not on cooldown")));
			ASSERT_THAT(IsNull(FailingSlot->CooldownEndTimes.Find(ArcTacticsSlotTestHelpers::CooldownKey(*FailingConfig, 0))));
		}
		Pool->CancelAndFreeInstance(EntityManager, Entity, FailingHandle, nullptr);

		ArcTacticsInstancePoolTestsPrivate::FBranchSpec InstantSpec;
		InstantSpec.Mode = 0;
		InstantSpec.Cooldown = 4.0f;
		InstantSpec.CooldownStart = EArcTacticsCooldownStart::OnEnter;
		UArcTacticsConfig* InstantConfig = ArcTacticsInstancePoolTestsPrivate::MakeBranchConfig({InstantSpec});
		ASSERT_THAT(IsTrue(InstantConfig->CompiledData.IsExecutable()));
		const FArcTacticsInstanceHandle InstantHandle = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*InstantConfig), ArcTacticsInstancePoolTestsPrivate::MakeSettings(0.0f));
		FArcTacticsInstanceSlot* InstantSlot = Pool->Resolve(InstantHandle);
		ASSERT_THAT(IsNotNull(InstantSlot));
		if (InstantSlot)
		{
			const double InstantTime = World.GetTimeSeconds();
			Pool->RequestDecision(InstantHandle);
			UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, InstantHandle, 0.0f, nullptr);
			ASSERT_THAT(IsFalse(InstantSlot->ActiveAction.IsSet()));
			const double* InstantStamp = InstantSlot->CooldownEndTimes.Find(ArcTacticsSlotTestHelpers::CooldownKey(*InstantConfig, 0));
			ASSERT_THAT(IsTrue(InstantStamp && FMath::IsNearlyEqual(InstantTime + 4.0, *InstantStamp, 0.001),
				TEXT("OnEnter stamps when Enter succeeds immediately")));
			UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, InstantHandle, 0.0f, nullptr);
			ASSERT_THAT(IsTrue(ArcTacticsInstancePoolTestsPrivate::HasOutcome(
				*InstantSlot, 0, EArcTacticsCandidateOutcome::Vetoed, TEXT("ActionCooldown"))));
		}
		Pool->CancelAndFreeInstance(EntityManager, Entity, InstantHandle, nullptr);

		UArcTacticsPoolOutputSink* UnusedSink = NewObject<UArcTacticsPoolOutputSink>(GetTransientPackage());
		UArcTacticsConfig* PublishFailConfig = ArcTacticsInstancePoolTestsPrivate::MakeLifecycleConfig(1, *UnusedSink);
		PublishFailConfig->Actions[0].ParameterBag.SetValueObject(TEXT("Sink"), nullptr);
		PublishFailConfig->Actions[0].Parameters.Cooldown = 4.0f;
		PublishFailConfig->Actions[0].Parameters.CooldownStart = EArcTacticsCooldownStart::OnEnter;
		ArcTacticsInstancePoolTestsPrivate::CompileOrLogDiagnostics(*PublishFailConfig, 9);
		ASSERT_THAT(IsTrue(PublishFailConfig->CompiledData.IsExecutable()));
		const FArcTacticsInstanceHandle PublishFailHandle = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*PublishFailConfig), ArcTacticsInstancePoolTestsPrivate::MakeSettings(0.0f));
		FArcTacticsInstanceSlot* PublishFailSlot = Pool->Resolve(PublishFailHandle);
		ASSERT_THAT(IsNotNull(PublishFailSlot));
		if (PublishFailSlot)
		{
			Pool->RequestDecision(PublishFailHandle);
			UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, PublishFailHandle, 0.0f, nullptr);
			ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::PackageInstance(*PublishFailSlot).Diagnostic == TEXT("ActiveActionOutputBindingFailed"),
				TEXT("The running Enter is turned into a failure by the output publish")));
			ASSERT_THAT(IsFalse(PublishFailSlot->ActiveAction.IsSet()));
			ASSERT_THAT(IsNull(PublishFailSlot->CooldownEndTimes.Find(ArcTacticsSlotTestHelpers::CooldownKey(*PublishFailConfig, 0)),
				TEXT("OnEnter does not stamp when publishing the Enter outputs fails")));
		}
		Pool->CancelAndFreeInstance(EntityManager, Entity, PublishFailHandle, nullptr);
		ArcTacticsInstancePoolTestsPrivate::DestroyEntity(World, EntityManager, Entity);
	}

	TEST_METHOD(WorldPool_CooldownOnEndStampsAtSuccessAndFailure)
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		if (!EntitySubsystem || !Pool)
		{
			return;
		}
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		const FMassEntityHandle Entity = ArcTacticsInstancePoolTestsPrivate::CreateTestEntity(EntityManager);
		ArcTacticsInstancePoolTestsPrivate::FBranchSpec Spec;
		Spec.Mode = 1;
		Spec.Cooldown = 10.0f;
		Spec.CooldownStart = EArcTacticsCooldownStart::OnEnd;
		UArcTacticsConfig* Config = ArcTacticsInstancePoolTestsPrivate::MakeBranchConfig({Spec});
		ASSERT_THAT(IsTrue(Config->CompiledData.IsExecutable()));
		const FArcTacticsInstanceHandle Handle = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*Config), ArcTacticsInstancePoolTestsPrivate::MakeSettings(0.0f));
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		if (!Slot)
		{
			return;
		}
		const double StartTime = World.GetTimeSeconds();
		Pool->RequestDecision(Handle);
		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Handle, 0.0f, nullptr);
		ASSERT_THAT(IsTrue(Slot->ActiveAction.IsSet()));
		ASSERT_THAT(IsNull(Slot->CooldownEndTimes.Find(ArcTacticsSlotTestHelpers::CooldownKey(*Config, 0)), TEXT("OnEnd does not stamp at enter")));

		World.TimeSeconds = StartTime + 3.0;
		UArcTacticsUtilityProcessor::TickAction(
			*Pool, EntityManager, Handle, EArcTacticsActionTickLane::Utility, true, nullptr);
		ASSERT_THAT(IsFalse(Slot->ActiveAction.IsSet()));
		const double* Stamp = Slot->CooldownEndTimes.Find(ArcTacticsSlotTestHelpers::CooldownKey(*Config, 0));
		ASSERT_THAT(IsTrue(Stamp && FMath::IsNearlyEqual(StartTime + 13.0, *Stamp, 0.001),
			TEXT("OnEnd stamps end time plus the captured Cooldown on success")));

		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Handle, 0.0f, nullptr);
		ASSERT_THAT(IsFalse(Slot->ActiveAction.IsSet()));
		ASSERT_THAT(IsTrue(ArcTacticsInstancePoolTestsPrivate::HasOutcome(
			*Slot, 0, EArcTacticsCandidateOutcome::Vetoed, TEXT("ActionCooldown"))));

		World.TimeSeconds = StartTime + 13.0;
		Pool->RequestDecision(Handle);
		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Handle, 0.0f, nullptr);
		ASSERT_THAT(IsTrue(Slot->ActiveAction.IsSet()));
		Pool->CancelAndFreeInstance(EntityManager, Entity, Handle, nullptr);

		ArcTacticsInstancePoolTestsPrivate::FBranchSpec FailingSpec;
		FailingSpec.Mode = 3;
		FailingSpec.Cooldown = 4.0f;
		FailingSpec.CooldownStart = EArcTacticsCooldownStart::OnEnd;
		UArcTacticsConfig* FailingConfig = ArcTacticsInstancePoolTestsPrivate::MakeBranchConfig({FailingSpec});
		ASSERT_THAT(IsTrue(FailingConfig->CompiledData.IsExecutable()));
		const FArcTacticsInstanceHandle FailingHandle = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*FailingConfig), ArcTacticsInstancePoolTestsPrivate::MakeSettings(0.0f));
		FArcTacticsInstanceSlot* FailingSlot = Pool->Resolve(FailingHandle);
		ASSERT_THAT(IsNotNull(FailingSlot));
		if (FailingSlot)
		{
			const double FailTime = World.GetTimeSeconds();
			Pool->RequestDecision(FailingHandle);
			UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, FailingHandle, 0.0f, nullptr);
			ASSERT_THAT(IsFalse(FailingSlot->ActiveAction.IsSet()));
			const double* FailStamp = FailingSlot->CooldownEndTimes.Find(ArcTacticsSlotTestHelpers::CooldownKey(*FailingConfig, 0));
			ASSERT_THAT(IsTrue(FailStamp && FMath::IsNearlyEqual(FailTime + 4.0, *FailStamp, 0.001),
				TEXT("OnEnd stamps when Enter fails")));
		}
		Pool->CancelAndFreeInstance(EntityManager, Entity, FailingHandle, nullptr);

		ArcTacticsInstancePoolTestsPrivate::FBranchSpec TickFailingSpec;
		TickFailingSpec.Mode = 4;
		TickFailingSpec.Cooldown = 6.0f;
		TickFailingSpec.CooldownStart = EArcTacticsCooldownStart::OnEnd;
		UArcTacticsConfig* TickFailingConfig = ArcTacticsInstancePoolTestsPrivate::MakeBranchConfig({TickFailingSpec});
		ASSERT_THAT(IsTrue(TickFailingConfig->CompiledData.IsExecutable()));
		const FArcTacticsInstanceHandle TickFailingHandle = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*TickFailingConfig), ArcTacticsInstancePoolTestsPrivate::MakeSettings(0.0f));
		FArcTacticsInstanceSlot* TickFailingSlot = Pool->Resolve(TickFailingHandle);
		ASSERT_THAT(IsNotNull(TickFailingSlot));
		if (TickFailingSlot)
		{
			Pool->RequestDecision(TickFailingHandle);
			UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, TickFailingHandle, 0.0f, nullptr);
			ASSERT_THAT(IsTrue(TickFailingSlot->ActiveAction.IsSet()));
			ASSERT_THAT(IsNull(TickFailingSlot->CooldownEndTimes.Find(ArcTacticsSlotTestHelpers::CooldownKey(*TickFailingConfig, 0))));
			const double TickFailTime = World.GetTimeSeconds() + 2.0;
			World.TimeSeconds = TickFailTime;
			UArcTacticsUtilityProcessor::TickAction(
				*Pool, EntityManager, TickFailingHandle, EArcTacticsActionTickLane::Utility, true, nullptr);
			ASSERT_THAT(IsFalse(TickFailingSlot->ActiveAction.IsSet()));
			const double* TickFailStamp = TickFailingSlot->CooldownEndTimes.Find(ArcTacticsSlotTestHelpers::CooldownKey(*TickFailingConfig, 0));
			ASSERT_THAT(IsTrue(TickFailStamp && FMath::IsNearlyEqual(TickFailTime + 6.0, *TickFailStamp, 0.001),
				TEXT("OnEnd stamps when Tick fails")));
		}
		Pool->CancelAndFreeInstance(EntityManager, Entity, TickFailingHandle, nullptr);
		ArcTacticsInstancePoolTestsPrivate::DestroyEntity(World, EntityManager, Entity);
	}

	TEST_METHOD(WorldPool_HigherPriorityReplacesRunningActionAndPriorityVetoPrecedesCooldown)
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		if (!EntitySubsystem || !Pool)
		{
			return;
		}
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		const FMassEntityHandle Entity = ArcTacticsInstancePoolTestsPrivate::CreateTestEntity(EntityManager);
		UArcTacticsPoolTestObject* RunningCancels = NewObject<UArcTacticsPoolTestObject>(GetTransientPackage());
		RunningCancels->AddToRoot();
		ArcTacticsInstancePoolTestsPrivate::FBranchSpec Running;
		Running.Mode = 2;
		Running.Cooldown = 5.0f;
		Running.CancelTarget = RunningCancels;
		ArcTacticsInstancePoolTestsPrivate::FBranchSpec Higher;
		Higher.Mode = 2;
		Higher.Priority = 1;
		ArcTacticsInstancePoolTestsPrivate::FBranchSpec Equal;
		Equal.Mode = 2;
		UArcTacticsConfig* Config = ArcTacticsInstancePoolTestsPrivate::MakeBranchConfig({Running, Higher, Equal});
		ASSERT_THAT(IsTrue(Config->CompiledData.IsExecutable()));
		const FArcTacticsInstanceHandle Handle = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*Config), ArcTacticsInstancePoolTestsPrivate::MakeSettings(0.0f));
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		if (!Slot)
		{
			return;
		}
		const double StartTime = World.GetTimeSeconds();
		Slot->CooldownEndTimes.Add(ArcTacticsSlotTestHelpers::CooldownKey(*Config, 1), StartTime + 5.0);
		Slot->CooldownEndTimes.Add(ArcTacticsSlotTestHelpers::CooldownKey(*Config, 2), StartTime + 100.0);
		Pool->RequestDecision(Handle);
		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Handle, 0.0f, nullptr);
		ASSERT_THAT(IsTrue(Slot->ActiveAction.IsSet()));
		ASSERT_THAT(AreEqual(0, Slot->ActiveAction.SourceIndex));
		const uint32 RunningSerial = Slot->ActiveAction.Serial;

		World.TimeSeconds = StartTime + 1.0;
		Pool->RequestDecision(Handle);
		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Handle, 0.0f, nullptr);
		ASSERT_THAT(IsTrue(ArcTacticsInstancePoolTestsPrivate::HasOutcome(
			*Slot, 0, EArcTacticsCandidateOutcome::Vetoed, TEXT("BelowActivePriority"))));
		ASSERT_THAT(IsTrue(ArcTacticsInstancePoolTestsPrivate::HasOutcome(
			*Slot, 2, EArcTacticsCandidateOutcome::Vetoed, TEXT("BelowActivePriority")),
			TEXT("An equal-priority candidate on cooldown reports the priority veto first")));
		ASSERT_THAT(IsTrue(ArcTacticsInstancePoolTestsPrivate::HasOutcome(
			*Slot, 1, EArcTacticsCandidateOutcome::Vetoed, TEXT("ActionCooldown")),
			TEXT("A higher-priority candidate passes the priority veto and meets its cooldown")));
		ASSERT_THAT(IsTrue(Slot->ActiveAction.IsSet()));
		ASSERT_THAT(AreEqual(RunningSerial, Slot->ActiveAction.Serial,
			TEXT("A decision with no selection keeps the running action")));
		ASSERT_THAT(AreEqual(0, RunningCancels->CancelCount,
			TEXT("A decision with no selection does not cancel the running action")));
		ASSERT_THAT(IsNull(Slot->CooldownEndTimes.Find(ArcTacticsSlotTestHelpers::CooldownKey(*Config, 0))));

		World.TimeSeconds = StartTime + 6.0;
		Pool->RequestDecision(Handle);
		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Handle, 0.0f, nullptr);
		ASSERT_THAT(IsTrue(ArcTacticsInstancePoolTestsPrivate::HasOutcome(
			*Slot, 1, EArcTacticsCandidateOutcome::Accepted, nullptr)));
		ASSERT_THAT(IsTrue(Slot->ActiveAction.IsSet()));
		ASSERT_THAT(AreEqual(1, Slot->ActiveAction.SourceIndex,
			TEXT("A strictly higher priority candidate replaces the running action")));
		ASSERT_THAT(IsTrue(Slot->ActiveAction.Serial != RunningSerial));
		ASSERT_THAT(AreEqual(1, RunningCancels->CancelCount,
			TEXT("Replacement cancels the running action exactly once")));
		const double* ReplacedStamp = Slot->CooldownEndTimes.Find(ArcTacticsSlotTestHelpers::CooldownKey(*Config, 0));
		ASSERT_THAT(IsTrue(ReplacedStamp && FMath::IsNearlyEqual(StartTime + 11.0, *ReplacedStamp, 0.001),
			TEXT("Replacement is an end and stamps the OnEnd cooldown")));
		Pool->CancelAndFreeInstance(EntityManager, Entity, Handle, nullptr);
		ASSERT_THAT(AreEqual(1, RunningCancels->CancelCount));
		RunningCancels->RemoveFromRoot();
		ArcTacticsInstancePoolTestsPrivate::DestroyEntity(World, EntityManager, Entity);
	}

	TEST_METHOD(WorldPool_FailedDecisionWithActiveActionDoesNotRetry)
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		if (!EntitySubsystem || !Pool)
		{
			return;
		}
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		const FMassEntityHandle Entity = ArcTacticsInstancePoolTestsPrivate::CreateTestEntity(EntityManager);
		ArcTacticsInstancePoolTestsPrivate::FBranchSpec Running;
		Running.Mode = 2;
		UArcTacticsConfig* Config = ArcTacticsInstancePoolTestsPrivate::MakeBranchConfig({Running});
		Config->EditorBindings.AddBinding(
			ArcTacticsInstancePoolTestsPrivate::MakePath(Config->ContextID, TEXT("Self.Index")),
			ArcTacticsInstancePoolTestsPrivate::MakePath(Config->DecisionSettingsID, TEXT("MinScore")));
		ArcTacticsInstancePoolTestsPrivate::CompileOrLogDiagnostics(*Config, 9);
		ASSERT_THAT(IsTrue(Config->CompiledData.IsExecutable()));
		const FArcTacticsInstanceHandle Handle = Pool->AllocateInstance(
			EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*Config), ArcTacticsInstancePoolTestsPrivate::MakeSettings(0.0f));
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		if (!Slot)
		{
			return;
		}
		Slot->Context.Self = FMassEntityHandle(0, 1);
		Pool->RequestDecision(Handle);
		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Handle, 0.0f, nullptr);
		ASSERT_THAT(IsTrue(Slot->ActiveAction.IsSet()));
		const uint32 ActiveSerial = Slot->ActiveAction.Serial;

		World.TimeSeconds = Slot->NextDecisionTime + 1.0;
		Slot->Context.Self = FMassEntityHandle(INDEX_NONE, 1);
		Pool->RequestDecision(Handle);
		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Handle, 0.0f, nullptr);
		ASSERT_THAT(AreEqual(EArcTacticsDecisionStatus::Failed, Slot->DecisionStatus));
		ASSERT_THAT(IsTrue(Slot->Diagnostic == TEXT("InvalidDecisionSettings")));
		ASSERT_THAT(IsFalse(Slot->bDecisionRequested,
			TEXT("A failed decision with an active action does not request a retry")));
		ASSERT_THAT(IsTrue(Slot->ActiveAction.IsSet()));
		ASSERT_THAT(AreEqual(ActiveSerial, Slot->ActiveAction.Serial));
		Pool->CancelAndFreeInstance(EntityManager, Entity, Handle, nullptr);
		ArcTacticsInstancePoolTestsPrivate::DestroyEntity(World, EntityManager, Entity);
	}

	TEST_METHOD(WorldPool_SignalledRequestsWithinIntervalRunOneDecisionAtItsEnd)
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		UMassSignalSubsystem* Signals = World.GetSubsystem<UMassSignalSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		ASSERT_THAT(IsNotNull(Signals));
		if (!EntitySubsystem || !Pool || !Signals)
		{
			return;
		}
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		UArcTacticsConfig* Config = ArcTacticsInstancePoolTestsPrivate::MakeConfig(false);
		ASSERT_THAT(IsTrue(Config->CompiledData.IsExecutable()));
		const FArcTacticsDecisionSettings Settings = ArcTacticsInstancePoolTestsPrivate::MakeSettings(2.0f);
		const FMassEntityHandle Entity = ArcTacticsInstancePoolTestsPrivate::CreateTacticsEntity(EntityManager, *Config, Settings);
		FArcTacticsInstanceFragment* InstanceFragment = EntityManager.GetFragmentDataPtr<FArcTacticsInstanceFragment>(Entity);
		ASSERT_THAT(IsNotNull(InstanceFragment));
		if (!InstanceFragment)
		{
			return;
		}
		const FArcTacticsInstanceHandle Handle = Pool->AllocateInstance(EntityManager, Entity, ArcTacticsSlotTestHelpers::Configs(*Config), Settings);
		InstanceFragment->Handle = Handle;
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		if (!Slot)
		{
			return;
		}
		FTickableGameObject& SignalTicker = *Signals;
		UArcTacticsUtilityProcessor* Processor = ArcTacticsInstancePoolTestsPrivate::MakeUtilityProcessor(World, EntityManager);
		const double StartTime = World.GetTimeSeconds();
		const uint32 InitialGeneration = Slot->DecisionGeneration;

		Pool->RequestDecision(Handle);
		ArcTacticsInstancePoolTestsPrivate::RunUtilityProcessor(*Processor, EntityManager);
		ASSERT_THAT(AreEqual(InitialGeneration + 1u, Slot->DecisionGeneration,
			TEXT("A request outside the interval is delivered by signal and runs one decision")));
		const uint32 FirstGeneration = Slot->DecisionGeneration;
		ASSERT_THAT(IsNear(StartTime + 2.0, Slot->NextDecisionTime, 0.001));

		World.TimeSeconds = StartTime + 0.5;
		Pool->RequestDecision(Handle);
		ArcTacticsInstancePoolTestsPrivate::RunUtilityProcessor(*Processor, EntityManager);
		World.TimeSeconds = StartTime + 1.0;
		Pool->RequestDecision(Handle);
		ArcTacticsInstancePoolTestsPrivate::RunUtilityProcessor(*Processor, EntityManager);
		SignalTicker.Tick(0.0f);
		ArcTacticsInstancePoolTestsPrivate::RunUtilityProcessor(*Processor, EntityManager);
		ASSERT_THAT(AreEqual(FirstGeneration, Slot->DecisionGeneration,
			TEXT("Requests inside the interval do not run a decision")));
		ASSERT_THAT(IsTrue(Slot->bDecisionRequested));

		int32 DeliveredDecisionSignals = 0;
		const FDelegateHandle CounterHandle =
			Signals->GetSignalDelegateByName(UE::ArcTactics::Signals::DecisionDue).AddLambda(
				[&DeliveredDecisionSignals, Entity](FName, TConstArrayView<FMassEntityHandle> Entities)
				{
					for (const FMassEntityHandle& Signalled : Entities)
					{
						if (Signalled == Entity)
						{
							++DeliveredDecisionSignals;
						}
					}
				});
		World.TimeSeconds = StartTime + 2.0;
		DeliveredDecisionSignals = 0;
		SignalTicker.Tick(0.0f);
		const int32 DeliveredAtIntervalEnd = DeliveredDecisionSignals;
		Signals->GetSignalDelegateByName(UE::ArcTactics::Signals::DecisionDue).Remove(CounterHandle);
		ASSERT_THAT(AreEqual(1, DeliveredAtIntervalEnd,
			TEXT("Two requests inside the interval queue exactly one delayed DecisionDue")));
		ArcTacticsInstancePoolTestsPrivate::RunUtilityProcessor(*Processor, EntityManager);
		ASSERT_THAT(AreEqual(FirstGeneration + 1u, Slot->DecisionGeneration,
			TEXT("Two requests inside the interval run exactly one decision, delivered at the interval end")));
		ASSERT_THAT(IsFalse(Slot->bDecisionRequested));

		World.TimeSeconds = StartTime + 10.0;
		SignalTicker.Tick(0.0f);
		ArcTacticsInstancePoolTestsPrivate::RunUtilityProcessor(*Processor, EntityManager);
		ASSERT_THAT(AreEqual(FirstGeneration + 1u, Slot->DecisionGeneration,
			TEXT("No further decision is scheduled after the coalesced one")));
		ArcTacticsInstancePoolTestsPrivate::DestroyEntity(World, EntityManager, Entity);
	}
};
