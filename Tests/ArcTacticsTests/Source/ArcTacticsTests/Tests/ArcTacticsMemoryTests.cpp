#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

#if WITH_EDITOR

#include "Actions/ArcTacticsSequenceAction.h"
#include "ArcTQSCompiler.h"
#include "ArcTQSPropertyFunction.h"
#include "ArcTQSQueryDefinition.h"
#include "ArcTQSQuerySubsystem.h"
#include "Debugger/ArcTacticsDebug.h"
#include "Decision/ArcTacticsCompiler.h"
#include "Decision/ArcTacticsConfig.h"
#include "Generators/ArcTQSGenerator_Circle.h"
#include "GlobalTasks/ArcTacticsDataTasks.h"
#include "Mass/ArcTacticsFragments.h"
#include "Mass/ArcTacticsInstanceSubsystem.h"
#include "Mass/ArcTacticsLifecycleProcessor.h"
#include "Mass/ArcTacticsUtilityProcessor.h"
#include "Mass/EntityBuilder.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassNavMeshNavigationFragments.h"
#include "Memory/ArcMassMemory.h"
#include "NativeGameplayTags.h"
#include "SmoothOrientation/MassSmoothOrientationFragments.h"
#include "Steering/MassSteeringFragments.h"
#include "Steps/ArcTacticsMemorySteps.h"
#include "TargetQuery/Functions/ArcTQSFunctions_Gameplay.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcTacticsMemoryTest_Location, "Test.ArcTactics.Memory.Location");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcTacticsMemoryTest_Entity, "Test.ArcTactics.Memory.Entity");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcTacticsMemoryTest_Float, "Test.ArcTactics.Memory.Float");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcTacticsMemoryTest_Bool, "Test.ArcTactics.Memory.Bool");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcTacticsMemoryTest_Tag, "Test.ArcTactics.Memory.Tag");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcTacticsMemoryTest_Absent, "Test.ArcTactics.Memory.Absent");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcTacticsMemoryTest_Mismatch, "Test.ArcTactics.Memory.Mismatch");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcTacticsMemoryTest_Replace, "Test.ArcTactics.Memory.Replace");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcTacticsMemoryTest_Source, "Test.ArcTactics.Memory.Step.Source");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcTacticsMemoryTest_Copy, "Test.ArcTactics.Memory.Step.Copy");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcTacticsMemoryTest_Replaced, "Test.ArcTactics.Memory.Step.Replaced");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcTacticsMemoryTest_Forgotten, "Test.ArcTactics.Memory.Step.Forgotten");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcTacticsMemoryTest_Patrol, "Test.ArcTactics.Memory.Patrol");

namespace ArcTacticsMemoryTestsPrivate
{
	const FGuid ActionID(80, 1, 0, 0);
	const FGuid PatrolTaskID(80, 2, 0, 0);

	FPropertyBindingPath MakePath(const FGuid ID, const TCHAR* Path)
	{
		FPropertyBindingPath Result(ID);
		Result.FromString(Path);
		return Result;
	}

	bool Compile(UArcTacticsConfig& Config)
	{
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		const bool bSucceeded = FArcTacticsCompiler::Compile(Config, 9, Config.CompiledData, Diagnostics);
		for (const FArcTacticsCompilerDiagnostic& Diagnostic : Diagnostics)
		{
			UE_LOGF(LogTemp, Display, "ArcTacticsMemoryTests compile diagnostic: NodeID=%ls Message=%ls",
				*Diagnostic.NodeID.ToString(), *Diagnostic.Message.ToString());
		}
		return bSucceeded && Config.CompiledData.IsExecutable();
	}

