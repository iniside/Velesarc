#include "CQTest.h"

#if WITH_EDITOR

#include "Actions/ArcTacticsSequenceAction.h"
#include "ArcTacticsEditorClipboard.h"
#include "ArcTacticsEditorViewModel.h"
#include "Decision/ArcTacticsCompiler.h"
#include "Decision/ArcTacticsConfig.h"
#include "Decision/ArcTacticsDecision.h"
#include "Decision/ArcTacticsInstance.h"
#include "Fixtures/ArcTacticsSlotTestHelpers.h"
#include "Decision/ArcTacticsSelectors.h"
#include "Fixtures/ArcTacticsTestTypes.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Mass/ArcTacticsOption.h"
#include "Steps/ArcTacticsMoveToStep.h"
#include "Steps/ArcTacticsUseAbilityFocusStep.h"
#include "Steps/ArcTacticsUseAbilityStep.h"

namespace ArcTacticsStepTestsPrivate
{
	const FGuid ItemTaskID(20, 21, 22, 23);
	const FGuid SelectorNodeID(30, 31, 32, 33);
	const FGuid ConsiderationID(42, 43, 44, 45);

	UArcTacticsConfig* MakeConfig()
	{
		UArcTacticsConfig* Config = NewObject<UArcTacticsConfig>(GetTransientPackage(), NAME_None, RF_Transactional);
		Config->ContextID = FGuid(101, 102, 103, 104);
		Config->EnsureValidNodeIDs();
		return Config;
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
			if (Path.GetSegment(SegmentIndex).GetName() != Expected.GetSegment(SegmentIndex).GetName()
				|| Path.GetSegment(SegmentIndex).GetArrayIndex() != Expected.GetSegment(SegmentIndex).GetArrayIndex())
			{
				return false;
			}
		}
		return true;
	}

	void Bind(UArcTacticsConfig& Config, const FGuid SourceID, const TCHAR* SourcePath, const FGuid TargetID, const TCHAR* TargetPath, const bool bOutput = false)
	{
		Config.EditorBindings.AddBinding(MakePath(SourceID, SourcePath), MakePath(TargetID, TargetPath));
		if (bOutput)
		{
			Config.EditorBindings.ForEachMutableBinding(
				[TargetID, TargetPath](FPropertyBindingBinding& Binding)
				{
					if (PathMatches(Binding.GetTargetPath(), TargetID, TargetPath))
					{
						static_cast<FArcTacticsBinding&>(Binding).SetIsOutputBinding(true);
					}
				});
		}
	}

	bool HasBinding(const UArcTacticsConfig& Config, const FGuid SourceID, const TCHAR* SourcePath, const FGuid TargetID, const TCHAR* TargetPath, const bool bOutput)
	{
		for (const FArcTacticsBinding& Binding : Config.EditorBindings.GetBindings())
		{
			if (PathMatches(Binding.GetSourcePath(), SourceID, SourcePath)
				&& PathMatches(Binding.GetTargetPath(), TargetID, TargetPath)
				&& Binding.IsOutputBinding() == bOutput)
			{
				return true;
			}
		}
		return false;
	}

	int32 CountBindingsTargeting(const UArcTacticsConfig& Config, const FGuid TargetID)
	{
		int32 Count = 0;
		for (const FArcTacticsBinding& Binding : Config.EditorBindings.GetBindings())
		{
			if (Binding.GetTargetPath().GetStructID() == TargetID)
			{
				++Count;
			}
		}
		return Count;
	}

	bool CompileConfig(UArcTacticsConfig& Config, TArray<FArcTacticsCompilerDiagnostic>& Diagnostics)
	{
		Diagnostics.Reset();
		return FArcTacticsCompiler::Compile(Config, 17, Config.CompiledData, Diagnostics);
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

	bool HasDiagnosticFor(const TArray<FArcTacticsCompilerDiagnostic>& Diagnostics, const FGuid NodeID, const TCHAR* Needle)
	{
		for (const FArcTacticsCompilerDiagnostic& Diagnostic : Diagnostics)
		{
			if (Diagnostic.NodeID == NodeID && Diagnostic.Message.ToString().Contains(Needle, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	FArcTacticsNodeDefinition MakeMoveStep()
	{
		FArcTacticsNodeDefinition Step;
		Step.Node.InitializeAs<FArcTacticsMoveToStep>();
		Step.InstanceData.InitializeAs<FArcTacticsMoveToStepInstanceData>();
		return Step;
	}

	FArcTacticsNodeDefinition MakeFixtureStep()
	{
		FArcTacticsNodeDefinition Step;
		Step.Node.InitializeAs<FArcTacticsTestStep>();
		Step.InstanceData.InitializeAs<FArcTacticsTestStepInstanceData>();
		return Step;
	}

	FArcTacticsActionDefinition MakeSequenceAction(const TArray<FArcTacticsNodeDefinition>& Steps)
	{
		FArcTacticsActionDefinition Definition;
		Definition.Action.Node.InitializeAs<FArcTacticsSequenceAction>();
		Definition.Action.InstanceData.InitializeAs<FArcTacticsSequenceActionInstanceData>();
		Definition.Steps = Steps;
		return Definition;
	}

	UArcTacticsConfig* MakeSelectorConfig(const bool bSequenceAction, const TArray<FArcTacticsNodeDefinition>& Steps)
	{
		UArcTacticsConfig* Config = MakeConfig();
		FArcTacticsNodeDefinition Task;
		Task.ID = ItemTaskID;
		Task.Node.InitializeAs<FArcTacticsTestTask>();
		FArcTacticsTestTaskInstanceData TaskData;
		TaskData.Locations = {FVector(0.2, 1.0, 2.0), FVector(0.8, 3.0, 4.0)};
		Task.InstanceData.InitializeAs<FArcTacticsTestTaskInstanceData>(TaskData);
		Config->GlobalTasks.Add(Task);

		FArcTacticsSelectorDefinition Selector;
		Selector.Selector.ID = SelectorNodeID;
		Selector.Selector.Name = TEXT("Items");
		Selector.Selector.Node.InitializeAs<FArcTacticsLocationSelector>();
		Selector.Selector.InstanceData.InitializeAs<FArcTacticsLocationSelectorInstanceData>();
		if (bSequenceAction)
		{
			Selector.Action.Node.InitializeAs<FArcTacticsSequenceAction>();
			Selector.Action.InstanceData.InitializeAs<FArcTacticsSequenceActionInstanceData>();
		}
		else
		{
			Selector.Action.Node.InitializeAs<FArcTacticsTestAction>();
			Selector.Action.InstanceData.InitializeAs<FArcTacticsTestActionInstanceData>();
		}
		Selector.Steps = Steps;
		FArcTacticsNodeDefinition Consideration;
		Consideration.ID = ConsiderationID;
		Consideration.Node.InitializeAs<FArcTacticsTestConsideration>();
		Consideration.InstanceData.InitializeAs<FArcTacticsTestConsiderationInstanceData>();
		Selector.Considerations.Add(Consideration);
		Selector.ParameterBag.AddProperty(TEXT("Reach"), EPropertyBagPropertyType::Float);
		Selector.ParameterBag.SetValueFloat(TEXT("Reach"), 300.0f);
		Config->Selectors.Add(Selector);

		Bind(*Config, ItemTaskID, TEXT("Locations"), SelectorNodeID, TEXT("Items"));
		Bind(*Config, SelectorNodeID, TEXT("CurrentLocation.X"), ConsiderationID, TEXT("BoundScore"));
		Config->EnsureValidNodeIDs();
		if (!bSequenceAction)
		{
			Bind(*Config, Config->ContextID, TEXT("Self"), Config->Selectors[0].Action.ID, TEXT("Entity"));
		}
		return Config;
	}

	bool BeginSelectorDecision(UArcTacticsConfig& Config, FArcTacticsInstanceSlot& Instance)
	{
		FArcTacticsTaskContext Context;
		return ArcTacticsSlotTestHelpers::InitializeSlot(Instance, Config.CompiledData, FArcTacticsDecisionSettings(), FMassEntityHandle(41, 1), 41)
			&& ArcTacticsDecision::BeginDecision(Instance, Context)
				== EArcTacticsDecisionStatus::Succeeded;
	}

	class FClipboardRestore final
	{
	public:
		FClipboardRestore()
		{
			FPlatformApplicationMisc::ClipboardPaste(PreviousText);
		}

		~FClipboardRestore()
		{
			FPlatformApplicationMisc::ClipboardCopy(*PreviousText);
		}

	private:
		FString PreviousText;
	};

	FArcTacticsEditorSelection StepSelection(const FGuid ID)
	{
		return {EArcTacticsEditorItemKind::Step, ID};
	}

	FArcTacticsEditorSelection ActionSelection(const FGuid ID)
	{
		return {EArcTacticsEditorItemKind::Action, ID};
	}
}

TEST_CLASS(ArcTacticsStepAuthoringTests, "ArcTactics.StepAuthoring")
{
	TEST_METHOD(AddStep_AssignsStableIdsInOrderAndRejectsWrongParentAndRole)
	{
		UArcTacticsConfig* Config = ArcTacticsStepTestsPrivate::MakeConfig();
		const TSharedRef<FArcTacticsEditorViewModel> ViewModel = FArcTacticsEditorViewModel::Create(Config);
		ASSERT_THAT(IsTrue(ViewModel->Add(
			EArcTacticsEditorItemKind::Action, FArcTacticsEditorSelection::Config(), FArcTacticsSequenceAction::StaticStruct())));
		ASSERT_THAT(AreEqual(1, Config->Actions.Num()));
		const FGuid ActionID = Config->Actions[0].Action.ID;
		const FArcTacticsEditorSelection ActionSel = ArcTacticsStepTestsPrivate::ActionSelection(ActionID);

		ASSERT_THAT(IsFalse(ViewModel->CanAdd(
			EArcTacticsEditorItemKind::Step, FArcTacticsEditorSelection::Config(), FArcTacticsMoveToStep::StaticStruct()),
			TEXT("A step cannot be added at the config root")));
		ASSERT_THAT(IsFalse(ViewModel->CanAdd(
			EArcTacticsEditorItemKind::Step, ActionSel, FArcTacticsTestAction::StaticStruct()),
			TEXT("An action type has the wrong role for a step slot")));
		ASSERT_THAT(IsFalse(ViewModel->CanAdd(
			EArcTacticsEditorItemKind::Step, ActionSel, FArcTacticsStepNode::StaticStruct()),
			TEXT("The role base type is not an authorable step")));
		ASSERT_THAT(IsFalse(ViewModel->CanAdd(
			EArcTacticsEditorItemKind::Action, ActionSel, FArcTacticsMoveToStep::StaticStruct()),
			TEXT("A step type has the wrong role for an action slot")));

		ASSERT_THAT(IsTrue(ViewModel->Add(EArcTacticsEditorItemKind::Step, ActionSel, FArcTacticsMoveToStep::StaticStruct())));
		ASSERT_THAT(IsTrue(ViewModel->Add(EArcTacticsEditorItemKind::Step, ActionSel, FArcTacticsUseAbilityStep::StaticStruct())));
		ASSERT_THAT(AreEqual(2, Config->Actions[0].Steps.Num()));
		if (Config->Actions[0].Steps.Num() < 2)
		{
			return;
		}
		const FGuid FirstID = Config->Actions[0].Steps[0].ID;
		const FGuid SecondID = Config->Actions[0].Steps[1].ID;
		ASSERT_THAT(IsTrue(FirstID.IsValid() && SecondID.IsValid()));
		ASSERT_THAT(IsFalse(FirstID == SecondID));
		ASSERT_THAT(IsFalse(FirstID == ActionID || SecondID == ActionID));
		ASSERT_THAT(IsTrue(Config->Actions[0].Steps[0].Node.GetScriptStruct() == FArcTacticsMoveToStep::StaticStruct()));
		ASSERT_THAT(IsTrue(Config->Actions[0].Steps[1].Node.GetScriptStruct() == FArcTacticsUseAbilityStep::StaticStruct()));

		ASSERT_THAT(IsTrue(ViewModel->Add(
			EArcTacticsEditorItemKind::Step, ArcTacticsStepTestsPrivate::StepSelection(FirstID), FArcTacticsUseAbilityFocusStep::StaticStruct())));
		ASSERT_THAT(AreEqual(3, Config->Actions[0].Steps.Num()));
		ASSERT_THAT(IsTrue(Config->Actions[0].Steps[1].Node.GetScriptStruct() == FArcTacticsUseAbilityFocusStep::StaticStruct(),
			TEXT("Adding with a step selected inserts right after it")));
		ASSERT_THAT(IsTrue(Config->Actions[0].Steps[2].ID == SecondID));

		FArcTacticsEditorSelection Found;
		ASSERT_THAT(IsTrue(ViewModel->FindSelectionForStructID(FirstID, Found)));
		ASSERT_THAT(IsTrue(Found.Kind == EArcTacticsEditorItemKind::Step && Found.ID == FirstID));

		ASSERT_THAT(IsTrue(ViewModel->Compile()));
		const FArcTacticsCompiledData& Compiled = Config->CompiledData;
		ASSERT_THAT(AreEqual(1, Compiled.Actions.Num()));
		if (Compiled.Actions.Num() < 1)
		{
			return;
		}
		ASSERT_THAT(AreEqual(3, Compiled.Actions[0].Steps.Num()));
		for (int32 StepIndex = 0; StepIndex < Compiled.Actions[0].Steps.Num(); ++StepIndex)
		{
			const int32 NodeIndex = Compiled.Actions[0].Steps[StepIndex].Get();
			ASSERT_THAT(IsTrue(Compiled.Nodes.IsValidIndex(NodeIndex)));
			if (!Compiled.Nodes.IsValidIndex(NodeIndex))
			{
				return;
			}
			ASSERT_THAT(IsTrue(Compiled.Nodes[NodeIndex].Kind == EArcTacticsCompiledNodeKind::Step));
			ASSERT_THAT(AreEqual(1 + StepIndex, Compiled.Nodes[NodeIndex].ExecutionIndex));
		}
	}

	TEST_METHOD(DeleteStep_RemovesItsBindingsAndInvalidatesCompile)
	{
		UArcTacticsConfig* Config = ArcTacticsStepTestsPrivate::MakeConfig();
		Config->Actions.Add(ArcTacticsStepTestsPrivate::MakeSequenceAction({ArcTacticsStepTestsPrivate::MakeFixtureStep(), ArcTacticsStepTestsPrivate::MakeFixtureStep()}));
		Config->Actions[0].ParameterBag.AddProperty(TEXT("Reach"), EPropertyBagPropertyType::Float);
		Config->Actions[0].ParameterBag.AddProperty(TEXT("Published"), EPropertyBagPropertyType::Float);
		Config->EnsureValidNodeIDs();
		const FGuid BagID = Config->Actions[0].ParameterBagID;
		const FGuid FirstID = Config->Actions[0].Steps[0].ID;
		const FGuid SecondID = Config->Actions[0].Steps[1].ID;
		ArcTacticsStepTestsPrivate::Bind(*Config, BagID, TEXT("Reach"), FirstID, TEXT("Required"));
		ArcTacticsStepTestsPrivate::Bind(*Config, BagID, TEXT("Published"), FirstID, TEXT("Result"), true);
		ArcTacticsStepTestsPrivate::Bind(*Config, BagID, TEXT("Reach"), SecondID, TEXT("Required"));
		const TSharedRef<FArcTacticsEditorViewModel> ViewModel = FArcTacticsEditorViewModel::Create(Config);
		ASSERT_THAT(IsTrue(ViewModel->Compile()));
		ASSERT_THAT(IsTrue(ViewModel->IsCompiledArtifactValid()));

		ASSERT_THAT(IsTrue(ViewModel->Delete(ArcTacticsStepTestsPrivate::StepSelection(FirstID))));
		ASSERT_THAT(AreEqual(1, Config->Actions[0].Steps.Num()));
		ASSERT_THAT(IsTrue(Config->Actions[0].Steps[0].ID == SecondID));
		ASSERT_THAT(AreEqual(0, ArcTacticsStepTestsPrivate::CountBindingsTargeting(*Config, FirstID)));
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::HasBinding(*Config, BagID, TEXT("Reach"), SecondID, TEXT("Required"), false),
			TEXT("Deleting one step must not touch a sibling's binding")));
		ASSERT_THAT(IsFalse(ViewModel->IsCompiledArtifactValid()));
		ASSERT_THAT(IsFalse(ViewModel->IsSelectionValid(ArcTacticsStepTestsPrivate::StepSelection(FirstID))));
		ASSERT_THAT(IsTrue(ViewModel->Compile()));
		ASSERT_THAT(AreEqual(1, Config->CompiledData.Actions[0].Steps.Num()));

		ASSERT_THAT(IsTrue(ViewModel->Delete(ArcTacticsStepTestsPrivate::StepSelection(SecondID))));
		ASSERT_THAT(IsFalse(ViewModel->Compile(), TEXT("A Sequence left without steps must not compile")));
	}

	TEST_METHOD(DuplicateStep_GetsNewIdAndRemapsItsBindings)
	{
		UArcTacticsConfig* Config = ArcTacticsStepTestsPrivate::MakeConfig();
		Config->Actions.Add(ArcTacticsStepTestsPrivate::MakeSequenceAction({ArcTacticsStepTestsPrivate::MakeFixtureStep()}));
		Config->Actions[0].ParameterBag.AddProperty(TEXT("Reach"), EPropertyBagPropertyType::Float);
		Config->Actions[0].ParameterBag.AddProperty(TEXT("Published"), EPropertyBagPropertyType::Float);
		Config->EnsureValidNodeIDs();
		const FGuid BagID = Config->Actions[0].ParameterBagID;
		const FGuid OriginalID = Config->Actions[0].Steps[0].ID;
		ArcTacticsStepTestsPrivate::Bind(*Config, BagID, TEXT("Reach"), OriginalID, TEXT("Required"));
		ArcTacticsStepTestsPrivate::Bind(*Config, BagID, TEXT("Published"), OriginalID, TEXT("Result"), true);
		const TSharedRef<FArcTacticsEditorViewModel> ViewModel = FArcTacticsEditorViewModel::Create(Config);

		ASSERT_THAT(IsTrue(ViewModel->Duplicate(ArcTacticsStepTestsPrivate::StepSelection(OriginalID))));
		ASSERT_THAT(AreEqual(2, Config->Actions[0].Steps.Num()));
		if (Config->Actions[0].Steps.Num() < 2)
		{
			return;
		}
		ASSERT_THAT(IsTrue(Config->Actions[0].Steps[0].ID == OriginalID));
		const FGuid DuplicateID = Config->Actions[0].Steps[1].ID;
		ASSERT_THAT(IsTrue(DuplicateID.IsValid()));
		ASSERT_THAT(IsFalse(DuplicateID == OriginalID));
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::HasBinding(*Config, BagID, TEXT("Reach"), DuplicateID, TEXT("Required"), false)));
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::HasBinding(*Config, BagID, TEXT("Published"), DuplicateID, TEXT("Result"), true)));
		ASSERT_THAT(AreEqual(2, ArcTacticsStepTestsPrivate::CountBindingsTargeting(*Config, OriginalID)));
		ASSERT_THAT(AreEqual(2, ArcTacticsStepTestsPrivate::CountBindingsTargeting(*Config, DuplicateID)));
		ASSERT_THAT(IsTrue(ViewModel->Compile()));
	}

	TEST_METHOD(DuplicateAction_CarriesStepsWithNewIdsAndRemappedBindings)
	{
		UArcTacticsConfig* Config = ArcTacticsStepTestsPrivate::MakeConfig();
		Config->Actions.Add(ArcTacticsStepTestsPrivate::MakeSequenceAction({ArcTacticsStepTestsPrivate::MakeFixtureStep()}));
		Config->Actions[0].ParameterBag.AddProperty(TEXT("Reach"), EPropertyBagPropertyType::Float);
		Config->Actions[0].ParameterBag.AddProperty(TEXT("Published"), EPropertyBagPropertyType::Float);
		Config->EnsureValidNodeIDs();
		const FGuid ActionID = Config->Actions[0].Action.ID;
		const FGuid OriginalBagID = Config->Actions[0].ParameterBagID;
		const FGuid OriginalStepID = Config->Actions[0].Steps[0].ID;
		ArcTacticsStepTestsPrivate::Bind(*Config, OriginalBagID, TEXT("Reach"), OriginalStepID, TEXT("Required"));
		ArcTacticsStepTestsPrivate::Bind(*Config, OriginalBagID, TEXT("Published"), OriginalStepID, TEXT("Result"), true);
		const TSharedRef<FArcTacticsEditorViewModel> ViewModel = FArcTacticsEditorViewModel::Create(Config);

		ASSERT_THAT(IsTrue(ViewModel->Duplicate(ArcTacticsStepTestsPrivate::ActionSelection(ActionID))));
		ASSERT_THAT(AreEqual(2, Config->Actions.Num()));
		if (Config->Actions.Num() < 2 || Config->Actions[1].Steps.Num() != 1)
		{
			ASSERT_THAT(IsTrue(false, TEXT("The duplicated action must carry its step")));
			return;
		}
		const FGuid DuplicateStepID = Config->Actions[1].Steps[0].ID;
		const FGuid DuplicateBagID = Config->Actions[1].ParameterBagID;
		ASSERT_THAT(IsTrue(DuplicateStepID.IsValid()));
		ASSERT_THAT(IsFalse(DuplicateStepID == OriginalStepID));
		ASSERT_THAT(IsFalse(DuplicateBagID == OriginalBagID));
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::HasBinding(*Config, DuplicateBagID, TEXT("Reach"), DuplicateStepID, TEXT("Required"), false)));
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::HasBinding(*Config, DuplicateBagID, TEXT("Published"), DuplicateStepID, TEXT("Result"), true)));
		ASSERT_THAT(IsFalse(ArcTacticsStepTestsPrivate::HasBinding(*Config, OriginalBagID, TEXT("Reach"), DuplicateStepID, TEXT("Required"), false),
			TEXT("The duplicate step must not read the original action's bag")));
		ASSERT_THAT(AreEqual(2, ArcTacticsStepTestsPrivate::CountBindingsTargeting(*Config, OriginalStepID)));
		ASSERT_THAT(IsTrue(ViewModel->Compile()));
		ASSERT_THAT(AreEqual(1, Config->CompiledData.Actions[1].Steps.Num()));
	}

	TEST_METHOD(CopyPasteStep_RemapsBranchScopeAndRejectsMissingBagProperty)
	{
		UArcTacticsConfig* Config = ArcTacticsStepTestsPrivate::MakeConfig();
		Config->Actions.Add(ArcTacticsStepTestsPrivate::MakeSequenceAction({ArcTacticsStepTestsPrivate::MakeFixtureStep()}));
		Config->Actions.Add(ArcTacticsStepTestsPrivate::MakeSequenceAction({ArcTacticsStepTestsPrivate::MakeMoveStep()}));
		Config->Actions.Add(ArcTacticsStepTestsPrivate::MakeSequenceAction({ArcTacticsStepTestsPrivate::MakeMoveStep()}));
		Config->Actions[0].ParameterBag.AddProperty(TEXT("Reach"), EPropertyBagPropertyType::Float);
		Config->Actions[1].ParameterBag.AddProperty(TEXT("Reach"), EPropertyBagPropertyType::Float);
		Config->EnsureValidNodeIDs();
		const FGuid SourceBagID = Config->Actions[0].ParameterBagID;
		const FGuid DestinationBagID = Config->Actions[1].ParameterBagID;
		const FGuid SourceStepID = Config->Actions[0].Steps[0].ID;
		ArcTacticsStepTestsPrivate::Bind(*Config, SourceBagID, TEXT("Reach"), SourceStepID, TEXT("Required"));
		const FArcTacticsEditorSelection WithReach = ArcTacticsStepTestsPrivate::ActionSelection(Config->Actions[1].Action.ID);
		const FArcTacticsEditorSelection WithoutReach = ArcTacticsStepTestsPrivate::ActionSelection(Config->Actions[2].Action.ID);
		const TSharedRef<FArcTacticsEditorViewModel> ViewModel = FArcTacticsEditorViewModel::Create(Config);

		ArcTacticsStepTestsPrivate::FClipboardRestore ClipboardRestore;
		ASSERT_THAT(IsTrue(ViewModel->Copy(ArcTacticsStepTestsPrivate::StepSelection(SourceStepID))));
		ASSERT_THAT(IsFalse(ViewModel->CanPaste(WithoutReach),
			TEXT("The step's bag source has no counterpart in the destination bag")));
		ASSERT_THAT(IsFalse(ViewModel->Paste(WithoutReach)));
		ASSERT_THAT(AreEqual(1, Config->Actions[2].Steps.Num()));

		ASSERT_THAT(IsTrue(ViewModel->CanPaste(WithReach)));
		ASSERT_THAT(IsTrue(ViewModel->Paste(WithReach)));
		ASSERT_THAT(AreEqual(2, Config->Actions[1].Steps.Num()));
		if (Config->Actions[1].Steps.Num() != 2)
		{
			return;
		}
		const FGuid PastedID = Config->Actions[1].Steps[1].ID;
		ASSERT_THAT(IsTrue(PastedID.IsValid()));
		ASSERT_THAT(IsFalse(PastedID == SourceStepID));
		ASSERT_THAT(IsTrue(Config->Actions[1].Steps[1].Node.GetScriptStruct() == FArcTacticsTestStep::StaticStruct()));
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::HasBinding(*Config, DestinationBagID, TEXT("Reach"), PastedID, TEXT("Required"), false),
			TEXT("The pasted step must read the destination action's bag, not the source's")));
		ASSERT_THAT(IsFalse(ArcTacticsStepTestsPrivate::HasBinding(*Config, SourceBagID, TEXT("Reach"), PastedID, TEXT("Required"), false)));
		ASSERT_THAT(AreEqual(1, ArcTacticsStepTestsPrivate::CountBindingsTargeting(*Config, SourceStepID)));
		ASSERT_THAT(IsTrue(ViewModel->Compile()));
	}

	TEST_METHOD(CopyPasteStep_IntoAnotherSelectorAction_RemapsCurrentOutputToDestinationSelector)
	{
		UArcTacticsConfig* Config = ArcTacticsStepTestsPrivate::MakeConfig();
		const TSharedRef<FArcTacticsEditorViewModel> ViewModel = FArcTacticsEditorViewModel::Create(Config);
		for (int32 SelectorIndex = 0; SelectorIndex < 2; ++SelectorIndex)
		{
			ASSERT_THAT(IsTrue(ViewModel->Add(
				EArcTacticsEditorItemKind::Selector, FArcTacticsEditorSelection::Config(), FArcTacticsLocationSelector::StaticStruct())));
		}
		ASSERT_THAT(AreEqual(2, Config->Selectors.Num()));
		if (Config->Selectors.Num() != 2)
		{
			return;
		}
		for (int32 SelectorIndex = 0; SelectorIndex < 2; ++SelectorIndex)
		{
			ASSERT_THAT(IsTrue(ViewModel->ReplaceType(
				{EArcTacticsEditorItemKind::SelectorAction, Config->Selectors[SelectorIndex].Action.ID},
				FArcTacticsSequenceAction::StaticStruct())));
		}
		const FGuid FirstSelectorID = Config->Selectors[0].Selector.ID;
		const FGuid SecondSelectorID = Config->Selectors[1].Selector.ID;
		ASSERT_THAT(IsTrue(ViewModel->Add(
			EArcTacticsEditorItemKind::Step,
			{EArcTacticsEditorItemKind::SelectorAction, Config->Selectors[0].Action.ID},
			FArcTacticsMoveToStep::StaticStruct())));
		ASSERT_THAT(AreEqual(1, Config->Selectors[0].Steps.Num()));
		if (Config->Selectors[0].Steps.Num() != 1)
		{
			return;
		}
		const FGuid StepID = Config->Selectors[0].Steps[0].ID;
		ArcTacticsStepTestsPrivate::Bind(*Config, FirstSelectorID, TEXT("CurrentLocation"), StepID, TEXT("Destination"));

		ArcTacticsStepTestsPrivate::FClipboardRestore ClipboardRestore;
		ASSERT_THAT(IsTrue(ViewModel->Copy(ArcTacticsStepTestsPrivate::StepSelection(StepID))));
		const FArcTacticsEditorSelection Destination =
			{EArcTacticsEditorItemKind::SelectorAction, Config->Selectors[1].Action.ID};
		ASSERT_THAT(IsTrue(ViewModel->CanPaste(Destination)));
		ASSERT_THAT(IsTrue(ViewModel->Paste(Destination)));
		ASSERT_THAT(AreEqual(1, Config->Selectors[1].Steps.Num()));
		if (Config->Selectors[1].Steps.Num() != 1)
		{
			return;
		}
		const FGuid PastedID = Config->Selectors[1].Steps[0].ID;
		ASSERT_THAT(IsFalse(PastedID == StepID));
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::HasBinding(*Config, SecondSelectorID, TEXT("CurrentLocation"), PastedID, TEXT("Destination"), false),
			TEXT("The pasted step must read the destination selector's Current output")));
		ASSERT_THAT(IsFalse(ArcTacticsStepTestsPrivate::HasBinding(*Config, FirstSelectorID, TEXT("CurrentLocation"), PastedID, TEXT("Destination"), false)));
	}

	TEST_METHOD(MoveStep_ReordersWithinActionKeepingBindingsButIsRejectedAcrossActions)
	{
		UArcTacticsConfig* Config = ArcTacticsStepTestsPrivate::MakeConfig();
		Config->Actions.Add(ArcTacticsStepTestsPrivate::MakeSequenceAction({ArcTacticsStepTestsPrivate::MakeFixtureStep(), ArcTacticsStepTestsPrivate::MakeFixtureStep()}));
		Config->Actions.Add(ArcTacticsStepTestsPrivate::MakeSequenceAction({ArcTacticsStepTestsPrivate::MakeMoveStep()}));
		Config->Actions[0].ParameterBag.AddProperty(TEXT("Reach"), EPropertyBagPropertyType::Float);
		Config->EnsureValidNodeIDs();
		const FGuid BagID = Config->Actions[0].ParameterBagID;
		const FGuid FirstID = Config->Actions[0].Steps[0].ID;
		const FGuid SecondID = Config->Actions[0].Steps[1].ID;
		const FGuid ForeignStepID = Config->Actions[1].Steps[0].ID;
		const FArcTacticsEditorSelection OtherAction = ArcTacticsStepTestsPrivate::ActionSelection(Config->Actions[1].Action.ID);
		ArcTacticsStepTestsPrivate::Bind(*Config, BagID, TEXT("Reach"), FirstID, TEXT("Required"));
		ArcTacticsStepTestsPrivate::Bind(*Config, BagID, TEXT("Reach"), SecondID, TEXT("Required"));
		const TSharedRef<FArcTacticsEditorViewModel> ViewModel = FArcTacticsEditorViewModel::Create(Config);
		ASSERT_THAT(IsTrue(ViewModel->Compile()));

		ASSERT_THAT(IsFalse(ViewModel->CanMove(
			ArcTacticsStepTestsPrivate::StepSelection(FirstID), OtherAction, EArcTacticsEditorInsertLocation::Into)));
		ASSERT_THAT(IsFalse(ViewModel->Move(
			ArcTacticsStepTestsPrivate::StepSelection(FirstID), OtherAction, EArcTacticsEditorInsertLocation::Into)));
		ASSERT_THAT(IsFalse(ViewModel->CanMove(
			ArcTacticsStepTestsPrivate::StepSelection(FirstID),
			ArcTacticsStepTestsPrivate::StepSelection(ForeignStepID),
			EArcTacticsEditorInsertLocation::Before)));
		ASSERT_THAT(IsFalse(ViewModel->Move(
			ArcTacticsStepTestsPrivate::StepSelection(FirstID),
			ArcTacticsStepTestsPrivate::StepSelection(ForeignStepID),
			EArcTacticsEditorInsertLocation::After)));
		ASSERT_THAT(IsFalse(ViewModel->CanMove(
			ArcTacticsStepTestsPrivate::StepSelection(ForeignStepID),
			ArcTacticsStepTestsPrivate::StepSelection(FirstID),
			EArcTacticsEditorInsertLocation::Before)));
		ASSERT_THAT(AreEqual(2, Config->Actions[0].Steps.Num()));
		ASSERT_THAT(AreEqual(1, Config->Actions[1].Steps.Num()));
		ASSERT_THAT(IsTrue(Config->Actions[0].Steps[0].ID == FirstID));
		ASSERT_THAT(IsTrue(ViewModel->IsCompiledArtifactValid(), TEXT("A rejected move must not invalidate the artifact")));

		ASSERT_THAT(IsTrue(ViewModel->CanMove(
			ArcTacticsStepTestsPrivate::StepSelection(SecondID),
			ArcTacticsStepTestsPrivate::StepSelection(FirstID),
			EArcTacticsEditorInsertLocation::Before)));
		ASSERT_THAT(IsTrue(ViewModel->Move(
			ArcTacticsStepTestsPrivate::StepSelection(SecondID),
			ArcTacticsStepTestsPrivate::StepSelection(FirstID),
			EArcTacticsEditorInsertLocation::Before)));
		ASSERT_THAT(IsTrue(Config->Actions[0].Steps[0].ID == SecondID));
		ASSERT_THAT(IsTrue(Config->Actions[0].Steps[1].ID == FirstID));
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::HasBinding(*Config, BagID, TEXT("Reach"), FirstID, TEXT("Required"), false),
			TEXT("A binding follows its step by ID, not by index")));
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::HasBinding(*Config, BagID, TEXT("Reach"), SecondID, TEXT("Required"), false)));
		ASSERT_THAT(IsTrue(ViewModel->Compile()));
	}
};

