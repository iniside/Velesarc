#include "CQTest.h"

#if WITH_EDITOR

#include "ArcTacticsEditorClipboard.h"
#include "ArcTacticsEditorViewModel.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "Decision/ArcTacticsSelectors.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Fixtures/ArcTacticsPoolTestTypes.h"
#include "Fixtures/ArcTacticsTestTypes.h"

namespace ArcTacticsAuthoringTestsPrivate
{
	UArcTacticsConfig* MakeConfig()
	{
		UArcTacticsConfig* Config = NewObject<UArcTacticsConfig>(GetTransientPackage(), NAME_None, RF_Transactional);
		Config->ContextID = FGuid(101, 102, 103, 104);
		return Config;
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

	void AddRequiredEntityBinding(UArcTacticsConfig& Config, const FGuid TargetID)
	{
		Config.EditorBindings.AddBinding(
			FPropertyBindingPath(Config.ContextID, TEXT("Self")),
			FPropertyBindingPath(TargetID, TEXT("Entity")));
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

	void AddOutputBinding(UArcTacticsConfig& Config, const FGuid NodeID, const FGuid BagID)
	{
		Config.EditorBindings.AddBinding(
			FPropertyBindingPath(BagID, TEXT("Published")),
			FPropertyBindingPath(NodeID, TEXT("OutputInteger")));
		Config.EditorBindings.ForEachMutableBinding(
			[NodeID](FPropertyBindingBinding& Binding)
			{
				if (PathMatches(Binding.GetTargetPath(), NodeID, TEXT("OutputInteger")))
				{
					static_cast<FArcTacticsBinding&>(Binding).SetIsOutputBinding(true);
				}
			});
	}

	void AddBinding(UArcTacticsConfig& Config, const FGuid SourceID, const TCHAR* SourcePath, const FGuid TargetID, const TCHAR* TargetPath, const bool bOutput)
	{
		Config.EditorBindings.AddBinding(
			MakePath(SourceID, SourcePath),
			MakePath(TargetID, TargetPath));
		Config.EditorBindings.ForEachMutableBinding(
			[TargetID, TargetPath, bOutput](FPropertyBindingBinding& Binding)
			{
				if (bOutput && PathMatches(Binding.GetTargetPath(), TargetID, TargetPath))
				{
					static_cast<FArcTacticsBinding&>(Binding).SetIsOutputBinding(true);
				}
			});
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
}

TEST_CLASS(ArcTacticsAuthoringTests, "ArcTactics.EditorAuthoring")
{
	TEST_METHOD(DuplicateCopyPasteMoveDelete_PreservesAndRemapsBindings)
	{
		UArcTacticsConfig* Config = ArcTacticsAuthoringTestsPrivate::MakeConfig();
		const TSharedRef<FArcTacticsEditorViewModel> ViewModel = FArcTacticsEditorViewModel::Create(Config);
		const FArcTacticsEditorSelection ConfigSelection = FArcTacticsEditorSelection::Config();

		ASSERT_THAT(IsTrue(ViewModel->Add(
			EArcTacticsEditorItemKind::Action,
			ConfigSelection,
			FArcTacticsTestAction::StaticStruct())));
		ASSERT_THAT(AreEqual(1, Config->Actions.Num()));
		if (Config->Actions.Num() < 1)
		{
			return;
		}
		const FGuid OriginalActionID = Config->Actions[0].Action.ID;
		const FGuid OriginalParametersID = Config->Actions[0].ParametersID;
		const FGuid OriginalBagID = Config->Actions[0].ParameterBagID;
		Config->Actions[0].ParameterBag.AddProperty(TEXT("Published"), EPropertyBagPropertyType::Int32);
		ArcTacticsAuthoringTestsPrivate::AddRequiredEntityBinding(*Config, OriginalActionID);
		ArcTacticsAuthoringTestsPrivate::AddOutputBinding(*Config, OriginalActionID, OriginalBagID);

		const FArcTacticsEditorSelection OriginalSelection =
			{EArcTacticsEditorItemKind::Action, OriginalActionID};
		ASSERT_THAT(IsTrue(ViewModel->Duplicate(OriginalSelection)));
		ASSERT_THAT(AreEqual(2, Config->Actions.Num()));
		const FGuid DuplicatedActionID = Config->Actions[1].Action.ID;
		ASSERT_THAT(IsFalse(DuplicatedActionID == OriginalActionID));
		ASSERT_THAT(IsFalse(Config->Actions[1].ParametersID == OriginalParametersID));
		ASSERT_THAT(IsFalse(Config->Actions[1].ParameterBagID == OriginalBagID));
		ASSERT_THAT(IsTrue(ArcTacticsAuthoringTestsPrivate::HasBinding(
			*Config, Config->ContextID, TEXT("Self"), DuplicatedActionID, TEXT("Entity"), false)));
		ASSERT_THAT(IsTrue(ArcTacticsAuthoringTestsPrivate::HasBinding(
			*Config, Config->Actions[1].ParameterBagID, TEXT("Published"), DuplicatedActionID, TEXT("OutputInteger"), true)));

		{
			ArcTacticsAuthoringTestsPrivate::FClipboardRestore ClipboardRestore;
			ASSERT_THAT(IsTrue(ViewModel->Copy(OriginalSelection)));
			ASSERT_THAT(IsTrue(ViewModel->CanPaste(ConfigSelection)));
			ASSERT_THAT(IsTrue(ViewModel->Paste(ConfigSelection)));
		}
		ASSERT_THAT(AreEqual(3, Config->Actions.Num()));
		const FGuid PastedActionID = Config->Actions[2].Action.ID;
		ASSERT_THAT(IsFalse(PastedActionID == OriginalActionID));
		ASSERT_THAT(IsFalse(PastedActionID == DuplicatedActionID));
		ASSERT_THAT(IsTrue(ArcTacticsAuthoringTestsPrivate::HasBinding(
			*Config, Config->ContextID, TEXT("Self"), PastedActionID, TEXT("Entity"), false)));
		ASSERT_THAT(IsTrue(ArcTacticsAuthoringTestsPrivate::HasBinding(
			*Config, Config->Actions[2].ParameterBagID, TEXT("Published"), PastedActionID, TEXT("OutputInteger"), true)));
		ASSERT_THAT(IsTrue(ViewModel->Compile()));
		ASSERT_THAT(IsTrue(ViewModel->IsCompiledArtifactValid()));

		ASSERT_THAT(IsTrue(ViewModel->Move(
			{EArcTacticsEditorItemKind::Action, PastedActionID},
			{EArcTacticsEditorItemKind::Action, OriginalActionID},
			EArcTacticsEditorInsertLocation::Before)));
		ASSERT_THAT(IsTrue(Config->Actions.IsValidIndex(0)));
		ASSERT_THAT(IsTrue(Config->Actions[0].Action.ID == PastedActionID));
		ASSERT_THAT(IsFalse(ViewModel->IsCompiledArtifactValid()));
		ASSERT_THAT(IsTrue(ArcTacticsAuthoringTestsPrivate::HasBinding(
			*Config, Config->ContextID, TEXT("Self"), PastedActionID, TEXT("Entity"), false)));

		ASSERT_THAT(IsTrue(ViewModel->Compile()));
		ASSERT_THAT(IsTrue(ViewModel->IsCompiledArtifactValid()));
		const int32 BindingsBeforeDelete = ArcTacticsAuthoringTestsPrivate::CountBindingsTargeting(
			*Config, DuplicatedActionID);
		ASSERT_THAT(IsTrue(BindingsBeforeDelete > 0));
		ASSERT_THAT(IsTrue(ViewModel->Delete(
			{EArcTacticsEditorItemKind::Action, DuplicatedActionID})));
		ASSERT_THAT(IsFalse(ViewModel->IsSelectionValid(
			{EArcTacticsEditorItemKind::Action, DuplicatedActionID})));
		ASSERT_THAT(IsFalse(ViewModel->IsCompiledArtifactValid()));
		ASSERT_THAT(AreEqual(0, ArcTacticsAuthoringTestsPrivate::CountBindingsTargeting(
			*Config, DuplicatedActionID)));
	}

	TEST_METHOD(ReplaceTypeAndTransactions_RestoreLogicalAuthoringState)
	{
		UArcTacticsConfig* Config = ArcTacticsAuthoringTestsPrivate::MakeConfig();
		const TSharedRef<FArcTacticsEditorViewModel> ViewModel = FArcTacticsEditorViewModel::Create(Config);
		ASSERT_THAT(IsTrue(ViewModel->Add(
			EArcTacticsEditorItemKind::Action,
			FArcTacticsEditorSelection::Config(),
			FArcTacticsTestAction::StaticStruct())));
		if (Config->Actions.Num() < 1)
		{
			return;
		}
		const FGuid ActionID = Config->Actions[0].Action.ID;
		const FGuid BagID = Config->Actions[0].ParameterBagID;
		Config->Actions[0].ParameterBag.AddProperty(TEXT("Published"), EPropertyBagPropertyType::Int32);
		ArcTacticsAuthoringTestsPrivate::AddRequiredEntityBinding(*Config, ActionID);
		ArcTacticsAuthoringTestsPrivate::AddOutputBinding(*Config, ActionID, BagID);
		const FArcTacticsEditorSelection Selection =
			{EArcTacticsEditorItemKind::Action, ActionID};

		ASSERT_THAT(IsFalse(ViewModel->CanReplaceType(Selection, FArcTacticsTestTask::StaticStruct())));
		ASSERT_THAT(IsFalse(ViewModel->ReplaceType(Selection, FArcTacticsTestTask::StaticStruct())));
		ASSERT_THAT(IsTrue(ViewModel->CanReplaceType(Selection, FArcTacticsPoolLifecycleAction::StaticStruct())));
		ASSERT_THAT(IsTrue(ViewModel->ReplaceType(Selection, FArcTacticsPoolLifecycleAction::StaticStruct())));
		ASSERT_THAT(IsTrue(Config->Actions[0].Action.Node.GetScriptStruct() == FArcTacticsPoolLifecycleAction::StaticStruct()));
		ASSERT_THAT(IsTrue(ArcTacticsAuthoringTestsPrivate::HasBinding(
			*Config, Config->ContextID, TEXT("Self"), ActionID, TEXT("Entity"), false) == false));
		ASSERT_THAT(IsTrue(ArcTacticsAuthoringTestsPrivate::CountBindingsTargeting(*Config, ActionID) == 0));
		ASSERT_THAT(IsTrue(ViewModel->CanReplaceType(Selection, FArcTacticsTestAction::StaticStruct())));
		ASSERT_THAT(IsTrue(ViewModel->ReplaceType(Selection, FArcTacticsTestAction::StaticStruct())));
		Config->Actions[0].ParameterBag.AddProperty(TEXT("Published"), EPropertyBagPropertyType::Int32);
		ArcTacticsAuthoringTestsPrivate::AddRequiredEntityBinding(*Config, ActionID);
		ArcTacticsAuthoringTestsPrivate::AddOutputBinding(*Config, ActionID, Config->Actions[0].ParameterBagID);
		ASSERT_THAT(IsTrue(ArcTacticsAuthoringTestsPrivate::HasBinding(
			*Config, Config->ContextID, TEXT("Self"), ActionID, TEXT("Entity"), false)));
		ASSERT_THAT(IsTrue(ArcTacticsAuthoringTestsPrivate::HasBinding(
			*Config, Config->Actions[0].ParameterBagID, TEXT("Published"), ActionID, TEXT("OutputInteger"), true)));

		ASSERT_THAT(IsTrue(ViewModel->Compile()));
		ASSERT_THAT(IsTrue(ViewModel->IsCompiledArtifactValid()));
		ASSERT_THAT(IsTrue(ViewModel->Duplicate(Selection)));
		ASSERT_THAT(AreEqual(2, Config->Actions.Num()));
		ASSERT_THAT(IsFalse(ViewModel->IsCompiledArtifactValid()));
		const FGuid DuplicatedActionID = Config->Actions[1].Action.ID;
		const FGuid DuplicatedBagID = Config->Actions[1].ParameterBagID;
		ASSERT_THAT(IsTrue(ArcTacticsAuthoringTestsPrivate::HasBinding(
			*Config, Config->ContextID, TEXT("Self"), DuplicatedActionID, TEXT("Entity"), false)));
		ASSERT_THAT(IsTrue(ArcTacticsAuthoringTestsPrivate::HasBinding(
			*Config, DuplicatedBagID, TEXT("Published"), DuplicatedActionID, TEXT("OutputInteger"), true)));
		ASSERT_THAT(IsTrue(GEditor != nullptr));
		ASSERT_THAT(IsTrue(GEditor && GEditor->Trans != nullptr));
		if (!GEditor || !GEditor->Trans)
		{
			return;
		}
		TArray<FArcTacticsCompilerDiagnostic> UndoDiagnostics;
		ASSERT_THAT(IsTrue(Config->CompileEditorData(UndoDiagnostics)));
		ASSERT_THAT(IsTrue(ViewModel->IsCompiledArtifactValid()));
		ASSERT_THAT(IsTrue(GEditor->Trans->Undo()));
		ASSERT_THAT(AreEqual(1, Config->Actions.Num()));
		ASSERT_THAT(IsFalse(ViewModel->IsCompiledArtifactValid()));
		ASSERT_THAT(AreEqual(0, ArcTacticsAuthoringTestsPrivate::CountBindingsTargeting(
			*Config, DuplicatedActionID)));
		ASSERT_THAT(IsTrue(ArcTacticsAuthoringTestsPrivate::HasBinding(
			*Config, Config->ContextID, TEXT("Self"), ActionID, TEXT("Entity"), false)));
		ASSERT_THAT(IsTrue(ArcTacticsAuthoringTestsPrivate::HasBinding(
			*Config, Config->Actions[0].ParameterBagID, TEXT("Published"), ActionID, TEXT("OutputInteger"), true)));
		UndoDiagnostics.Reset();
		ASSERT_THAT(IsTrue(Config->CompileEditorData(UndoDiagnostics)));
		ASSERT_THAT(IsTrue(ViewModel->IsCompiledArtifactValid()));
		ASSERT_THAT(IsTrue(GEditor->Trans->Redo()));
		ASSERT_THAT(AreEqual(2, Config->Actions.Num()));
		ASSERT_THAT(IsFalse(ViewModel->IsCompiledArtifactValid()));
		ASSERT_THAT(IsTrue(ArcTacticsAuthoringTestsPrivate::HasBinding(
			*Config, Config->ContextID, TEXT("Self"), DuplicatedActionID, TEXT("Entity"), false)));
		ASSERT_THAT(IsTrue(ArcTacticsAuthoringTestsPrivate::HasBinding(
			*Config, DuplicatedBagID, TEXT("Published"), DuplicatedActionID, TEXT("OutputInteger"), true)));
	}

	TEST_METHOD(MoveConsideration_RejectsCrossScopeBinding)
	{
		UArcTacticsConfig* Config = ArcTacticsAuthoringTestsPrivate::MakeConfig();
		const TSharedRef<FArcTacticsEditorViewModel> ViewModel = FArcTacticsEditorViewModel::Create(Config);
		const FArcTacticsEditorSelection ConfigSelection = FArcTacticsEditorSelection::Config();
		ASSERT_THAT(IsTrue(ViewModel->Add(
			EArcTacticsEditorItemKind::Action,
			ConfigSelection,
			FArcTacticsTestAction::StaticStruct())));
		ASSERT_THAT(IsTrue(ViewModel->Add(
			EArcTacticsEditorItemKind::Action,
			ConfigSelection,
			FArcTacticsTestAction::StaticStruct())));
		if (Config->Actions.Num() < 2)
		{
			return;
		}
		const FGuid SourceActionID = Config->Actions[0].Action.ID;
		const FGuid DestinationActionID = Config->Actions[1].Action.ID;
		const FGuid SourceBagID = Config->Actions[0].ParameterBagID;
		Config->Actions[0].ParameterBag.AddProperty(TEXT("Published"), EPropertyBagPropertyType::Float);
		ArcTacticsAuthoringTestsPrivate::AddRequiredEntityBinding(*Config, SourceActionID);
		ArcTacticsAuthoringTestsPrivate::AddRequiredEntityBinding(*Config, DestinationActionID);
		const FArcTacticsEditorSelection SourceActionSelection =
			{EArcTacticsEditorItemKind::Action, SourceActionID};
		const FArcTacticsEditorSelection DestinationActionSelection =
			{EArcTacticsEditorItemKind::Action, DestinationActionID};
		ASSERT_THAT(IsTrue(ViewModel->Add(
			EArcTacticsEditorItemKind::Consideration,
			SourceActionSelection,
			FArcTacticsTestConsideration::StaticStruct())));
		ASSERT_THAT(IsTrue(ViewModel->Add(
			EArcTacticsEditorItemKind::Consideration,
			DestinationActionSelection,
			FArcTacticsTestConsideration::StaticStruct())));
		ASSERT_THAT(IsTrue(Config->Actions.IsValidIndex(0)));
		ASSERT_THAT(IsTrue(Config->Actions.IsValidIndex(1)));
		if (!Config->Actions.IsValidIndex(0)
			|| !Config->Actions.IsValidIndex(1))
		{
			return;
		}
		ASSERT_THAT(IsTrue(Config->Actions[0].Considerations.IsValidIndex(0)));
		ASSERT_THAT(IsTrue(Config->Actions[1].Considerations.IsValidIndex(0)));
		if (!Config->Actions[0].Considerations.IsValidIndex(0)
			|| !Config->Actions[1].Considerations.IsValidIndex(0))
		{
			return;
		}
		const FGuid SourceConsiderationID = Config->Actions[0].Considerations[0].ID;
		Config->EditorBindings.AddBinding(
			FPropertyBindingPath(SourceBagID, TEXT("Published")),
			FPropertyBindingPath(SourceConsiderationID, TEXT("BoundScore")));
		const FGuid DestinationConsiderationID = Config->Actions[1].Considerations[0].ID;
		Config->GlobalParameters.AddProperty(TEXT("DestinationScore"), EPropertyBagPropertyType::Float);
		Config->EditorBindings.AddBinding(
			FPropertyBindingPath(Config->GlobalParametersID, TEXT("DestinationScore")),
			FPropertyBindingPath(DestinationConsiderationID, TEXT("BoundScore")));
		ASSERT_THAT(IsTrue(ViewModel->Compile()));

		const FArcTacticsEditorSelection SourceConsiderationSelection =
			{EArcTacticsEditorItemKind::Consideration, SourceConsiderationID};
		ASSERT_THAT(IsFalse(ViewModel->CanMove(
			SourceConsiderationSelection,
			DestinationActionSelection,
			EArcTacticsEditorInsertLocation::Into)));
		ASSERT_THAT(IsFalse(ViewModel->Move(
			SourceConsiderationSelection,
			DestinationActionSelection,
			EArcTacticsEditorInsertLocation::Into)));
		ASSERT_THAT(IsTrue(Config->Actions[0].Considerations.ContainsByPredicate(
			[SourceConsiderationID](const FArcTacticsNodeDefinition& Node)
			{
				return Node.ID == SourceConsiderationID;
			})));
		ASSERT_THAT(IsTrue(ArcTacticsAuthoringTestsPrivate::HasBinding(
			*Config, SourceBagID, TEXT("Published"), SourceConsiderationID, TEXT("BoundScore"), false)));
		ASSERT_THAT(IsFalse(Config->Actions[1].Considerations.ContainsByPredicate(
			[SourceConsiderationID](const FArcTacticsNodeDefinition& Node)
			{
				return Node.ID == SourceConsiderationID;
			})));
	}

	TEST_METHOD(DuplicateAndPasteSelector_RemapsSelectorNodeScopeAndPreservesExternalSources)
	{
		UArcTacticsConfig* Config = ArcTacticsAuthoringTestsPrivate::MakeConfig();
		const TSharedRef<FArcTacticsEditorViewModel> ViewModel = FArcTacticsEditorViewModel::Create(Config);
		const FArcTacticsEditorSelection ConfigSelection = FArcTacticsEditorSelection::Config();
		ASSERT_THAT(IsTrue(ViewModel->Add(
			EArcTacticsEditorItemKind::GlobalTask,
			ConfigSelection,
			FArcTacticsTestTask::StaticStruct())));
		ASSERT_THAT(AreEqual(1, Config->GlobalTasks.Num()));
		if (Config->GlobalTasks.Num() < 1)
		{
			return;
		}
		const FGuid GlobalTaskID = Config->GlobalTasks[0].ID;
		FArcTacticsTestTaskInstanceData& TaskData =
			Config->GlobalTasks[0].InstanceData.GetMutable<FArcTacticsTestTaskInstanceData>();
		TaskData.Locations.Add(FVector(0.75, 0.0, 0.0));

		ASSERT_THAT(IsFalse(ViewModel->Add(
			EArcTacticsEditorItemKind::Selector,
			ConfigSelection,
			FArcTacticsSelectorNode::StaticStruct()),
			TEXT("The abstract selector base is not an authorable selector type")));
		ASSERT_THAT(IsTrue(ViewModel->Add(
			EArcTacticsEditorItemKind::Selector,
			ConfigSelection,
			FArcTacticsLocationSelector::StaticStruct())));
		ASSERT_THAT(AreEqual(1, Config->Selectors.Num()));
		if (Config->Selectors.Num() < 1)
		{
			return;
		}
		const FGuid OriginalSelectorID = Config->Selectors[0].Selector.ID;
		const FGuid OriginalParametersID = Config->Selectors[0].ParametersID;
		const FGuid OriginalBagID = Config->Selectors[0].ParameterBagID;
		const FGuid OriginalActionID = Config->Selectors[0].Action.ID;
		ASSERT_THAT(IsTrue(Config->Selectors[0].Selector.Node.GetScriptStruct() == FArcTacticsLocationSelector::StaticStruct()));
		ASSERT_THAT(IsFalse(Config->Selectors[0].Action.Node.IsValid(),
			TEXT("A new selector's action slot starts empty")));
		ASSERT_THAT(IsTrue(ViewModel->ReplaceType(
			{EArcTacticsEditorItemKind::SelectorAction, OriginalActionID},
			FArcTacticsTestAction::StaticStruct())));
		ASSERT_THAT(IsTrue(Config->Selectors[0].Action.Node.GetScriptStruct() == FArcTacticsTestAction::StaticStruct()));
		ArcTacticsAuthoringTestsPrivate::AddBinding(
			*Config, GlobalTaskID, TEXT("Locations"), OriginalSelectorID, TEXT("Items"), false);
		ArcTacticsAuthoringTestsPrivate::AddRequiredEntityBinding(*Config, OriginalActionID);
		ASSERT_THAT(IsTrue(ViewModel->Add(
			EArcTacticsEditorItemKind::Consideration,
			{EArcTacticsEditorItemKind::Selector, OriginalSelectorID},
			FArcTacticsTestConsideration::StaticStruct())));
		ASSERT_THAT(IsTrue(Config->Selectors.IsValidIndex(0)));
		ASSERT_THAT(IsTrue(Config->Selectors[0].Considerations.IsValidIndex(0)));
		if (!Config->Selectors.IsValidIndex(0) || !Config->Selectors[0].Considerations.IsValidIndex(0))
		{
			return;
		}
		const FGuid OriginalConsiderationID = Config->Selectors[0].Considerations[0].ID;
		ArcTacticsAuthoringTestsPrivate::AddBinding(
			*Config, OriginalSelectorID, TEXT("CurrentLocation"), OriginalActionID, TEXT("Location"), false);
		ArcTacticsAuthoringTestsPrivate::AddBinding(
			*Config, OriginalSelectorID, TEXT("CurrentLocation.X"), OriginalConsiderationID, TEXT("BoundScore"), false);

		const FArcTacticsEditorSelection OriginalSelection =
			{EArcTacticsEditorItemKind::Selector, OriginalSelectorID};
		ASSERT_THAT(IsTrue(ViewModel->Compile()));
		ASSERT_THAT(IsTrue(ViewModel->Duplicate(OriginalSelection)));
		ASSERT_THAT(AreEqual(2, Config->Selectors.Num()));
		ASSERT_THAT(IsFalse(ViewModel->IsCompiledArtifactValid()));
		if (Config->Selectors.Num() < 2)
		{
			return;
		}
		const FArcTacticsSelectorDefinition& Duplicate = Config->Selectors[1];
		const FGuid DuplicateSelectorID = Duplicate.Selector.ID;
		ASSERT_THAT(IsTrue(Duplicate.Considerations.IsValidIndex(0)));
		if (!Duplicate.Considerations.IsValidIndex(0))
		{
			return;
		}
		ASSERT_THAT(IsFalse(Duplicate.Selector.ID == OriginalSelectorID));
		ASSERT_THAT(IsFalse(Duplicate.Action.ID == OriginalActionID));
		ASSERT_THAT(IsFalse(Duplicate.ParametersID == OriginalParametersID));
		ASSERT_THAT(IsFalse(Duplicate.ParameterBagID == OriginalBagID));
		ASSERT_THAT(IsTrue(ArcTacticsAuthoringTestsPrivate::HasBinding(
			*Config, GlobalTaskID, TEXT("Locations"), DuplicateSelectorID, TEXT("Items"), false),
			TEXT("The external global-task source of Items must be preserved on the duplicate")));
		ASSERT_THAT(IsTrue(ArcTacticsAuthoringTestsPrivate::HasBinding(
			*Config, Config->ContextID, TEXT("Self"), Duplicate.Action.ID, TEXT("Entity"), false)));
		ASSERT_THAT(IsTrue(ArcTacticsAuthoringTestsPrivate::HasBinding(
			*Config, DuplicateSelectorID, TEXT("CurrentLocation"), Duplicate.Action.ID, TEXT("Location"), false)));
		ASSERT_THAT(IsTrue(ArcTacticsAuthoringTestsPrivate::HasBinding(
			*Config, DuplicateSelectorID, TEXT("CurrentLocation.X"), Duplicate.Considerations[0].ID, TEXT("BoundScore"), false)));
		ASSERT_THAT(IsFalse(ArcTacticsAuthoringTestsPrivate::HasBinding(
			*Config, OriginalSelectorID, TEXT("CurrentLocation"), Duplicate.Action.ID, TEXT("Location"), false),
			TEXT("The duplicate must not read the original selector's output")));
		ASSERT_THAT(IsTrue(ViewModel->Compile()));
		ASSERT_THAT(IsTrue(ViewModel->IsCompiledArtifactValid()));

		{
			ArcTacticsAuthoringTestsPrivate::FClipboardRestore ClipboardRestore;
			ASSERT_THAT(IsTrue(ViewModel->Copy(OriginalSelection)));
			ASSERT_THAT(IsTrue(ViewModel->CanPaste(ConfigSelection)));
			ASSERT_THAT(IsTrue(ViewModel->Paste(ConfigSelection)));
		}
		ASSERT_THAT(AreEqual(3, Config->Selectors.Num()));
		ASSERT_THAT(IsFalse(ViewModel->IsCompiledArtifactValid()));
		if (Config->Selectors.Num() < 3)
		{
			return;
		}
		const FArcTacticsSelectorDefinition& Pasted = Config->Selectors[2];
		ASSERT_THAT(IsTrue(Pasted.Considerations.IsValidIndex(0)));
		if (!Pasted.Considerations.IsValidIndex(0))
		{
			return;
		}
		ASSERT_THAT(IsFalse(Pasted.Selector.ID == OriginalSelectorID));
		ASSERT_THAT(IsFalse(Pasted.Selector.ID == DuplicateSelectorID));
		ASSERT_THAT(IsTrue(ArcTacticsAuthoringTestsPrivate::HasBinding(
			*Config, GlobalTaskID, TEXT("Locations"), Pasted.Selector.ID, TEXT("Items"), false)));
		ASSERT_THAT(IsTrue(ArcTacticsAuthoringTestsPrivate::HasBinding(
			*Config, Pasted.Selector.ID, TEXT("CurrentLocation.X"), Pasted.Considerations[0].ID, TEXT("BoundScore"), false)));
		ASSERT_THAT(IsTrue(ViewModel->Compile()));
		ASSERT_THAT(IsTrue(ViewModel->IsCompiledArtifactValid()));
	}
};

#endif
