#include "CQTest.h"

#if WITH_EDITOR

#include "Actions/ArcTacticsSequenceAction.h"
#include "ArcTQSPropertyFunction.h"
#include "Decision/ArcTacticsCompiler.h"
#include "Decision/ArcTacticsConfig.h"
#include "Decision/ArcTacticsDecision.h"
#include "Decision/ArcTacticsInstance.h"
#include "Decision/ArcTacticsSelectors.h"
#include "Fixtures/ArcTacticsFunctionTestTypes.h"
#include "Fixtures/ArcTacticsSlotTestHelpers.h"
#include "Fixtures/ArcTacticsTestTypes.h"
#include "Functions/ArcTQSFunctions_Vector.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Steps/ArcTacticsMemorySteps.h"
#include "TargetQuery/Functions/ArcTQSFunctions_Gameplay.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcTacticsFunctionTest_KeyA, "Test.ArcTactics.Function.KeyA");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcTacticsFunctionTest_KeyB, "Test.ArcTactics.Function.KeyB");

namespace ArcTacticsFunctionTestsPrivate
{
	const FGuid TaskID(70, 1, 0, 0);
	const FGuid ActionID(70, 2, 0, 0);
	const FGuid SelectorID(70, 3, 0, 0);
	const FGuid SelectorActionID(70, 4, 0, 0);
	const FGuid ConsiderationID(70, 5, 0, 0);
	const FGuid StepID(70, 6, 0, 0);
	const FGuid StandaloneActionID(70, 7, 0, 0);
	const FGuid SecondStepID(70, 8, 0, 0);

	UArcTacticsConfig* MakeConfig()
	{
		UArcTacticsConfig* Config = NewObject<UArcTacticsConfig>(GetTransientPackage());
		Config->ContextID = FGuid(70, 0, 0, 1);
		Config->EnsureValidNodeIDs();
		return Config;
	}

	FPropertyBindingPath MakePath(const FGuid ID, const TCHAR* Path)
	{
		FPropertyBindingPath Result(ID);
		Result.FromString(Path);
		return Result;
	}

	void Bind(UArcTacticsConfig& Config, const FGuid SourceID, const TCHAR* SourcePath, const FGuid TargetID, const TCHAR* TargetPath)
	{
		Config.EditorBindings.AddBinding(MakePath(SourceID, SourcePath), MakePath(TargetID, TargetPath));
	}

	template <typename TFunction>
	FGuid BindFunction(UArcTacticsConfig& Config, const TFunction& Function, const FGuid TargetID, const TCHAR* TargetPath)
	{
		FInstancedStruct Node;
		Node.InitializeAs<TFunction>(Function);
		const FGuid FunctionID = FGuid::NewGuid();
		Node.GetMutable<FArcTQSPropertyFunction>().ID = FunctionID;
		const FProperty* OutputProperty = ArcTQS::FindFunctionOutputProperty(TFunction::StaticStruct());
		check(OutputProperty);
		Config.EditorBindings.AddCompiledBinding(FArcTacticsBinding(
			FConstStructView(Node),
			FPropertyBindingPath(FunctionID, OutputProperty->GetFName()),
			MakePath(TargetID, TargetPath)));
		return FunctionID;
	}

