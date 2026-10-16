// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTacticsEditorClipboard.h"

#include "HAL/PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "ArcTacticsEditorClipboard"

namespace ArcTacticsEditorClipboardPrivate
{
	constexpr int32 CurrentVersion = 13;

	class FImportErrorContext final : public FOutputDevice
	{
	public:
		virtual void Serialize(
			const TCHAR* Message,
			ELogVerbosity::Type Verbosity,
			const FName& Category) override
		{
			++NumErrors;
		}

		int32 NumErrors = 0;
	};

	const FArcTacticsActionDefinition* FindAction(
		const UArcTacticsConfig& Config,
		const FGuid& ID)
	{
		return Config.Actions.FindByPredicate([ID](const FArcTacticsActionDefinition& Action)
		{
			return Action.Action.ID == ID;
		});
	}

	template <typename DefinitionType>
	const TArray<FArcTacticsNodeDefinition>& GetBranchNodes(
		const DefinitionType& Definition,
		const EArcTacticsEditorItemKind Kind)
	{
		switch (Kind)
		{
		case EArcTacticsEditorItemKind::Step:
			return Definition.Steps;
		case EArcTacticsEditorItemKind::ActiveStep:
			return Definition.ActiveSteps;
		default:
			return Definition.Considerations;
		}
	}

	const FArcTacticsNodeDefinition* FindBranchNode(
		const UArcTacticsConfig& Config,
		const FGuid& ID,
		const EArcTacticsEditorItemKind Kind,
		FGuid& OutParametersID,
		FGuid& OutParameterBagID,
		FGuid& OutSelectorID)
	{
		for (const FArcTacticsActionDefinition& Action : Config.Actions)
		{
			const TArray<FArcTacticsNodeDefinition>& Nodes = GetBranchNodes(Action, Kind);
			if (const FArcTacticsNodeDefinition* Result = Nodes.FindByPredicate(
				[ID](const FArcTacticsNodeDefinition& Node)
				{
					return Node.ID == ID;
				}))
			{
				OutParametersID = Action.ParametersID;
				OutParameterBagID = Action.ParameterBagID;
				OutSelectorID.Invalidate();
				return Result;
			}
		}
		for (const FArcTacticsSelectorDefinition& Selector : Config.Selectors)
		{
			const TArray<FArcTacticsNodeDefinition>& Nodes = GetBranchNodes(Selector, Kind);
			if (const FArcTacticsNodeDefinition* Result = Nodes.FindByPredicate(
				[ID](const FArcTacticsNodeDefinition& Node)
				{
					return Node.ID == ID;
				}))
			{
				OutParametersID = Selector.ParametersID;
				OutParameterBagID = Selector.ParameterBagID;
				OutSelectorID = Selector.Selector.ID;
				return Result;
			}
		}
		return nullptr;
	}

	void AddNodeID(const FArcTacticsNodeDefinition& Node, TSet<FGuid>& OutIDs)
	{
		OutIDs.Add(Node.ID);
	}

	void AddActionIDs(const FArcTacticsActionDefinition& Action, TSet<FGuid>& OutIDs)
	{
		OutIDs.Add(Action.ParametersID);
		OutIDs.Add(Action.ParameterBagID);
		AddNodeID(Action.Action, OutIDs);
		for (const FArcTacticsNodeDefinition& Step : Action.Steps)
		{
			AddNodeID(Step, OutIDs);
		}
		for (const FArcTacticsNodeDefinition& ActiveStep : Action.ActiveSteps)
		{
			AddNodeID(ActiveStep, OutIDs);
		}
		for (const FArcTacticsNodeDefinition& Consideration : Action.Considerations)
		{
			AddNodeID(Consideration, OutIDs);
		}
	}

	void AddSelectorIDs(const FArcTacticsSelectorDefinition& Selector, TSet<FGuid>& OutIDs)
	{
		AddNodeID(Selector.Selector, OutIDs);
		OutIDs.Add(Selector.ParametersID);
		OutIDs.Add(Selector.ParameterBagID);
		AddNodeID(Selector.Action, OutIDs);
		for (const FArcTacticsNodeDefinition& Step : Selector.Steps)
		{
			AddNodeID(Step, OutIDs);
		}
		for (const FArcTacticsNodeDefinition& ActiveStep : Selector.ActiveSteps)
		{
			AddNodeID(ActiveStep, OutIDs);
		}
		for (const FArcTacticsNodeDefinition& Consideration : Selector.Considerations)
		{
			AddNodeID(Consideration, OutIDs);
		}
	}

	void CollectBindings(
		const UArcTacticsConfig& Config,
		TSet<FGuid> TargetIDs,
		TArray<FArcTacticsBinding>& OutBindings)
	{
		UArcTacticsConfig::AddOwnedFunctionIDs(Config.EditorBindings.GetBindings(), TargetIDs);
		for (const FArcTacticsBinding& Binding : Config.EditorBindings.GetBindings())
		{
			if (TargetIDs.Contains(Binding.GetTargetPath().GetStructID()))
			{
				OutBindings.Add(Binding);
			}
		}
	}
}