	UArcTacticsConfig* NewConfig()
	{
		UArcTacticsConfig* Config = NewObject<UArcTacticsConfig>(
			GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UArcTacticsConfig::StaticClass(), TEXT("MemoryConfig")));
		Config->ContextID = FGuid(80, 0, 0, 1);
		return Config;
	}

	template <typename TStep, typename TData>
	FArcTacticsNodeDefinition MakeStep(const FName Name, const TData& Data)
	{
		FArcTacticsNodeDefinition Step;
		Step.Name = Name;
		Step.Node.InitializeAs<TStep>();
		Step.InstanceData.InitializeAs<TData>(Data);
		return Step;
	}

	UArcTacticsConfig* MakeRoundTripConfig()
	{
		UArcTacticsConfig* Config = NewConfig();
		FArcTacticsActionDefinition Action;
		Action.Action.ID = ActionID;
		Action.Action.Name = TEXT("Remember");
		Action.Action.Node.InitializeAs<FArcTacticsSequenceAction>();
		Action.Action.InstanceData.InitializeAs<FArcTacticsSequenceActionInstanceData>();

		FArcTacticsWriteMemoryFloatStepInstanceData WriteSource;
		WriteSource.Key = TAG_ArcTacticsMemoryTest_Source;
		WriteSource.Value = 7.25f;
		Action.Steps.Add(MakeStep<FArcTacticsWriteMemoryFloatStep>(TEXT("WriteSource"), WriteSource));

		FArcTacticsWriteMemoryFloatStepInstanceData WriteCopy;
		WriteCopy.Key = TAG_ArcTacticsMemoryTest_Copy;
		Action.Steps.Add(MakeStep<FArcTacticsWriteMemoryFloatStep>(TEXT("WriteCopy"), WriteCopy));

		FArcTacticsWriteMemoryBoolStepInstanceData WriteBool;
		WriteBool.Key = TAG_ArcTacticsMemoryTest_Replaced;
		WriteBool.bValue = true;
		Action.Steps.Add(MakeStep<FArcTacticsWriteMemoryBoolStep>(TEXT("WriteBool"), WriteBool));

		FArcTacticsForgetMemoryStepInstanceData Forget;
		Forget.Key = TAG_ArcTacticsMemoryTest_Forgotten;
		Action.Steps.Add(MakeStep<FArcTacticsForgetMemoryStep>(TEXT("Forget"), Forget));
		Config->Actions.Add(Action);
		Config->EnsureValidNodeIDs();

		FInstancedStruct Read;
		FArcTQSFunction_MemoryFloat ReadData;
		ReadData.Key = TAG_ArcTacticsMemoryTest_Source;
		ReadData.Fallback = -1.0f;
		Read.InitializeAs<FArcTQSFunction_MemoryFloat>(ReadData);
		const FGuid ReadID = FGuid::NewGuid();
		Read.GetMutable<FArcTQSPropertyFunction>().ID = ReadID;
		Config->EditorBindings.AddCompiledBinding(FArcTacticsBinding(
			FConstStructView(Read),
			FPropertyBindingPath(ReadID, GET_MEMBER_NAME_CHECKED(FArcTQSFunction_MemoryFloat, Result)),
			MakePath(Config->Actions[0].Steps[1].ID, TEXT("Value"))));
		return Compile(*Config) ? Config : nullptr;
	}

	UArcTacticsConfig* MakeForgetOnlyConfig()
	{
		UArcTacticsConfig* Config = NewConfig();
		FArcTacticsActionDefinition Action;
		Action.Action.ID = ActionID;
		Action.Action.Name = TEXT("ForgetOnly");
		Action.Action.Node.InitializeAs<FArcTacticsSequenceAction>();
		Action.Action.InstanceData.InitializeAs<FArcTacticsSequenceActionInstanceData>();
		FArcTacticsForgetMemoryStepInstanceData Forget;
		Forget.Key = TAG_ArcTacticsMemoryTest_Forgotten;
		Action.Steps.Add(MakeStep<FArcTacticsForgetMemoryStep>(TEXT("Forget"), Forget));
		Config->Actions.Add(Action);
		Config->EnsureValidNodeIDs();
		return Compile(*Config) ? Config : nullptr;
	}

	UArcTQSQueryDefinition* MakeCircleQuery()
	{
		UArcTQSQueryDefinition* Definition = NewObject<UArcTQSQueryDefinition>(GetTransientPackage());
		FArcTQSGenerator_Circle Generator;
		Generator.NumRings = 1;
		Generator.DegreesPerPoint = 360.0f;
		Generator.DistanceBetweenRings = 10.0f;
		Definition->Generator.InitializeAs<FArcTQSGenerator_Circle>(Generator);
		Definition->RepairNodeIDs();
		Definition->EditorBindings.AddBinding(
			FPropertyBindingPath(Definition->QuerierID, TEXT("Location")),
			FPropertyBindingPath(Definition->Generator.Get<FArcTQSGenerator>().ID, TEXT("Center")));
		TArray<FArcTQSCompilerDiagnostic> Diagnostics;
		Definition->CompileEditorData(Diagnostics);
		return Definition->IsExecutable() ? Definition : nullptr;
	}

	UArcTacticsConfig* BuildPatrolConfig(UArcTQSQueryDefinition& Query, const FGameplayTag MemoryKey)
	{
		UArcTacticsConfig* Config = NewConfig();
		FArcTacticsNodeDefinition Task;
		Task.ID = PatrolTaskID;
		Task.Name = TEXT("Patrol");
		Task.Node.InitializeAs<FArcTacticsPatrolPointsTask>();
		FArcTacticsPatrolPointsTaskInstanceData Data;
		Data.TargetQuery.SetDefinition(&Query);
		Data.MemoryKey = MemoryKey;
		Task.InstanceData.InitializeAs<FArcTacticsPatrolPointsTaskInstanceData>(Data);
		Config->GlobalTasks.Add(Task);
		Config->EnsureValidNodeIDs();
		return Config;
	}

	UArcTacticsConfig* MakePatrolConfig(UArcTQSQueryDefinition& Query)
	{
		UArcTacticsConfig* Config = BuildPatrolConfig(Query, TAG_ArcTacticsMemoryTest_Patrol);
		return Compile(*Config) ? Config : nullptr;
	}

	bool CompileWithDiagnostics(UArcTacticsConfig& Config, TArray<FArcTacticsCompilerDiagnostic>& Diagnostics)
	{
		Diagnostics.Reset();
		const bool bSucceeded = FArcTacticsCompiler::Compile(Config, 9, Config.CompiledData, Diagnostics);
		for (const FArcTacticsCompilerDiagnostic& Diagnostic : Diagnostics)
		{
			UE_LOGF(LogTemp, Display, "ArcTacticsMemoryTests compile diagnostic: NodeID=%ls Message=%ls",
				*Diagnostic.NodeID.ToString(), *Diagnostic.Message.ToString());
		}
		return bSucceeded && Config.CompiledData.IsExecutable();
	}

	bool HasErrorOnNode(const TArray<FArcTacticsCompilerDiagnostic>& Diagnostics, const FGuid NodeID, const TCHAR* Message)
	{
		return Diagnostics.ContainsByPredicate([NodeID, Message](const FArcTacticsCompilerDiagnostic& Diagnostic)
		{
			return Diagnostic.bIsError && Diagnostic.NodeID == NodeID && Diagnostic.Message.ToString() == Message;
		});
	}

	int32 CountErrors(const TArray<FArcTacticsCompilerDiagnostic>& Diagnostics)
	{
		int32 Count = 0;
		for (const FArcTacticsCompilerDiagnostic& Diagnostic : Diagnostics)
		{
			Count += Diagnostic.bIsError ? 1 : 0;
		}
		return Count;
	}

	template <typename TStep, typename TData>
	UArcTacticsConfig* BuildSingleStepConfig(const FName StepName, const TData& Data)
	{
		UArcTacticsConfig* Config = NewConfig();
		FArcTacticsActionDefinition Action;
		Action.Action.ID = ActionID;
		Action.Action.Name = TEXT("KeyCheck");
		Action.Action.Node.InitializeAs<FArcTacticsSequenceAction>();
		Action.Action.InstanceData.InitializeAs<FArcTacticsSequenceActionInstanceData>();
		Action.Steps.Add(MakeStep<TStep>(StepName, Data));
		Config->Actions.Add(Action);
		Config->EnsureValidNodeIDs();
		return Config;
	}

	UArcTacticsConfig* BuildWriteStepConfig(const FGameplayTag Key)
	{
		FArcTacticsWriteMemoryFloatStepInstanceData Write;
		Write.Key = Key;
		return BuildSingleStepConfig<FArcTacticsWriteMemoryFloatStep>(TEXT("Write"), Write);
	}

	UArcTacticsConfig* BuildForgetStepConfig(const FGameplayTag Key)
	{
		FArcTacticsForgetMemoryStepInstanceData Forget;
		Forget.Key = Key;
		return BuildSingleStepConfig<FArcTacticsForgetMemoryStep>(TEXT("Forget"), Forget);
	}

	UArcTacticsConfig* BuildFunctionKeyConfig(const FGameplayTag FunctionKey, FGuid& OutFunctionID)
	{
		UArcTacticsConfig* Config = BuildWriteStepConfig(TAG_ArcTacticsMemoryTest_Copy);
		FInstancedStruct Read;
		FArcTQSFunction_MemoryFloat ReadData;
		ReadData.Key = FunctionKey;
		Read.InitializeAs<FArcTQSFunction_MemoryFloat>(ReadData);
		OutFunctionID = FGuid::NewGuid();
		Read.GetMutable<FArcTQSPropertyFunction>().ID = OutFunctionID;
		Config->EditorBindings.AddCompiledBinding(FArcTacticsBinding(
			FConstStructView(Read),
			FPropertyBindingPath(OutFunctionID, GET_MEMBER_NAME_CHECKED(FArcTQSFunction_MemoryFloat, Result)),
			MakePath(Config->Actions[0].Steps[0].ID, TEXT("Value"))));
		return Config;
	}

	FArcTacticsDecisionSettings ZeroIntervalSettings()
	{
		FArcTacticsDecisionSettings Settings;
		Settings.DecisionInterval = 0.0f;
		return Settings;
	}

	FMassEntityHandle CreateEntity(FMassEntityManager& EntityManager, const UArcTacticsConfig& Config)
	{
		UE::Mass::FEntityBuilder Builder = EntityManager.MakeEntityBuilder();
		Builder.Add<FArcTacticsInstanceFragment>();
		Builder.Add<FArcTacticsTag>();
		FArcTacticsConfigFragment& ConfigFragment = Builder.Add_GetRef<FArcTacticsConfigFragment>();
		ConfigFragment.Configs.Add(&Config);
		ConfigFragment.Settings = ZeroIntervalSettings();
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
		return Builder.Commit();
	}

	FArcMassMemoryFragment* AddMemory(FMassEntityManager& EntityManager, const FMassEntityHandle Entity)
	{
		return EntityManager.AddSparseElementToEntity(Entity, FArcMassMemoryFragment::StaticStruct()).GetPtr<FArcMassMemoryFragment>();
	}

	FArcMassMemoryFragment* FindMemory(FMassEntityManager& EntityManager, const FMassEntityHandle Entity)
	{
		return EntityManager.GetMutableSparseElementDataForEntity(FArcMassMemoryFragment::StaticStruct(), Entity).GetPtr<FArcMassMemoryFragment>();
	}

	void RunLifecycle(UWorld& World, FMassEntityManager& EntityManager)
	{
		UArcTacticsLifecycleProcessor* Processor = NewObject<UArcTacticsLifecycleProcessor>(&World);
		Processor->CallInitialize(EntityManager.GetOwner(), EntityManager.AsShared());
		FMassProcessingContext ProcessingContext(EntityManager, 0.0f);
		UE::Mass::Executor::Run(*Processor, ProcessingContext);
	}

	FArcTacticsInstanceHandle GetHandle(FMassEntityManager& EntityManager, const FMassEntityHandle Entity)
	{
		const FArcTacticsInstanceFragment* Fragment = EntityManager.GetFragmentDataPtr<FArcTacticsInstanceFragment>(Entity);
		return Fragment ? Fragment->Handle : FArcTacticsInstanceHandle();
	}

	const FArcTacticsPatrolPointsTaskInstanceData* FindPatrolData(FArcTacticsInstanceSlot& Slot)
	{
		if (Slot.Packages.IsEmpty() || !Slot.Packages[0]->Definitions.IsValid())
		{
			return nullptr;
		}
		const FArcTacticsCompiledData& CompiledData = *Slot.Packages[0]->Definitions;
		if (!CompiledData.GlobalTasks.IsValidIndex(0))
		{
			return nullptr;
		}
		const int32 NodeIndex = CompiledData.GlobalTasks[0].Get();
		const int32 DataIndex = CompiledData.Nodes.IsValidIndex(NodeIndex) ? CompiledData.Nodes[NodeIndex].InstanceData.GetIndex() : INDEX_NONE;
		const FInstancedStructContainer& NodeData = Slot.Packages[0]->Instance.NodeInstanceData;
		return NodeData.IsValidIndex(DataIndex) ? NodeData[DataIndex].GetPtr<const FArcTacticsPatrolPointsTaskInstanceData>() : nullptr;
	}

	bool StoresType(const FArcMassMemoryFragment& Fragment, const FGameplayTag Key, const UScriptStruct* Type)
	{
		const FInstancedStruct* Stored = Fragment.Values.Find(Key);
		return Stored && Stored->GetScriptStruct() == Type;
	}
}