	bool Compile(UArcTacticsConfig& Config, TArray<FArcTacticsCompilerDiagnostic>& Diagnostics)
	{
		Diagnostics.Reset();
		const bool bSucceeded = FArcTacticsCompiler::Compile(Config, 17, Config.CompiledData, Diagnostics);
		for (const FArcTacticsCompilerDiagnostic& Diagnostic : Diagnostics)
		{
			UE_LOGF(LogTemp, Display, "ArcTacticsFunctionTests compile diagnostic: NodeID=%ls Message=%ls",
				*Diagnostic.NodeID.ToString(), *Diagnostic.Message.ToString());
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

	const FArcTacticsBinding* FindBindingTo(const UArcTacticsConfig& Config, const FGuid TargetID, const FName TargetRoot)
	{
		return Config.EditorBindings.GetBindings().FindByPredicate([TargetID, TargetRoot](const FArcTacticsBinding& Binding)
		{
			return Binding.GetTargetPath().GetStructID() == TargetID
				&& Binding.GetTargetPath().NumSegments() > 0
				&& Binding.GetTargetPath().GetSegment(0).GetName() == TargetRoot;
		});
	}

	EArcTacticsBindingValidationResult Validate(UArcTacticsConfig& Config, const FArcTacticsBinding& Binding)
	{
		const FGuid SourceID = Binding.GetSourcePath().GetStructID();
		const FGuid TargetID = Binding.GetTargetPath().GetStructID();
		TInstancedStruct<FPropertyBindingBindableStructDescriptor> SourceDescriptor;
		TInstancedStruct<FPropertyBindingBindableStructDescriptor> TargetDescriptor;
		FArcTacticsBindingEndpoint SourceEndpoint;
		FArcTacticsBindingEndpoint TargetEndpoint;
		if (!Config.GetBindableStructByID(SourceID, SourceDescriptor)
			|| !Config.GetBindableStructByID(TargetID, TargetDescriptor)
			|| !Config.ResolveBindingEndpoint(SourceID, SourceEndpoint)
			|| !Config.ResolveBindingEndpoint(TargetID, TargetEndpoint))
		{
			return EArcTacticsBindingValidationResult::InvalidTarget;
		}
		return Config.EditorBindings.ValidateEditorBinding(
			Binding,
			SourceDescriptor.Get().Struct,
			SourceEndpoint,
			TargetDescriptor.Get().Struct,
			TargetEndpoint,
			Config.IsBindingSourceAccessible(SourceEndpoint, TargetEndpoint, Binding.IsOutputBinding()));
	}

	void RemoveBindingTo(UArcTacticsConfig& Config, const FGuid TargetID, const FName TargetRoot)
	{
		Config.EditorBindings.RemoveBindings([TargetID, TargetRoot](FPropertyBindingBinding& Binding)
		{
			return Binding.GetTargetPath().GetStructID() == TargetID
				&& Binding.GetTargetPath().NumSegments() > 0
				&& Binding.GetTargetPath().GetSegment(0).GetName() == TargetRoot;
		});
	}

	template <typename TNode, typename TData>
	FArcTacticsNodeDefinition MakeNode(const FGuid ID, const TData& Data)
	{
		FArcTacticsNodeDefinition Definition;
		Definition.ID = ID;
		Definition.Node.InitializeAs<TNode>();
		Definition.InstanceData.InitializeAs<TData>(Data);
		return Definition;
	}

	void AddTestAction(UArcTacticsConfig& Config, const FGuid ID)
	{
		FArcTacticsActionDefinition Action;
		Action.Action = MakeNode<FArcTacticsTestAction>(ID, FArcTacticsTestActionInstanceData());
		Config.Actions.Add(Action);
		Bind(Config, Config.ContextID, TEXT("Self"), ID, TEXT("Entity"));
	}

	bool RunDecision(FArcTacticsInstanceSlot& Instance, const FMassEntityHandle Entity)
	{
		FArcTacticsTaskContext Context;
		Context.Entity = Entity;
		return ArcTacticsDecision::BeginDecision(Instance, Context) == EArcTacticsDecisionStatus::Succeeded;
	}

	UArcTacticsConfig* MakeSelectorConfig(const bool bSequence)
	{
		UArcTacticsConfig* Config = MakeConfig();
		FArcTacticsTestTaskInstanceData TaskData;
		TaskData.Locations = {FVector(0.9, 5.0, 6.0), FVector(0.1, 7.0, 8.0)};
		Config->GlobalTasks.Add(MakeNode<FArcTacticsTestTask>(TaskID, TaskData));

		FArcTacticsSelectorDefinition Selector;
		Selector.Selector = MakeNode<FArcTacticsLocationSelector>(SelectorID, FArcTacticsLocationSelectorInstanceData());
		Selector.Selector.Name = TEXT("Items");
		if (bSequence)
		{
			Selector.Action = MakeNode<FArcTacticsSequenceAction>(SelectorActionID, FArcTacticsSequenceActionInstanceData());
			Selector.Steps.Add(MakeNode<FArcTacticsTestStep>(StepID, FArcTacticsTestStepInstanceData()));
		}
		else
		{
			Selector.Action = MakeNode<FArcTacticsTestAction>(SelectorActionID, FArcTacticsTestActionInstanceData());
		}
		Selector.Considerations.Add(MakeNode<FArcTacticsTestConsideration>(ConsiderationID, FArcTacticsTestConsiderationInstanceData()));
		Config->Selectors.Add(Selector);
		Config->EnsureValidNodeIDs();

		Bind(*Config, TaskID, TEXT("Locations"), SelectorID, TEXT("Items"));
		Bind(*Config, SelectorID, TEXT("CurrentLocation.X"), ConsiderationID, TEXT("BoundScore"));
		if (bSequence)
		{
			const FGuid DistanceID = BindFunction(*Config, FArcTQSFunction_VectorDistance(), StepID, TEXT("Required"));
			Bind(*Config, SelectorID, TEXT("CurrentLocation"), DistanceID, TEXT("A"));
		}
		else
		{
			Bind(*Config, Config->ContextID, TEXT("Self"), SelectorActionID, TEXT("Entity"));
			FArcTQSFunction_VectorAdd Add;
			Add.B = FVector(100.0, 0.0, 0.0);
			const FGuid AddID = BindFunction(*Config, Add, SelectorActionID, TEXT("Location"));
			Bind(*Config, SelectorID, TEXT("CurrentLocation"), AddID, TEXT("A"));
		}
		return Config;
	}

	UArcTacticsConfig* MakeMemoryStepConfig(const FGameplayTag FloatKey)
	{
		UArcTacticsConfig* Config = MakeConfig();
		FArcTacticsActionDefinition Action;
		Action.Action = MakeNode<FArcTacticsSequenceAction>(ActionID, FArcTacticsSequenceActionInstanceData());
		FArcTacticsWriteMemoryFloatStepInstanceData WriteData;
		WriteData.Key = FloatKey;
		FArcTacticsNodeDefinition Step;
		Step.ID = StepID;
		Step.Node.InitializeAs<FArcTacticsWriteMemoryFloatStep>();
		Step.InstanceData.InitializeAs<FArcTacticsWriteMemoryFloatStepInstanceData>(WriteData);
		Action.Steps.Add(Step);
		Config->Actions.Add(Action);
		Config->GlobalParameters.AddProperty(TEXT("KeyTag"), EPropertyBagPropertyType::Struct, FGameplayTag::StaticStruct());
		Config->GlobalParameters.AddProperty(TEXT("Amount"), EPropertyBagPropertyType::Float);
		AddTestAction(*Config, StandaloneActionID);
		Config->EnsureValidNodeIDs();
		return Config;
	}
}

TEST_CLASS(ArcTacticsFunctionRuntimeTests, "ArcTactics.Functions.Runtime")
{
	TEST_METHOD(NestedFunctions_ReadContextGlobalsAndTaskOutputsInPostOrderBeforeTheCopy)
	{
		using namespace ArcTacticsFunctionTestsPrivate;
		UArcTacticsConfig* Config = MakeConfig();
		FArcTacticsTestTaskInstanceData TaskData;
		TaskData.Record.Location = FVector(1.0, 2.0, 3.0);
		Config->GlobalTasks.Add(MakeNode<FArcTacticsTestTask>(TaskID, TaskData));
		Config->GlobalParameters.AddProperty(TEXT("OffsetY"), EPropertyBagPropertyType::Double);
		Config->GlobalParameters.SetValueDouble(TEXT("OffsetY"), 0.5);
		AddTestAction(*Config, ActionID);
		Config->EnsureValidNodeIDs();

		const FGuid OuterID = BindFunction(*Config, FArcTQSFunction_VectorAdd(), ActionID, TEXT("Location"));
		const FGuid ScaleID = BindFunction(*Config, FArcTQSFunction_VectorScale(), OuterID, TEXT("A"));
		Bind(*Config, TaskID, TEXT("Record.Location"), ScaleID, TEXT("Vector"));
		Bind(*Config, TaskID, TEXT("Scalar"), ScaleID, TEXT("Scale"));
		FArcTQSFunction_MakeVector MakeVector;
		MakeVector.Z = 100.0;
		const FGuid MakeID = BindFunction(*Config, MakeVector, OuterID, TEXT("B"));
		Bind(*Config, Config->ContextID, TEXT("Self.Index"), MakeID, TEXT("X"));
		Bind(*Config, Config->GlobalParametersID, TEXT("OffsetY"), MakeID, TEXT("Y"));

		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(Compile(*Config, Diagnostics), TEXT("A function tree feeding an action input must compile")));
		ASSERT_THAT(AreEqual(3, Config->CompiledData.Functions.Num()));
		ASSERT_THAT(IsTrue(Config->CompiledData.Functions[2].Function.GetScriptStruct() == FArcTQSFunction_VectorAdd::StaticStruct(),
			TEXT("The outer function is authored first but compiles after its inputs (post-order)")));

		FArcTacticsInstanceSlot Instance;
		const FMassEntityHandle Entity(8, 2);
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), Entity, 8)));
		Instance.Context.Self = Entity;
		ASSERT_THAT(IsTrue(RunDecision(Instance, Entity)));
		ASSERT_THAT(IsTrue(Instance.HasSelectedAction()));
		const FArcTacticsTestActionInstanceData* ActionData =
			Instance.SelectedAction.ActionData.GetPtr<FArcTacticsTestActionInstanceData>();
		ASSERT_THAT(IsNotNull(ActionData));
		if (!ActionData)
		{
			return;
		}
		ASSERT_THAT(IsNear(19.0, ActionData->Location.X, 0.001,
			TEXT("X = Record.Location.X * Scalar (11, set only by the task's Enter) + Context.Self.Index (8)")));
		ASSERT_THAT(IsNear(22.5, ActionData->Location.Y, 0.001,
			TEXT("Y = Record.Location.Y * 11 + GlobalParameters.OffsetY")));
		ASSERT_THAT(IsNear(133.0, ActionData->Location.Z, 0.001,
			TEXT("Z = Record.Location.Z * 11 + the unbound literal Z of Make Vector")));
	}

	TEST_METHOD(Function_ResetsToItsCompiledDefaultBeforeEveryEvaluation)
	{
		using namespace ArcTacticsFunctionTestsPrivate;
		UArcTacticsConfig* Config = MakeConfig();
		AddTestAction(*Config, ActionID);
		Config->EnsureValidNodeIDs();
		BindFunction(*Config, FArcTacticsTestAccumulateFunction(), ActionID, TEXT("NestedValue"));
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(Compile(*Config, Diagnostics)));

		FArcTacticsInstanceSlot Instance;
		const FMassEntityHandle Entity(9, 2);
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), Entity, 9)));
		Instance.Context.Self = Entity;
		ASSERT_THAT(IsTrue(RunDecision(Instance, Entity)));
		const FArcTacticsTestActionInstanceData* FirstData =
			Instance.SelectedAction.ActionData.GetPtr<FArcTacticsTestActionInstanceData>();
		ASSERT_THAT(IsNotNull(FirstData));
		if (!FirstData)
		{
			return;
		}
		ASSERT_THAT(IsNear(2.5f, FirstData->NestedValue, 0.001f));

		ASSERT_THAT(IsTrue(RunDecision(Instance, Entity)));
		const FArcTacticsTestActionInstanceData* SecondData =
			Instance.SelectedAction.ActionData.GetPtr<FArcTacticsTestActionInstanceData>();
		ASSERT_THAT(IsNotNull(SecondData));
		if (!SecondData)
		{
			return;
		}
		ASSERT_THAT(IsNear(2.5f, SecondData->NestedValue, 0.001f,
			TEXT("The function accumulates into Result, so a second evaluation without the reset would give 5")));

		Instance.ActivateSelectedAction();
		ASSERT_THAT(IsTrue(Instance.ActiveAction.IsSet()));
		FArcTacticsTestActionInstanceData* ActiveData =
			Instance.ActiveAction.ActionData.GetMutablePtr<FArcTacticsTestActionInstanceData>();
		ASSERT_THAT(IsNotNull(ActiveData));
		if (!ActiveData)
		{
			return;
		}
		ActiveData->NestedValue = -1.0f;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::ApplyActiveActionInputs(
			Instance, ArcTacticsSlotTestHelpers::Package(Instance), FArcTQSFunctionContext())));
		ASSERT_THAT(IsNear(2.5f, ActiveData->NestedValue, 0.001f,
			TEXT("Re-applying active inputs evaluates the function again from its default")));
	}

	TEST_METHOD(SelectorAction_FunctionInActiveBindingsReadsTheSelectedItemNotTheLastScored)
	{
		using namespace ArcTacticsFunctionTestsPrivate;
		UArcTacticsConfig* Config = MakeSelectorConfig(false);
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(Compile(*Config, Diagnostics)));

		FArcTacticsInstanceSlot Instance;
		const FMassEntityHandle Entity(10, 2);
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), Entity, 10)));
		Instance.Context.Self = Entity;
		ASSERT_THAT(IsTrue(RunDecision(Instance, Entity)));
		ASSERT_THAT(AreEqual(2, Instance.Candidates.Num()));
		ASSERT_THAT(IsTrue(Instance.HasSelectedAction()));
		const FArcTacticsTestActionInstanceData* SelectedData =
			Instance.SelectedAction.ActionData.GetPtr<FArcTacticsTestActionInstanceData>();
		ASSERT_THAT(IsNotNull(SelectedData));
		if (!SelectedData)
		{
			return;
		}
		ASSERT_THAT(IsTrue(SelectedData->Location.Equals(FVector(100.9, 5.0, 6.0), 0.001),
			TEXT("The first item scores higher and wins; the second item is the last one scored")));

		Instance.ActivateSelectedAction();
		FArcTacticsTestActionInstanceData* ActiveData =
			Instance.ActiveAction.ActionData.GetMutablePtr<FArcTacticsTestActionInstanceData>();
		ASSERT_THAT(IsNotNull(ActiveData));
		if (!ActiveData)
		{
			return;
		}
		ActiveData->Location = FVector::ZeroVector;
		ASSERT_THAT(IsTrue(ArcTacticsDecision::ApplyActiveActionInputs(
			Instance, ArcTacticsSlotTestHelpers::Package(Instance), FArcTQSFunctionContext())));
		ASSERT_THAT(IsTrue(ActiveData->Location.Equals(FVector(100.9, 5.0, 6.0), 0.001),
			*FString::Printf(TEXT("The function input reads the winner's captured Current, got %s"), *ActiveData->Location.ToString())));
	}

	TEST_METHOD(SelectorStep_FunctionInActiveStepBindingsReadsTheSelectedItemNotTheLastScored)
	{
		using namespace ArcTacticsFunctionTestsPrivate;
		UArcTacticsConfig* Config = MakeSelectorConfig(true);
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(Compile(*Config, Diagnostics), TEXT("A function satisfies the step's required input")));

		FArcTacticsInstanceSlot Instance;
		const FMassEntityHandle Entity(11, 2);
		ASSERT_THAT(IsTrue(ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config->CompiledData, FArcTacticsDecisionSettings(), Entity, 11)));
		Instance.Context.Self = Entity;
		ASSERT_THAT(IsTrue(RunDecision(Instance, Entity)));
		ASSERT_THAT(IsTrue(Instance.HasSelectedAction()));
		const double WinnerLength = FVector(0.9, 5.0, 6.0).Length();
		Instance.ActivateSelectedAction();
		ASSERT_THAT(AreEqual(1, Instance.ActiveAction.StepData.Num()));
		if (Instance.ActiveAction.StepData.Num() != 1)
		{
			return;
		}
		FArcTacticsTestStepInstanceData* StepData =
			Instance.ActiveAction.StepData[0].GetPtr<FArcTacticsTestStepInstanceData>();
		ASSERT_THAT(IsNotNull(StepData));
		if (!StepData)
		{
			return;
		}
		ASSERT_THAT(IsNear(WinnerLength, static_cast<double>(StepData->Required), 0.001));
		StepData->Required = -1.0f;
		const FArcTacticsStepAccess Steps(
			Config->CompiledData, Instance.ActiveAction, &ArcTacticsSlotTestHelpers::PackageInstance(Instance), FArcTQSFunctionContext());
		ASSERT_THAT(IsTrue(Steps.ApplyStepInputs(0)));
		ASSERT_THAT(IsNear(WinnerLength, static_cast<double>(StepData->Required), 0.001,
			*FString::Printf(TEXT("The step's function reads the winner's Current (%.4f), not the last item (%.4f); got %.4f"),
				WinnerLength, FVector(0.1, 7.0, 8.0).Length(), StepData->Required)));
	}
};