TEST_CLASS(ArcTacticsStepBindingTests, "ArcTactics.StepBindings")
{
	TEST_METHOD(Step_ReadsSelectorCurrentAndBranchBag_AndDecisionCopiesValuesIntoStepData)
	{
		UArcTacticsConfig* Config = ArcTacticsStepTestsPrivate::MakeSelectorConfig(true, {ArcTacticsStepTestsPrivate::MakeMoveStep()});
		const FGuid StepID = Config->Selectors[0].Steps[0].ID;
		const FGuid BagID = Config->Selectors[0].ParameterBagID;
		ASSERT_THAT(IsTrue(Config->IsBindingSourceAccessible(ArcTacticsStepTestsPrivate::SelectorNodeID, StepID)));
		ASSERT_THAT(IsTrue(Config->IsBindingSourceAccessible(BagID, StepID)));
		ArcTacticsStepTestsPrivate::Bind(*Config, ArcTacticsStepTestsPrivate::SelectorNodeID, TEXT("CurrentLocation"), StepID, TEXT("Destination"));
		ArcTacticsStepTestsPrivate::Bind(*Config, BagID, TEXT("Reach"), StepID, TEXT("QueryApproachDistance"));

		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::CompileConfig(*Config, Diagnostics)));
		ASSERT_THAT(AreEqual(1, Config->CompiledData.Selectors[0].Steps.Num()));

		FArcTacticsInstanceSlot Instance;
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::BeginSelectorDecision(*Config, Instance)));
		ASSERT_THAT(IsTrue(Instance.HasSelectedAction()));
		ASSERT_THAT(AreEqual(1, Instance.SelectedAction.StepData.Num()));
		if (Instance.SelectedAction.StepData.Num() != 1)
		{
			return;
		}
		const FArcTacticsMoveToStepInstanceData* StepData =
			Instance.SelectedAction.StepData[0].GetPtr<FArcTacticsMoveToStepInstanceData>();
		ASSERT_THAT(IsNotNull(StepData));
		if (!StepData)
		{
			return;
		}
		ASSERT_THAT(IsNear(0.8, StepData->Destination.X, 0.001, TEXT("The higher-scoring location item is the selected candidate")));
		ASSERT_THAT(IsNear(3.0, StepData->Destination.Y, 0.001));
		ASSERT_THAT(IsNear(4.0, StepData->Destination.Z, 0.001));
		ASSERT_THAT(IsNear(300.0f, StepData->QueryApproachDistance, 0.001f));

		Instance.ActivateSelectedAction();
		ASSERT_THAT(IsTrue(Instance.ActiveAction.IsSet()));
		ASSERT_THAT(AreEqual(1, Instance.ActiveAction.StepData.Num()));
		if (Instance.ActiveAction.StepData.Num() == 1)
		{
			const FArcTacticsMoveToStepInstanceData* ActiveStep =
				Instance.ActiveAction.StepData[0].GetPtr<FArcTacticsMoveToStepInstanceData>();
			ASSERT_THAT(IsNotNull(ActiveStep));
			if (ActiveStep)
			{
				ASSERT_THAT(IsNear(3.0, ActiveStep->Destination.Y, 0.001, TEXT("A new active action owns the evaluated step data")));
			}
		}
	}

	TEST_METHOD(Step_CannotReadSiblingStep_InEitherDirection)
	{
		UArcTacticsConfig* Config = ArcTacticsStepTestsPrivate::MakeSelectorConfig(
			true, {ArcTacticsStepTestsPrivate::MakeFixtureStep(), ArcTacticsStepTestsPrivate::MakeFixtureStep()});
		const FGuid FirstID = Config->Selectors[0].Steps[0].ID;
		const FGuid SecondID = Config->Selectors[0].Steps[1].ID;
		const FGuid BagID = Config->Selectors[0].ParameterBagID;
		ArcTacticsStepTestsPrivate::Bind(*Config, BagID, TEXT("Reach"), FirstID, TEXT("Required"));
		ArcTacticsStepTestsPrivate::Bind(*Config, BagID, TEXT("Reach"), SecondID, TEXT("Required"));
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::CompileConfig(*Config, Diagnostics), TEXT("Control: both steps read only the branch bag")));

		ASSERT_THAT(IsFalse(Config->IsBindingSourceAccessible(FirstID, SecondID)));
		ASSERT_THAT(IsFalse(Config->IsBindingSourceAccessible(SecondID, FirstID)));
		ASSERT_THAT(IsTrue(Config->IsBindingSourceAccessible(BagID, SecondID)));

		ArcTacticsStepTestsPrivate::Bind(*Config, FirstID, TEXT("Result"), SecondID, TEXT("Optional"));
		ASSERT_THAT(IsFalse(ArcTacticsStepTestsPrivate::CompileConfig(*Config, Diagnostics),
			TEXT("A step reading a sibling step's Output must be rejected")));
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("scope"))));
		ASSERT_THAT(IsFalse(Config->CompiledData.IsExecutable()));
	}

	TEST_METHOD(StepOutput_WritesOnlyOwnBranchBag)
	{
		UArcTacticsConfig* Config = ArcTacticsStepTestsPrivate::MakeConfig();
		Config->Actions.Add(ArcTacticsStepTestsPrivate::MakeSequenceAction({ArcTacticsStepTestsPrivate::MakeFixtureStep()}));
		Config->Actions.Add(ArcTacticsStepTestsPrivate::MakeSequenceAction({ArcTacticsStepTestsPrivate::MakeMoveStep()}));
		Config->Actions[0].ParameterBag.AddProperty(TEXT("Published"), EPropertyBagPropertyType::Float);
		Config->Actions[0].ParameterBag.AddProperty(TEXT("Reach"), EPropertyBagPropertyType::Float);
		Config->Actions[1].ParameterBag.AddProperty(TEXT("Published"), EPropertyBagPropertyType::Float);
		Config->EnsureValidNodeIDs();
		const FGuid StepID = Config->Actions[0].Steps[0].ID;
		const FGuid OwnBagID = Config->Actions[0].ParameterBagID;
		const FGuid OtherBagID = Config->Actions[1].ParameterBagID;
		ArcTacticsStepTestsPrivate::Bind(*Config, OwnBagID, TEXT("Reach"), StepID, TEXT("Required"));
		ArcTacticsStepTestsPrivate::Bind(*Config, OwnBagID, TEXT("Published"), StepID, TEXT("Result"), true);
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::CompileConfig(*Config, Diagnostics), TEXT("Control: output into the own branch bag")));
		ASSERT_THAT(IsTrue(Config->IsBindingSourceAccessible(OwnBagID, StepID, true)));
		ASSERT_THAT(IsFalse(Config->IsBindingSourceAccessible(OtherBagID, StepID, true)));

		Config->EditorBindings.Reset();
		ArcTacticsStepTestsPrivate::Bind(*Config, OwnBagID, TEXT("Reach"), StepID, TEXT("Required"));
		ArcTacticsStepTestsPrivate::Bind(*Config, OtherBagID, TEXT("Published"), StepID, TEXT("Result"), true);
		ASSERT_THAT(IsFalse(ArcTacticsStepTestsPrivate::CompileConfig(*Config, Diagnostics),
			TEXT("A step output written into another action's bag must be rejected")));
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("scope"))));
	}

	TEST_METHOD(StepInStandaloneAction_CannotReadSelectorCurrentOutput)
	{
		UArcTacticsConfig* Config = ArcTacticsStepTestsPrivate::MakeSelectorConfig(true, {ArcTacticsStepTestsPrivate::MakeMoveStep()});
		Config->Actions.Add(ArcTacticsStepTestsPrivate::MakeSequenceAction({ArcTacticsStepTestsPrivate::MakeMoveStep()}));
		Config->EnsureValidNodeIDs();
		const FGuid SelectorStepID = Config->Selectors[0].Steps[0].ID;
		const FGuid StandaloneStepID = Config->Actions[0].Steps[0].ID;
		ArcTacticsStepTestsPrivate::Bind(*Config, ArcTacticsStepTestsPrivate::SelectorNodeID, TEXT("CurrentLocation"), SelectorStepID, TEXT("Destination"));
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::CompileConfig(*Config, Diagnostics), TEXT("Control: the selector's own step reads Current*")));
		ASSERT_THAT(IsTrue(Config->IsBindingSourceAccessible(ArcTacticsStepTestsPrivate::SelectorNodeID, SelectorStepID)));
		ASSERT_THAT(IsFalse(Config->IsBindingSourceAccessible(ArcTacticsStepTestsPrivate::SelectorNodeID, StandaloneStepID)));

		ArcTacticsStepTestsPrivate::Bind(*Config, ArcTacticsStepTestsPrivate::SelectorNodeID, TEXT("CurrentLocation"), StandaloneStepID, TEXT("Destination"));
		ASSERT_THAT(IsFalse(ArcTacticsStepTestsPrivate::CompileConfig(*Config, Diagnostics)));
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::HasDiagnosticContaining(Diagnostics, TEXT("scope"))));
	}
};