TEST_CLASS(ArcTacticsMemoryApiTests, "ArcTactics.Memory.Api")
{
	FActorTestSpawner Spawner;

	TEST_METHOD(Fragment_WritesAndReadsEveryValueTypeAndForgets)
	{
		using namespace ArcTacticsMemoryTestsPrivate;
		FArcMassMemoryFragment Fragment;
		const FMassEntityHandle Remembered(5, 6);
		ArcMassMemory::Write(Fragment, TAG_ArcTacticsMemoryTest_Location, FVector(1.0, 2.0, 3.0));
		ArcMassMemory::Write(Fragment, TAG_ArcTacticsMemoryTest_Entity, Remembered);
		ArcMassMemory::Write(Fragment, TAG_ArcTacticsMemoryTest_Float, 2.5f);
		ArcMassMemory::Write(Fragment, TAG_ArcTacticsMemoryTest_Bool, true);
		ArcMassMemory::Write(Fragment, TAG_ArcTacticsMemoryTest_Tag, TAG_ArcTacticsMemoryTest_Location.GetTag());
		ASSERT_THAT(AreEqual(5, Fragment.Values.Num()));
		ASSERT_THAT(IsTrue(StoresType(Fragment, TAG_ArcTacticsMemoryTest_Location, TBaseStructure<FVector>::Get())));
		ASSERT_THAT(IsTrue(StoresType(Fragment, TAG_ArcTacticsMemoryTest_Entity, FMassEntityHandle::StaticStruct())));
		ASSERT_THAT(IsTrue(StoresType(Fragment, TAG_ArcTacticsMemoryTest_Float, FArcMemoryFloat::StaticStruct())));
		ASSERT_THAT(IsTrue(StoresType(Fragment, TAG_ArcTacticsMemoryTest_Bool, FArcMemoryBool::StaticStruct())));
		ASSERT_THAT(IsTrue(StoresType(Fragment, TAG_ArcTacticsMemoryTest_Tag, FGameplayTag::StaticStruct())));

		FVector Location = FVector::ZeroVector;
		FMassEntityHandle Entity;
		float Float = 0.0f;
		bool bBool = false;
		FGameplayTag Tag;
		ASSERT_THAT(IsTrue(ArcMassMemory::Read(Fragment, TAG_ArcTacticsMemoryTest_Location, Location)));
		ASSERT_THAT(IsTrue(Location.Equals(FVector(1.0, 2.0, 3.0))));
		ASSERT_THAT(IsTrue(ArcMassMemory::Read(Fragment, TAG_ArcTacticsMemoryTest_Entity, Entity)));
		ASSERT_THAT(IsTrue(Entity == Remembered));
		ASSERT_THAT(IsTrue(ArcMassMemory::Read(Fragment, TAG_ArcTacticsMemoryTest_Float, Float)));
		ASSERT_THAT(IsNear(2.5f, Float, 0.0001f));
		ASSERT_THAT(IsTrue(ArcMassMemory::Read(Fragment, TAG_ArcTacticsMemoryTest_Bool, bBool)));
		ASSERT_THAT(IsTrue(bBool));
		ASSERT_THAT(IsTrue(ArcMassMemory::Read(Fragment, TAG_ArcTacticsMemoryTest_Tag, Tag)));
		ASSERT_THAT(IsTrue(Tag == TAG_ArcTacticsMemoryTest_Location.GetTag()));

		FVector Absent(4.0, 4.0, 4.0);
		ASSERT_THAT(IsFalse(ArcMassMemory::Read(Fragment, TAG_ArcTacticsMemoryTest_Absent, Absent)));
		ASSERT_THAT(IsTrue(Absent.Equals(FVector(4.0, 4.0, 4.0)), TEXT("An absent key leaves Out untouched")));

		ArcMassMemory::Forget(Fragment, TAG_ArcTacticsMemoryTest_Float);
		Float = -3.0f;
		ASSERT_THAT(IsFalse(ArcMassMemory::Read(Fragment, TAG_ArcTacticsMemoryTest_Float, Float)));
		ASSERT_THAT(IsNear(-3.0f, Float, 0.0001f));
		ASSERT_THAT(AreEqual(4, Fragment.Values.Num(), TEXT("Forget removes only its own key")));
	}

	TEST_METHOD(Fragment_ReadOfAnotherTypeReturnsFalseLeavesOutUntouchedAndWarnsOnce)
	{
		ArcMassMemory::ResetReportedMismatchesForTesting();
		FArcMassMemoryFragment Fragment;
		ArcMassMemory::Write(Fragment, TAG_ArcTacticsMemoryTest_Mismatch, 1.5f);
		TestRunner->AddExpectedMessagePlain(
			TEXT("NPC memory key Test.ArcTactics.Memory.Mismatch holds ArcMemoryFloat but was read as Vector"),
			ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		FVector Out(9.0, 9.0, 9.0);
		ASSERT_THAT(IsFalse(ArcMassMemory::Read(Fragment, TAG_ArcTacticsMemoryTest_Mismatch, Out)));
		ASSERT_THAT(IsTrue(Out.Equals(FVector(9.0, 9.0, 9.0)), TEXT("A type mismatch leaves Out untouched")));
		ASSERT_THAT(IsFalse(ArcMassMemory::Read(Fragment, TAG_ArcTacticsMemoryTest_Mismatch, Out),
			TEXT("A repeated mismatch still fails; its warning is not logged again")));
		float Float = 0.0f;
		ASSERT_THAT(IsTrue(ArcMassMemory::Read(Fragment, TAG_ArcTacticsMemoryTest_Mismatch, Float)));
		ASSERT_THAT(IsNear(1.5f, Float, 0.0001f));
	}

	TEST_METHOD(Fragment_WriteOfAnotherTypeReplacesTheValue)
	{
		FArcMassMemoryFragment Fragment;
		ArcMassMemory::Write(Fragment, TAG_ArcTacticsMemoryTest_Replace, 1.0f);
		ArcMassMemory::Write(Fragment, TAG_ArcTacticsMemoryTest_Replace, FVector(4.0, 5.0, 6.0));
		ASSERT_THAT(AreEqual(1, Fragment.Values.Num()));
		ASSERT_THAT(IsTrue(ArcTacticsMemoryTestsPrivate::StoresType(Fragment, TAG_ArcTacticsMemoryTest_Replace, TBaseStructure<FVector>::Get())));
		FVector Location = FVector::ZeroVector;
		ASSERT_THAT(IsTrue(ArcMassMemory::Read(Fragment, TAG_ArcTacticsMemoryTest_Replace, Location)));
		ASSERT_THAT(IsTrue(Location.Equals(FVector(4.0, 5.0, 6.0))));
	}

	TEST_METHOD(Entity_OverloadsFailWithoutTheMemoryFragment)
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* EntitySubsystem = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		UE::Mass::FEntityBuilder WithoutBuilder = EntityManager.MakeEntityBuilder();
		WithoutBuilder.Add<FTransformFragment>();
		const FMassEntityHandle Without = WithoutBuilder.Commit();
		UE::Mass::FEntityBuilder WithBuilder = EntityManager.MakeEntityBuilder();
		WithBuilder.Add<FTransformFragment>();
		const FMassEntityHandle With = WithBuilder.Commit();
		ASSERT_THAT(IsNotNull(ArcTacticsMemoryTestsPrivate::AddMemory(EntityManager, With)));

		ASSERT_THAT(IsFalse(ArcMassMemory::Write(EntityManager, Without, TAG_ArcTacticsMemoryTest_Float, 3.0f)));
		float Float = -2.0f;
		ASSERT_THAT(IsFalse(ArcMassMemory::Read(EntityManager, Without, TAG_ArcTacticsMemoryTest_Float, Float)));
		ASSERT_THAT(IsNear(-2.0f, Float, 0.0001f));
		ASSERT_THAT(IsFalse(ArcMassMemory::Forget(EntityManager, Without, TAG_ArcTacticsMemoryTest_Float)));

		ASSERT_THAT(IsTrue(ArcMassMemory::Write(EntityManager, With, TAG_ArcTacticsMemoryTest_Float, 3.0f)));
		ASSERT_THAT(IsTrue(ArcMassMemory::Read(EntityManager, With, TAG_ArcTacticsMemoryTest_Float, Float)));
		ASSERT_THAT(IsNear(3.0f, Float, 0.0001f));
		ASSERT_THAT(IsTrue(ArcMassMemory::Forget(EntityManager, With, TAG_ArcTacticsMemoryTest_Float)));
		ASSERT_THAT(IsFalse(ArcMassMemory::Read(EntityManager, With, TAG_ArcTacticsMemoryTest_Float, Float)));
	}
};