TEST_CLASS(ArcTacticsFunctionRuleTests, "ArcTactics.Functions.Rules")
{
	TEST_METHOD(FunctionInputFromInaccessibleSource_IsRejectedByValidatorAndCompiler)
	{
		using namespace ArcTacticsFunctionTestsPrivate;
		UArcTacticsConfig* Config = MakeSelectorConfig(false);
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(Compile(*Config, Diagnostics), TEXT("Control: the selector's own action function reads Current")));
		const FArcTacticsBinding* OwnFunction = FindBindingTo(*Config, SelectorActionID, TEXT("Location"));
		ASSERT_THAT(IsNotNull(OwnFunction));
		if (!OwnFunction)
		{
			return;
		}
		const FGuid OwnFunctionID = OwnFunction->GetSourcePath().GetStructID();
		ASSERT_THAT(IsTrue(Config->IsBindingSourceAccessible(SelectorID, OwnFunctionID)));

		AddTestAction(*Config, StandaloneActionID);
		Config->EnsureValidNodeIDs();
		const FGuid ForeignFunctionID = BindFunction(*Config, FArcTQSFunction_VectorAdd(), StandaloneActionID, TEXT("Location"));
		Bind(*Config, SelectorID, TEXT("CurrentLocation"), ForeignFunctionID, TEXT("A"));
		ASSERT_THAT(IsFalse(Config->IsBindingSourceAccessible(SelectorID, ForeignFunctionID),
			TEXT("A function rooted at a standalone action takes that action's scope")));
		const FArcTacticsBinding* ForeignInput = FindBindingTo(*Config, ForeignFunctionID, TEXT("A"));
		ASSERT_THAT(IsNotNull(ForeignInput));
		if (ForeignInput)
		{
			ASSERT_THAT(IsTrue(Validate(*Config, *ForeignInput) == EArcTacticsBindingValidationResult::InvalidScope));
		}
		ASSERT_THAT(IsFalse(Compile(*Config, Diagnostics)));
		ASSERT_THAT(IsTrue(HasDiagnosticContaining(Diagnostics, TEXT("scope"))));
		ASSERT_THAT(IsFalse(Config->CompiledData.IsExecutable()));
	}

	TEST_METHOD(OutputBinding_NeverAcceptsAFunction)
	{
		using namespace ArcTacticsFunctionTestsPrivate;
		UArcTacticsConfig* Config = MakeConfig();
		AddTestAction(*Config, ActionID);
		Config->Actions[0].ParameterBag.AddProperty(TEXT("Published"), EPropertyBagPropertyType::Float);
		Config->EnsureValidNodeIDs();
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(Compile(*Config, Diagnostics), TEXT("Control: the config compiles without the function")));

		const FGuid FunctionID = BindFunction(*Config, FArcTacticsTestAccumulateFunction(), ActionID, TEXT("OutputValue"));
		Config->EditorBindings.ForEachMutableBinding([FunctionID](FPropertyBindingBinding& Binding)
		{
			if (Binding.GetSourcePath().GetStructID() == FunctionID)
			{
				static_cast<FArcTacticsBinding&>(Binding).SetIsOutputBinding(true);
			}
		});
		ASSERT_THAT(IsFalse(Config->IsBindingSourceAccessible(FunctionID, ActionID, true)));
		ASSERT_THAT(IsFalse(Compile(*Config, Diagnostics)));
		ASSERT_THAT(IsTrue(HasDiagnosticContaining(Diagnostics, TEXT("output binding cannot use a function"))));

		Config->RefreshEditorBindingData();
		ASSERT_THAT(IsFalse(Config->EditorBindings.GetBindings().ContainsByPredicate([](const FArcTacticsBinding& Binding)
		{
			return Binding.GetPropertyFunctionNode().IsValid();
		}), TEXT("The binding refresh prunes an output binding that carries a function")));
	}

	TEST_METHOD(MemoryKey_BindingIntoAStepOrFunctionKeyIsRejectedByValidatorAndCompiler)
	{
		using namespace ArcTacticsFunctionTestsPrivate;
		UArcTacticsConfig* Config = MakeMemoryStepConfig(TAG_ArcTacticsFunctionTest_KeyA);
		FArcTQSFunction_MemoryFloat Read;
		Read.Key = TAG_ArcTacticsFunctionTest_KeyA;
		const FGuid ReadID = BindFunction(*Config, Read, StandaloneActionID, TEXT("NestedValue"));
		Bind(*Config, Config->GlobalParametersID, TEXT("Amount"), ReadID, TEXT("Fallback"));
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(Compile(*Config, Diagnostics), TEXT("Control: literal keys and a bound Fallback compile")));
		const FArcTacticsBinding* FallbackInput = FindBindingTo(*Config, ReadID, TEXT("Fallback"));
		ASSERT_THAT(IsNotNull(FallbackInput));
		if (FallbackInput)
		{
			ASSERT_THAT(IsTrue(Validate(*Config, *FallbackInput) == EArcTacticsBindingValidationResult::Valid));
		}

		Bind(*Config, Config->GlobalParametersID, TEXT("KeyTag"), StepID, TEXT("Key"));
		const FArcTacticsBinding* StepKey = FindBindingTo(*Config, StepID, TEXT("Key"));
		ASSERT_THAT(IsNotNull(StepKey));
		if (StepKey)
		{
			ASSERT_THAT(IsTrue(Validate(*Config, *StepKey) == EArcTacticsBindingValidationResult::LiteralOnlyTarget));
		}
		ASSERT_THAT(IsFalse(Compile(*Config, Diagnostics), TEXT("A binding into a write step's Key must not compile")));
		ASSERT_THAT(IsTrue(HasDiagnosticContaining(Diagnostics, TEXT("memory key"))));

		RemoveBindingTo(*Config, StepID, TEXT("Key"));
		ASSERT_THAT(IsTrue(Compile(*Config, Diagnostics)));
		Bind(*Config, Config->GlobalParametersID, TEXT("KeyTag"), ReadID, TEXT("Key"));
		const FArcTacticsBinding* FunctionKey = FindBindingTo(*Config, ReadID, TEXT("Key"));
		ASSERT_THAT(IsNotNull(FunctionKey));
		if (FunctionKey)
		{
			ASSERT_THAT(IsTrue(Validate(*Config, *FunctionKey) == EArcTacticsBindingValidationResult::LiteralOnlyTarget));
		}
		ASSERT_THAT(IsFalse(Compile(*Config, Diagnostics), TEXT("A binding into a read function's Key must not compile")));
		ASSERT_THAT(IsTrue(HasDiagnosticContaining(Diagnostics, TEXT("memory key"))));
	}

	TEST_METHOD(MemoryKey_OneKeyWithTwoTypesInOneConfigIsACompileError)
	{
		using namespace ArcTacticsFunctionTestsPrivate;
		UArcTacticsConfig* Config = MakeMemoryStepConfig(TAG_ArcTacticsFunctionTest_KeyB);
		FArcTQSFunction_MemoryFloat SameType;
		SameType.Key = TAG_ArcTacticsFunctionTest_KeyB;
		BindFunction(*Config, SameType, StandaloneActionID, TEXT("NestedValue"));
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(Compile(*Config, Diagnostics), TEXT("Control: a Float step and a Float read share the key")));

		FArcTQSFunction_MemoryLocation OtherType;
		OtherType.Key = TAG_ArcTacticsFunctionTest_KeyB;
		BindFunction(*Config, OtherType, StandaloneActionID, TEXT("Location"));
		ASSERT_THAT(IsFalse(Compile(*Config, Diagnostics), TEXT("A Vector read of a key the config writes as Float must not compile")));
		ASSERT_THAT(IsTrue(HasDiagnosticContaining(Diagnostics, TEXT("Test.ArcTactics.Function.KeyB"))));
		ASSERT_THAT(IsTrue(HasDiagnosticContaining(Diagnostics, TEXT("is used as"))));

		RemoveBindingTo(*Config, StandaloneActionID, TEXT("Location"));
		ASSERT_THAT(IsTrue(Compile(*Config, Diagnostics)));
		FArcTacticsWriteMemoryLocationStepInstanceData LocationData;
		LocationData.Key = TAG_ArcTacticsFunctionTest_KeyB;
		FArcTacticsNodeDefinition LocationStep;
		LocationStep.ID = SecondStepID;
		LocationStep.Node.InitializeAs<FArcTacticsWriteMemoryLocationStep>();
		LocationStep.InstanceData.InitializeAs<FArcTacticsWriteMemoryLocationStepInstanceData>(LocationData);
		Config->Actions[0].Steps.Add(LocationStep);
		ASSERT_THAT(IsFalse(Compile(*Config, Diagnostics), TEXT("Two write steps with one key and two types must not compile")));
		ASSERT_THAT(IsTrue(HasDiagnosticContaining(Diagnostics, TEXT("is used as"))));
	}
};

#endif
