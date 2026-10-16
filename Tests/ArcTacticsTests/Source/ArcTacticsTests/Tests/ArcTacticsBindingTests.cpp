#include "CQTest.h"

#include "Decision/ArcTacticsCompiler.h"
#include "Decision/ArcTacticsConfig.h"
#include "Decision/ArcTacticsInstance.h"
#include "Fixtures/ArcTacticsSlotTestHelpers.h"
#include "Decision/ArcTacticsSelectors.h"
#include "Decision/ArcTacticsDecision.h"
#include "Mass/ArcTacticsOption.h"
#include "GameFramework/Actor.h"
#include "Curves/CurveFloat.h"
#include "UObject/UnrealType.h"
#include "Fixtures/ArcTacticsTestTypes.h"

namespace ArcTacticsBindingTestsPrivate
{
	UArcTacticsConfig* MakeConfig()
	{
		UArcTacticsConfig* Config = NewObject<UArcTacticsConfig>(GetTransientPackage());
		Config->ContextID = FGuid(1, 2, 3, 4);
		Config->EnsureValidNodeIDs();
		return Config;
	}

	bool CompileConfig(UArcTacticsConfig& Config, TArray<FArcTacticsCompilerDiagnostic>& Diagnostics)
	{
		const bool bSucceeded = FArcTacticsCompiler::Compile(Config, 17, Config.CompiledData, Diagnostics);
		if (!bSucceeded)
		{
			for (const FArcTacticsCompilerDiagnostic& Diagnostic : Diagnostics)
			{
				UE_LOGF(LogTemp, Warning, "ArcTacticsBindingTests compile diagnostic: NodeID=%ls Message=%ls",
					*Diagnostic.NodeID.ToString(), *Diagnostic.Message.ToString());
			}
		}
		return bSucceeded;
	}