TEST_CLASS(ArcTacticsMemoryStepTests, "ArcTactics.Memory.Steps")
{
	FActorTestSpawner Spawner;

	TEST_METHOD(Sequence_WriteReadReplaceAndForgetRoundTripOnTheUtilityLane)
	{
		using namespace ArcTacticsMemoryTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		UArcTacticsConfig* Config = MakeRoundTripConfig();
		ASSERT_THAT(IsNotNull(Config, TEXT("A Float write and a Float read share one key, so the config compiles")));
		const FMassEntityHandle Entity = CreateEntity(EntityManager, *Config);
		FArcMassMemoryFragment* Memory = AddMemory(EntityManager, Entity);
		ASSERT_THAT(IsNotNull(Memory));
		ArcMassMemory::Write(*Memory, TAG_ArcTacticsMemoryTest_Replaced, FVector(1.0, 1.0, 1.0));
		ArcMassMemory::Write(*Memory, TAG_ArcTacticsMemoryTest_Forgotten, TAG_ArcTacticsMemoryTest_Source.GetTag());
		RunLifecycle(World, EntityManager);
		const FArcTacticsInstanceHandle Handle = GetHandle(EntityManager, Entity);
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		ASSERT_THAT(IsTrue(Slot->Packages.Num() == 1 && Slot->Packages[0]->bActive));

		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Handle, 0.0f, nullptr);
		Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsTrue(Slot && Slot->ActiveAction.IsSet(), TEXT("The Sequence enters its first memory step and runs")));
		Memory = FindMemory(EntityManager, Entity);
		ASSERT_THAT(IsNotNull(Memory));
		float Source = 0.0f;
		ASSERT_THAT(IsFalse(ArcMassMemory::Read(*Memory, TAG_ArcTacticsMemoryTest_Source, Source),
			TEXT("A memory step's Enter only initializes scratch; it writes in Tick")));

		UArcTacticsUtilityProcessor::TickAction(*Pool, EntityManager, Handle, EArcTacticsActionTickLane::Utility, true, nullptr);
		Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		ASSERT_THAT(IsFalse(Slot->ActiveAction.IsSet(), TEXT("Every utility-lane step finishes in one tick")));
		ASSERT_THAT(IsTrue(Slot->LastEndedAction.Status == EArcTacticsActionEndStatus::Succeeded));
		Memory = FindMemory(EntityManager, Entity);
		ASSERT_THAT(IsNotNull(Memory));
		ASSERT_THAT(IsTrue(ArcMassMemory::Read(*Memory, TAG_ArcTacticsMemoryTest_Source, Source)));
		ASSERT_THAT(IsNear(7.25f, Source, 0.0001f));
		float Copy = 0.0f;
		ASSERT_THAT(IsTrue(ArcMassMemory::Read(*Memory, TAG_ArcTacticsMemoryTest_Copy, Copy)));
		ASSERT_THAT(IsNear(7.25f, Copy, 0.0001f,
			TEXT("The second step's Memory Float input runs at step entry and reads what the first step wrote (Fallback is -1)")));
		ASSERT_THAT(IsTrue(StoresType(*Memory, TAG_ArcTacticsMemoryTest_Replaced, FArcMemoryBool::StaticStruct()),
			TEXT("Writing a Bool over a remembered Vector replaces it")));
		bool bReplaced = false;
		ASSERT_THAT(IsTrue(ArcMassMemory::Read(*Memory, TAG_ArcTacticsMemoryTest_Replaced, bReplaced) && bReplaced));
		ASSERT_THAT(IsFalse(Memory->Values.Contains(TAG_ArcTacticsMemoryTest_Forgotten.GetTag()), TEXT("Forget Memory removes its key")));

		FArcTacticsDebugSnapshot Snapshot;
		ASSERT_THAT(IsTrue(ArcTacticsDebug::BuildEntitySnapshot(World, Entity, Snapshot)));
		ASSERT_THAT(IsTrue(Snapshot.bHasMemory));
		const FArcTacticsDebugMemoryEntry* SourceEntry = Snapshot.Memory.FindByPredicate([](const FArcTacticsDebugMemoryEntry& Entry)
		{
			return Entry.Key == TAG_ArcTacticsMemoryTest_Source.GetTag();
		});
		ASSERT_THAT(IsNotNull(SourceEntry, TEXT("The debug snapshot lists the entity's memory")));
		if (SourceEntry)
		{
			ASSERT_THAT(AreEqual(FString(TEXT("ArcMemoryFloat")), SourceEntry->TypeName));
		}
	}

	TEST_METHOD(Sequence_WriteWithoutMemoryFragmentFailsWithMemoryFragmentMissing)
	{
		using namespace ArcTacticsMemoryTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		UArcTacticsConfig* Config = MakeRoundTripConfig();
		ASSERT_THAT(IsNotNull(Config));
		const FMassEntityHandle Entity = CreateEntity(EntityManager, *Config);
		RunLifecycle(World, EntityManager);
		const FArcTacticsInstanceHandle Handle = GetHandle(EntityManager, Entity);
		ASSERT_THAT(IsNotNull(Pool->Resolve(Handle)));

		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Handle, 0.0f, nullptr);
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsTrue(Slot && Slot->ActiveAction.IsSet()));
		UArcTacticsUtilityProcessor::TickAction(*Pool, EntityManager, Handle, EArcTacticsActionTickLane::Utility, true, nullptr);
		Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsTrue(Slot && !Slot->ActiveAction.IsSet()));

		FArcTacticsDebugSnapshot Snapshot;
		ASSERT_THAT(IsTrue(ArcTacticsDebug::BuildEntitySnapshot(World, Entity, Snapshot)));
		const FArcTacticsDebugEndedAction& Ended = Snapshot.LastEndedAction;
		ASSERT_THAT(IsTrue(Ended.bSet));
		ASSERT_THAT(IsTrue(Ended.Status == EArcTacticsActionEndStatus::Failed));
		ASSERT_THAT(IsTrue(Ended.Reason == EArcTacticsActionEndReason::Completed));
		ASSERT_THAT(AreEqual(FName(TEXT("MemoryFragmentMissing")), Ended.FailureReason));
		ASSERT_THAT(IsFalse(Snapshot.bHasMemory));
		TArray<FString> Lines;
		ArcTacticsDebug::FormatSnapshot(Snapshot, Lines);
		ASSERT_THAT(IsTrue(Lines.ContainsByPredicate([](const FString& Line)
		{
			return Line.Contains(TEXT("failure=MemoryFragmentMissing"));
		}), TEXT("The dump prints the failure reason of the last ended action")));
	}

	TEST_METHOD(Sequence_ForgetWithoutMemoryFragmentFailsWithMemoryFragmentMissing)
	{
		using namespace ArcTacticsMemoryTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		UArcTacticsConfig* Config = MakeForgetOnlyConfig();
		ASSERT_THAT(IsNotNull(Config, TEXT("A Sequence whose only step is Forget Memory compiles")));
		const FMassEntityHandle Entity = CreateEntity(EntityManager, *Config);
		RunLifecycle(World, EntityManager);
		const FArcTacticsInstanceHandle Handle = GetHandle(EntityManager, Entity);
		ASSERT_THAT(IsNotNull(Pool->Resolve(Handle)));

		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Handle, 0.0f, nullptr);
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsTrue(Slot && Slot->ActiveAction.IsSet(), TEXT("The Sequence enters its Forget Memory step")));
		UArcTacticsUtilityProcessor::TickAction(*Pool, EntityManager, Handle, EArcTacticsActionTickLane::Utility, true, nullptr);
		Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsTrue(Slot && !Slot->ActiveAction.IsSet()));

		FArcTacticsDebugSnapshot Snapshot;
		ASSERT_THAT(IsTrue(ArcTacticsDebug::BuildEntitySnapshot(World, Entity, Snapshot)));
		const FArcTacticsDebugEndedAction& Ended = Snapshot.LastEndedAction;
		ASSERT_THAT(IsTrue(Ended.bSet));
		ASSERT_THAT(IsTrue(Ended.Status == EArcTacticsActionEndStatus::Failed));
		ASSERT_THAT(IsTrue(Ended.Reason == EArcTacticsActionEndReason::Completed));
		ASSERT_THAT(AreEqual(FName(TEXT("MemoryFragmentMissing")), Ended.FailureReason,
			TEXT("Forget Memory on an entity without the memory fragment names the missing fragment")));
		ASSERT_THAT(IsFalse(Snapshot.bHasMemory));
	}
};