TEST_CLASS(ArcTacticsStepCompileTests, "ArcTactics.StepCompile")
{
	TEST_METHOD(EmptySequence_FailsCompile_AndOneStepCompiles)
	{
		UArcTacticsConfig* Config = ArcTacticsStepTestsPrivate::MakeConfig();
		Config->Actions.Add(ArcTacticsStepTestsPrivate::MakeSequenceAction({}));
		Config->EnsureValidNodeIDs();
		const FGuid ActionID = Config->Actions[0].Action.ID;
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;

		ASSERT_THAT(IsFalse(ArcTacticsStepTestsPrivate::CompileConfig(*Config, Diagnostics)));
		ASSERT_THAT(IsFalse(Config->CompiledData.IsExecutable()));
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::HasDiagnosticFor(Diagnostics, ActionID, TEXT("at least one step"))));

		Config->Actions[0].Steps.Add(ArcTacticsStepTestsPrivate::MakeMoveStep());
		Config->EnsureValidNodeIDs();
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::CompileConfig(*Config, Diagnostics),
			TEXT("Control: the same action with one step compiles")));
		ASSERT_THAT(IsTrue(Config->CompiledData.IsExecutable()));
	}

	TEST_METHOD(EmptySelectorSequence_FailsCompile)
	{
		UArcTacticsConfig* Config = ArcTacticsStepTestsPrivate::MakeSelectorConfig(true, {});
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsFalse(ArcTacticsStepTestsPrivate::CompileConfig(*Config, Diagnostics)));
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::HasDiagnosticFor(Diagnostics, Config->Selectors[0].Action.ID, TEXT("at least one step"))));

		Config->Selectors[0].Steps.Add(ArcTacticsStepTestsPrivate::MakeMoveStep());
		Config->EnsureValidNodeIDs();
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::CompileConfig(*Config, Diagnostics)));
	}

	TEST_METHOD(StepsUnderNonSequenceAction_FailCompile)
	{
		UArcTacticsConfig* Config = ArcTacticsStepTestsPrivate::MakeConfig();
		FArcTacticsActionDefinition Definition;
		Definition.Action.Node.InitializeAs<FArcTacticsTestAction>();
		Definition.Action.InstanceData.InitializeAs<FArcTacticsTestActionInstanceData>();
		Config->Actions.Add(Definition);
		Config->EnsureValidNodeIDs();
		const FGuid ActionID = Config->Actions[0].Action.ID;
		ArcTacticsStepTestsPrivate::Bind(*Config, Config->ContextID, TEXT("Self"), ActionID, TEXT("Entity"));
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::CompileConfig(*Config, Diagnostics), TEXT("Control: the action alone compiles")));

		Config->Actions[0].Steps.Add(ArcTacticsStepTestsPrivate::MakeMoveStep());
		Config->EnsureValidNodeIDs();
		ASSERT_THAT(IsFalse(ArcTacticsStepTestsPrivate::CompileConfig(*Config, Diagnostics)));
		ASSERT_THAT(IsFalse(Config->CompiledData.IsExecutable()));
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::HasDiagnosticFor(Diagnostics, ActionID, TEXT("Only a Tactical Action Sequence"))));
	}

	TEST_METHOD(StepsUnderNonSequenceSelectorAction_FailCompile)
	{
		UArcTacticsConfig* Config = ArcTacticsStepTestsPrivate::MakeSelectorConfig(false, {});
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::CompileConfig(*Config, Diagnostics), TEXT("Control: the selector action alone compiles")));

		Config->Selectors[0].Steps.Add(ArcTacticsStepTestsPrivate::MakeMoveStep());
		Config->EnsureValidNodeIDs();
		ASSERT_THAT(IsFalse(ArcTacticsStepTestsPrivate::CompileConfig(*Config, Diagnostics)));
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::HasDiagnosticFor(
			Diagnostics, Config->Selectors[0].Action.ID, TEXT("Only a Tactical Action Sequence"))));
	}

	TEST_METHOD(UnboundRequiredStepInput_FailsCompile_AndBindingItCompiles)
	{
		UArcTacticsConfig* Config = ArcTacticsStepTestsPrivate::MakeConfig();
		Config->Actions.Add(ArcTacticsStepTestsPrivate::MakeSequenceAction({ArcTacticsStepTestsPrivate::MakeFixtureStep()}));
		Config->GlobalParameters.AddProperty(TEXT("Reach"), EPropertyBagPropertyType::Float);
		Config->EnsureValidNodeIDs();
		const FGuid StepID = Config->Actions[0].Steps[0].ID;
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;

		ASSERT_THAT(IsFalse(ArcTacticsStepTestsPrivate::CompileConfig(*Config, Diagnostics)));
		ASSERT_THAT(IsFalse(Config->CompiledData.IsExecutable()));
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::HasDiagnosticFor(Diagnostics, StepID, TEXT("required")),
			TEXT("The rejection must be attributed to the step with the unbound required input")));

		ArcTacticsStepTestsPrivate::Bind(*Config, Config->GlobalParametersID, TEXT("Reach"), StepID, TEXT("Required"));
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::CompileConfig(*Config, Diagnostics),
			TEXT("Control: binding the required input makes the same config compile")));
		ASSERT_THAT(IsTrue(Config->CompiledData.IsExecutable()));
	}

	TEST_METHOD(SelectorStepWithUnboundRequiredInput_FailsCompile)
	{
		UArcTacticsConfig* Config = ArcTacticsStepTestsPrivate::MakeSelectorConfig(true, {ArcTacticsStepTestsPrivate::MakeFixtureStep()});
		const FGuid StepID = Config->Selectors[0].Steps[0].ID;
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsFalse(ArcTacticsStepTestsPrivate::CompileConfig(*Config, Diagnostics)));
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::HasDiagnosticFor(Diagnostics, StepID, TEXT("required"))));

		ArcTacticsStepTestsPrivate::Bind(*Config, Config->Selectors[0].ParameterBagID, TEXT("Reach"), StepID, TEXT("Required"));
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::CompileConfig(*Config, Diagnostics)));
	}

	TEST_METHOD(StepCompiledNodes_UseStepKindAndExecutionIndexAfterConsiderations)
	{
		UArcTacticsConfig* Config = ArcTacticsStepTestsPrivate::MakeSelectorConfig(
			true, {ArcTacticsStepTestsPrivate::MakeMoveStep(), ArcTacticsStepTestsPrivate::MakeMoveStep()});
		TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
		ASSERT_THAT(IsTrue(ArcTacticsStepTestsPrivate::CompileConfig(*Config, Diagnostics)));
		const FArcTacticsCompiledData& Compiled = Config->CompiledData;
		ASSERT_THAT(AreEqual(1, Compiled.Selectors.Num()));
		if (Compiled.Selectors.Num() != 1)
		{
			return;
		}
		const FArcTacticsCompiledSelector& Selector = Compiled.Selectors[0];
		ASSERT_THAT(AreEqual(2, Selector.Steps.Num()));
		ASSERT_THAT(AreEqual(1, Selector.Considerations.Num()));
		if (Selector.Steps.Num() != 2)
		{
			return;
		}
		for (int32 StepIndex = 0; StepIndex < 2; ++StepIndex)
		{
			const int32 NodeIndex = Selector.Steps[StepIndex].Get();
			ASSERT_THAT(IsTrue(Compiled.Nodes.IsValidIndex(NodeIndex)));
			if (!Compiled.Nodes.IsValidIndex(NodeIndex))
			{
				return;
			}
			ASSERT_THAT(IsTrue(Compiled.Nodes[NodeIndex].Kind == EArcTacticsCompiledNodeKind::Step));
			ASSERT_THAT(AreEqual(Selector.Considerations.Num() + 1 + StepIndex, Compiled.Nodes[NodeIndex].ExecutionIndex));
		}
	}
};

#endif