	bool HasDiagnosticContaining(const TArray<FArcTacticsCompilerDiagnostic>& Diagnostics, const TCHAR* Needle)
	{
		for (const FArcTacticsCompilerDiagnostic& Diagnostic : Diagnostics)
		{
			if (Diagnostic.Message.ToString().Contains(Needle, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	FPropertyBindingPath MakePath(const FGuid SourceID, const TCHAR* Path)
	{
		FPropertyBindingPath Result(SourceID);
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

	void MarkOutputBinding(UArcTacticsConfig& Config, const FGuid TargetID, const TCHAR* TargetPath)
	{
		Config.EditorBindings.ForEachMutableBinding(
			[TargetID, TargetPath](FPropertyBindingBinding& Binding)
			{
				FArcTacticsBinding* TacticsBinding = static_cast<FArcTacticsBinding*>(&Binding);
				if (PathMatches(TacticsBinding->GetTargetPath(), TargetID, TargetPath))
				{
					TacticsBinding->SetIsOutputBinding(true);
				}
			});
	}

	float GetBagFloatAt(const FInstancedPropertyBag& Bag, const TCHAR* Name, const int32 Index)
	{
		const UPropertyBag* Struct = Bag.GetPropertyBagStruct();
		const FArrayProperty* ArrayProperty = Struct ? FindFProperty<FArrayProperty>(Struct, Name) : nullptr;
		if (!ArrayProperty)
		{
			return NAN;
		}
		FScriptArrayHelper Helper(ArrayProperty,
			ArrayProperty->ContainerPtrToValuePtr<void>(Bag.GetValue().GetMemory()));
		if (!Helper.IsValidIndex(Index))
		{
			return NAN;
		}
		const uint8* RawPtr = static_cast<const uint8*>(Helper.GetRawPtr(Index));
		return *reinterpret_cast<const float*>(RawPtr);
	}

	void AddBagOutputBinding(UArcTacticsConfig& Config, const FGuid BagID, const FGuid TargetID, const TCHAR* TargetPath)
	{
		Config.EditorBindings.AddBinding(FPropertyBindingPath(BagID, TEXT("Published")), FPropertyBindingPath(TargetID, TargetPath));
		MarkOutputBinding(Config, TargetID, TargetPath);
	}

	bool BeginSelectorDecision(UArcTacticsConfig& Config, FArcTacticsInstanceSlot& Instance, const int32 EntityIndex)
	{
		FArcTacticsTaskContext Context;
		return ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config.CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(EntityIndex, 1), EntityIndex)
			&& ArcTacticsDecision::BeginDecision(Instance, Context)
				== EArcTacticsDecisionStatus::Succeeded;
	}

	UArcTacticsConfig* MakeBoundActionConfig()
	{
		UArcTacticsConfig* Config = MakeConfig();
		FArcTacticsNodeDefinition Action;
		Action.ID = FGuid(5, 6, 7, 8);
		Action.Node.InitializeAs<FArcTacticsTestAction>();
		FArcTacticsTestActionInstanceData Defaults;
		Defaults.Literal = 91;
		Defaults.Scalar = 37;
		Action.InstanceData.InitializeAs<FArcTacticsTestActionInstanceData>(Defaults);
		FArcTacticsActionDefinition ActionDefinition;
		ActionDefinition.Action = Action;
		Config->Actions.Add(ActionDefinition);
		Config->EditorBindings.AddBinding(
			FPropertyBindingPath(Config->ContextID,
				GET_MEMBER_NAME_CHECKED(FArcTacticsContextData, Self)),
			FPropertyBindingPath(Action.ID,
				GET_MEMBER_NAME_CHECKED(FArcTacticsTestActionInstanceData, Entity)));
		Config->EnsureValidNodeIDs();
		return Config;
	}

	void AddPriorityAction(UArcTacticsConfig& Config, const FGuid ActionID, const float Weight, const int32 Priority)
	{
		FArcTacticsActionDefinition ActionDefinition;
		ActionDefinition.Action.ID = ActionID;
		ActionDefinition.Action.Node.InitializeAs<FArcTacticsTestAction>();
		ActionDefinition.Action.InstanceData.InitializeAs<FArcTacticsTestActionInstanceData>();
		ActionDefinition.Parameters.Weight = Weight;
		ActionDefinition.Parameters.Priority = Priority;
		Config.Actions.Add(ActionDefinition);
		Config.EditorBindings.AddBinding(
			FPropertyBindingPath(Config.ContextID, GET_MEMBER_NAME_CHECKED(FArcTacticsContextData, Self)),
			FPropertyBindingPath(ActionID, GET_MEMBER_NAME_CHECKED(FArcTacticsTestActionInstanceData, Entity)));
	}

	UArcTacticsConfig* MakePriorityConfig()
	{
		UArcTacticsConfig* Config = MakeConfig();
		AddPriorityAction(*Config, FGuid(90, 1, 0, 0), 0.9f, 0);
		AddPriorityAction(*Config, FGuid(90, 2, 0, 0), 0.5f, 1);
		AddPriorityAction(*Config, FGuid(90, 3, 0, 0), 0.3f, 1);
		Config->EnsureValidNodeIDs();
		return Config;
	}

	const FArcTacticsCandidate* FindActionCandidate(const FArcTacticsInstanceSlot& Instance, const int32 ActionIndex)
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

	UArcTacticsConfig* MakeTaskActionConfig(const uint8 TaskMode = 0)
	{
		UArcTacticsConfig* Config = MakeConfig();
		const FGuid TaskID(20, 21, 22, 23);
		const FGuid ActionID(24, 25, 26, 27);
		FArcTacticsNodeDefinition Task;
		Task.ID = TaskID;
		Task.Node.InitializeAs<FArcTacticsTestTask>();
		FArcTacticsTestTaskInstanceData TaskDefaults;
		TaskDefaults.Mode = TaskMode;
		TaskDefaults.Record.Key = 7;
		TaskDefaults.Record.Score = 0.75f;
		TaskDefaults.Entity = FMassEntityHandle(31, 2);
		TaskDefaults.Actor = NewObject<AActor>(GetTransientPackage());
		FArcTacticsTestRecord FirstRecord;
		FirstRecord.Key = 1;
		FirstRecord.Score = 0.2f;
		FirstRecord.Location = FVector(1.0f, 0.0f, 0.0f);
		TaskDefaults.Records.Add(FirstRecord);
		FArcTacticsTestRecord SecondRecord;
		SecondRecord.Key = 2;
		SecondRecord.Score = 0.8f;
		SecondRecord.Location = FVector(2.0f, 0.0f, 0.0f);
		TaskDefaults.Records.Add(SecondRecord);
		Task.InstanceData.InitializeAs<FArcTacticsTestTaskInstanceData>(TaskDefaults);
		Config->GlobalTasks.Add(Task);

		FArcTacticsNodeDefinition Action;
		Action.ID = ActionID;
		Action.Node.InitializeAs<FArcTacticsTestAction>();
		Action.InstanceData.InitializeAs<FArcTacticsTestActionInstanceData>();
		FArcTacticsActionDefinition ActionDefinition;
		ActionDefinition.Action = Action;
		Config->Actions.Add(ActionDefinition);
		Config->EditorBindings.AddBinding(FPropertyBindingPath(TaskID, TEXT("Scalar")), FPropertyBindingPath(ActionID, TEXT("Scalar")));
		Config->EditorBindings.AddBinding(FPropertyBindingPath(TaskID, TEXT("Record")), FPropertyBindingPath(ActionID, TEXT("Record")));
		Config->EditorBindings.AddBinding(FPropertyBindingPath(TaskID, TEXT("Entity")), FPropertyBindingPath(ActionID, TEXT("Entity")));
		Config->EditorBindings.AddBinding(FPropertyBindingPath(TaskID, TEXT("Actor")), FPropertyBindingPath(ActionID, TEXT("Actor")));
		Config->EditorBindings.AddBinding(FPropertyBindingPath(TaskID, TEXT("Records")), FPropertyBindingPath(ActionID, TEXT("Records")));
		Config->EditorBindings.AddBinding(MakePath(TaskID, TEXT("Record.Score")), FPropertyBindingPath(ActionID, TEXT("NestedValue")));
		Config->EnsureValidNodeIDs();
		return Config;
	}

	enum class ESelectorKind : uint8
	{
		Entity,
		Location,
		Target
	};

	const FGuid ItemTaskID(20, 21, 22, 23);
	const FGuid SelectorNodeID(30, 31, 32, 33);

	const TCHAR* GetItemsOutputName(const ESelectorKind Kind)
	{
		switch (Kind)
		{
		case ESelectorKind::Entity:
			return TEXT("Entities");
		case ESelectorKind::Target:
			return TEXT("Targets");
		default:
			return TEXT("Locations");
		}
	}

	void InitializeSelectorNode(FArcTacticsNodeDefinition& Definition, const ESelectorKind Kind)
	{
		switch (Kind)
		{
		case ESelectorKind::Entity:
			Definition.Node.InitializeAs<FArcTacticsEntitySelector>();
			Definition.InstanceData.InitializeAs<FArcTacticsEntitySelectorInstanceData>();
			break;
		case ESelectorKind::Target:
			Definition.Node.InitializeAs<FArcTacticsTargetSelector>();
			Definition.InstanceData.InitializeAs<FArcTacticsTargetSelectorInstanceData>();
			break;
		default:
			Definition.Node.InitializeAs<FArcTacticsLocationSelector>();
			Definition.InstanceData.InitializeAs<FArcTacticsLocationSelectorInstanceData>();
			break;
		}
	}

	void FillSelectorItems(FArcTacticsTestTaskInstanceData& Data)
	{
		Data.Entities = {FMassEntityHandle(101, 1), FMassEntityHandle(102, 1)};
		Data.Locations = {FVector(0.2, 1.0, 2.0), FVector(0.8, 3.0, 4.0)};
		Data.Targets.Reset();
		for (int32 ItemIndex = 0; ItemIndex < 2; ++ItemIndex)
		{
			FArcTacticsTarget Target;
			Target.Entity = Data.Entities[ItemIndex];
			Target.LastLocation = Data.Locations[ItemIndex];
			Data.Targets.Add(Target);
		}
	}

	void AddItemScoreBinding(UArcTacticsConfig& Config, const ESelectorKind Kind, const FGuid TargetID, const TCHAR* TargetPath)
	{
		switch (Kind)
		{
		case ESelectorKind::Entity:
			Config.EditorBindings.AddBinding(MakePath(ItemTaskID, TEXT("Record.Score")), MakePath(TargetID, TargetPath));
			break;
		case ESelectorKind::Target:
			Config.EditorBindings.AddBinding(MakePath(SelectorNodeID, TEXT("CurrentTarget.LastLocation.X")), MakePath(TargetID, TargetPath));
			break;
		default:
			Config.EditorBindings.AddBinding(MakePath(SelectorNodeID, TEXT("CurrentLocation.X")), MakePath(TargetID, TargetPath));
			break;
		}
	}

	UArcTacticsConfig* MakeSelectorConfig(const bool bEmpty, const bool bVeto = false, const float MinScore = 0.0f,
		const bool bKeepStandalone = false, const ESelectorKind Kind = ESelectorKind::Location)
	{
		UArcTacticsConfig* Config = MakeTaskActionConfig();
		Config->DecisionSettings.MinScore = MinScore;
		Config->EditorBindings.Reset();
		if (!bKeepStandalone)
		{
			Config->Actions.Reset();
		}
		if (bKeepStandalone)
		{
			Config->EditorBindings.AddBinding(FPropertyBindingPath(ItemTaskID, TEXT("Scalar")), FPropertyBindingPath(FGuid(24, 25, 26, 27), TEXT("Scalar")));
			Config->EditorBindings.AddBinding(FPropertyBindingPath(ItemTaskID, TEXT("Record")), FPropertyBindingPath(FGuid(24, 25, 26, 27), TEXT("Record")));
			Config->EditorBindings.AddBinding(FPropertyBindingPath(ItemTaskID, TEXT("Entity")), FPropertyBindingPath(FGuid(24, 25, 26, 27), TEXT("Entity")));
			Config->EditorBindings.AddBinding(FPropertyBindingPath(ItemTaskID, TEXT("Actor")), FPropertyBindingPath(FGuid(24, 25, 26, 27), TEXT("Actor")));
			Config->EditorBindings.AddBinding(FPropertyBindingPath(ItemTaskID, TEXT("Records")), FPropertyBindingPath(FGuid(24, 25, 26, 27), TEXT("Records")));
		}
		const FGuid ActionID(38, 39, 40, 41);
		const FGuid ConsiderationID(42, 43, 44, 45);
		FArcTacticsTestTaskInstanceData& TaskDefaults =
			Config->GlobalTasks[0].InstanceData.GetMutable<FArcTacticsTestTaskInstanceData>();
		FillSelectorItems(TaskDefaults);
		FArcTacticsSelectorDefinition Selector;
		Selector.Selector.ID = SelectorNodeID;
		Selector.Selector.Name = TEXT("Items");
		InitializeSelectorNode(Selector.Selector, Kind);
		Selector.Action.ID = ActionID;
		Selector.Action.Node.InitializeAs<FArcTacticsTestAction>();
		Selector.Action.InstanceData.InitializeAs<FArcTacticsTestActionInstanceData>();
		FArcTacticsNodeDefinition Consideration;
		Consideration.ID = ConsiderationID;
		Consideration.Node.InitializeAs<FArcTacticsTestConsideration>();
		FArcTacticsTestConsiderationInstanceData ConsiderationDefaults;
		ConsiderationDefaults.bVeto = bVeto;
		Consideration.InstanceData.InitializeAs<FArcTacticsTestConsiderationInstanceData>(ConsiderationDefaults);
		Selector.Considerations.Add(Consideration);
		Config->Selectors.Add(Selector);
		Config->EditorBindings.AddBinding(
			FPropertyBindingPath(ItemTaskID, GetItemsOutputName(Kind)),
			FPropertyBindingPath(SelectorNodeID, TEXT("Items")));
		switch (Kind)
		{
		case ESelectorKind::Entity:
			Config->EditorBindings.AddBinding(
				FPropertyBindingPath(SelectorNodeID, TEXT("CurrentEntity")),
				FPropertyBindingPath(ActionID, TEXT("Entity")));
			break;
		case ESelectorKind::Target:
			Config->EditorBindings.AddBinding(
				MakePath(SelectorNodeID, TEXT("CurrentTarget.Entity")),
				FPropertyBindingPath(ActionID, TEXT("Entity")));
			Config->EditorBindings.AddBinding(
				FPropertyBindingPath(SelectorNodeID, TEXT("CurrentTarget")),
				FPropertyBindingPath(ActionID, TEXT("Target")));
			break;
		default:
			Config->EditorBindings.AddBinding(
				FPropertyBindingPath(ItemTaskID, TEXT("Entity")),
				FPropertyBindingPath(ActionID, TEXT("Entity")));
			Config->EditorBindings.AddBinding(
				FPropertyBindingPath(SelectorNodeID, TEXT("CurrentLocation")),
				FPropertyBindingPath(ActionID, TEXT("Location")));
			break;
		}
		AddItemScoreBinding(*Config, Kind, ConsiderationID, TEXT("BoundScore"));
		if (bEmpty)
		{
			FArcTacticsTestTaskInstanceData& Defaults =
				Config->GlobalTasks[0].InstanceData.GetMutable<FArcTacticsTestTaskInstanceData>();
			Defaults.Entities.Reset();
			Defaults.Locations.Reset();
			Defaults.Targets.Reset();
		}
		Config->EnsureValidNodeIDs();
		return Config;
	}

	int32 GetSelectorDataIndex(const FArcTacticsCompiledData& CompiledData)
	{
		if (!CompiledData.Selectors.IsValidIndex(0))
		{
			return INDEX_NONE;
		}
		const int32 NodeIndex = CompiledData.Selectors[0].Selector.Get();
		return CompiledData.Nodes.IsValidIndex(NodeIndex) ? CompiledData.Nodes[NodeIndex].InstanceData.GetIndex() : INDEX_NONE;
	}

	template <typename InstanceDataType>
	const InstanceDataType* GetCapturedSelectorData(const FArcTacticsCompiledData& CompiledData, const FArcTacticsOption& Option)
	{
		const int32 DataIndex = GetSelectorDataIndex(CompiledData);
		return Option.CapturedNodeData.IsValidIndex(DataIndex)
			? Option.CapturedNodeData[DataIndex].template GetPtr<InstanceDataType>()
			: nullptr;
	}
}

TEST_CLASS(ArcTacticsBindingTests, "ArcTactics.Bindings")
{
	TEST_METHOD(EmptyConfig_CompilesToExecutableRuntimeData)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeConfig();
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		const bool bCompiled = ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics);

		ASSERT_THAT(IsTrue(bCompiled, TEXT("A valid transient config must compile")));
		ASSERT_THAT(IsTrue(Config->CompiledData.IsExecutable(), TEXT("Only executable compiled data may enter runtime")));
		ASSERT_THAT(AreEqual(17u, Config->CompiledData.Revision));
		ASSERT_THAT(AreEqual(0, Diagnostics.Num()));
	}

	TEST_METHOD(MissingContextId_RejectsCompilation)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeConfig();
		Config->ContextID = FGuid();
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;

		const bool bCompiled = ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics);

		ASSERT_THAT(IsFalse(bCompiled, TEXT("The compiler must reject a context without a stable authoring ID")));
		ASSERT_THAT(IsFalse(Config->CompiledData.IsExecutable(), TEXT("Failed compilation must leave no executable artifact")));
		ASSERT_THAT(AreEqual(1, Diagnostics.Num(),
			TEXT("Every other root ID is valid, so the sole diagnostic must be the missing context endpoint")));
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("missing its stable ID"))));
		ASSERT_THAT(IsFalse(Diagnostics[0].NodeID.IsValid(),
			TEXT("The missing context endpoint has no stable ID to attribute the diagnostic to")));
	}

	TEST_METHOD(RequiredBindingWithoutSource_RejectsCompilation)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeConfig();
		FArcTacticsNodeDefinition Action;
		Action.ID = FGuid(5, 6, 7, 8);
		Action.Node.InitializeAs<FArcTacticsTestAction>();
		Action.InstanceData.InitializeAs<FArcTacticsTestActionInstanceData>();
		FArcTacticsActionDefinition ActionDefinition;
		ActionDefinition.Action = Action;
		Config->Actions.Add(ActionDefinition);
		Config->EnsureValidNodeIDs();
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;

		const bool bCompiled = ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics);

		ASSERT_THAT(IsFalse(bCompiled, TEXT("A required action input without a binding must be rejected")));
		ASSERT_THAT(IsFalse(Config->CompiledData.IsExecutable()));
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("required")),
			TEXT("The rejection must specifically report the unbound required input")));
	}

	TEST_METHOD(RequiredBinding_CopiesThroughProductionDecisionAndPreservesLiteral)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeBoundActionConfig();
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));

		FArcTacticsInstanceSlot Instance;
		const FMassEntityHandle Entity(8, 2);
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), Entity, 13)));
		Instance.Context.Self = Entity;
		FArcTacticsTaskContext Context;
		Context.Entity = Entity;

		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(IsTrue(Instance.HasSelectedAction(), TEXT("A bound action must enter the shared selection pool")));
		if (!Instance.HasSelectedAction())
		{
			return;
		}
		const FMassEntityHandle BoundEntity =
			Instance.SelectedAction.ActionData.GetPtr<FArcTacticsTestActionInstanceData>()->Entity;
		ASSERT_THAT(IsTrue(BoundEntity.Index == Entity.Index
			&& BoundEntity.SerialNumber == Entity.SerialNumber));
		ASSERT_THAT(AreEqual(91,
			Instance.SelectedAction.ActionData.GetPtr<FArcTacticsTestActionInstanceData>()->Literal,
			TEXT("Unbound parameter defaults must survive binding copy")));
		Instance.ActivateSelectedAction();
		ASSERT_THAT(IsTrue(Instance.ActiveAction.IsSet()));
		ASSERT_THAT(IsFalse(Instance.HasSelectedAction(), TEXT("Activation consumes the selection")));
		if (Instance.Candidates.Num() == 0)
		{
			return;
		}
		Instance.Candidates[0].Option.ActionData.GetMutable<FArcTacticsTestActionInstanceData>().Literal = 4;
		ASSERT_THAT(AreEqual(91,
			Instance.ActiveAction.ActionData.GetPtr<FArcTacticsTestActionInstanceData>()->Literal,
			TEXT("Activated data must own a copy of evaluation scratch")));
	}

	TEST_METHOD(RoleMetadata_RejectsMissingAndConflictingExposedRoles)
	{
		UArcTacticsConfig* MissingConfig = ArcTacticsBindingTestsPrivate::MakeConfig();
		FArcTacticsNodeDefinition MissingAction;
		MissingAction.ID = FGuid(5, 6, 7, 8);
		MissingAction.Node.InitializeAs<FArcTacticsTestMissingRoleAction>();
		MissingAction.InstanceData.InitializeAs<FArcTacticsTestMissingRoleInstanceData>();
		FArcTacticsActionDefinition MissingDefinition;
		MissingDefinition.Action = MissingAction;
		MissingConfig->Actions.Add(MissingDefinition);
		MissingConfig->EnsureValidNodeIDs();
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsFalse(ArcTacticsBindingTestsPrivate::CompileConfig(*MissingConfig, Diagnostics)));
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("exactly one tactics role"))));

		UArcTacticsConfig* ConflictingConfig = ArcTacticsBindingTestsPrivate::MakeConfig();
		FArcTacticsNodeDefinition ConflictingAction;
		ConflictingAction.ID = FGuid(9, 10, 11, 12);
		ConflictingAction.Node.InitializeAs<FArcTacticsTestConflictingRoleAction>();
		ConflictingAction.InstanceData.InitializeAs<FArcTacticsTestConflictingRoleInstanceData>();
		FArcTacticsActionDefinition ConflictingDefinition;
		ConflictingDefinition.Action = ConflictingAction;
		ConflictingConfig->Actions.Add(ConflictingDefinition);
		ConflictingConfig->EnsureValidNodeIDs();
		Diagnostics.Reset();
		ASSERT_THAT(IsFalse(ArcTacticsBindingTestsPrivate::CompileConfig(*ConflictingConfig, Diagnostics)));
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("exactly one tactics role"))));

		UArcTacticsConfig* BadNodeConfig = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false);
		BadNodeConfig->Selectors[0].Considerations[0].Node.InitializeAs<FArcTacticsTestBadConsideration>();
		Diagnostics.Reset();
		ASSERT_THAT(IsFalse(ArcTacticsBindingTestsPrivate::CompileConfig(*BadNodeConfig, Diagnostics)));
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("FArcTacticsConsiderationNode"))));

		UArcTacticsConfig* BadSelectorConfig = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false);
		BadSelectorConfig->Selectors[0].Selector.Node.InitializeAs<FArcTacticsTestAction>();
		Diagnostics.Reset();
		ASSERT_THAT(IsFalse(ArcTacticsBindingTestsPrivate::CompileConfig(*BadSelectorConfig, Diagnostics)));
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("FArcTacticsSelectorNode"))));
	}

	TEST_METHOD(NestedRootRole_AllowsChildPathWithoutChildRoleMetadata)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeBoundActionConfig();
		FArcTacticsNodeDefinition Task;
		Task.ID = FGuid(9, 10, 11, 12);
		Task.Node.InitializeAs<FArcTacticsTestTask>();
		Task.InstanceData.InitializeAs<FArcTacticsTestTaskInstanceData>();
		Config->GlobalTasks.Add(Task);
		Config->EditorBindings.AddBinding(
			FPropertyBindingPath(Task.ID, TEXT("Scalar")),
			ArcTacticsBindingTestsPrivate::MakePath(Config->Actions[0].Action.ID, TEXT("NestedInput.Child")));
		Config->EnsureValidNodeIDs();
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		FArcTacticsInstanceSlot Instance;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(12, 2), 12)));
		FArcTacticsTaskContext Context;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		const FArcTacticsTestActionInstanceData* ActionData =
			Instance.SelectedAction.ActionData.GetPtr<FArcTacticsTestActionInstanceData>();
		ASSERT_THAT(IsNotNull(ActionData));
		if (ActionData)
		{
			ASSERT_THAT(AreEqual(11, ActionData->NestedInput.Child,
				TEXT("The task's Enter() sets Scalar at runtime, proving a live output copy rather than an authored default")));
		}
	}

	TEST_METHOD(ParameterBinding_OverridesDefaultWhileOptionalInputsRemainUnbound)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeBoundActionConfig();
		FArcTacticsNodeDefinition Task;
		Task.ID = FGuid(9, 10, 11, 12);
		Task.Node.InitializeAs<FArcTacticsTestTask>();
		Task.InstanceData.InitializeAs<FArcTacticsTestTaskInstanceData>();
		Config->GlobalTasks.Add(Task);
		Config->EditorBindings.AddBinding(
			FPropertyBindingPath(Task.ID, TEXT("Scalar")),
			FPropertyBindingPath(Config->Actions[0].Action.ID, TEXT("Literal")));
		Config->EnsureValidNodeIDs();
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));

		FArcTacticsInstanceSlot Instance;
		const FMassEntityHandle Entity(13, 2);
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), Entity, 14)));
		Instance.Context.Self = Entity;
		FArcTacticsTaskContext Context;
		Context.Entity = Entity;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		const FArcTacticsTestActionInstanceData* ActionData =
			Instance.SelectedAction.ActionData.GetPtr<FArcTacticsTestActionInstanceData>();
		ASSERT_THAT(IsNotNull(ActionData));
		if (ActionData)
		{
			ASSERT_THAT(AreEqual(11, ActionData->Literal,
				TEXT("The task's Enter() sets Scalar at runtime, proving a live output copy rather than an authored default")));
			ASSERT_THAT(AreEqual(37, ActionData->Scalar,
				TEXT("An unbound optional input must retain its authored nonzero default")));
		}
	}

	TEST_METHOD(OutputBinding_PublishesOwnBagWithDirectionalNumericConversion)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeBoundActionConfig();
		FArcTacticsActionDefinition& ActionDefinition = Config->Actions[0];
		ActionDefinition.ParameterBag.AddProperty(TEXT("Published"), EPropertyBagPropertyType::Float);
		ActionDefinition.ParameterBag.AddProperty(TEXT("PublishedValue"), EPropertyBagPropertyType::Float);
		const FPropertyBagPropertyDesc PublishedValuesDesc(
			TEXT("PublishedValues"), EPropertyBagContainerType::Array, EPropertyBagPropertyType::Float);
		ActionDefinition.ParameterBag.AddProperties({PublishedValuesDesc});
		const FGuid ActionID = ActionDefinition.Action.ID;
		Config->EditorBindings.AddBinding(
			FPropertyBindingPath(ActionDefinition.ParameterBagID, TEXT("PublishedValue")),
			FPropertyBindingPath(ActionID, TEXT("OutputValue")));
		Config->EditorBindings.AddBinding(
			FPropertyBindingPath(ActionDefinition.ParameterBagID, TEXT("Published")),
			FPropertyBindingPath(ActionID, TEXT("OutputInteger")));
		Config->EditorBindings.AddBinding(
			ArcTacticsBindingTestsPrivate::MakePath(ActionDefinition.ParameterBagID, TEXT("PublishedValues[1]")),
			ArcTacticsBindingTestsPrivate::MakePath(ActionID, TEXT("OutputValues[0]")));
		Config->EditorBindings.AddBinding(
			FPropertyBindingPath(ActionDefinition.ParameterBagID, TEXT("PublishedValues")),
			FPropertyBindingPath(ActionID, TEXT("OutputValues")));
		ArcTacticsBindingTestsPrivate::MarkOutputBinding(*Config, ActionID, TEXT("OutputValue"));
		ArcTacticsBindingTestsPrivate::MarkOutputBinding(*Config, ActionDefinition.Action.ID, TEXT("OutputInteger"));
		ArcTacticsBindingTestsPrivate::MarkOutputBinding(*Config, ActionID, TEXT("OutputValues[0]"));
		ArcTacticsBindingTestsPrivate::MarkOutputBinding(*Config, ActionID, TEXT("OutputValues"));
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));

		FArcTacticsInstanceSlot Instance;
		const FMassEntityHandle Entity(15, 2);
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), Entity, 15)));
		Instance.Context.Self = Entity;
		FArcTacticsTaskContext Context;
		Context.Entity = Entity;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		Instance.ActivateSelectedAction();
		ASSERT_THAT(IsTrue(Instance.ActiveAction.IsSet()));
		FArcTacticsTestActionInstanceData* ActionData =
			Instance.ActiveAction.ActionData.GetMutablePtr<FArcTacticsTestActionInstanceData>();
		ASSERT_THAT(IsNotNull(ActionData));
		if (!ActionData)
		{
			return;
		}
		ActionData->OutputInteger = 7;
		ActionData->OutputValue = 9.0f;
		ActionData->OutputValues = { 3.0f, 7.0f };
		ASSERT_THAT(IsTrue(ArcTacticsDecision::PublishActiveActionOutputs(Instance, ArcTacticsSlotTestHelpers::Package(Instance), FArcTQSFunctionContext())));
		const TValueOrError<float, EPropertyBagResult> Published =
			Instance.ActiveAction.ParameterBag.GetValueFloat(TEXT("Published"));
		ASSERT_THAT(IsTrue(Published.IsValid()));
		if (Published.IsValid())
		{
			ASSERT_THAT(IsNear(7.0f, Published.GetValue(), 0.001f));
		}
		const TValueOrError<float, EPropertyBagResult> PublishedValue =
			Instance.ActiveAction.ParameterBag.GetValueFloat(TEXT("PublishedValue"));
		ASSERT_THAT(IsTrue(PublishedValue.IsValid()));
		if (PublishedValue.IsValid())
		{
			ASSERT_THAT(IsNear(9.0f, PublishedValue.GetValue(), 0.001f));
		}
		ASSERT_THAT(IsNear(3.0f,
			ArcTacticsBindingTestsPrivate::GetBagFloatAt(Instance.ActiveAction.ParameterBag, TEXT("PublishedValues"), 1),
			0.001f,
			TEXT("Reverse output bindings must apply indexed destinations after whole-array copies")));
	}

	TEST_METHOD(TraitDecisionInterval_RejectsInvalidValue)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeConfig();
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		FArcTacticsDecisionSettings Settings;
		Settings.DecisionInterval = -1.0f;
		FArcTacticsInstanceSlot Instance;
		ASSERT_THAT(IsFalse(ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, Settings, FMassEntityHandle(19, 2), 19)));
		ASSERT_THAT(IsTrue(Instance.Diagnostic == TEXT("InvalidTraitDecisionSettings")));
	}

	TEST_METHOD(BranchWeightBinding_ResolvesAfterGlobalTaskAndRejectsSiblingWrites)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeTaskActionConfig();
		Config->EditorBindings.AddBinding(
			ArcTacticsBindingTestsPrivate::MakePath(FGuid(20, 21, 22, 23), TEXT("Record.Score")),
			FPropertyBindingPath(Config->Actions[0].ParametersID, TEXT("Weight")));
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		FArcTacticsInstanceSlot Instance;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(17, 2), 17)));
		FArcTacticsTaskContext Context;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(IsTrue(Instance.HasSelectedAction()));
		if (Instance.HasSelectedAction())
		{
			ASSERT_THAT(IsNear(0.75f, Instance.SelectedAction.Parameters.Weight, 0.001f));
		}

		UArcTacticsConfig* SelectorConfig = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false);
		SelectorConfig->EditorBindings.AddBinding(
			ArcTacticsBindingTestsPrivate::MakePath(SelectorConfig->Selectors[0].Selector.ID, TEXT("CurrentLocation.X")),
			FPropertyBindingPath(SelectorConfig->Selectors[0].ParametersID, TEXT("Weight")));
		Diagnostics.Reset();
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*SelectorConfig, Diagnostics)));
		FArcTacticsInstanceSlot SelectorInstance;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(SelectorInstance, SelectorConfig->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(27, 2), 27)));
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(SelectorInstance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(IsTrue(SelectorInstance.HasSelectedAction()));
		ASSERT_THAT(IsTrue(SelectorInstance.Candidates.Num() == 2));
		if (SelectorInstance.Candidates.Num() == 2)
		{
			ASSERT_THAT(IsNear(0.04f, SelectorInstance.Candidates[0].Option.Score, 0.001f,
				TEXT("Weight and the consideration both read CurrentLocation.X, so the accepted score is Weight * BoundScore")));
			ASSERT_THAT(IsNear(0.64f, SelectorInstance.Candidates[1].Option.Score, 0.001f));
			ASSERT_THAT(IsNear(0.8f, SelectorInstance.SelectedAction.Parameters.Weight, 0.001f));
		}

		UArcTacticsConfig* SiblingConfig = ArcTacticsBindingTestsPrivate::MakeTaskActionConfig();
		FArcTacticsNodeDefinition SecondAction;
		SecondAction.ID = FGuid(70, 71, 72, 73);
		SecondAction.Node.InitializeAs<FArcTacticsTestAction>();
		SecondAction.InstanceData.InitializeAs<FArcTacticsTestActionInstanceData>();
		FArcTacticsActionDefinition SecondDefinition;
		SecondDefinition.Action = SecondAction;
		SiblingConfig->Actions.Add(SecondDefinition);
		SiblingConfig->EnsureValidNodeIDs();
		SiblingConfig->EditorBindings.AddBinding(
			FPropertyBindingPath(FGuid(24, 25, 26, 27), TEXT("OutputValue")),
			FPropertyBindingPath(FGuid(70, 71, 72, 73), TEXT("Scalar")));
		Diagnostics.Reset();
		ASSERT_THAT(IsFalse(ArcTacticsBindingTestsPrivate::CompileConfig(*SiblingConfig, Diagnostics)));
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("scope"))));
	}

	TEST_METHOD(InheritedConsiderationParameters_BindResponseCurve)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false);
		const FGuid TaskID(20, 21, 22, 23);
		const FGuid ConsiderationID = Config->Selectors[0].Considerations[0].ID;
		FArcTacticsTestTaskInstanceData& TaskDefaults =
			Config->GlobalTasks[0].InstanceData.GetMutable<FArcTacticsTestTaskInstanceData>();
		TaskDefaults.Mode = 4;
		Config->EditorBindings.AddBinding(
			ArcTacticsBindingTestsPrivate::MakePath(TaskID, TEXT("ResponseCurve")),
			FPropertyBindingPath(ConsiderationID, TEXT("ResponseCurve")));
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		const bool bCompiled = ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics);
		ASSERT_THAT(IsTrue(bCompiled));
		if (!bCompiled)
		{
			return;
		}
		FArcTacticsInstanceSlot Instance;
		const bool bInitialized = ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(28, 2), 28);
		ASSERT_THAT(IsTrue(bInitialized));
		if (!bInitialized)
		{
			return;
		}
		FArcTacticsTaskContext Context;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(IsTrue(Instance.HasSelectedAction(),
			TEXT("The inherited consideration ResponseCurve binding must execute before scoring")));
		ASSERT_THAT(IsTrue(Instance.Candidates.Num() == 2));
		if (Instance.Candidates.Num() == 2)
		{
			ASSERT_THAT(IsNear(0.8f, Instance.Candidates[0].Option.Score, 0.001f));
			ASSERT_THAT(IsNear(0.2f, Instance.Candidates[1].Option.Score, 0.001f));
		}
	}

	TEST_METHOD(RootDecisionSettings_BindGlobalOutputsAndRejectInvalidRuntimeValues)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeTaskActionConfig();
		const FGuid TaskID(20, 21, 22, 23);
		Config->EditorBindings.AddBinding(
			ArcTacticsBindingTestsPrivate::MakePath(TaskID, TEXT("Record.Score")),
			FPropertyBindingPath(Config->DecisionSettingsID, TEXT("MinScore")));
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		const bool bCompiled = ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics);
		ASSERT_THAT(IsTrue(bCompiled));
		if (!bCompiled)
		{
			return;
		}
		FArcTacticsInstanceSlot Instance;
		const bool bInitialized = ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(29, 2), 29);
		ASSERT_THAT(IsTrue(bInitialized));
		if (!bInitialized)
		{
			return;
		}
		FArcTacticsTaskContext Context;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(IsNear(0.75f, ArcTacticsSlotTestHelpers::PackageInstance(Instance).DecisionSettings.MinScore, 0.001f));
		ASSERT_THAT(IsTrue(Instance.HasSelectedAction()));

		UArcTacticsConfig* InvalidConfig = ArcTacticsBindingTestsPrivate::MakeTaskActionConfig(8);
		InvalidConfig->EditorBindings.AddBinding(
			FPropertyBindingPath(TaskID, TEXT("Scalar")),
			FPropertyBindingPath(InvalidConfig->DecisionSettingsID, TEXT("MinScore")));
		TArray<FArcTacticsCompilerDiagnostic> InvalidDiagnostics;
		const bool bInvalidCompiled = ArcTacticsBindingTestsPrivate::CompileConfig(*InvalidConfig, InvalidDiagnostics);
		ASSERT_THAT(IsTrue(bInvalidCompiled));
		if (!bInvalidCompiled)
		{
			return;
		}
		FArcTacticsInstanceSlot InvalidInstance;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(InvalidInstance, InvalidConfig->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(30, 2), 30)));
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(InvalidInstance, Context)
			== EArcTacticsDecisionStatus::Failed));
		ASSERT_THAT(IsTrue(InvalidInstance.Diagnostic == TEXT("InvalidDecisionSettings")));
		ASSERT_THAT(IsFalse(InvalidInstance.HasSelectedAction()));
		ASSERT_THAT(AreEqual(0, InvalidInstance.Candidates.Num()));
	}

	TEST_METHOD(GlobalTaskChain_PublishesFirstValueAndSecondConsumesIt)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeTaskActionConfig();
		const FGuid FirstTaskID(20, 21, 22, 23);
		const FGuid SecondTaskID(70, 71, 72, 73);
		const FGuid ActionID(24, 25, 26, 27);
		FArcTacticsNodeDefinition SecondTask;
		SecondTask.ID = SecondTaskID;
		SecondTask.Node.InitializeAs<FArcTacticsTestTask>();
		FArcTacticsTestTaskInstanceData SecondDefaults;
		SecondDefaults.Mode = 3;
		SecondTask.InstanceData.InitializeAs<FArcTacticsTestTaskInstanceData>(SecondDefaults);
		Config->GlobalTasks.Add(SecondTask);
		Config->GlobalParameters.AddProperty(TEXT("Published"), EPropertyBagPropertyType::Int32);
		Config->GlobalParameters.SetValueInt32(TEXT("Published"), 0);
		Config->EditorBindings.Reset();
		Config->EditorBindings.AddBinding(
			FPropertyBindingPath(Config->GlobalParametersID, TEXT("Published")),
			FPropertyBindingPath(FirstTaskID, TEXT("Scalar")));
		ArcTacticsBindingTestsPrivate::MarkOutputBinding(*Config, FirstTaskID, TEXT("Scalar"));
		Config->EditorBindings.AddBinding(
			FPropertyBindingPath(Config->GlobalParametersID, TEXT("Published")),
			FPropertyBindingPath(SecondTaskID, TEXT("Scalar")));
		ArcTacticsBindingTestsPrivate::MarkOutputBinding(*Config, SecondTaskID, TEXT("Scalar"));
		Config->EditorBindings.AddBinding(
			FPropertyBindingPath(Config->GlobalParametersID, TEXT("Published")),
			FPropertyBindingPath(SecondTaskID, TEXT("Input")));
		Config->EditorBindings.AddBinding(
			FPropertyBindingPath(FirstTaskID, TEXT("Entity")),
			FPropertyBindingPath(ActionID, TEXT("Entity")));
		Config->EditorBindings.AddBinding(
			FPropertyBindingPath(SecondTaskID, TEXT("Scalar")),
			FPropertyBindingPath(ActionID, TEXT("Scalar")));
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		const bool bCompiled = ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics);
		ASSERT_THAT(IsTrue(bCompiled));
		if (!bCompiled)
		{
			return;
		}
		FArcTacticsInstanceSlot Instance;
		const bool bInitialized = ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(31, 2), 31);
		ASSERT_THAT(IsTrue(bInitialized));
		if (!bInitialized)
		{
			return;
		}
		FArcTacticsTaskContext Context;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(AreEqual(22, ArcTacticsSlotTestHelpers::PackageInstance(Instance).GlobalParameters.GetValueInt32(TEXT("Published")).GetValue()));
		const FArcTacticsTestActionInstanceData* ActionData =
			Instance.SelectedAction.ActionData.GetPtr<FArcTacticsTestActionInstanceData>();
		ASSERT_THAT(IsNotNull(ActionData));
		if (ActionData)
		{
			ASSERT_THAT(AreEqual(22, ActionData->Scalar,
				TEXT("The action must read the second global task's published value")));
		}
	}

	TEST_METHOD(TraitTopPercent_InvalidValueRejectsInitialization)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeTaskActionConfig();
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		const bool bCompiled = ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics);
		ASSERT_THAT(IsTrue(bCompiled));
		if (!bCompiled)
		{
			return;
		}
		FArcTacticsDecisionSettings Settings;
		Settings.Selection.TopPercent = 2.0f;
		FArcTacticsInstanceSlot Instance;
		ASSERT_THAT(IsFalse(ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, Settings, FMassEntityHandle(35, 2), 35)));
		ASSERT_THAT(AreEqual(EArcTacticsDecisionStatus::Failed, Instance.DecisionStatus));
		ASSERT_THAT(IsTrue(Instance.Diagnostic == TEXT("InvalidTraitDecisionSettings")));
		ASSERT_THAT(IsFalse(Instance.HasSelectedAction()));
	}

	TEST_METHOD(BoundRuntimeBranchWeight_InvalidProducerRejectsCandidate)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false);
		const FGuid TaskID(20, 21, 22, 23);
		Config->GlobalTasks[0].InstanceData.GetMutable<FArcTacticsTestTaskInstanceData>().Mode = 8;
		Config->EditorBindings.AddBinding(
			FPropertyBindingPath(TaskID, TEXT("Scalar")),
			FPropertyBindingPath(Config->Selectors[0].ParametersID, TEXT("Weight")));
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		const bool bCompiled = ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics);
		ASSERT_THAT(IsTrue(bCompiled));
		if (!bCompiled)
		{
			return;
		}
		FArcTacticsInstanceSlot Instance;
		const bool bInitialized = ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(36, 2), 36);
		ASSERT_THAT(IsTrue(bInitialized));
		if (!bInitialized)
		{
			return;
		}
		FArcTacticsTaskContext Context;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(IsFalse(Instance.HasSelectedAction()));
		ASSERT_THAT(AreEqual(2, Instance.Candidates.Num()));
		if (Instance.Candidates.Num() == 2)
		{
			ASSERT_THAT(IsTrue(Instance.Candidates[0].Outcome == EArcTacticsCandidateOutcome::BindingFailed));
			ASSERT_THAT(IsTrue(Instance.Candidates[0].RejectionReason == TEXT("InvalidPairParameters")));
		}
	}

	TEST_METHOD(TraitSelectionMode_ChangesSelectionPolicy)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false);
		FArcTacticsDecisionSettings Settings;
		Settings.Selection.Mode = EArcTacticsSelectionMode::RandomFromTopPercent;
		Settings.Selection.TopPercent = 1.0f;
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		const bool bCompiled = ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics);
		ASSERT_THAT(IsTrue(bCompiled));
		if (!bCompiled)
		{
			return;
		}
		FArcTacticsInstanceSlot Instance;
		const bool bInitialized = ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, Settings, FMassEntityHandle(37, 2), 37);
		ASSERT_THAT(IsTrue(bInitialized));
		if (!bInitialized)
		{
			return;
		}
		FArcTacticsTaskContext Context;
		Instance.RandomStream.Initialize(7);
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(IsTrue(Instance.Settings.Selection.Mode == EArcTacticsSelectionMode::RandomFromTopPercent));
		ASSERT_THAT(AreEqual(2, Instance.Candidates.Num()));
		ASSERT_THAT(IsTrue(Instance.HasSelectedAction()));
		if (Instance.HasSelectedAction())
		{
			const FArcTacticsTestActionInstanceData* Selected =
				Instance.SelectedAction.ActionData.GetPtr<FArcTacticsTestActionInstanceData>();
			ASSERT_THAT(IsNotNull(Selected));
			ASSERT_THAT(AreEqual(0, Instance.SelectedAction.ItemIndex,
				TEXT("The bound RandomFromTopPercent policy must select the seeded lower-score item")));
			if (Selected)
			{
				ASSERT_THAT(IsNear(0.2f, static_cast<float>(Selected->Location.X), 0.001f,
					TEXT("The selected action must read the selected item's CurrentLocation")));
			}
		}
	}

	TEST_METHOD(ScopeMatrix_RejectsParentContextSelectorOutputAndSiblingWrites)
	{
		UArcTacticsConfig* ParentConfig = ArcTacticsBindingTestsPrivate::MakeTaskActionConfig();
		ParentConfig->GlobalParameters.AddProperty(TEXT("Published"), EPropertyBagPropertyType::Int32);
		ParentConfig->EditorBindings.AddBinding(
			FPropertyBindingPath(ParentConfig->DecisionSettingsID, TEXT("MinScore")),
			FPropertyBindingPath(ParentConfig->GlobalParametersID, TEXT("Published")));
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsFalse(ArcTacticsBindingTestsPrivate::CompileConfig(*ParentConfig, Diagnostics)));
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("scope"))));

		UArcTacticsConfig* ContextConfig = ArcTacticsBindingTestsPrivate::MakeBoundActionConfig();
		const FGuid ActionID = ContextConfig->Actions[0].Action.ID;
		const FGuid BagID = ContextConfig->Actions[0].ParameterBagID;
		ContextConfig->Actions[0].ParameterBag.AddProperty(TEXT("Published"), EPropertyBagPropertyType::Int32);
		ContextConfig->EditorBindings.AddBinding(
			FPropertyBindingPath(BagID, TEXT("Published")),
			FPropertyBindingPath(ContextConfig->ContextID, TEXT("Self")));
		ArcTacticsBindingTestsPrivate::MarkOutputBinding(*ContextConfig, ContextConfig->ContextID, TEXT("Self"));
		bool bContextBindingIsOutput = false;
		for (const FArcTacticsBinding& Binding : ContextConfig->EditorBindings.GetBindings())
		{
			if (ArcTacticsBindingTestsPrivate::PathMatches(Binding.GetTargetPath(), ContextConfig->ContextID, TEXT("Self")))
			{
				bContextBindingIsOutput = Binding.IsOutputBinding();
			}
		}
		ASSERT_THAT(IsTrue(bContextBindingIsOutput,
			TEXT("The binding must actually be marked as an output binding before proving it is rejected by scope")));
		Diagnostics.Reset();
		ASSERT_THAT(IsFalse(ArcTacticsBindingTestsPrivate::CompileConfig(*ContextConfig, Diagnostics)));
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("scope")),
			TEXT("An output binding must not write into the read-only Context endpoint")));

		UArcTacticsConfig* ControlConfig = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false);
		ControlConfig->Selectors[0].ParameterBag.AddProperty(TEXT("Published"), EPropertyBagPropertyType::Int32);
		ArcTacticsBindingTestsPrivate::AddBagOutputBinding(*ControlConfig,
			ControlConfig->Selectors[0].ParameterBagID, ControlConfig->Selectors[0].Action.ID, TEXT("OutputInteger"));
		Diagnostics.Reset();
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*ControlConfig, Diagnostics),
			TEXT("Publishing the selector action's output into the selector bag is the valid control for the selector-node rejection")));

		UArcTacticsConfig* SelectorOutputConfig = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false);
		const FGuid SelectorNodeID = SelectorOutputConfig->Selectors[0].Selector.ID;
		SelectorOutputConfig->Selectors[0].ParameterBag.AddProperty(TEXT("Published"), EPropertyBagPropertyType::Int32);
		ArcTacticsBindingTestsPrivate::AddBagOutputBinding(*SelectorOutputConfig,
			SelectorOutputConfig->Selectors[0].ParameterBagID, SelectorNodeID, TEXT("CurrentLocation"));
		bool bSelectorBindingIsOutput = false;
		for (const FArcTacticsBinding& Binding : SelectorOutputConfig->EditorBindings.GetBindings())
		{
			if (ArcTacticsBindingTestsPrivate::PathMatches(Binding.GetTargetPath(), SelectorNodeID, TEXT("CurrentLocation")))
			{
				bSelectorBindingIsOutput = Binding.IsOutputBinding();
			}
		}
		ASSERT_THAT(IsTrue(bSelectorBindingIsOutput,
			TEXT("The binding must actually be marked as an output binding before proving it is rejected by scope")));
		Diagnostics.Reset();
		ASSERT_THAT(IsFalse(ArcTacticsBindingTestsPrivate::CompileConfig(*SelectorOutputConfig, Diagnostics)));
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("scope")),
			TEXT("An output binding must not write into a selector node's Current output")));

		UArcTacticsConfig* SiblingSelectorConfig = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false);
		FArcTacticsSelectorDefinition Duplicate = SiblingSelectorConfig->Selectors[0];
		SiblingSelectorConfig->DuplicateAuthoringData(Duplicate);
		const FGuid FirstSelectorActionID = SiblingSelectorConfig->Selectors[0].Action.ID;
		const FGuid SecondSelectorActionID = Duplicate.Action.ID;
		SiblingSelectorConfig->Selectors.Add(Duplicate);
		SiblingSelectorConfig->EditorBindings.AddBinding(
			FPropertyBindingPath(FirstSelectorActionID, TEXT("OutputValue")),
			FPropertyBindingPath(SecondSelectorActionID, TEXT("Scalar")));
		Diagnostics.Reset();
		ASSERT_THAT(IsFalse(ArcTacticsBindingTestsPrivate::CompileConfig(*SiblingSelectorConfig, Diagnostics)));
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("scope"))));
	}

	TEST_METHOD(ConsiderationOutput_FeedsNextConsiderationThroughOwnBag)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false);
		FArcTacticsSelectorDefinition& Selector = Config->Selectors[0];
		Selector.ParameterBag.AddProperty(TEXT("Published"), EPropertyBagPropertyType::Float);
		Selector.ParameterBag.AddProperty(TEXT("BranchWeight"), EPropertyBagPropertyType::Float);
		Selector.ParameterBag.SetValueFloat(TEXT("BranchWeight"), 0.5f);
		FArcTacticsNodeDefinition Second;
		Second.ID = FGuid(46, 47, 48, 49);
		Second.Node.InitializeAs<FArcTacticsTestConsideration>();
		Second.InstanceData.InitializeAs<FArcTacticsTestConsiderationInstanceData>();
		Selector.Considerations.Add(Second);
		FArcTacticsTestConsiderationInstanceData& FirstConsiderationDefaults =
			Selector.Considerations[0].InstanceData.GetMutable<FArcTacticsTestConsiderationInstanceData>();
		FirstConsiderationDefaults.bPublishEight = true;
		const FGuid BagID = Selector.ParameterBagID;
		Config->EditorBindings.AddBinding(
			FPropertyBindingPath(BagID, TEXT("BranchWeight")),
			FPropertyBindingPath(Selector.Considerations[0].ID, TEXT("PublishedScore")));
		Config->EditorBindings.AddBinding(
			FPropertyBindingPath(BagID, TEXT("BranchWeight")),
			FPropertyBindingPath(Second.ID, TEXT("BoundScore")));
		Config->EditorBindings.AddBinding(
			FPropertyBindingPath(BagID, TEXT("BranchWeight")),
			FPropertyBindingPath(Selector.ParametersID, TEXT("Weight")));
		ArcTacticsBindingTestsPrivate::MarkOutputBinding(*Config, Selector.Considerations[0].ID, TEXT("PublishedScore"));
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		FArcTacticsInstanceSlot Instance;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(18, 2), 18)));
		FArcTacticsTaskContext Context;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(AreEqual(2, Instance.Candidates.Num()));
		if (Instance.Candidates.Num() == 2)
		{
			ASSERT_THAT(IsTrue(Instance.Candidates[0].Outcome == EArcTacticsCandidateOutcome::Accepted));
			ASSERT_THAT(IsNear(0.2f, Instance.Candidates[0].Option.Score, 0.001f,
				TEXT("The second consideration must consume the first consideration's published bag value")));
			ASSERT_THAT(IsNear(0.5f, Instance.Candidates[0].Option.Parameters.Weight, 0.001f,
				TEXT("Branch Weight is consumed before consideration outputs mutate the bag")));
			const TValueOrError<float, EPropertyBagResult> PublishedWeight =
			Instance.Candidates[0].Option.ParameterBag.GetValueFloat(TEXT("BranchWeight"));
			ASSERT_THAT(IsTrue(PublishedWeight.IsValid()));
			if (PublishedWeight.IsValid())
			{
				ASSERT_THAT(IsNear(0.8f, PublishedWeight.GetValue(), 0.001f,
					TEXT("The consideration output must publish the new Weight value into its own bag")));
			}
		}
	}

	TEST_METHOD(GlobalTaskOutputLifecycle_PublishesRunningAndSucceededButNotFailedOrCancelled)
	{
		UArcTacticsConfig* RunningConfig = ArcTacticsBindingTestsPrivate::MakeTaskActionConfig(1);
		RunningConfig->GlobalParameters.AddProperty(TEXT("Published"), EPropertyBagPropertyType::Int32);
		RunningConfig->GlobalParameters.SetValueInt32(TEXT("Published"), -1);
		RunningConfig->EditorBindings.AddBinding(
			FPropertyBindingPath(RunningConfig->GlobalParametersID, TEXT("Published")),
			FPropertyBindingPath(FGuid(20, 21, 22, 23), TEXT("Scalar")));
		ArcTacticsBindingTestsPrivate::MarkOutputBinding(*RunningConfig, FGuid(20, 21, 22, 23), TEXT("Scalar"));
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*RunningConfig, Diagnostics)));
		FArcTacticsInstanceSlot Running;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Running, RunningConfig->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(19, 2), 19)));
		FArcTacticsTaskContext Context;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Running, Context)
			== EArcTacticsDecisionStatus::Running));
		ASSERT_THAT(AreEqual(11, ArcTacticsSlotTestHelpers::PackageInstance(Running).GlobalParameters.GetValueInt32(TEXT("Published")).GetValue()));
		ASSERT_THAT(IsTrue(ArcTacticsDecision::TickDecision(Running, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(AreEqual(22, ArcTacticsSlotTestHelpers::PackageInstance(Running).GlobalParameters.GetValueInt32(TEXT("Published")).GetValue()));
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Running, Context)
			== EArcTacticsDecisionStatus::Running));
		ASSERT_THAT(AreEqual(11, ArcTacticsSlotTestHelpers::PackageInstance(Running).GlobalParameters.GetValueInt32(TEXT("Published")).GetValue()));
		FArcTacticsInstanceSlot Cancelled;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Cancelled, RunningConfig->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(21, 2), 21)));
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Cancelled, Context)
			== EArcTacticsDecisionStatus::Running));
		ArcTacticsDecision::CancelDecision(Cancelled, Context);
		ASSERT_THAT(AreEqual(11, ArcTacticsSlotTestHelpers::PackageInstance(Cancelled).GlobalParameters.GetValueInt32(TEXT("Published")).GetValue()));

		UArcTacticsConfig* FailedConfig = ArcTacticsBindingTestsPrivate::MakeTaskActionConfig(2);
		FailedConfig->GlobalParameters.AddProperty(TEXT("Published"), EPropertyBagPropertyType::Int32);
		FailedConfig->GlobalParameters.SetValueInt32(TEXT("Published"), -1);
		FailedConfig->EditorBindings.AddBinding(
			FPropertyBindingPath(FailedConfig->GlobalParametersID, TEXT("Published")),
			FPropertyBindingPath(FGuid(20, 21, 22, 23), TEXT("Scalar")));
		ArcTacticsBindingTestsPrivate::MarkOutputBinding(*FailedConfig, FGuid(20, 21, 22, 23), TEXT("Scalar"));
		Diagnostics.Reset();
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*FailedConfig, Diagnostics)));
		FArcTacticsInstanceSlot Failed;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Failed, FailedConfig->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(20, 2), 20)));
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Failed, Context)
			== EArcTacticsDecisionStatus::Failed));
		ASSERT_THAT(AreEqual(-1, ArcTacticsSlotTestHelpers::PackageInstance(Failed).GlobalParameters.GetValueInt32(TEXT("Published")).GetValue()));
	}

	TEST_METHOD(DuplicateAction_RemapsScopedIDsAndPreservesOutputBindingDirection)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeBoundActionConfig();
		FArcTacticsActionDefinition& Original = Config->Actions[0];
		Original.ParameterBag.AddProperty(TEXT("Published"), EPropertyBagPropertyType::Float);
		Config->EditorBindings.AddBinding(
			FPropertyBindingPath(Original.ParameterBagID, TEXT("Published")),
			FPropertyBindingPath(Original.Action.ID, TEXT("OutputInteger")));
		ArcTacticsBindingTestsPrivate::MarkOutputBinding(*Config, Original.Action.ID, TEXT("OutputInteger"));
		FArcTacticsActionDefinition Duplicate = Original;
		const FGuid OriginalActionID = Original.Action.ID;
		const FGuid OriginalBagID = Original.ParameterBagID;
		const TMap<FGuid, FGuid> Remap = Config->DuplicateAuthoringData(Duplicate);
		ASSERT_THAT(IsTrue(Remap.Contains(OriginalActionID)));
		ASSERT_THAT(IsTrue(Remap.Contains(OriginalBagID)));
		ASSERT_THAT(IsFalse(Duplicate.Action.ID == OriginalActionID));
		ASSERT_THAT(IsFalse(Duplicate.ParameterBagID == OriginalBagID));
		Config->Actions.Add(Duplicate);
		bool bFoundRemappedOutput = false;
		bool bFoundRemappedOutputSource = false;
		bool bFoundRemappedForward = false;
		for (const FArcTacticsBinding& Binding : Config->EditorBindings.GetBindings())
		{
			if (ArcTacticsBindingTestsPrivate::PathMatches(Binding.GetTargetPath(), Duplicate.Action.ID, TEXT("OutputInteger")))
			{
				bFoundRemappedOutput = Binding.IsOutputBinding();
				bFoundRemappedOutputSource = Binding.GetSourcePath().GetStructID() == Duplicate.ParameterBagID;
			}
			if (ArcTacticsBindingTestsPrivate::PathMatches(Binding.GetTargetPath(), Duplicate.Action.ID, TEXT("Entity")))
			{
				bFoundRemappedForward = Binding.GetSourcePath().GetStructID() == Config->ContextID;
			}
		}
		ASSERT_THAT(IsTrue(bFoundRemappedOutput));
		ASSERT_THAT(IsTrue(bFoundRemappedOutputSource));
		ASSERT_THAT(IsTrue(bFoundRemappedForward));
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		FArcTacticsInstanceSlot Instance;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(25, 2), 25)));
		FArcTacticsTaskContext Context;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(AreEqual(2, Instance.Candidates.Num(),
			TEXT("A duplicated action must retain remapped executable bindings")));
	}

	TEST_METHOD(DuplicateSelector_RemapsSelectorNodeBagAndConsiderationBindings)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false);
		FArcTacticsSelectorDefinition Original = Config->Selectors[0];
		Config->EditorBindings.AddBinding(
			ArcTacticsBindingTestsPrivate::MakePath(Original.Selector.ID, TEXT("CurrentLocation.X")),
			FPropertyBindingPath(Original.ParametersID, TEXT("Weight")));
		const FGuid OriginalSelectorID = Original.Selector.ID;
		const FGuid OriginalActionID = Original.Action.ID;
		const FGuid OriginalBagID = Original.ParameterBagID;
		const FGuid OriginalConsiderationID = Original.Considerations[0].ID;
		const TMap<FGuid, FGuid> Remap = Config->DuplicateAuthoringData(Original);
		ASSERT_THAT(IsTrue(Remap.Contains(OriginalSelectorID)));
		ASSERT_THAT(IsTrue(Remap.Contains(OriginalActionID)));
		ASSERT_THAT(IsTrue(Remap.Contains(OriginalBagID)));
		ASSERT_THAT(IsTrue(Remap.Contains(OriginalConsiderationID)));
		ASSERT_THAT(IsFalse(Original.Selector.ID == OriginalSelectorID));
		ASSERT_THAT(IsFalse(Original.Action.ID == OriginalActionID));
		ASSERT_THAT(IsFalse(Original.ParameterBagID == OriginalBagID));
		ASSERT_THAT(IsFalse(Original.Considerations[0].ID == OriginalConsiderationID));
		Config->Selectors.Add(Original);
		bool bFoundSelectorParameters = false;
		bool bFoundSelectorNodeSource = false;
		bool bFoundSelectorConsideration = false;
		bool bFoundSelectorItemsFromGlobalTask = false;
		bool bFoundLocationWeightRemap = false;
		for (const FArcTacticsBinding& Binding : Config->EditorBindings.GetBindings())
		{
			if (Binding.GetTargetPath().GetStructID() == Original.ParametersID)
			{
				bFoundSelectorParameters = true;
			}
			if (Binding.GetSourcePath().GetStructID() == Original.Selector.ID)
			{
				bFoundSelectorNodeSource = true;
			}
			if (ArcTacticsBindingTestsPrivate::PathMatches(Binding.GetSourcePath(), FGuid(20, 21, 22, 23), TEXT("Locations"))
				&& ArcTacticsBindingTestsPrivate::PathMatches(Binding.GetTargetPath(), Original.Selector.ID, TEXT("Items")))
			{
				bFoundSelectorItemsFromGlobalTask = true;
			}
			if (ArcTacticsBindingTestsPrivate::PathMatches(Binding.GetSourcePath(), Original.Selector.ID, TEXT("CurrentLocation.X"))
				&& ArcTacticsBindingTestsPrivate::PathMatches(Binding.GetTargetPath(), Original.ParametersID, TEXT("Weight")))
			{
				bFoundLocationWeightRemap = true;
			}
			if (Binding.GetTargetPath().GetStructID() == Original.Considerations[0].ID)
			{
				bFoundSelectorConsideration = true;
			}
		}
		ASSERT_THAT(IsTrue(bFoundSelectorParameters));
		ASSERT_THAT(IsTrue(bFoundSelectorNodeSource));
		ASSERT_THAT(IsTrue(bFoundSelectorConsideration));
		ASSERT_THAT(IsTrue(bFoundSelectorItemsFromGlobalTask,
			TEXT("The duplicate's Items binding must keep its external global-task source and target the new selector node")));
		ASSERT_THAT(IsTrue(bFoundLocationWeightRemap));
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		FArcTacticsInstanceSlot Instance;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(26, 2), 26)));
		FArcTacticsTaskContext Context;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(AreEqual(4, Instance.Candidates.Num(),
			TEXT("Duplicated selector must compile and contribute both array items")));
	}

	TEST_METHOD(InvalidRuntimeValues_RejectSelectionWithoutStaleWinner)
	{
		UArcTacticsConfig* InvalidSettings = ArcTacticsBindingTestsPrivate::MakeTaskActionConfig();
		InvalidSettings->DecisionSettings.MinScore = -1.0f;
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsFalse(ArcTacticsBindingTestsPrivate::CompileConfig(*InvalidSettings, Diagnostics)));

		UArcTacticsConfig* InvalidRaw = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false);
		FArcTacticsTaskContext Context;
		FArcTacticsTestTaskInstanceData& Defaults =
			InvalidRaw->GlobalTasks[0].InstanceData.GetMutable<FArcTacticsTestTaskInstanceData>();
		Defaults.Locations[0].X = NAN;
		Diagnostics.Reset();
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*InvalidRaw, Diagnostics)));
		FArcTacticsInstanceSlot RawInstance;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(RawInstance, InvalidRaw->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(23, 2), 23)));
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(RawInstance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(AreEqual(2, RawInstance.Candidates.Num()));
		if (RawInstance.Candidates.Num() == 2)
		{
			ASSERT_THAT(IsTrue(RawInstance.Candidates[0].Outcome == EArcTacticsCandidateOutcome::BindingFailed));
			ASSERT_THAT(IsTrue(RawInstance.Candidates[0].RejectionReason == TEXT("InvalidConsiderationScore")));
			ASSERT_THAT(IsTrue(RawInstance.Candidates[1].Outcome == EArcTacticsCandidateOutcome::Accepted));
			ASSERT_THAT(IsTrue(RawInstance.HasSelectedAction()));
		}

		UArcTacticsConfig* AllInvalid = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false);
		FArcTacticsTestTaskInstanceData& AllInvalidDefaults =
			AllInvalid->GlobalTasks[0].InstanceData.GetMutable<FArcTacticsTestTaskInstanceData>();
		AllInvalidDefaults.Locations[0].X = NAN;
		AllInvalidDefaults.Locations[1].X = NAN;
		Diagnostics.Reset();
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*AllInvalid, Diagnostics)));
		FArcTacticsInstanceSlot AllInvalidInstance;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(AllInvalidInstance, AllInvalid->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(32, 2), 32)));
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(AllInvalidInstance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(IsFalse(AllInvalidInstance.HasSelectedAction()));
	}

	TEST_METHOD(BoundRuntimeResponseCurve_InvalidProducerRejectsCandidate)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false);
		const FGuid TaskID(20, 21, 22, 23);
		const FGuid ConsiderationID = Config->Selectors[0].Considerations[0].ID;
		FArcTacticsTestTaskInstanceData& RuntimeDefaults =
			Config->GlobalTasks[0].InstanceData.GetMutable<FArcTacticsTestTaskInstanceData>();
		RuntimeDefaults.Mode = 5;
		Config->EditorBindings.AddBinding(
			ArcTacticsBindingTestsPrivate::MakePath(TaskID, TEXT("ResponseCurve")),
			FPropertyBindingPath(ConsiderationID, TEXT("ResponseCurve")));
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		const bool bCompiled = ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics);
		ASSERT_THAT(IsTrue(bCompiled));
		if (!bCompiled)
		{
			return;
		}
		FArcTacticsInstanceSlot Instance;
		const bool bInitialized = ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(34, 2), 34);
		ASSERT_THAT(IsTrue(bInitialized));
		if (!bInitialized)
		{
			return;
		}
		FArcTacticsTaskContext Context;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(IsFalse(Instance.HasSelectedAction()));
		ASSERT_THAT(AreEqual(2, Instance.Candidates.Num()));
		if (Instance.Candidates.Num() == 2)
		{
			ASSERT_THAT(IsTrue(Instance.Candidates[0].Outcome == EArcTacticsCandidateOutcome::BindingFailed));
			ASSERT_THAT(IsTrue(Instance.Candidates[0].RejectionReason == TEXT("InvalidResponseCurve")));
		}
	}

	TEST_METHOD(SelectorBranchBag_ResetsBetweenCandidatesAndPreservesWinnerScope)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false);
		FArcTacticsSelectorDefinition& Selector = Config->Selectors[0];
		Selector.ParameterBag.AddProperty(TEXT("CandidateValue"), EPropertyBagPropertyType::Float);
		const FGuid BagID = Selector.ParameterBagID;
		const FGuid ConsiderationID = Selector.Considerations[0].ID;
		Selector.ParameterBag.SetValueFloat(TEXT("CandidateValue"), 0.0f);
		Config->EditorBindings.AddBinding(
			FPropertyBindingPath(BagID, TEXT("CandidateValue")),
			FPropertyBindingPath(ConsiderationID, TEXT("InputFromBag")));
		Config->EditorBindings.AddBinding(
			FPropertyBindingPath(BagID, TEXT("CandidateValue")),
			FPropertyBindingPath(ConsiderationID, TEXT("PublishedScore")));
		ArcTacticsBindingTestsPrivate::MarkOutputBinding(*Config, ConsiderationID, TEXT("PublishedScore"));
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));

		FArcTacticsInstanceSlot Instance;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(16, 2), 16)));
		FArcTacticsTaskContext Context;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(AreEqual(2, Instance.Candidates.Num()));
		if (Instance.Candidates.Num() == 2)
		{
			const TValueOrError<float, EPropertyBagResult> First =
				Instance.Candidates[0].Option.ParameterBag.GetValueFloat(TEXT("CandidateValue"));
			const TValueOrError<float, EPropertyBagResult> Second =
				Instance.Candidates[1].Option.ParameterBag.GetValueFloat(TEXT("CandidateValue"));
			ASSERT_THAT(IsTrue(First.IsValid() && Second.IsValid()));
			if (First.IsValid() && Second.IsValid())
			{
				ASSERT_THAT(IsNear(0.2f, First.GetValue(), 0.001f,
					TEXT("The first candidate must publish its own bound score into its bag")));
				ASSERT_THAT(IsNear(0.8f, Second.GetValue(), 0.001f,
					TEXT("The second candidate must observe a reset bag before publishing its score")));
			}
			Instance.ActivateSelectedAction();
			ASSERT_THAT(IsTrue(Instance.ActiveAction.IsSet()));
			const TValueOrError<float, EPropertyBagResult> Active =
				Instance.ActiveAction.ParameterBag.GetValueFloat(TEXT("CandidateValue"));
			ASSERT_THAT(IsTrue(Active.IsValid()));
			if (Active.IsValid())
			{
				ASSERT_THAT(IsNear(0.8f, Active.GetValue(), 0.001f));
			}
			Instance.Candidates[0].Option.ParameterBag.SetValueFloat(TEXT("CandidateValue"), 99.0f);
			ArcTacticsSlotTestHelpers::PackageInstance(Instance).BranchParameterBags.Reset();
			ArcTacticsSlotTestHelpers::PackageInstance(Instance).NodeInstanceData.Reset();
			Instance.Candidates.Reset();
			ArcTacticsSlotTestHelpers::PackageInstance(Instance).GlobalParameters.Reset();
			ASSERT_THAT(IsTrue(Active.IsValid()));
			const TValueOrError<float, EPropertyBagResult> Durable =
				Instance.ActiveAction.ParameterBag.GetValueFloat(TEXT("CandidateValue"));
			ASSERT_THAT(IsTrue(Durable.IsValid()));
			if (Durable.IsValid())
			{
				ASSERT_THAT(IsNear(0.8f, Durable.GetValue(), 0.001f,
					TEXT("The adopted winner must survive candidate scratch and global input mutation")));
			}
		}
	}

	TEST_METHOD(BindingWithMissingPath_RejectsCompilation)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeBoundActionConfig();
		Config->EditorBindings.Reset();
		FPropertyBindingPath MissingSource(Config->ContextID, TEXT("DoesNotExist"));
		FPropertyBindingPath Target(Config->Actions[0].Action.ID,
			GET_MEMBER_NAME_CHECKED(FArcTacticsTestActionInstanceData, Entity));
		Config->EditorBindings.AddBinding(MissingSource, Target);
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;

		ASSERT_THAT(IsFalse(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics),
			TEXT("A binding path that does not resolve must fail compilation")));
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("source path is invalid")),
			TEXT("A missing source path must report the source-path diagnostic, not any other invalid-path branch")));
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("DoesNotExist")),
			TEXT("The diagnostic must name the specific missing property")));
	}

	TEST_METHOD(GlobalTaskOutputs_CopyScalarStructEntityAndActor)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeTaskActionConfig();
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		FArcTacticsInstanceSlot Instance;
		const FMassEntityHandle Entity(41, 2);
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), Entity, 3)));
		Instance.Context.Self = Entity;
		FArcTacticsTaskContext Context;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		const FArcTacticsTestActionInstanceData* ActionData =
			Instance.SelectedAction.ActionData.GetPtr<FArcTacticsTestActionInstanceData>();
		ASSERT_THAT(IsNotNull(ActionData));
		if (!ActionData)
		{
			return;
		}
		ASSERT_THAT(AreEqual(11, ActionData->Scalar));
		ASSERT_THAT(AreEqual(7, ActionData->Record.Key));
		ASSERT_THAT(AreEqual(31, ActionData->Entity.Index));
		ASSERT_THAT(AreEqual(2, ActionData->Entity.SerialNumber));
		ASSERT_THAT(IsNotNull(ActionData->Actor));
		ASSERT_THAT(AreEqual(2, ActionData->Records.Num()));
		ASSERT_THAT(AreEqual(2, ActionData->Records[1].Key,
			TEXT("Array binding must copy the complete reflected array")));
	}

	TEST_METHOD(IndexedArrayBinding_OverridesWholeArrayInEitherAuthoringOrder)
	{
		UArcTacticsConfig* Forward = ArcTacticsBindingTestsPrivate::MakeTaskActionConfig();
		const FGuid TaskID(20, 21, 22, 23);
		const FGuid ActionID(24, 25, 26, 27);
		Forward->EditorBindings.AddBinding(
			ArcTacticsBindingTestsPrivate::MakePath(TaskID, TEXT("Record.Score")),
			ArcTacticsBindingTestsPrivate::MakePath(ActionID, TEXT("Records[1].Score")));
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Forward, Diagnostics)));
		FArcTacticsInstanceSlot ForwardInstance;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(ForwardInstance, Forward->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(45, 1), 45)));
		FArcTacticsTaskContext Context;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(ForwardInstance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		const FArcTacticsTestActionInstanceData* ForwardAction =
			ForwardInstance.SelectedAction.ActionData.GetPtr<FArcTacticsTestActionInstanceData>();
		ASSERT_THAT(IsNotNull(ForwardAction));
		if (ForwardAction)
		{
			ASSERT_THAT(IsNear(0.75f, ForwardAction->Records[1].Score, 0.001f));
		}

		UArcTacticsConfig* Reverse = ArcTacticsBindingTestsPrivate::MakeTaskActionConfig();
		Reverse->EditorBindings.Reset();
		Reverse->EditorBindings.AddBinding(
			ArcTacticsBindingTestsPrivate::MakePath(TaskID, TEXT("Record.Score")),
			ArcTacticsBindingTestsPrivate::MakePath(ActionID, TEXT("Records[1].Score")));
		Reverse->EditorBindings.AddBinding(FPropertyBindingPath(TaskID, TEXT("Records")),
			FPropertyBindingPath(ActionID, TEXT("Records")));
		Reverse->EditorBindings.AddBinding(FPropertyBindingPath(TaskID, TEXT("Entity")),
			FPropertyBindingPath(ActionID, TEXT("Entity")));
		Diagnostics.Reset();
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Reverse, Diagnostics)));
		FArcTacticsInstanceSlot ReverseInstance;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(ReverseInstance, Reverse->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(46, 1), 46)));
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(ReverseInstance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		const FArcTacticsTestActionInstanceData* ReverseAction =
			ReverseInstance.SelectedAction.ActionData.GetPtr<FArcTacticsTestActionInstanceData>();
		ASSERT_THAT(IsNotNull(ReverseAction));
		if (ReverseAction)
		{
			ASSERT_THAT(IsNear(0.75f, ReverseAction->Records[1].Score, 0.001f));
		}
	}

	TEST_METHOD(TaskRunning_TickSuccessAndCancelDoNotPublishStaleCandidates)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeTaskActionConfig(1);
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		FArcTacticsInstanceSlot Instance;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(42, 1), 4)));
		FArcTacticsTaskContext Context;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Running));
		ASSERT_THAT(AreEqual(0, Instance.Candidates.Num()));
		ASSERT_THAT(IsTrue(ArcTacticsDecision::TickDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(IsTrue(Instance.Candidates.Num() > 0));

		FArcTacticsInstanceSlot Cancelled;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Cancelled, Config->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(43, 1), 5)));
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Cancelled, Context)
			== EArcTacticsDecisionStatus::Running));
		ArcTacticsDecision::CancelDecision(Cancelled, Context);
		ASSERT_THAT(IsTrue(Cancelled.DecisionStatus == EArcTacticsDecisionStatus::Cancelled));
		ASSERT_THAT(AreEqual(0, Cancelled.Candidates.Num()));
		ASSERT_THAT(AreEqual(1, ArcTacticsSlotTestHelpers::PackageInstance(Cancelled).NodeInstanceData[0].GetPtr<FArcTacticsTestTaskInstanceData>()->CancelCount));

		UArcTacticsConfig* FailedConfig = ArcTacticsBindingTestsPrivate::MakeTaskActionConfig(2);
		Diagnostics.Reset();
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*FailedConfig, Diagnostics)));
		FArcTacticsInstanceSlot Failed;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Failed, FailedConfig->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(44, 1), 6)));
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Failed, Context)
			== EArcTacticsDecisionStatus::Failed));
		ASSERT_THAT(AreEqual(0, Failed.Candidates.Num()));
		ASSERT_THAT(IsFalse(Failed.HasSelectedAction(), TEXT("Failed tasks must clear the previous selected candidate")));
	}

	TEST_METHOD(CompiledRuntime_RemainsExecutableAfterAuthoringDataIsCleared)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeTaskActionConfig();
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		Config->GlobalTasks.Reset();
		Config->Actions.Reset();
		Config->EditorBindings.Reset();
		FArcTacticsInstanceSlot Instance;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(49, 1), 8)));
		FArcTacticsTaskContext Context;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(IsTrue(Instance.HasSelectedAction(),
			TEXT("Runtime execution must use compiled tables after authoring data is unavailable")));
	}

	TEST_METHOD(Selector_EveryItemBecomesCandidateWithItsIdentityAndEmptyArraySucceeds)
	{
		const ArcTacticsBindingTestsPrivate::ESelectorKind Kinds[] = {
			ArcTacticsBindingTestsPrivate::ESelectorKind::Entity,
			ArcTacticsBindingTestsPrivate::ESelectorKind::Location,
			ArcTacticsBindingTestsPrivate::ESelectorKind::Target};
		for (const ArcTacticsBindingTestsPrivate::ESelectorKind Kind : Kinds)
		{
			UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false, false, 0.0f, false, Kind);
			TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
			ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
			FArcTacticsInstanceSlot Instance;
			ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::BeginSelectorDecision(*Config, Instance, 51)));
			ASSERT_THAT(AreEqual(2, Instance.Candidates.Num()));
			if (Instance.Candidates.Num() != 2)
			{
				return;
			}
			for (int32 ItemIndex = 0; ItemIndex < 2; ++ItemIndex)
			{
				const FArcTacticsOption& Option = Instance.Candidates[ItemIndex].Option;
				ASSERT_THAT(IsTrue(Instance.Candidates[ItemIndex].Outcome == EArcTacticsCandidateOutcome::Accepted));
				ASSERT_THAT(AreEqual(ItemIndex, Option.ItemIndex));
				const FMassEntityHandle ExpectedEntity(101 + ItemIndex, 1);
				if (Kind == ArcTacticsBindingTestsPrivate::ESelectorKind::Location)
				{
					ASSERT_THAT(IsTrue(Option.Identity.Type == EArcTacticsValueIdentity::Vector));
					ASSERT_THAT(IsTrue(Option.Identity.Vector.Equals(
						FVector(ItemIndex == 0 ? 0.2 : 0.8, ItemIndex == 0 ? 1.0 : 3.0, ItemIndex == 0 ? 2.0 : 4.0), 0.0001)));
				}
				else
				{
					ASSERT_THAT(IsTrue(Option.Identity.Type == EArcTacticsValueIdentity::Entity));
					ASSERT_THAT(IsTrue(Option.Identity.Entity == ExpectedEntity));
				}
			}

			UArcTacticsConfig* EmptyConfig = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(true, false, 0.0f, false, Kind);
			Diagnostics.Reset();
			ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*EmptyConfig, Diagnostics)));
			FArcTacticsInstanceSlot EmptyInstance;
			ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::BeginSelectorDecision(*EmptyConfig, EmptyInstance, 52)));
			ASSERT_THAT(AreEqual(0, EmptyInstance.Candidates.Num()));
			ASSERT_THAT(IsFalse(EmptyInstance.HasSelectedAction()));
		}
	}

	TEST_METHOD(Selector_ActiveSelectorBranchVetoesEveryItemButNotTheStandaloneBranch)
	{
		const ArcTacticsBindingTestsPrivate::ESelectorKind Kinds[] = {
			ArcTacticsBindingTestsPrivate::ESelectorKind::Location,
			ArcTacticsBindingTestsPrivate::ESelectorKind::Target};
		for (const ArcTacticsBindingTestsPrivate::ESelectorKind Kind : Kinds)
		{
			UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false, false, 0.0f, true, Kind);
			Config->Actions[0].Parameters.Weight = 0.5f;
			TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
			ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
			ASSERT_THAT(AreEqual(1, Config->CompiledData.Actions.Num()));
			FArcTacticsInstanceSlot Instance;
			ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::BeginSelectorDecision(*Config, Instance, 53)));
			ASSERT_THAT(IsTrue(Instance.SelectedAction.bFromSelector));
			ASSERT_THAT(AreEqual(1, Instance.SelectedAction.ItemIndex));
			Instance.ActivateSelectedAction();
			const uint32 ActiveSerial = Instance.ActiveAction.Serial;
			ASSERT_THAT(IsTrue(ActiveSerial != 0u));
			Config->CompiledData.Selectors[0].DefaultParameters.Priority = 5;
			Config->CompiledData.Actions[0].DefaultParameters.Priority = 1;
			FArcTacticsTaskContext Context;
			ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
				== EArcTacticsDecisionStatus::Succeeded));
			ASSERT_THAT(AreEqual(3, Instance.Candidates.Num()));
			for (const FArcTacticsCandidate& Candidate : Instance.Candidates)
			{
				if (Candidate.Option.bFromSelector)
				{
					ASSERT_THAT(IsTrue(Candidate.Outcome == EArcTacticsCandidateOutcome::Vetoed
						&& Candidate.RejectionReason == TEXT("BelowActivePriority"),
						TEXT("Every item of the active selector branch is vetoed even when it now has a higher priority")));
				}
				else
				{
					ASSERT_THAT(IsTrue(Candidate.Outcome == EArcTacticsCandidateOutcome::Accepted,
						TEXT("The standalone branch is not the active branch and outranks the active priority")));
				}
			}
			ASSERT_THAT(IsTrue(Instance.HasSelectedAction()));
			ASSERT_THAT(IsFalse(Instance.SelectedAction.bFromSelector));
			ASSERT_THAT(AreEqual(ActiveSerial, Instance.ActiveAction.Serial));
		}
	}

	TEST_METHOD(IncompatibleBinding_AndOutputTarget_AreRejected)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeTaskActionConfig();
		Config->EditorBindings.Reset();
		Config->EditorBindings.AddBinding(FPropertyBindingPath(FGuid(20, 21, 22, 23), TEXT("Scalar")),
			FPropertyBindingPath(FGuid(24, 25, 26, 27), TEXT("Entity")));
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsFalse(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("incompatible")),
			TEXT("Mismatched property types must report an incompatible binding diagnostic")));

		Config = ArcTacticsBindingTestsPrivate::MakeTaskActionConfig();
		Config->EditorBindings.Reset();
		Config->EditorBindings.AddBinding(FPropertyBindingPath(FGuid(20, 21, 22, 23), TEXT("Scalar")),
			FPropertyBindingPath(FGuid(24, 25, 26, 27), TEXT("OutputValue")));
		Diagnostics.Reset();
		ASSERT_THAT(IsFalse(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("may only target Input")),
			TEXT("A forward binding into an otherwise in-scope Output-role property must be rejected")));
	}

	TEST_METHOD(SelectorOutput_ReadersOutsideItsSelectorAndForwardTaskDependency_AreRejected)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false);
		const FGuid SelectorNodeID = Config->Selectors[0].Selector.ID;
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		FArcTacticsNodeDefinition Standalone;
		Standalone.ID = FGuid(60, 61, 62, 63);
		Standalone.Node.InitializeAs<FArcTacticsTestAction>();
		Standalone.InstanceData.InitializeAs<FArcTacticsTestActionInstanceData>();
		FArcTacticsActionDefinition ActionDefinition;
		ActionDefinition.Action = Standalone;
		Config->Actions.Add(ActionDefinition);
		Config->EnsureValidNodeIDs();
		Config->EditorBindings.AddBinding(FPropertyBindingPath(FGuid(20, 21, 22, 23), TEXT("Entity")),
			FPropertyBindingPath(Standalone.ID, TEXT("Entity")));
		Diagnostics.Reset();
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics),
			TEXT("The valid standalone action binding must compile before the scope violation is added")));
		Config->EditorBindings.AddBinding(FPropertyBindingPath(SelectorNodeID, TEXT("CurrentLocation")),
			FPropertyBindingPath(Standalone.ID, TEXT("Location")));
		Diagnostics.Reset();
		ASSERT_THAT(IsFalse(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("scope")),
			TEXT("A selector's current output must not bind into a standalone action")));

		UArcTacticsConfig* TaskReaderConfig = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false);
		const FGuid SecondTaskID(70, 71, 72, 73);
		FArcTacticsNodeDefinition SecondTask;
		SecondTask.ID = SecondTaskID;
		SecondTask.Node.InitializeAs<FArcTacticsTestTask>();
		SecondTask.InstanceData.InitializeAs<FArcTacticsTestTaskInstanceData>();
		TaskReaderConfig->GlobalTasks.Add(SecondTask);
		TaskReaderConfig->EditorBindings.AddBinding(FPropertyBindingPath(FGuid(20, 21, 22, 23), TEXT("Scalar")),
			FPropertyBindingPath(SecondTaskID, TEXT("Input")));
		Diagnostics.Reset();
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*TaskReaderConfig, Diagnostics),
			TEXT("A global task binding from an earlier task is the valid control")));
		TaskReaderConfig->EditorBindings.RemoveBindings(FPropertyBindingPath(SecondTaskID, TEXT("Input")));
		TaskReaderConfig->EditorBindings.AddBinding(
			ArcTacticsBindingTestsPrivate::MakePath(SelectorNodeID, TEXT("CurrentLocation.X")),
			FPropertyBindingPath(SecondTaskID, TEXT("Input")));
		Diagnostics.Reset();
		ASSERT_THAT(IsFalse(ArcTacticsBindingTestsPrivate::CompileConfig(*TaskReaderConfig, Diagnostics)));
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("scope")),
			TEXT("A global task must not read a selector's current output")));

		UArcTacticsConfig* SiblingConfig = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false);
		FArcTacticsSelectorDefinition Duplicate = SiblingConfig->Selectors[0];
		SiblingConfig->DuplicateAuthoringData(Duplicate);
		SiblingConfig->Selectors.Add(Duplicate);
		Diagnostics.Reset();
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*SiblingConfig, Diagnostics),
			TEXT("Two independent selectors are the valid control for the sibling-branch rejection")));
		SiblingConfig->EditorBindings.RemoveBindings(FPropertyBindingPath(Duplicate.Action.ID, TEXT("Location")));
		SiblingConfig->EditorBindings.AddBinding(
			FPropertyBindingPath(SiblingConfig->Selectors[0].Selector.ID, TEXT("CurrentLocation")),
			FPropertyBindingPath(Duplicate.Action.ID, TEXT("Location")));
		Diagnostics.Reset();
		ASSERT_THAT(IsFalse(ArcTacticsBindingTestsPrivate::CompileConfig(*SiblingConfig, Diagnostics)));
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("scope")),
			TEXT("Another selector's action must not read this selector's current output")));

		UArcTacticsConfig* ItemsConfig = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false);
		ItemsConfig->EditorBindings.RemoveBindings(FPropertyBindingPath(SelectorNodeID, TEXT("Items")));
		ItemsConfig->EditorBindings.AddBinding(
			FPropertyBindingPath(SelectorNodeID, TEXT("CurrentLocation")),
			FPropertyBindingPath(SelectorNodeID, TEXT("Items")));
		Diagnostics.Reset();
		ASSERT_THAT(IsFalse(ArcTacticsBindingTestsPrivate::CompileConfig(*ItemsConfig, Diagnostics)));
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("scope")),
			TEXT("A selector's Items may read only Context, Global Parameters, and completed Global Task outputs")));

		Config = ArcTacticsBindingTestsPrivate::MakeTaskActionConfig();
		Diagnostics.Reset();
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		FArcTacticsNodeDefinition ForwardTask;
		ForwardTask.ID = FGuid(70, 71, 72, 73);
		ForwardTask.Node.InitializeAs<FArcTacticsTestTask>();
		ForwardTask.InstanceData.InitializeAs<FArcTacticsTestTaskInstanceData>();
		Config->GlobalTasks.Add(ForwardTask);
		Config->EditorBindings.AddBinding(FPropertyBindingPath(FGuid(20, 21, 22, 23), TEXT("Scalar")),
			FPropertyBindingPath(FGuid(70, 71, 72, 73), TEXT("Input")));
		Diagnostics.Reset();
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics),
			TEXT("An earlier task may bind its output into a later task")));
		Config->EditorBindings.AddBinding(FPropertyBindingPath(FGuid(70, 71, 72, 73), TEXT("Scalar")),
			FPropertyBindingPath(FGuid(20, 21, 22, 23), TEXT("Input")));
		Diagnostics.Reset();
		ASSERT_THAT(IsFalse(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("scope")),
			TEXT("Reverse task dependency must report a scope or cycle diagnostic")));
	}

	TEST_METHOD(SelectionPolicies_UseSeededChoicesAndHonorVetoAndMinimum)
	{
		UArcTacticsConfig* Highest = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false, false, 0.5f);
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Highest, Diagnostics)));
		FArcTacticsInstanceSlot HighestInstance;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(HighestInstance, Highest->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(80, 1), 10)));
		FArcTacticsTaskContext Context;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(HighestInstance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(IsTrue(HighestInstance.Candidates[0].Outcome == EArcTacticsCandidateOutcome::BelowMinScore));
		ASSERT_THAT(IsTrue(HighestInstance.Candidates[1].Outcome == EArcTacticsCandidateOutcome::Accepted));
		ASSERT_THAT(AreEqual(1, HighestInstance.SelectedAction.ItemIndex));

		UArcTacticsConfig* Random = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false);
		FArcTacticsDecisionSettings RandomSettings;
		RandomSettings.Selection.Mode = EArcTacticsSelectionMode::RandomFromTopPercent;
		RandomSettings.Selection.TopPercent = 1.0f;
		Diagnostics.Reset();
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Random, Diagnostics)));
		FArcTacticsInstanceSlot RandomA;
		FArcTacticsInstanceSlot RandomB;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(RandomA, Random->CompiledData, RandomSettings, FMassEntityHandle(81, 1), 77)));
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(RandomB, Random->CompiledData, RandomSettings, FMassEntityHandle(82, 1), 77)));
		TArray<int32> RandomAKeys;
		TArray<int32> RandomBKeys;
		for (int32 Iteration = 0; Iteration < 8; ++Iteration)
		{
			ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(RandomA, Context)
				== EArcTacticsDecisionStatus::Succeeded));
			RandomAKeys.Add(RandomA.SelectedAction.ItemIndex);
			RandomA.SelectedAction.Reset();
			ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(RandomB, Context)
				== EArcTacticsDecisionStatus::Succeeded));
			RandomBKeys.Add(RandomB.SelectedAction.ItemIndex);
			RandomB.SelectedAction.Reset();
		}
		bool bSawNonMax = false;
		for (int32 Index = 0; Index < RandomAKeys.Num(); ++Index)
		{
			bSawNonMax |= RandomAKeys[Index] != 1;
			ASSERT_THAT(AreEqual(RandomAKeys[Index], RandomBKeys[Index],
				TEXT("Seeded random selection must reproduce the complete choice sequence")));
		}
		ASSERT_THAT(IsTrue(bSawNonMax, TEXT("Full-pool random selection must sometimes choose below the maximum")));

		RandomSettings.Selection.TopPercent = 0.5f;
		FArcTacticsInstanceSlot Prefix;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Prefix, Random->CompiledData, RandomSettings, FMassEntityHandle(85, 1), 80)));
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Prefix, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(AreEqual(1, Prefix.SelectedAction.ItemIndex,
			TEXT("Top-percent selection must exclude candidates below the sorted prefix")));

		RandomSettings.Selection.Mode = EArcTacticsSelectionMode::WeightedRandom;
		FArcTacticsInstanceSlot WeightedA;
		FArcTacticsInstanceSlot WeightedB;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(WeightedA, Random->CompiledData, RandomSettings, FMassEntityHandle(83, 1), 78)));
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(WeightedB, Random->CompiledData, RandomSettings, FMassEntityHandle(87, 1), 78)));
		bool bSawWeightedLower = false;
		for (int32 Iteration = 0; Iteration < 32; ++Iteration)
		{
			ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(WeightedA, Context)
				== EArcTacticsDecisionStatus::Succeeded));
			ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(WeightedB, Context)
				== EArcTacticsDecisionStatus::Succeeded));
			const int32 WeightedAKey = WeightedA.SelectedAction.ItemIndex;
			const int32 WeightedBKey = WeightedB.SelectedAction.ItemIndex;
			bSawWeightedLower |= WeightedAKey == 0;
			ASSERT_THAT(AreEqual(WeightedAKey, WeightedBKey,
				TEXT("Weighted random must reproduce seeded choices")));
			WeightedA.SelectedAction.Reset();
			WeightedB.SelectedAction.Reset();
		}
		ASSERT_THAT(IsTrue(bSawWeightedLower,
			TEXT("Weighted random must select a lower positive-score candidate over repeated seeded draws")));

		UArcTacticsConfig* Shared = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false, false, 0.0f, true);
		Diagnostics.Reset();
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Shared, Diagnostics)));
		FArcTacticsInstanceSlot SharedInstance;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(SharedInstance, Shared->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(86, 1), 81)));
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(SharedInstance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(AreEqual(3, SharedInstance.Candidates.Num(),
			TEXT("Standalone actions and selector items must enter one shared pool")));

		UArcTacticsConfig* Veto = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false, true);
		Diagnostics.Reset();
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Veto, Diagnostics)));
		FArcTacticsInstanceSlot VetoInstance;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(VetoInstance, Veto->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(84, 1), 79)));
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(VetoInstance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(IsTrue(VetoInstance.Candidates[0].Outcome == EArcTacticsCandidateOutcome::Vetoed));
		ASSERT_THAT(IsFalse(VetoInstance.HasSelectedAction()));
	}

	TEST_METHOD(BindingFailure_ResetsCandidateDataAndDoesNotLeakPriorSelection)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeTaskActionConfig();
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		FArcTacticsInstanceSlot Instance;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(90, 1), 91)));
		FArcTacticsTaskContext Context;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(IsTrue(Instance.HasSelectedAction()));
		Config->CompiledData.Bindings.ForEachMutableBinding(
			[](FPropertyBindingBinding& Binding)
			{
				const FConstStructView SourceHandleView = Binding.GetSourceDataHandleStruct();
				FArcTacticsDataHandle* SourceHandle = const_cast<FArcTacticsDataHandle*>(
					SourceHandleView.GetPtr<FArcTacticsDataHandle>());
				if (SourceHandle)
				{
					*SourceHandle = FArcTacticsDataHandle();
				}
			});
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(IsTrue(Instance.Candidates[0].Outcome == EArcTacticsCandidateOutcome::BindingFailed));
		ASSERT_THAT(IsFalse(Instance.HasSelectedAction(), TEXT("Binding failure must not publish stale prior selection")));
	}

	TEST_METHOD(SelectorItemsBindingFailure_RejectsSelectorAndDoesNotLeakPriorSelection)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false);
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		FArcTacticsInstanceSlot Instance;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::BeginSelectorDecision(*Config, Instance, 91)));
		ASSERT_THAT(AreEqual(2, Instance.Candidates.Num()));
		ASSERT_THAT(IsTrue(Instance.HasSelectedAction()));
		Config->CompiledData.Bindings.ForEachMutableBinding(
			[](FPropertyBindingBinding& Binding)
			{
				const FConstStructView SourceHandleView = Binding.GetSourceDataHandleStruct();
				FArcTacticsDataHandle* SourceHandle = const_cast<FArcTacticsDataHandle*>(
					SourceHandleView.GetPtr<FArcTacticsDataHandle>());
				if (SourceHandle)
				{
					*SourceHandle = FArcTacticsDataHandle();
				}
			});
		FArcTacticsTaskContext Context;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(AreEqual(1, Instance.Candidates.Num(),
			TEXT("A failed Items binding rejects the selector once instead of contributing item candidates")));
		if (Instance.Candidates.Num() == 1)
		{
			ASSERT_THAT(IsTrue(Instance.Candidates[0].Outcome == EArcTacticsCandidateOutcome::BindingFailed));
			ASSERT_THAT(IsTrue(Instance.Candidates[0].RejectionReason == TEXT("SelectorBindingFailed")));
		}
		ASSERT_THAT(IsFalse(Instance.HasSelectedAction(), TEXT("Binding failure must not publish stale prior selection")));
	}

	TEST_METHOD(SelectorItemBindingFailure_RejectsOnlyTheFailingItemAndKeepsTheRest)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false);
		FArcTacticsTestTaskInstanceData& Defaults =
			Config->GlobalTasks[0].InstanceData.GetMutable<FArcTacticsTestTaskInstanceData>();
		Defaults.Locations[1].X = NAN;
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		FArcTacticsInstanceSlot Instance;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::BeginSelectorDecision(*Config, Instance, 92)));
		ASSERT_THAT(AreEqual(2, Instance.Candidates.Num()));
		if (Instance.Candidates.Num() < 2)
		{
			return;
		}
		ASSERT_THAT(IsTrue(Instance.Candidates[0].Outcome == EArcTacticsCandidateOutcome::Accepted));
		ASSERT_THAT(IsTrue(Instance.Candidates[1].Outcome == EArcTacticsCandidateOutcome::BindingFailed));
		ASSERT_THAT(IsTrue(Instance.Candidates[1].RejectionReason == TEXT("InvalidConsiderationScore")));
		ASSERT_THAT(IsTrue(Instance.HasSelectedAction()));
		ASSERT_THAT(AreEqual(0, Instance.SelectedAction.ItemIndex,
			TEXT("The failing item must not displace the item that scored")));
	}

	TEST_METHOD(EntitySelector_CurrentEntityFeedsActionBagAndWinnerCapture)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(
			false, false, 0.0f, false, ArcTacticsBindingTestsPrivate::ESelectorKind::Entity);
		FArcTacticsSelectorDefinition& Selector = Config->Selectors[0];
		Selector.ParameterBag.AddProperty(TEXT("Who"), EPropertyBagPropertyType::Struct, FMassEntityHandle::StaticStruct());
		Config->EditorBindings.AddBinding(
			FPropertyBindingPath(Selector.Selector.ID, TEXT("CurrentEntity")),
			FPropertyBindingPath(Selector.ParameterBagID, TEXT("Who")));
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		FArcTacticsInstanceSlot Instance;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::BeginSelectorDecision(*Config, Instance, 61)));
		ASSERT_THAT(AreEqual(2, Instance.Candidates.Num()));
		if (Instance.Candidates.Num() != 2)
		{
			return;
		}
		for (int32 ItemIndex = 0; ItemIndex < 2; ++ItemIndex)
		{
			const FArcTacticsOption& Option = Instance.Candidates[ItemIndex].Option;
			const FMassEntityHandle ExpectedEntity(101 + ItemIndex, 1);
			const FArcTacticsTestActionInstanceData* ActionData = Option.ActionData.GetPtr<FArcTacticsTestActionInstanceData>();
			ASSERT_THAT(IsNotNull(ActionData));
			if (ActionData)
			{
				ASSERT_THAT(IsTrue(ActionData->Entity == ExpectedEntity,
					TEXT("The selector's action must read that item's CurrentEntity")));
			}
			const TValueOrError<FStructView, EPropertyBagResult> Who =
				Option.ParameterBag.GetValueStruct(TEXT("Who"), FMassEntityHandle::StaticStruct());
			ASSERT_THAT(IsTrue(Who.IsValid()));
			if (Who.IsValid())
			{
				const FMassEntityHandle* BagEntity = Who.GetValue().GetPtr<FMassEntityHandle>();
				ASSERT_THAT(IsNotNull(BagEntity));
				if (BagEntity)
				{
					ASSERT_THAT(IsTrue(*BagEntity == ExpectedEntity,
						TEXT("The selector's Parameter Bag must be bindable from CurrentEntity")));
				}
			}
		}
		ASSERT_THAT(IsTrue(Instance.HasSelectedAction()));
		ASSERT_THAT(AreEqual(0, Instance.SelectedAction.ItemIndex,
			TEXT("Equal scores fall back to authored candidate order")));
		const FArcTacticsEntitySelectorInstanceData* Captured =
			ArcTacticsBindingTestsPrivate::GetCapturedSelectorData<FArcTacticsEntitySelectorInstanceData>(
				Config->CompiledData, Instance.SelectedAction);
		ASSERT_THAT(IsNotNull(Captured));
		if (Captured)
		{
			ASSERT_THAT(IsTrue(Captured->CurrentEntity == FMassEntityHandle(101, 1),
				TEXT("The winner's captured selector data must hold the winner's item, not the last item evaluated")));
		}
	}

	TEST_METHOD(LocationSelector_CurrentLocationFeedsConsiderationWeightBagActionAndWinnerCapture)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false);
		FArcTacticsSelectorDefinition& Selector = Config->Selectors[0];
		Selector.ParameterBag.AddProperty(TEXT("CapturedX"), EPropertyBagPropertyType::Float);
		Config->EditorBindings.AddBinding(
			ArcTacticsBindingTestsPrivate::MakePath(Selector.Selector.ID, TEXT("CurrentLocation.X")),
			FPropertyBindingPath(Selector.ParameterBagID, TEXT("CapturedX")));
		Config->EditorBindings.AddBinding(
			ArcTacticsBindingTestsPrivate::MakePath(Selector.Selector.ID, TEXT("CurrentLocation.X")),
			FPropertyBindingPath(Selector.ParametersID, TEXT("Weight")));
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		FArcTacticsInstanceSlot Instance;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::BeginSelectorDecision(*Config, Instance, 62)));
		ASSERT_THAT(AreEqual(2, Instance.Candidates.Num()));
		if (Instance.Candidates.Num() != 2)
		{
			return;
		}
		const double ExpectedX[2] = {0.2, 0.8};
		const double ExpectedY[2] = {1.0, 3.0};
		for (int32 ItemIndex = 0; ItemIndex < 2; ++ItemIndex)
		{
			const FArcTacticsOption& Option = Instance.Candidates[ItemIndex].Option;
			const FArcTacticsTestActionInstanceData* ActionData = Option.ActionData.GetPtr<FArcTacticsTestActionInstanceData>();
			ASSERT_THAT(IsNotNull(ActionData));
			if (ActionData)
			{
				ASSERT_THAT(IsTrue(ActionData->Location.Equals(FVector(ExpectedX[ItemIndex], ExpectedY[ItemIndex], ItemIndex == 0 ? 2.0 : 4.0), 0.0001),
					TEXT("The selector's action must read the whole CurrentLocation")));
			}
			ASSERT_THAT(IsNear(static_cast<float>(ExpectedX[ItemIndex]), Option.Parameters.Weight, 0.001f,
				TEXT("Branch Parameters must be bindable from CurrentLocation")));
			const TValueOrError<float, EPropertyBagResult> CapturedX = Option.ParameterBag.GetValueFloat(TEXT("CapturedX"));
			ASSERT_THAT(IsTrue(CapturedX.IsValid()));
			if (CapturedX.IsValid())
			{
				ASSERT_THAT(IsNear(static_cast<float>(ExpectedX[ItemIndex]), CapturedX.GetValue(), 0.001f,
					TEXT("The selector's Parameter Bag must be bindable from CurrentLocation")));
			}
			ASSERT_THAT(IsNear(static_cast<float>(ExpectedX[ItemIndex] * ExpectedX[ItemIndex]), Option.Score, 0.001f,
				TEXT("The consideration and Weight both read CurrentLocation.X")));
		}
		ASSERT_THAT(IsTrue(Instance.HasSelectedAction()));
		ASSERT_THAT(AreEqual(1, Instance.SelectedAction.ItemIndex));
		const FArcTacticsLocationSelectorInstanceData* Captured =
			ArcTacticsBindingTestsPrivate::GetCapturedSelectorData<FArcTacticsLocationSelectorInstanceData>(
				Config->CompiledData, Instance.SelectedAction);
		ASSERT_THAT(IsNotNull(Captured));
		if (Captured)
		{
			ASSERT_THAT(IsTrue(Captured->CurrentLocation.Equals(FVector(0.8, 3.0, 4.0), 0.0001)));
		}
	}

	TEST_METHOD(TargetSelector_CurrentTargetFeedsActionAndConsideration)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(
			false, false, 0.0f, false, ArcTacticsBindingTestsPrivate::ESelectorKind::Target);
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		FArcTacticsInstanceSlot Instance;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::BeginSelectorDecision(*Config, Instance, 63)));
		ASSERT_THAT(AreEqual(2, Instance.Candidates.Num()));
		if (Instance.Candidates.Num() != 2)
		{
			return;
		}
		for (int32 ItemIndex = 0; ItemIndex < 2; ++ItemIndex)
		{
			const FArcTacticsOption& Option = Instance.Candidates[ItemIndex].Option;
			const FArcTacticsTestActionInstanceData* ActionData = Option.ActionData.GetPtr<FArcTacticsTestActionInstanceData>();
			ASSERT_THAT(IsNotNull(ActionData));
			if (ActionData)
			{
				ASSERT_THAT(IsTrue(ActionData->Entity == FMassEntityHandle(101 + ItemIndex, 1),
					TEXT("The action must read CurrentTarget.Entity")));
				ASSERT_THAT(IsTrue(ActionData->Target.Entity == FMassEntityHandle(101 + ItemIndex, 1)));
				ASSERT_THAT(IsNear(ItemIndex == 0 ? 0.2f : 0.8f, static_cast<float>(ActionData->Target.LastLocation.X), 0.001f,
					TEXT("The action must read the whole CurrentTarget including LastLocation")));
			}
			ASSERT_THAT(IsNear(ItemIndex == 0 ? 0.2f : 0.8f, Option.Score, 0.001f,
				TEXT("The consideration must read CurrentTarget.LastLocation.X")));
			ASSERT_THAT(IsTrue(Option.Identity.Type == EArcTacticsValueIdentity::Entity));
			ASSERT_THAT(IsTrue(Option.Identity.Entity == FMassEntityHandle(101 + ItemIndex, 1)));
		}
		ASSERT_THAT(AreEqual(1, Instance.SelectedAction.ItemIndex));
		const FArcTacticsTargetSelectorInstanceData* Captured =
			ArcTacticsBindingTestsPrivate::GetCapturedSelectorData<FArcTacticsTargetSelectorInstanceData>(
				Config->CompiledData, Instance.SelectedAction);
		ASSERT_THAT(IsNotNull(Captured));
		if (Captured)
		{
			ASSERT_THAT(IsTrue(Captured->CurrentTarget.Entity == FMassEntityHandle(102, 1)));
			ASSERT_THAT(IsTrue(Captured->CurrentTarget.LastLocation.Equals(FVector(0.8, 3.0, 4.0), 0.0001)));
		}
	}

	TEST_METHOD(TargetSelector_IdentityIsEntity)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(
			false, false, 0.0f, false, ArcTacticsBindingTestsPrivate::ESelectorKind::Target);
		FArcTacticsTestTaskInstanceData& TaskDefaults =
			Config->GlobalTasks[0].InstanceData.GetMutable<FArcTacticsTestTaskInstanceData>();
		TaskDefaults.Targets[1].Entity = TaskDefaults.Targets[0].Entity;
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		FArcTacticsInstanceSlot Instance;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::BeginSelectorDecision(*Config, Instance, 64)));
		ASSERT_THAT(AreEqual(2, Instance.Candidates.Num()));
		if (Instance.Candidates.Num() != 2)
		{
			return;
		}
		ASSERT_THAT(IsTrue(Instance.Candidates[0].Option.HasSameIdentity(Instance.Candidates[1].Option),
			TEXT("Two targets with the same Entity and different LastLocation share one identity")));
	}

	TEST_METHOD(SelectorItems_MissingBindingIsACompileError)
	{
		const ArcTacticsBindingTestsPrivate::ESelectorKind Kinds[] = {
			ArcTacticsBindingTestsPrivate::ESelectorKind::Entity,
			ArcTacticsBindingTestsPrivate::ESelectorKind::Location,
			ArcTacticsBindingTestsPrivate::ESelectorKind::Target};
		for (const ArcTacticsBindingTestsPrivate::ESelectorKind Kind : Kinds)
		{
			UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeSelectorConfig(false, false, 0.0f, false, Kind);
			TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
			ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics),
				TEXT("The bound selector is the valid control")));
			Config->EditorBindings.RemoveBindings(
				FPropertyBindingPath(Config->Selectors[0].Selector.ID, TEXT("Items")));
			Diagnostics.Reset();
			ASSERT_THAT(IsFalse(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
			ASSERT_THAT(IsFalse(Config->CompiledData.IsExecutable()));
			ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("Required Input 'Items'")),
				TEXT("The rejection must name the unbound Items input")));
		}
	}

	TEST_METHOD(TwoInstances_IsolateReflectedDataAndCompiledDefaults)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeTaskActionConfig();
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		FArcTacticsInstanceSlot First;
		FArcTacticsInstanceSlot Second;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(First, Config->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(92, 1), 1)));
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Second, Config->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(93, 1), 2)));
		ArcTacticsSlotTestHelpers::PackageInstance(First).NodeInstanceData[0].Get<FArcTacticsTestTaskInstanceData>().Scalar = 99;
		ASSERT_THAT(AreEqual(0, ArcTacticsSlotTestHelpers::PackageInstance(Second).NodeInstanceData[0].GetPtr<FArcTacticsTestTaskInstanceData>()->Scalar));
		ASSERT_THAT(AreEqual(0, Config->CompiledData.DefaultInstanceData[0].GetPtr<FArcTacticsTestTaskInstanceData>()->Scalar,
			TEXT("Mutating one initialized instance must not mutate shared compiled defaults")));
	}

	TEST_METHOD(RevisionChange_MakesInstanceIncompatible)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakeConfig();
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));

		FArcTacticsInstanceSlot Instance;
		const FMassEntityHandle Entity(3, 1);
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), Entity, 42)));
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::PackageInstance(Instance).IsCompatible(Config->CompiledData)));

		FArcTacticsCompiledData NewRevision = Config->CompiledData;
		NewRevision.Revision++;
		ASSERT_THAT(IsFalse(ArcTacticsSlotTestHelpers::PackageInstance(Instance).IsCompatible(NewRevision), TEXT("An instance must reject replaced compiled revisions")));
	}

	TEST_METHOD(PriorityBuckets_TopEligibleBucketWinsOverHigherScoringLowerPriority)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakePriorityConfig();
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		FArcTacticsInstanceSlot Instance;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(94, 1), 94)));
		FArcTacticsTaskContext Context;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(AreEqual(3, Instance.Candidates.Num()));
		for (const FArcTacticsCandidate& Candidate : Instance.Candidates)
		{
			ASSERT_THAT(IsTrue(Candidate.Outcome == EArcTacticsCandidateOutcome::Accepted,
				TEXT("With no active action every candidate is evaluated")));
		}
		ASSERT_THAT(IsTrue(Instance.HasSelectedAction()));
		ASSERT_THAT(AreEqual(1, Instance.SelectedAction.SourceIndex,
			TEXT("The best candidate of the top bucket wins over a higher-scoring priority 0 candidate")));

		Config->CompiledData.Actions[1].DefaultParameters.Weight = 0.05f;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		const FArcTacticsCandidate* Ineligible = ArcTacticsBindingTestsPrivate::FindActionCandidate(Instance, 1);
		ASSERT_THAT(IsNotNull(Ineligible));
		ASSERT_THAT(IsTrue(Ineligible && Ineligible->Outcome == EArcTacticsCandidateOutcome::BelowMinScore));
		ASSERT_THAT(AreEqual(2, Instance.SelectedAction.SourceIndex,
			TEXT("An ineligible top-bucket candidate leaves the bucket to its other eligible member")));

		Config->CompiledData.Actions[2].DefaultParameters.Weight = 0.05f;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(IsTrue(Instance.HasSelectedAction()));
		ASSERT_THAT(AreEqual(0, Instance.SelectedAction.SourceIndex,
			TEXT("A bucket with no eligible candidate falls through to the next priority")));
	}

	TEST_METHOD(PriorityBuckets_ActiveActionVetoesItsBranchAndAtMostEqualPriority)
	{
		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakePriorityConfig();
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		FArcTacticsInstanceSlot Instance;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(95, 1), 95)));
		FArcTacticsTaskContext Context;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(AreEqual(1, Instance.SelectedAction.SourceIndex));
		Instance.ActivateSelectedAction();
		const uint32 ActiveSerial = Instance.ActiveAction.Serial;
		ASSERT_THAT(AreEqual(1, Instance.ActiveAction.Parameters.Priority));

		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(AreEqual(3, Instance.Candidates.Num()));
		for (const FArcTacticsCandidate& Candidate : Instance.Candidates)
		{
			ASSERT_THAT(IsTrue(Candidate.Outcome == EArcTacticsCandidateOutcome::Vetoed));
			ASSERT_THAT(IsTrue(Candidate.RejectionReason == TEXT("BelowActivePriority"),
				TEXT("Lower, equal and the active branch itself are vetoed while an action runs")));
		}
		ASSERT_THAT(IsFalse(Instance.HasSelectedAction()));
		ASSERT_THAT(IsTrue(Instance.ActiveAction.IsSet(), TEXT("A decision that selects nothing keeps the active action")));
		ASSERT_THAT(AreEqual(ActiveSerial, Instance.ActiveAction.Serial));

		Config->CompiledData.Actions[0].DefaultParameters.Priority = 2;
		Config->CompiledData.Actions[1].DefaultParameters.Priority = 3;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		const FArcTacticsCandidate* Higher = ArcTacticsBindingTestsPrivate::FindActionCandidate(Instance, 0);
		const FArcTacticsCandidate* ActiveBranch = ArcTacticsBindingTestsPrivate::FindActionCandidate(Instance, 1);
		const FArcTacticsCandidate* Equal = ArcTacticsBindingTestsPrivate::FindActionCandidate(Instance, 2);
		ASSERT_THAT(IsTrue(Higher && ActiveBranch && Equal));
		if (!Higher || !ActiveBranch || !Equal)
		{
			return;
		}
		ASSERT_THAT(IsTrue(Higher->Outcome == EArcTacticsCandidateOutcome::Accepted,
			TEXT("A strictly higher priority candidate is scored while an action runs")));
		ASSERT_THAT(IsTrue(ActiveBranch->Outcome == EArcTacticsCandidateOutcome::Vetoed
			&& ActiveBranch->RejectionReason == TEXT("BelowActivePriority"),
			TEXT("The active branch is vetoed even when its priority is now the highest")));
		ASSERT_THAT(IsTrue(Equal->Outcome == EArcTacticsCandidateOutcome::Vetoed
			&& Equal->RejectionReason == TEXT("BelowActivePriority")));
		ASSERT_THAT(IsTrue(Instance.HasSelectedAction()));
		ASSERT_THAT(AreEqual(0, Instance.SelectedAction.SourceIndex));
	}

	TEST_METHOD(ComputeSelectionChances_UsesOnlyTheTopPriorityBucketAndMirrorsSelectByMode)
	{
		const TArray<float> Scores = {0.9f, 0.3f, 0.5f, 0.95f};
		const TArray<int32> Priorities = {0, 1, 1, 0};
		FArcTacticsSelectionSettings Settings;
		TArray<float> Chances;

		Settings.Mode = EArcTacticsSelectionMode::HighestScore;
		ArcTacticsDecision::ComputeSelectionChances(Scores, Priorities, Settings, Chances);
		ASSERT_THAT(AreEqual(4, Chances.Num()));
		ASSERT_THAT(IsNear(0.0f, Chances[0], 0.0001f));
		ASSERT_THAT(IsNear(0.0f, Chances[1], 0.0001f));
		ASSERT_THAT(IsNear(1.0f, Chances[2], 0.0001f));
		ASSERT_THAT(IsNear(0.0f, Chances[3], 0.0001f,
			TEXT("The highest score of a lower bucket has no chance")));

		Settings.Mode = EArcTacticsSelectionMode::WeightedRandom;
		ArcTacticsDecision::ComputeSelectionChances(Scores, Priorities, Settings, Chances);
		ASSERT_THAT(IsNear(0.0f, Chances[0], 0.0001f));
		ASSERT_THAT(IsNear(0.375f, Chances[1], 0.0001f));
		ASSERT_THAT(IsNear(0.625f, Chances[2], 0.0001f));
		ASSERT_THAT(IsNear(0.0f, Chances[3], 0.0001f));

		Settings.Mode = EArcTacticsSelectionMode::RandomFromTopPercent;
		Settings.TopPercent = 1.0f;
		ArcTacticsDecision::ComputeSelectionChances(Scores, Priorities, Settings, Chances);
		ASSERT_THAT(IsNear(0.0f, Chances[0], 0.0001f));
		ASSERT_THAT(IsNear(0.5f, Chances[1], 0.0001f));
		ASSERT_THAT(IsNear(0.5f, Chances[2], 0.0001f));
		ASSERT_THAT(IsNear(0.0f, Chances[3], 0.0001f));

		const TArray<int32> ShortPriorities = {0, 1};
		ArcTacticsDecision::ComputeSelectionChances(Scores, ShortPriorities, Settings, Chances);
		ASSERT_THAT(AreEqual(4, Chances.Num()));
		for (const float Chance : Chances)
		{
			ASSERT_THAT(IsNear(0.0f, Chance, 0.0001f, TEXT("Mismatched priorities produce no chances")));
		}

		UArcTacticsConfig* Config = ArcTacticsBindingTestsPrivate::MakePriorityConfig();
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsBindingTestsPrivate::CompileConfig(*Config, Diagnostics)));
		FArcTacticsInstanceSlot Instance;
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(96, 1), 96)));
		FArcTacticsTaskContext Context;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::BeginDecision(Instance, Context)
			== EArcTacticsDecisionStatus::Succeeded));
		ASSERT_THAT(IsTrue(Instance.HasSelectedAction()));
		TArray<float> CandidateScores;
		TArray<int32> CandidatePriorities;
		TArray<int32> CandidateSources;
		for (const FArcTacticsCandidate& Candidate : Instance.Candidates)
		{
			if (Candidate.Outcome == EArcTacticsCandidateOutcome::Accepted)
			{
				CandidateScores.Add(Candidate.Option.Score);
				CandidatePriorities.Add(Candidate.Option.Parameters.Priority);
				CandidateSources.Add(Candidate.Option.SourceIndex);
			}
		}
		Settings.Mode = Instance.Settings.Selection.Mode;
		Settings.TopPercent = Instance.Settings.Selection.TopPercent;
		ArcTacticsDecision::ComputeSelectionChances(CandidateScores, CandidatePriorities, Settings, Chances);
		const int32 SelectedPosition = CandidateSources.IndexOfByKey(Instance.SelectedAction.SourceIndex);
		ASSERT_THAT(IsTrue(Chances.IsValidIndex(SelectedPosition)));
		if (Chances.IsValidIndex(SelectedPosition))
		{
			ASSERT_THAT(IsNear(1.0f, Chances[SelectedPosition], 0.0001f,
				TEXT("ComputeSelectionChances mirrors SelectByMode's bucketed HighestScore selection")));
		}
	}
};