TEST_CLASS(ArcTacticsPatrolMemoryTests, "ArcTactics.Memory.Patrol")
{
	FActorTestSpawner Spawner;

	TEST_METHOD(PatrolTask_HoldsAndForgetsThePointUnderItsMemoryKey)
	{
		using namespace ArcTacticsMemoryTestsPrivate;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UWorld& World = Spawner.GetWorld();
		UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
		UArcTacticsInstanceSubsystem* Pool = World.GetSubsystem<UArcTacticsInstanceSubsystem>();
		ASSERT_THAT(IsNotNull(EntitySubsystem));
		ASSERT_THAT(IsNotNull(Pool));
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		UArcTQSQueryDefinition* Query = MakeCircleQuery();
		ASSERT_THAT(IsNotNull(Query));
		UArcTacticsConfig* Config = MakePatrolConfig(*Query);
		ASSERT_THAT(IsNotNull(Config));
		const FMassEntityHandle Entity = CreateEntity(EntityManager, *Config);
		FArcMassMemoryFragment* Memory = AddMemory(EntityManager, Entity);
		ASSERT_THAT(IsNotNull(Memory));
		ArcMassMemory::Write(*Memory, TAG_ArcTacticsMemoryTest_Patrol, FVector(1000.0, 0.0, 0.0));
		ArcMassMemory::Write(*Memory, TAG_AI_Memory_PatrolPoint, FVector(2000.0, 0.0, 0.0));
		RunLifecycle(World, EntityManager);
		const FArcTacticsInstanceHandle Handle = GetHandle(EntityManager, Entity);
		ASSERT_THAT(IsNotNull(Pool->Resolve(Handle)));

		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Handle, 0.0f, nullptr);
		FArcTacticsInstanceSlot* Slot = Pool->Resolve(Handle);
		ASSERT_THAT(IsNotNull(Slot));
		const FArcTacticsPatrolPointsTaskInstanceData* Data = Slot ? FindPatrolData(*Slot) : nullptr;
		ASSERT_THAT(IsNotNull(Data));
		if (!Data)
		{
			return;
		}
		ASSERT_THAT(AreEqual(1, Data->Points.Num()));
		if (Data->Points.Num() == 1)
		{
			ASSERT_THAT(IsTrue(Data->Points[0].Equals(FVector(1000.0, 0.0, 0.0)),
				*FString::Printf(TEXT("The held point comes from MemoryKey, not AI.Memory.PatrolPoint; got %s"), *Data->Points[0].ToString())));
		}

		Memory = FindMemory(EntityManager, Entity);
		ASSERT_THAT(IsNotNull(Memory));
		ArcMassMemory::Write(*Memory, TAG_ArcTacticsMemoryTest_Patrol, FVector(100.0, 0.0, 0.0));
		Pool->RequestDecision(Handle);
		UArcTacticsUtilityProcessor::RunDecision(*Pool, EntityManager, Handle, 0.0f, nullptr);
		Memory = FindMemory(EntityManager, Entity);
		ASSERT_THAT(IsNotNull(Memory));
		ASSERT_THAT(IsFalse(Memory->Values.Contains(TAG_ArcTacticsMemoryTest_Patrol.GetTag()),
			TEXT("Arriving within ArrivalRange forgets the point under MemoryKey")));
		FVector Default = FVector::ZeroVector;
		ASSERT_THAT(IsTrue(ArcMassMemory::Read(*Memory, TAG_AI_Memory_PatrolPoint, Default) && Default.Equals(FVector(2000.0, 0.0, 0.0)),
			TEXT("The default patrol key is untouched by a task configured with another key")));

		UArcTQSQuerySubsystem* QuerySubsystem = World.GetSubsystem<UArcTQSQuerySubsystem>();
		ASSERT_THAT(IsNotNull(QuerySubsystem));
		Slot = Pool->Resolve(Handle);
		Data = Slot ? FindPatrolData(*Slot) : nullptr;
		ASSERT_THAT(IsTrue(Data && !Data->Query.bCompleted && Data->Query.QueryID != INDEX_NONE,
			TEXT("Forgetting the arrived point submits a new patrol query")));
		for (int32 TickIndex = 0; TickIndex < 16 && Data && !Data->Query.bCompleted; ++TickIndex)
		{
			QuerySubsystem->Tick(0.0f);
			Slot = Pool->Resolve(Handle);
			Data = Slot ? FindPatrolData(*Slot) : nullptr;
		}
		ASSERT_THAT(IsTrue(Data && Data->Query.bCompleted && Data->Query.bSucceeded,
			TEXT("The TQS subsystem completes the patrol query through the task's async completion")));
		if (!Data)
		{
			return;
		}
		ASSERT_THAT(AreEqual(1, Data->Points.Num()));
		if (Data->Points.Num() != 1)
		{
			return;
		}
		const FVector QueriedPoint = Data->Points[0];
		Memory = FindMemory(EntityManager, Entity);
		ASSERT_THAT(IsNotNull(Memory));
		FVector Held = FVector(-1.0, -1.0, -1.0);
		ASSERT_THAT(IsTrue(ArcMassMemory::Read(*Memory, TAG_ArcTacticsMemoryTest_Patrol, Held),
			TEXT("A completed patrol query holds its result under MemoryKey")));
		ASSERT_THAT(IsTrue(Held.Equals(QueriedPoint),
			*FString::Printf(TEXT("The held point is the query result %s; got %s"), *QueriedPoint.ToString(), *Held.ToString())));
		Default = FVector::ZeroVector;
		ASSERT_THAT(IsTrue(ArcMassMemory::Read(*Memory, TAG_AI_Memory_PatrolPoint, Default) && Default.Equals(FVector(2000.0, 0.0, 0.0)),
			TEXT("A completed patrol query does not write the default AI.Memory.PatrolPoint key")));
	}
};