bool FArcTacticsEditorClipboard::Copy(
	const UArcTacticsConfig& Config,
	const FArcTacticsEditorSelection& Selection,
	FText& OutError)
{
	FArcTacticsEditorClipboardPayload Payload;
	Payload.Kind = Selection.Kind;
	Payload.SourceAsset = FSoftObjectPath(&Config);
	Payload.SourceContextID = Config.ContextID;
	Payload.SourceGlobalParametersID = Config.GlobalParametersID;
	TSet<FGuid> TargetIDs;

	switch (Selection.Kind)
	{
	case EArcTacticsEditorItemKind::GlobalTask:
		if (const FArcTacticsNodeDefinition* Node = Config.GlobalTasks.FindByPredicate(
			[ID = Selection.ID](const FArcTacticsNodeDefinition& Candidate)
			{
				return Candidate.ID == ID;
			}))
		{
			Payload.Node = *Node;
			ArcTacticsEditorClipboardPrivate::AddNodeID(*Node, TargetIDs);
		}
		break;
	case EArcTacticsEditorItemKind::Action:
		if (const FArcTacticsActionDefinition* Action =
			ArcTacticsEditorClipboardPrivate::FindAction(Config, Selection.ID))
		{
			Payload.Action = *Action;
			Payload.SourceScopeParametersID = Action->ParametersID;
			Payload.SourceScopeParameterBagID = Action->ParameterBagID;
			ArcTacticsEditorClipboardPrivate::AddActionIDs(*Action, TargetIDs);
		}
		break;
	case EArcTacticsEditorItemKind::Selector:
		if (const FArcTacticsSelectorDefinition* Selector = Config.FindSelectorByID(Selection.ID))
		{
			Payload.Selector = *Selector;
			Payload.SourceScopeParametersID = Selector->ParametersID;
			Payload.SourceScopeParameterBagID = Selector->ParameterBagID;
			ArcTacticsEditorClipboardPrivate::AddSelectorIDs(*Selector, TargetIDs);
		}
		break;
	case EArcTacticsEditorItemKind::Step:
	case EArcTacticsEditorItemKind::ActiveStep:
	case EArcTacticsEditorItemKind::Consideration:
		if (const FArcTacticsNodeDefinition* Node =
			ArcTacticsEditorClipboardPrivate::FindBranchNode(
				Config,
				Selection.ID,
				Selection.Kind,
				Payload.SourceScopeParametersID,
				Payload.SourceScopeParameterBagID,
				Payload.SourceScopeSelectorID))
		{
			Payload.Node = *Node;
			ArcTacticsEditorClipboardPrivate::AddNodeID(*Node, TargetIDs);
		}
		break;
	case EArcTacticsEditorItemKind::Context:
	default:
		break;
	}

	if (TargetIDs.IsEmpty())
	{
		OutError = LOCTEXT("UnsupportedCopy", "The selected item cannot be copied.");
		return false;
	}

	ArcTacticsEditorClipboardPrivate::CollectBindings(Config, TargetIDs, Payload.Bindings);
	FString ExportedText;
	FArcTacticsEditorClipboardPayload::StaticStruct()->ExportText(
		ExportedText,
		&Payload,
		nullptr,
		nullptr,
		PPF_Copy,
		nullptr);
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
	OutError = FText::GetEmpty();
	return true;
}

bool FArcTacticsEditorClipboard::Read(
	FArcTacticsEditorClipboardPayload& OutPayload,
	FText& OutError)
{
	FString PastedText;
	FPlatformApplicationMisc::ClipboardPaste(PastedText);
	if (PastedText.IsEmpty())
	{
		OutError = LOCTEXT("EmptyClipboard", "The clipboard is empty.");
		return false;
	}

	OutPayload = FArcTacticsEditorClipboardPayload();
	ArcTacticsEditorClipboardPrivate::FImportErrorContext ErrorContext;
	const UScriptStruct* PayloadStruct = FArcTacticsEditorClipboardPayload::StaticStruct();
	PayloadStruct->ImportText(
		*PastedText,
		&OutPayload,
		nullptr,
		PPF_None,
		&ErrorContext,
		PayloadStruct->GetName());
	if (ErrorContext.NumErrors > 0
		|| OutPayload.Version != ArcTacticsEditorClipboardPrivate::CurrentVersion
		|| !OutPayload.SourceAsset.IsValid()
		|| !OutPayload.SourceContextID.IsValid()
		|| !OutPayload.SourceGlobalParametersID.IsValid())
	{
		OutPayload = FArcTacticsEditorClipboardPayload();
		OutError = LOCTEXT("InvalidClipboard", "The clipboard does not contain valid Arc Tactics authoring data.");
		return false;
	}

	const bool bHasExpectedRecord =
		(OutPayload.Kind == EArcTacticsEditorItemKind::GlobalTask && OutPayload.Node.ID.IsValid())
		|| (OutPayload.Kind == EArcTacticsEditorItemKind::Action && OutPayload.Action.Action.ID.IsValid())
		|| (OutPayload.Kind == EArcTacticsEditorItemKind::Selector && OutPayload.Selector.Selector.ID.IsValid())
		|| (OutPayload.Kind == EArcTacticsEditorItemKind::Step && OutPayload.Node.ID.IsValid())
		|| (OutPayload.Kind == EArcTacticsEditorItemKind::ActiveStep && OutPayload.Node.ID.IsValid())
		|| (OutPayload.Kind == EArcTacticsEditorItemKind::Consideration && OutPayload.Node.ID.IsValid());
	if (!bHasExpectedRecord)
	{
		OutPayload = FArcTacticsEditorClipboardPayload();
		OutError = LOCTEXT("UnsupportedClipboardRecord", "The clipboard item kind or record is not supported.");
		return false;
	}

	OutError = FText::GetEmpty();
	return true;
}

#undef LOCTEXT_NAMESPACE