TEST_CLASS(ArcTacticsMemoryKeyTests, "ArcTactics.Memory.Keys")
{
	TEST_METHOD(WriteMemoryStep_EmptyKeyIsACompileErrorNamingFieldAndStep)
	{
		using namespace ArcTacticsMemoryTestsPrivate;
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(CompileWithDiagnostics(*BuildWriteStepConfig(TAG_ArcTacticsMemoryTest_Float), Diagnostics),
			TEXT("Control: a Write Memory step with its key set compiles")));

		UArcTacticsConfig* EmptyKey = BuildWriteStepConfig(FGameplayTag());
		ASSERT_THAT(IsFalse(CompileWithDiagnostics(*EmptyKey, Diagnostics), TEXT("A Write Memory step with an empty key must not compile")));
		ASSERT_THAT(IsTrue(HasErrorOnNode(Diagnostics, EmptyKey->Actions[0].Steps[0].ID, TEXT("Memory key 'Key' on 'Write' is not set.")),
			TEXT("The error names the Key field and the step, and points at the step")));
		ASSERT_THAT(AreEqual(1, CountErrors(Diagnostics), TEXT("The empty key is the only reason the config fails")));
	}

	TEST_METHOD(ForgetMemoryStep_EmptyKeyIsACompileErrorNamingFieldAndStep)
	{
		using namespace ArcTacticsMemoryTestsPrivate;
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(CompileWithDiagnostics(*BuildForgetStepConfig(TAG_ArcTacticsMemoryTest_Forgotten), Diagnostics),
			TEXT("Control: a Forget Memory step with its key set compiles")));

		UArcTacticsConfig* EmptyKey = BuildForgetStepConfig(FGameplayTag());
		ASSERT_THAT(IsFalse(CompileWithDiagnostics(*EmptyKey, Diagnostics), TEXT("A Forget Memory step with an empty key must not compile")));
		ASSERT_THAT(IsTrue(HasErrorOnNode(Diagnostics, EmptyKey->Actions[0].Steps[0].ID, TEXT("Memory key 'Key' on 'Forget' is not set.")),
			TEXT("The error names the Key field and the step, and points at the step")));
		ASSERT_THAT(AreEqual(1, CountErrors(Diagnostics), TEXT("The empty key is the only reason the config fails")));
	}

	TEST_METHOD(MemoryFunctionInABinding_EmptyKeyIsACompileErrorNamingFieldAndFunction)
	{
		using namespace ArcTacticsMemoryTestsPrivate;
		FGuid SetKeyFunctionID;
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(CompileWithDiagnostics(*BuildFunctionKeyConfig(TAG_ArcTacticsMemoryTest_Source, SetKeyFunctionID), Diagnostics),
			TEXT("Control: a Memory Float function with its key set compiles")));

		FGuid EmptyKeyFunctionID;
		UArcTacticsConfig* EmptyKey = BuildFunctionKeyConfig(FGameplayTag(), EmptyKeyFunctionID);
		ASSERT_THAT(IsFalse(CompileWithDiagnostics(*EmptyKey, Diagnostics), TEXT("A Memory Float function with an empty key must not compile")));
		ASSERT_THAT(IsTrue(HasErrorOnNode(Diagnostics, EmptyKeyFunctionID, TEXT("Memory key 'Key' on 'Memory Float' is not set.")),
			TEXT("The error names the Key field and the function, and points at the function node")));
		ASSERT_THAT(AreEqual(1, CountErrors(Diagnostics), TEXT("The empty key is the only reason the config fails")));
	}

	TEST_METHOD(PatrolPointsTask_ClearedMemoryKeyIsACompileErrorNamingFieldAndTask)
	{
		using namespace ArcTacticsMemoryTestsPrivate;
		UArcTQSQueryDefinition* Query = MakeCircleQuery();
		ASSERT_THAT(IsNotNull(Query));
		if (!Query)
		{
			return;
		}
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(CompileWithDiagnostics(*BuildPatrolConfig(*Query, TAG_ArcTacticsMemoryTest_Patrol), Diagnostics),
			TEXT("Control: Generate Patrol Points with its MemoryKey set compiles")));

		UArcTacticsConfig* EmptyKey = BuildPatrolConfig(*Query, FGameplayTag());
		ASSERT_THAT(IsFalse(CompileWithDiagnostics(*EmptyKey, Diagnostics), TEXT("Generate Patrol Points with a cleared MemoryKey must not compile")));
		ASSERT_THAT(IsTrue(HasErrorOnNode(Diagnostics, PatrolTaskID, TEXT("Memory key 'MemoryKey' on 'Patrol' is not set.")),
			TEXT("The error names the MemoryKey field and the task, and points at the task")));
		ASSERT_THAT(AreEqual(1, CountErrors(Diagnostics), TEXT("The empty key is the only reason the config fails")));
	}
};

#endif
