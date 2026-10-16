// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTacticsEditorViewModel.h"

#include "ArcTQSPropertyFunction.h"
#include "ArcTacticsAuthoring.h"
#include "Decision/ArcTacticsConfig.h"
#include "Decision/ArcTacticsSelectors.h"
#include "Editor.h"
#include "PropertyBindingTypes.h"
#include "ScopedTransaction.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "ArcTacticsEditorViewModel"

namespace ArcTacticsEditorViewModelPrivate
{
	struct FBranchNodeLocation
	{
		TArray<FArcTacticsNodeDefinition>* Items = nullptr;
		FArcTacticsEditorSelection Owner;
		int32 Index = INDEX_NONE;
	};

	struct FConstBranchNodeLocation
	{
		const TArray<FArcTacticsNodeDefinition>* Items = nullptr;
		FArcTacticsEditorSelection Owner;
		int32 Index = INDEX_NONE;
	};

	void SetError(FText* OutError, const FText& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
	}

	int32 FindNodeIndex(const TArray<FArcTacticsNodeDefinition>& Nodes, const FGuid& ID)
	{
		return Nodes.IndexOfByPredicate([ID](const FArcTacticsNodeDefinition& Node)
		{
			return Node.ID == ID;
		});
	}

	int32 FindActionIndex(const UArcTacticsConfig& Config, const FGuid& ID)
	{
		return Config.Actions.IndexOfByPredicate([ID](const FArcTacticsActionDefinition& Action)
		{
			return Action.Action.ID == ID;
		});
	}

	int32 FindSelectorIndex(const UArcTacticsConfig& Config, const FGuid& ID)
	{
		return Config.Selectors.IndexOfByPredicate([ID](const FArcTacticsSelectorDefinition& Selector)
		{
			return Selector.Selector.ID == ID;
		});
	}

	bool IsBranchNodeKind(const EArcTacticsEditorItemKind Kind)
	{
		return Kind == EArcTacticsEditorItemKind::Step
			|| Kind == EArcTacticsEditorItemKind::ActiveStep
			|| Kind == EArcTacticsEditorItemKind::Consideration;
	}

	template <typename DefinitionType>
	TArray<FArcTacticsNodeDefinition>& GetBranchNodes(
		DefinitionType& Definition,
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

	bool FindBranchNode(
		UArcTacticsConfig& Config,
		const EArcTacticsEditorItemKind Kind,
		const FGuid& ID,
		FBranchNodeLocation& OutLocation)
	{
		for (FArcTacticsActionDefinition& Action : Config.Actions)
		{
			TArray<FArcTacticsNodeDefinition>& Nodes = GetBranchNodes(Action, Kind);
			const int32 Index = FindNodeIndex(Nodes, ID);
			if (Index != INDEX_NONE)
			{
				OutLocation.Items = &Nodes;
				OutLocation.Owner = {EArcTacticsEditorItemKind::Action, Action.Action.ID};
				OutLocation.Index = Index;
				return true;
			}
		}
		for (FArcTacticsSelectorDefinition& Selector : Config.Selectors)
		{
			TArray<FArcTacticsNodeDefinition>& Nodes = GetBranchNodes(Selector, Kind);
			const int32 Index = FindNodeIndex(Nodes, ID);
			if (Index != INDEX_NONE)
			{
				OutLocation.Items = &Nodes;
				OutLocation.Owner = {EArcTacticsEditorItemKind::SelectorAction, Selector.Action.ID};
				OutLocation.Index = Index;
				return true;
			}
		}
		return false;
	}

	bool FindBranchNode(
		const UArcTacticsConfig& Config,
		const EArcTacticsEditorItemKind Kind,
		const FGuid& ID,
		FConstBranchNodeLocation& OutLocation)
	{
		FBranchNodeLocation MutableLocation;
		if (!FindBranchNode(const_cast<UArcTacticsConfig&>(Config), Kind, ID, MutableLocation))
		{
			return false;
		}
		OutLocation.Items = MutableLocation.Items;
		OutLocation.Owner = MutableLocation.Owner;
		OutLocation.Index = MutableLocation.Index;
		return true;
	}

	bool ResolveBranchNodeOwner(
		UArcTacticsConfig& Config,
		const EArcTacticsEditorItemKind Kind,
		const FArcTacticsEditorSelection& Selection,
		FBranchNodeLocation& OutLocation)
	{
		if (Selection.Kind == Kind)
		{
			return FindBranchNode(Config, Kind, Selection.ID, OutLocation);
		}
		if (Selection.Kind == EArcTacticsEditorItemKind::Action)
		{
			const int32 ActionIndex = FindActionIndex(Config, Selection.ID);
			if (ActionIndex != INDEX_NONE)
			{
				OutLocation.Items = &GetBranchNodes(Config.Actions[ActionIndex], Kind);
				OutLocation.Owner = Selection;
				return true;
			}
		}
		if (Selection.Kind == EArcTacticsEditorItemKind::Selector)
		{
			const int32 SelectorIndex = FindSelectorIndex(Config, Selection.ID);
			if (SelectorIndex != INDEX_NONE)
			{
				FArcTacticsSelectorDefinition& Selector = Config.Selectors[SelectorIndex];
				OutLocation.Items = &GetBranchNodes(Selector, Kind);
				OutLocation.Owner = {EArcTacticsEditorItemKind::SelectorAction, Selector.Action.ID};
				return true;
			}
		}
		if (Selection.Kind == EArcTacticsEditorItemKind::SelectorAction)
		{
			if (FArcTacticsSelectorDefinition* Selector = Config.FindOwningSelector(Selection.ID))
			{
				if (Selector->Action.ID == Selection.ID)
				{
					OutLocation.Items = &GetBranchNodes(*Selector, Kind);
					OutLocation.Owner = Selection;
					return true;
				}
			}
		}
		return false;
	}

	bool ResolveBranchNodeOwner(
		const UArcTacticsConfig& Config,
		const EArcTacticsEditorItemKind Kind,
		const FArcTacticsEditorSelection& Selection,
		FConstBranchNodeLocation& OutLocation)
	{
		FBranchNodeLocation MutableLocation;
		if (!ResolveBranchNodeOwner(
			const_cast<UArcTacticsConfig&>(Config), Kind, Selection, MutableLocation))
		{
			return false;
		}
		OutLocation.Items = MutableLocation.Items;
		OutLocation.Owner = MutableLocation.Owner;
		OutLocation.Index = MutableLocation.Index;
		return true;
	}

	EArcTacticsNodeRole GetExpectedRole(const EArcTacticsEditorItemKind Kind)
	{
		switch (Kind)
		{
		case EArcTacticsEditorItemKind::GlobalTask:
			return EArcTacticsNodeRole::GlobalTask;
		case EArcTacticsEditorItemKind::Action:
		case EArcTacticsEditorItemKind::SelectorAction:
			return EArcTacticsNodeRole::Action;
		case EArcTacticsEditorItemKind::Step:
			return EArcTacticsNodeRole::Step;
		case EArcTacticsEditorItemKind::ActiveStep:
			return EArcTacticsNodeRole::ActiveStep;
		case EArcTacticsEditorItemKind::Consideration:
			return EArcTacticsNodeRole::Consideration;
		case EArcTacticsEditorItemKind::Selector:
			return EArcTacticsNodeRole::Selector;
		default:
			return EArcTacticsNodeRole::Invalid;
		}
	}

	const FArcTacticsNodeDefinition* ResolveNode(
		const UArcTacticsConfig& Config,
		const FArcTacticsEditorSelection& Selection)
	{
		if (Selection.Kind == EArcTacticsEditorItemKind::GlobalTask
			|| Selection.Kind == EArcTacticsEditorItemKind::Action
			|| Selection.Kind == EArcTacticsEditorItemKind::Selector
			|| Selection.Kind == EArcTacticsEditorItemKind::SelectorAction
			|| IsBranchNodeKind(Selection.Kind))
		{
			return Config.FindNodeByID(Selection.ID);
		}
		return nullptr;
	}

	FArcTacticsNodeDefinition* ResolveNode(
		UArcTacticsConfig& Config,
		const FArcTacticsEditorSelection& Selection)
	{
		return const_cast<FArcTacticsNodeDefinition*>(
			ResolveNode(static_cast<const UArcTacticsConfig&>(Config), Selection));
	}

	void AddPayloadIDs(const FArcTacticsEditorClipboardPayload& Payload, TSet<FGuid>& OutIDs)
	{
		switch (Payload.Kind)
		{
		case EArcTacticsEditorItemKind::GlobalTask:
		case EArcTacticsEditorItemKind::Step:
		case EArcTacticsEditorItemKind::ActiveStep:
		case EArcTacticsEditorItemKind::Consideration:
			OutIDs.Add(Payload.Node.ID);
			break;
		case EArcTacticsEditorItemKind::Action:
			OutIDs.Add(Payload.Action.ParametersID);
			OutIDs.Add(Payload.Action.ParameterBagID);
			OutIDs.Add(Payload.Action.Action.ID);
			for (const FArcTacticsNodeDefinition& Step : Payload.Action.Steps)
			{
				OutIDs.Add(Step.ID);
			}
			for (const FArcTacticsNodeDefinition& ActiveStep : Payload.Action.ActiveSteps)
			{
				OutIDs.Add(ActiveStep.ID);
			}
			for (const FArcTacticsNodeDefinition& Consideration : Payload.Action.Considerations)
			{
				OutIDs.Add(Consideration.ID);
			}
			break;
		case EArcTacticsEditorItemKind::Selector:
			OutIDs.Add(Payload.Selector.Selector.ID);
			OutIDs.Add(Payload.Selector.ParametersID);
			OutIDs.Add(Payload.Selector.ParameterBagID);
			OutIDs.Add(Payload.Selector.Action.ID);
			for (const FArcTacticsNodeDefinition& Step : Payload.Selector.Steps)
			{
				OutIDs.Add(Step.ID);
			}
			for (const FArcTacticsNodeDefinition& ActiveStep : Payload.Selector.ActiveSteps)
			{
				OutIDs.Add(ActiveStep.ID);
			}
			for (const FArcTacticsNodeDefinition& Consideration : Payload.Selector.Considerations)
			{
				OutIDs.Add(Consideration.ID);
			}
			break;
		default:
			break;
		}
	}

	void AddPayloadTargetIDs(const FArcTacticsEditorClipboardPayload& Payload, TSet<FGuid>& OutIDs)
	{
		switch (Payload.Kind)
		{
		case EArcTacticsEditorItemKind::GlobalTask:
		case EArcTacticsEditorItemKind::Step:
		case EArcTacticsEditorItemKind::ActiveStep:
		case EArcTacticsEditorItemKind::Consideration:
			OutIDs.Add(Payload.Node.ID);
			break;
		case EArcTacticsEditorItemKind::Action:
			OutIDs.Add(Payload.Action.ParametersID);
			OutIDs.Add(Payload.Action.ParameterBagID);
			OutIDs.Add(Payload.Action.Action.ID);
			for (const FArcTacticsNodeDefinition& Step : Payload.Action.Steps)
			{
				OutIDs.Add(Step.ID);
			}
			for (const FArcTacticsNodeDefinition& ActiveStep : Payload.Action.ActiveSteps)
			{
				OutIDs.Add(ActiveStep.ID);
			}
			for (const FArcTacticsNodeDefinition& Consideration : Payload.Action.Considerations)
			{
				OutIDs.Add(Consideration.ID);
			}
			break;
		case EArcTacticsEditorItemKind::Selector:
			OutIDs.Add(Payload.Selector.Selector.ID);
			OutIDs.Add(Payload.Selector.ParametersID);
			OutIDs.Add(Payload.Selector.ParameterBagID);
			OutIDs.Add(Payload.Selector.Action.ID);
			for (const FArcTacticsNodeDefinition& Step : Payload.Selector.Steps)
			{
				OutIDs.Add(Step.ID);
			}
			for (const FArcTacticsNodeDefinition& ActiveStep : Payload.Selector.ActiveSteps)
			{
				OutIDs.Add(ActiveStep.ID);
			}
			for (const FArcTacticsNodeDefinition& Consideration : Payload.Selector.Considerations)
			{
				OutIDs.Add(Consideration.ID);
			}
			break;
		default:
			break;
		}
	}

	const FArcTacticsNodeDefinition* FindPayloadNode(
		const FArcTacticsEditorClipboardPayload& Payload,
		const FGuid& ID)
	{
		if ((Payload.Kind == EArcTacticsEditorItemKind::GlobalTask
			|| IsBranchNodeKind(Payload.Kind))
			&& Payload.Node.ID == ID)
		{
			return &Payload.Node;
		}
		if (Payload.Kind == EArcTacticsEditorItemKind::Action)
		{
			if (Payload.Action.Action.ID == ID)
			{
				return &Payload.Action.Action;
			}
			if (const FArcTacticsNodeDefinition* Step = Payload.Action.Steps.FindByPredicate(
				[ID](const FArcTacticsNodeDefinition& Node)
				{
					return Node.ID == ID;
				}))
			{
				return Step;
			}
			if (const FArcTacticsNodeDefinition* ActiveStep = Payload.Action.ActiveSteps.FindByPredicate(
				[ID](const FArcTacticsNodeDefinition& Node)
				{
					return Node.ID == ID;
				}))
			{
				return ActiveStep;
			}
			return Payload.Action.Considerations.FindByPredicate(
				[ID](const FArcTacticsNodeDefinition& Node)
				{
					return Node.ID == ID;
				});
		}
		if (Payload.Kind == EArcTacticsEditorItemKind::Selector)
		{
			if (Payload.Selector.Selector.ID == ID)
			{
				return &Payload.Selector.Selector;
			}
			if (Payload.Selector.Action.ID == ID)
			{
				return &Payload.Selector.Action;
			}
			if (const FArcTacticsNodeDefinition* Step = Payload.Selector.Steps.FindByPredicate(
				[ID](const FArcTacticsNodeDefinition& Node)
				{
					return Node.ID == ID;
				}))
			{
				return Step;
			}
			if (const FArcTacticsNodeDefinition* ActiveStep = Payload.Selector.ActiveSteps.FindByPredicate(
				[ID](const FArcTacticsNodeDefinition& Node)
				{
					return Node.ID == ID;
				}))
			{
				return ActiveStep;
			}
			return Payload.Selector.Considerations.FindByPredicate(
				[ID](const FArcTacticsNodeDefinition& Node)
				{
					return Node.ID == ID;
				});
		}
		return nullptr;
	}

	bool ValidatePayloadNode(
		const UArcTacticsConfig& Config,
		const FArcTacticsNodeDefinition& Source,
		const EArcTacticsNodeRole ExpectedRole)
	{
		FArcTacticsNodeDefinition Node = Source;
		return Node.Node.IsValid()
			&& Config.SynchronizeNode(Node, ExpectedRole)
			&& Node.InstanceData.IsValid();
	}

	bool ValidateActionNodes(
		const UArcTacticsConfig& Config,
		const FArcTacticsActionDefinition& Action)
	{
		if (!ValidatePayloadNode(Config, Action.Action, EArcTacticsNodeRole::Action))
		{
			return false;
		}
		for (const FArcTacticsNodeDefinition& Step : Action.Steps)
		{
			if (!ValidatePayloadNode(Config, Step, EArcTacticsNodeRole::Step))
			{
				return false;
			}
		}
		for (const FArcTacticsNodeDefinition& ActiveStep : Action.ActiveSteps)
		{
			if (!ValidatePayloadNode(Config, ActiveStep, EArcTacticsNodeRole::ActiveStep))
			{
				return false;
			}
		}
		for (const FArcTacticsNodeDefinition& Consideration : Action.Considerations)
		{
			if (!ValidatePayloadNode(Config, Consideration, EArcTacticsNodeRole::Consideration))
			{
				return false;
			}
		}
		return true;
	}

	bool ValidateSelectorNodes(
		const UArcTacticsConfig& Config,
		const FArcTacticsSelectorDefinition& Selector)
	{
		if (!ValidatePayloadNode(Config, Selector.Selector, EArcTacticsNodeRole::Selector))
		{
			return false;
		}
		if (Selector.Action.Node.IsValid()
			? !ValidatePayloadNode(Config, Selector.Action, EArcTacticsNodeRole::Action)
			: Selector.Action.InstanceData.IsValid())
		{
			return false;
		}
		for (const FArcTacticsNodeDefinition& Step : Selector.Steps)
		{
			if (!ValidatePayloadNode(Config, Step, EArcTacticsNodeRole::Step))
			{
				return false;
			}
		}
		for (const FArcTacticsNodeDefinition& ActiveStep : Selector.ActiveSteps)
		{
			if (!ValidatePayloadNode(Config, ActiveStep, EArcTacticsNodeRole::ActiveStep))
			{
				return false;
			}
		}
		for (const FArcTacticsNodeDefinition& Consideration : Selector.Considerations)
		{
			if (!ValidatePayloadNode(Config, Consideration, EArcTacticsNodeRole::Consideration))
			{
				return false;
			}
		}
		return true;
	}

	bool ValidatePayloadNodes(
		const UArcTacticsConfig& Config,
		const FArcTacticsEditorClipboardPayload& Payload)
	{
		TSet<FGuid> IDs;
		AddPayloadIDs(Payload, IDs);
		int32 ExpectedIDCount = 0;
		switch (Payload.Kind)
		{
		case EArcTacticsEditorItemKind::GlobalTask:
		case EArcTacticsEditorItemKind::Step:
		case EArcTacticsEditorItemKind::ActiveStep:
		case EArcTacticsEditorItemKind::Consideration:
			ExpectedIDCount = 1;
			break;
		case EArcTacticsEditorItemKind::Action:
			ExpectedIDCount = 3 + Payload.Action.Steps.Num() + Payload.Action.ActiveSteps.Num()
				+ Payload.Action.Considerations.Num();
			break;
		case EArcTacticsEditorItemKind::Selector:
			ExpectedIDCount = 4 + Payload.Selector.Steps.Num() + Payload.Selector.ActiveSteps.Num()
				+ Payload.Selector.Considerations.Num();
			break;
		default:
			return false;
		}
		if (IDs.Num() != ExpectedIDCount || IDs.Contains(FGuid()))
		{
			return false;
		}
		if (!Payload.SourceContextID.IsValid()
			|| !Payload.SourceGlobalParametersID.IsValid()
			|| Payload.SourceContextID == Payload.SourceGlobalParametersID
			|| IDs.Contains(Payload.SourceContextID)
			|| IDs.Contains(Payload.SourceGlobalParametersID))
		{
			return false;
		}
		if (Payload.Kind == EArcTacticsEditorItemKind::Action
			&& (Payload.SourceScopeParametersID != Payload.Action.ParametersID
				|| Payload.SourceScopeParameterBagID != Payload.Action.ParameterBagID
				|| Payload.SourceScopeSelectorID.IsValid()))
		{
			return false;
		}
		if (Payload.Kind == EArcTacticsEditorItemKind::Selector
			&& (Payload.SourceScopeParametersID != Payload.Selector.ParametersID
				|| Payload.SourceScopeParameterBagID != Payload.Selector.ParameterBagID
				|| Payload.SourceScopeSelectorID.IsValid()))
		{
			return false;
		}
		if (IsBranchNodeKind(Payload.Kind)
			&& (!Payload.SourceScopeParametersID.IsValid()
				|| !Payload.SourceScopeParameterBagID.IsValid()
				|| Payload.SourceScopeParametersID == Payload.SourceScopeParameterBagID
				|| IDs.Contains(Payload.SourceScopeParametersID)
				|| IDs.Contains(Payload.SourceScopeParameterBagID)
				|| Payload.SourceScopeParametersID == Payload.SourceContextID
				|| Payload.SourceScopeParametersID == Payload.SourceGlobalParametersID
				|| Payload.SourceScopeParameterBagID == Payload.SourceContextID
				|| Payload.SourceScopeParameterBagID == Payload.SourceGlobalParametersID
				|| (Payload.SourceScopeSelectorID.IsValid()
					&& (IDs.Contains(Payload.SourceScopeSelectorID)
						|| Payload.SourceScopeSelectorID == Payload.SourceContextID
						|| Payload.SourceScopeSelectorID == Payload.SourceGlobalParametersID
						|| Payload.SourceScopeSelectorID == Payload.SourceScopeParametersID
						|| Payload.SourceScopeSelectorID == Payload.SourceScopeParameterBagID))))
		{
			return false;
		}

		switch (Payload.Kind)
		{
		case EArcTacticsEditorItemKind::GlobalTask:
			return ValidatePayloadNode(Config, Payload.Node, EArcTacticsNodeRole::GlobalTask);
		case EArcTacticsEditorItemKind::Action:
			return FMath::IsFinite(Payload.Action.Parameters.Weight)
				&& Payload.Action.Parameters.Weight >= 0.0f
				&& FMath::IsFinite(Payload.Action.Parameters.Cooldown)
				&& Payload.Action.Parameters.Cooldown >= 0.0f
				&& ValidateActionNodes(Config, Payload.Action);
		case EArcTacticsEditorItemKind::Selector:
			return FMath::IsFinite(Payload.Selector.Parameters.Weight)
				&& Payload.Selector.Parameters.Weight >= 0.0f
				&& FMath::IsFinite(Payload.Selector.Parameters.Cooldown)
				&& Payload.Selector.Parameters.Cooldown >= 0.0f
				&& ValidateSelectorNodes(Config, Payload.Selector);
		case EArcTacticsEditorItemKind::Step:
		case EArcTacticsEditorItemKind::ActiveStep:
		case EArcTacticsEditorItemKind::Consideration:
			return ValidatePayloadNode(Config, Payload.Node, GetExpectedRole(Payload.Kind));
		default:
			return false;
		}
	}

	int32 GetPasteIndex(
		const UArcTacticsConfig& Config,
		const FArcTacticsEditorClipboardPayload& Payload,
		const FArcTacticsEditorSelection& Destination)
	{
		switch (Payload.Kind)
		{
		case EArcTacticsEditorItemKind::GlobalTask:
			return Destination.Kind == EArcTacticsEditorItemKind::GlobalTask
				? FindNodeIndex(Config.GlobalTasks, Destination.ID) + 1
				: Config.GlobalTasks.Num();
		case EArcTacticsEditorItemKind::Action:
			return Destination.Kind == EArcTacticsEditorItemKind::Action
				? FindActionIndex(Config, Destination.ID) + 1
				: Config.Actions.Num();
		case EArcTacticsEditorItemKind::Selector:
			return Destination.Kind == EArcTacticsEditorItemKind::Selector
				? FindSelectorIndex(Config, Destination.ID) + 1
				: Config.Selectors.Num();
		case EArcTacticsEditorItemKind::Step:
		case EArcTacticsEditorItemKind::ActiveStep:
		case EArcTacticsEditorItemKind::Consideration:
		{
			FConstBranchNodeLocation Location;
			if (!ResolveBranchNodeOwner(Config, Payload.Kind, Destination, Location) || !Location.Items)
			{
				return INDEX_NONE;
			}
			return Destination.Kind == Payload.Kind
				? Location.Index + 1
				: Location.Items->Num();
		}
		default:
			return INDEX_NONE;
		}
	}

	bool IsPasteDestinationCompatible(
		const FArcTacticsEditorClipboardPayload& Payload,
		const FArcTacticsEditorSelection& Destination)
	{
		switch (Payload.Kind)
		{
		case EArcTacticsEditorItemKind::GlobalTask:
			return Destination.IsConfig() || Destination.Kind == EArcTacticsEditorItemKind::GlobalTask;
		case EArcTacticsEditorItemKind::Action:
			return Destination.IsConfig() || Destination.Kind == EArcTacticsEditorItemKind::Action;
		case EArcTacticsEditorItemKind::Selector:
			return Destination.IsConfig() || Destination.Kind == EArcTacticsEditorItemKind::Selector;
		case EArcTacticsEditorItemKind::Step:
		case EArcTacticsEditorItemKind::ActiveStep:
			return Destination.Kind == EArcTacticsEditorItemKind::Action
				|| Destination.Kind == EArcTacticsEditorItemKind::SelectorAction
				|| Destination.Kind == Payload.Kind;
		case EArcTacticsEditorItemKind::Consideration:
			return Destination.Kind == EArcTacticsEditorItemKind::Action
				|| Destination.Kind == EArcTacticsEditorItemKind::Selector
				|| Destination.Kind == EArcTacticsEditorItemKind::SelectorAction
				|| Destination.Kind == EArcTacticsEditorItemKind::Consideration;
		default:
			return false;
		}
	}

	int32 FindGlobalTaskIndex(const UArcTacticsConfig& Config, const FGuid& ID)
	{
		return FindNodeIndex(Config.GlobalTasks, ID);
	}

	bool IsSameAssetClipboardCurrent(
		const UArcTacticsConfig& Config,
		const FArcTacticsEditorClipboardPayload& Payload,
		const TSet<FGuid>& TargetIDs)
	{
		int32 CurrentBindingCount = 0;
		for (const FArcTacticsBinding& Current : Config.EditorBindings.GetBindings())
		{
			if (!TargetIDs.Contains(Current.GetTargetPath().GetStructID()))
			{
				continue;
			}
			++CurrentBindingCount;
			const bool bFound = Payload.Bindings.ContainsByPredicate(
				[&Current](const FArcTacticsBinding& Copied)
				{
					return Copied.GetSourcePath() == Current.GetSourcePath()
						&& Copied.GetTargetPath() == Current.GetTargetPath()
						&& Copied.IsOutputBinding() == Current.IsOutputBinding();
				});
			if (!bFound)
			{
				return false;
			}
		}
		return CurrentBindingCount == Payload.Bindings.Num();
	}

	bool IsSameAssetClipboardSource(
		const UArcTacticsConfig& Config,
		const FArcTacticsEditorClipboardPayload& Payload)
	{
		return Payload.SourceAsset == FSoftObjectPath(&Config)
			&& Payload.SourceContextID == Config.ContextID;
	}

	struct FScopeIDs
	{
		FGuid ParametersID;
		FGuid ParameterBagID;
		FGuid SelectorID;
	};

	bool ResolveScopeIDs(
		const UArcTacticsConfig& Config,
		const FArcTacticsEditorSelection& Owner,
		FScopeIDs& OutScope)
	{
		if (Owner.Kind == EArcTacticsEditorItemKind::Action)
		{
			const int32 ActionIndex = FindActionIndex(Config, Owner.ID);
			if (ActionIndex != INDEX_NONE)
			{
				const FArcTacticsActionDefinition& Action = Config.Actions[ActionIndex];
				OutScope.ParametersID = Action.ParametersID;
				OutScope.ParameterBagID = Action.ParameterBagID;
				return true;
			}
		}
		if (Owner.Kind == EArcTacticsEditorItemKind::SelectorAction)
		{
			if (const FArcTacticsSelectorDefinition* Selector = Config.FindOwningSelector(Owner.ID))
			{
				OutScope.ParametersID = Selector->ParametersID;
				OutScope.ParameterBagID = Selector->ParameterBagID;
				OutScope.SelectorID = Selector->Selector.ID;
				return true;
			}
		}
		return false;
	}

	FGuid MapPayloadSourceID(
		const UArcTacticsConfig& Config,
		const FArcTacticsEditorClipboardPayload& Payload,
		const FScopeIDs* DestinationScope,
		const FGuid SourceID)
	{
		if (SourceID == Payload.SourceContextID)
		{
			return Config.ContextID;
		}
		if (SourceID == Payload.SourceGlobalParametersID)
		{
			return Config.GlobalParametersID;
		}
		if (IsBranchNodeKind(Payload.Kind) && DestinationScope)
		{
			if (SourceID == Payload.SourceScopeParametersID)
			{
				return DestinationScope->ParametersID;
			}
			if (SourceID == Payload.SourceScopeParameterBagID)
			{
				return DestinationScope->ParameterBagID;
			}
			if (SourceID == Payload.SourceScopeSelectorID)
			{
				return DestinationScope->SelectorID;
			}
		}
		return SourceID;
	}

	FGuid MapScopeSourceID(
		const FScopeIDs& SourceScope,
		const FScopeIDs& DestinationScope,
		const FGuid SourceID)
	{
		if (SourceID == SourceScope.ParametersID)
		{
			return DestinationScope.ParametersID;
		}
		if (SourceID == SourceScope.ParameterBagID)
		{
			return DestinationScope.ParameterBagID;
		}
		if (SourceID == SourceScope.SelectorID)
		{
			return DestinationScope.SelectorID;
		}
		return SourceID;
	}

	bool ValidateMovedConsiderationBinding(
		const UArcTacticsConfig& Config,
		const FArcTacticsBinding& Binding,
		const FScopeIDs& SourceScope,
		const FScopeIDs& DestinationScope,
		const FArcTacticsBindingEndpoint& TargetEndpoint)
	{
		const FGuid SourceID = MapScopeSourceID(
			SourceScope, DestinationScope, Binding.GetSourcePath().GetStructID());
		if (!SourceID.IsValid())
		{
			return false;
		}
		FArcTacticsBinding RemappedBinding(
			FPropertyBindingPath(SourceID, Binding.GetSourcePath().GetSegments()),
			Binding.GetTargetPath(),
			Binding.IsOutputBinding());
		TInstancedStruct<FPropertyBindingBindableStructDescriptor> SourceDescriptor;
		TInstancedStruct<FPropertyBindingBindableStructDescriptor> TargetDescriptor;
		FArcTacticsBindingEndpoint SourceEndpoint;
		if (!Config.GetBindableStructByID(SourceID, SourceDescriptor)
			|| !Config.GetBindableStructByID(Binding.GetTargetPath().GetStructID(), TargetDescriptor)
			|| !Config.ResolveBindingEndpoint(SourceID, SourceEndpoint))
		{
			return false;
		}
		FArcTacticsBindingCollection ValidationCollection;
		return ValidationCollection.ValidateEditorBinding(
			RemappedBinding,
			SourceDescriptor.Get().Struct,
			SourceEndpoint,
			TargetDescriptor.Get().Struct,
			TargetEndpoint,
			Config.IsBindingSourceAccessible(
				SourceEndpoint, TargetEndpoint, Binding.IsOutputBinding()))
			== EArcTacticsBindingValidationResult::Valid;
	}

	bool ResolvePayloadEndpoint(
		const FArcTacticsEditorClipboardPayload& Payload,
		const FGuid ID,
		const int32 PasteIndex,
		const FArcTacticsBindingEndpoint& DestinationOwner,
		const int32 DestinationStepCount,
		FArcTacticsBindingEndpoint& OutEndpoint)
	{
		if (Payload.Kind == EArcTacticsEditorItemKind::GlobalTask && Payload.Node.ID == ID)
		{
			OutEndpoint = FArcTacticsBindingEndpoint(
				ID, EArcTacticsBindingEndpointKind::Node, EArcTacticsBindingOwner::GlobalTask,
				PasteIndex, EArcTacticsBindingPhase::GlobalTask, PasteIndex);
			return true;
		}
		if (Payload.Kind == EArcTacticsEditorItemKind::Consideration && Payload.Node.ID == ID)
		{
			OutEndpoint = FArcTacticsBindingEndpoint(
				ID, EArcTacticsBindingEndpointKind::Node, DestinationOwner.Owner,
				DestinationOwner.OwnerIndex, EArcTacticsBindingPhase::Consideration, PasteIndex);
			return true;
		}
		if (Payload.Kind == EArcTacticsEditorItemKind::Step && Payload.Node.ID == ID)
		{
			OutEndpoint = FArcTacticsBindingEndpoint(
				ID, EArcTacticsBindingEndpointKind::Node, DestinationOwner.Owner,
				DestinationOwner.OwnerIndex, EArcTacticsBindingPhase::Step,
				DestinationOwner.ExecutionIndex + 1 + PasteIndex);
			return true;
		}
		if (Payload.Kind == EArcTacticsEditorItemKind::ActiveStep && Payload.Node.ID == ID)
		{
			OutEndpoint = FArcTacticsBindingEndpoint(
				ID, EArcTacticsBindingEndpointKind::Node, DestinationOwner.Owner,
				DestinationOwner.OwnerIndex, EArcTacticsBindingPhase::Step,
				DestinationOwner.ExecutionIndex + 1 + DestinationStepCount + PasteIndex);
			return true;
		}
		if (Payload.Kind == EArcTacticsEditorItemKind::Action)
		{
			if (Payload.Action.ParametersID == ID)
			{
				OutEndpoint = FArcTacticsBindingEndpoint(
					ID, EArcTacticsBindingEndpointKind::BranchParameters, EArcTacticsBindingOwner::Action,
					PasteIndex, EArcTacticsBindingPhase::BranchInitialization);
				return true;
			}
			if (Payload.Action.ParameterBagID == ID)
			{
				OutEndpoint = FArcTacticsBindingEndpoint(
					ID, EArcTacticsBindingEndpointKind::BranchParameterBag, EArcTacticsBindingOwner::Action,
					PasteIndex, EArcTacticsBindingPhase::BranchInitialization);
				return true;
			}
			if (Payload.Action.Action.ID == ID)
			{
				OutEndpoint = FArcTacticsBindingEndpoint(
					ID, EArcTacticsBindingEndpointKind::Node, EArcTacticsBindingOwner::Action,
					PasteIndex, EArcTacticsBindingPhase::Action, Payload.Action.Considerations.Num());
				return true;
			}
			const int32 StepIndex = FindNodeIndex(Payload.Action.Steps, ID);
			if (StepIndex != INDEX_NONE)
			{
				OutEndpoint = FArcTacticsBindingEndpoint(
					ID, EArcTacticsBindingEndpointKind::Node, EArcTacticsBindingOwner::Action,
					PasteIndex, EArcTacticsBindingPhase::Step,
					Payload.Action.Considerations.Num() + 1 + StepIndex);
				return true;
			}
			const int32 ActiveStepIndex = FindNodeIndex(Payload.Action.ActiveSteps, ID);
			if (ActiveStepIndex != INDEX_NONE)
			{
				OutEndpoint = FArcTacticsBindingEndpoint(
					ID, EArcTacticsBindingEndpointKind::Node, EArcTacticsBindingOwner::Action,
					PasteIndex, EArcTacticsBindingPhase::Step,
					Payload.Action.Considerations.Num() + 1 + Payload.Action.Steps.Num() + ActiveStepIndex);
				return true;
			}
			const int32 ConsiderationIndex = FindNodeIndex(Payload.Action.Considerations, ID);
			if (ConsiderationIndex != INDEX_NONE)
			{
				OutEndpoint = FArcTacticsBindingEndpoint(
					ID, EArcTacticsBindingEndpointKind::Node, EArcTacticsBindingOwner::Action,
					PasteIndex, EArcTacticsBindingPhase::Consideration, ConsiderationIndex);
				return true;
			}
		}
		if (Payload.Kind == EArcTacticsEditorItemKind::Selector)
		{
			if (Payload.Selector.Selector.ID == ID)
			{
				OutEndpoint = FArcTacticsBindingEndpoint(
					ID, EArcTacticsBindingEndpointKind::Node, EArcTacticsBindingOwner::Selector,
					PasteIndex, EArcTacticsBindingPhase::Selector);
				return true;
			}
			if (Payload.Selector.ParametersID == ID)
			{
				OutEndpoint = FArcTacticsBindingEndpoint(
					ID, EArcTacticsBindingEndpointKind::BranchParameters, EArcTacticsBindingOwner::Selector,
					PasteIndex, EArcTacticsBindingPhase::BranchInitialization);
				return true;
			}
			if (Payload.Selector.ParameterBagID == ID)
			{
				OutEndpoint = FArcTacticsBindingEndpoint(
					ID, EArcTacticsBindingEndpointKind::BranchParameterBag, EArcTacticsBindingOwner::Selector,
					PasteIndex, EArcTacticsBindingPhase::BranchInitialization);
				return true;
			}
			if (Payload.Selector.Action.ID == ID)
			{
				OutEndpoint = FArcTacticsBindingEndpoint(
					ID, EArcTacticsBindingEndpointKind::Node, EArcTacticsBindingOwner::Selector,
					PasteIndex, EArcTacticsBindingPhase::Action, Payload.Selector.Considerations.Num());
				return true;
			}
			const int32 StepIndex = FindNodeIndex(Payload.Selector.Steps, ID);
			if (StepIndex != INDEX_NONE)
			{
				OutEndpoint = FArcTacticsBindingEndpoint(
					ID, EArcTacticsBindingEndpointKind::Node, EArcTacticsBindingOwner::Selector,
					PasteIndex, EArcTacticsBindingPhase::Step,
					Payload.Selector.Considerations.Num() + 1 + StepIndex);
				return true;
			}
			const int32 ActiveStepIndex = FindNodeIndex(Payload.Selector.ActiveSteps, ID);
			if (ActiveStepIndex != INDEX_NONE)
			{
				OutEndpoint = FArcTacticsBindingEndpoint(
					ID, EArcTacticsBindingEndpointKind::Node, EArcTacticsBindingOwner::Selector,
					PasteIndex, EArcTacticsBindingPhase::Step,
					Payload.Selector.Considerations.Num() + 1 + Payload.Selector.Steps.Num() + ActiveStepIndex);
				return true;
			}
			const int32 ConsiderationIndex = FindNodeIndex(Payload.Selector.Considerations, ID);
			if (ConsiderationIndex != INDEX_NONE)
			{
				OutEndpoint = FArcTacticsBindingEndpoint(
					ID, EArcTacticsBindingEndpointKind::Node, EArcTacticsBindingOwner::Selector,
					PasteIndex, EArcTacticsBindingPhase::Consideration, ConsiderationIndex);
				return true;
			}
		}
		return false;
	}

	const FArcTacticsBinding* FindPayloadFunctionBinding(
		const FArcTacticsEditorClipboardPayload& Payload,
		const FGuid& FunctionID)
	{
		if (!FunctionID.IsValid())
		{
			return nullptr;
		}
		return Payload.Bindings.FindByPredicate([&FunctionID](const FArcTacticsBinding& Binding)
		{
			const FArcTQSPropertyFunction* Function = Binding.GetPropertyFunctionNode().GetPtr<const FArcTQSPropertyFunction>();
			return Function && Function->ID == FunctionID;
		});
	}

	bool ResolvePayloadBindingEndpoint(
		const FArcTacticsEditorClipboardPayload& Payload,
		const FGuid ID,
		const int32 PasteIndex,
		const FArcTacticsBindingEndpoint& DestinationOwner,
		const int32 DestinationStepCount,
		FArcTacticsBindingEndpoint& OutEndpoint)
	{
		FGuid RootID = ID;
		for (int32 Hop = 0; Hop <= Payload.Bindings.Num(); ++Hop)
		{
			const FArcTacticsBinding* FunctionBinding = FindPayloadFunctionBinding(Payload, RootID);
			if (!FunctionBinding)
			{
				break;
			}
			RootID = FunctionBinding->GetTargetPath().GetStructID();
		}
		if (!ResolvePayloadEndpoint(Payload, RootID, PasteIndex, DestinationOwner, DestinationStepCount, OutEndpoint))
		{
			return false;
		}
		if (RootID != ID)
		{
			OutEndpoint.ID = ID;
			OutEndpoint.Kind = EArcTacticsBindingEndpointKind::Function;
		}
		return true;
	}

	const UStruct* FindPayloadStruct(
		const FArcTacticsEditorClipboardPayload& Payload,
		const FGuid ID)
	{
		if (const FArcTacticsNodeDefinition* Node = FindPayloadNode(Payload, ID))
		{
			return Node->InstanceData.GetScriptStruct();
		}
		if (const FArcTacticsBinding* FunctionBinding = FindPayloadFunctionBinding(Payload, ID))
		{
			return FunctionBinding->GetPropertyFunctionNode().GetScriptStruct();
		}
		if ((Payload.Kind == EArcTacticsEditorItemKind::Action && Payload.Action.ParametersID == ID)
			|| (Payload.Kind == EArcTacticsEditorItemKind::Selector && Payload.Selector.ParametersID == ID))
		{
			return FArcTacticsBranchParameters::StaticStruct();
		}
		if (Payload.Kind == EArcTacticsEditorItemKind::Action && Payload.Action.ParameterBagID == ID)
		{
			return Payload.Action.ParameterBag.GetPropertyBagStruct();
		}
		if (Payload.Kind == EArcTacticsEditorItemKind::Selector && Payload.Selector.ParameterBagID == ID)
		{
			return Payload.Selector.ParameterBag.GetPropertyBagStruct();
		}
		return nullptr;
	}

	bool ValidateBindingScopes(
		const UArcTacticsConfig& Config,
		const TSet<FGuid>& TargetIDs)
	{
		for (const FArcTacticsBinding& Binding : Config.EditorBindings.GetBindings())
		{
			const FGuid TargetID = Binding.GetTargetPath().GetStructID();
			if (TargetIDs.Contains(TargetID)
				&& !Config.IsBindingSourceAccessible(
					Binding.GetSourcePath().GetStructID(), TargetID, Binding.IsOutputBinding()))
			{
				return false;
			}
		}
		return true;
	}

	bool ValidatePasteBindings(
		UArcTacticsConfig& Config,
		const FArcTacticsEditorClipboardPayload& Payload,
		const FArcTacticsEditorSelection& Destination,
		const int32 PasteIndex,
		FText* OutError)
	{
		TSet<FGuid> TargetIDs;
		AddPayloadTargetIDs(Payload, TargetIDs);
		UArcTacticsConfig::AddOwnedFunctionIDs(Payload.Bindings, TargetIDs);
		const bool bSameAsset = IsSameAssetClipboardSource(Config, Payload);

		if (bSameAsset && !IsSameAssetClipboardCurrent(Config, Payload, TargetIDs))
		{
			SetError(OutError, LOCTEXT("StaleClipboardBindings", "The copied item's bindings changed or no longer exist."));
			return false;
		}

		FConstBranchNodeLocation DestinationOwner;
		FScopeIDs DestinationScope;
		FArcTacticsBindingEndpoint DestinationOwnerEndpoint;
		if (IsBranchNodeKind(Payload.Kind)
			&& (!ResolveBranchNodeOwner(Config, Payload.Kind, Destination, DestinationOwner)
				|| !ResolveScopeIDs(Config, DestinationOwner.Owner, DestinationScope)
				|| !Config.ResolveBindingEndpoint(
					DestinationOwner.Owner.ID, DestinationOwnerEndpoint)))
		{
			SetError(OutError, LOCTEXT("MissingPasteOwner", "The destination action no longer exists."));
			return false;
		}
		int32 DestinationStepCount = 0;
		if (Payload.Kind == EArcTacticsEditorItemKind::ActiveStep)
		{
			FConstBranchNodeLocation DestinationSteps;
			ResolveBranchNodeOwner(Config, EArcTacticsEditorItemKind::Step, DestinationOwner.Owner, DestinationSteps);
			DestinationStepCount = DestinationSteps.Items ? DestinationSteps.Items->Num() : 0;
		}

		for (const FArcTacticsBinding& Binding : Payload.Bindings)
		{
			const FGuid OldSourceID = Binding.GetSourcePath().GetStructID();
			const FGuid TargetID = Binding.GetTargetPath().GetStructID();
			if (!TargetIDs.Contains(TargetID))
			{
				SetError(OutError, LOCTEXT("ExternalPasteTarget", "The clipboard contains a binding target outside the copied item."));
				return false;
			}

			const FGuid SourceID = MapPayloadSourceID(
				Config,
				Payload,
				IsBranchNodeKind(Payload.Kind) ? &DestinationScope : nullptr,
				OldSourceID);
			FArcTacticsBindingEndpoint SourceEndpoint;
			if (!ResolvePayloadBindingEndpoint(
					Payload, OldSourceID, PasteIndex, DestinationOwnerEndpoint, DestinationStepCount, SourceEndpoint)
				&& !Config.ResolveBindingEndpoint(SourceID, SourceEndpoint))
			{
				SetError(OutError, LOCTEXT("MissingPasteSource", "A copied binding depends on a source that does not exist in the destination asset."));
				return false;
			}
			FArcTacticsBindingEndpoint TargetEndpoint;
			if (!ResolvePayloadBindingEndpoint(
				Payload, TargetID, PasteIndex, DestinationOwnerEndpoint, DestinationStepCount, TargetEndpoint))
			{
				SetError(OutError, LOCTEXT("MissingPasteTarget", "A copied binding target does not exist in the copied item."));
				return false;
			}
			const FConstStructView FunctionNode = Binding.GetPropertyFunctionNode();
			const FArcTQSPropertyFunction* Function = FunctionNode.GetPtr<const FArcTQSPropertyFunction>();
			const bool bValidFunction = FunctionNode.IsValid()
				? Function && Function->ID == OldSourceID && !Binding.IsOutputBinding()
				: SourceEndpoint.Kind != EArcTacticsBindingEndpointKind::Function;
			if (!bValidFunction)
			{
				SetError(OutError, LOCTEXT("InvalidPasteFunction", "A copied property function binding is invalid."));
				return false;
			}

			const UStruct* SourceStruct = FindPayloadStruct(Payload, OldSourceID);
			if (!SourceStruct)
			{
				TInstancedStruct<FPropertyBindingBindableStructDescriptor> Descriptor;
				if (Config.GetBindableStructByID(SourceID, Descriptor))
				{
					SourceStruct = Descriptor.Get().Struct;
				}
			}
			const UStruct* TargetStruct = FindPayloadStruct(Payload, TargetID);
			const EArcTacticsBindingValidationResult ValidationResult =
				Config.EditorBindings.ValidateEditorBinding(
					Binding,
					SourceStruct,
					SourceEndpoint,
					TargetStruct,
					TargetEndpoint,
					Config.IsBindingSourceAccessible(
						SourceEndpoint, TargetEndpoint, Binding.IsOutputBinding()));
			if (ValidationResult != EArcTacticsBindingValidationResult::Valid)
			{
				SetError(OutError, LOCTEXT("InvalidPasteBinding", "A copied binding is invalid for the destination role, scope, path, or type."));
				return false;
			}
		}
		return true;
	}

	bool ValidatePastePayload(
		UArcTacticsConfig& Config,
		const FArcTacticsEditorClipboardPayload& Payload,
		const FArcTacticsEditorSelection& Destination,
		FText* OutError)
	{
		if (!IsPasteDestinationCompatible(Payload, Destination))
		{
			SetError(OutError, LOCTEXT("IncompatiblePasteDestination", "The clipboard item cannot be pasted at this destination."));
			return false;
		}
		if (!ValidatePayloadNodes(Config, Payload))
		{
			SetError(OutError, LOCTEXT("InvalidPasteNode", "The clipboard contains a node with an invalid role or instance type."));
			return false;
		}
		const int32 PasteIndex = GetPasteIndex(Config, Payload, Destination);
		if (PasteIndex == INDEX_NONE)
		{
			SetError(OutError, LOCTEXT("MissingPasteContainer", "The paste container no longer exists."));
			return false;
		}
		return ValidatePasteBindings(Config, Payload, Destination, PasteIndex, OutError);
	}

	void InstallRemappedBindings(
		UArcTacticsConfig& Config,
		const FArcTacticsEditorClipboardPayload& Payload,
		const TMap<FGuid, FGuid>& Remap)
	{
		TSet<FGuid> OldTargetIDs;
		AddPayloadTargetIDs(Payload, OldTargetIDs);
		TSet<FGuid> NewTargetIDs;
		for (const FGuid OldTargetID : OldTargetIDs)
		{
			if (const FGuid* NewTargetID = Remap.Find(OldTargetID))
			{
				NewTargetIDs.Add(*NewTargetID);
			}
		}
		UArcTacticsConfig::AddOwnedFunctionIDs(Config.EditorBindings.GetBindings(), NewTargetIDs);
		Config.EditorBindings.RemoveBindings(
			[&NewTargetIDs](FPropertyBindingBinding& Binding)
			{
				return NewTargetIDs.Contains(Binding.GetTargetPath().GetStructID());
			});
		Config.AddRemappedBindings(Payload.Bindings, Remap);
	}

	bool ValidateGlobalTaskOrder(
		const UArcTacticsConfig& Config,
		const int32 SourceIndex,
		const int32 TargetIndex,
		const EArcTacticsEditorInsertLocation Location)
	{
		TArray<FGuid> OrderedIDs;
		for (const FArcTacticsNodeDefinition& Task : Config.GlobalTasks)
		{
			OrderedIDs.Add(Task.ID);
		}
		const FGuid MovingID = OrderedIDs[SourceIndex];
		OrderedIDs.RemoveAt(SourceIndex);
		int32 InsertIndex = TargetIndex + (Location == EArcTacticsEditorInsertLocation::After ? 1 : 0);
		if (SourceIndex < InsertIndex)
		{
			--InsertIndex;
		}
		OrderedIDs.Insert(MovingID, InsertIndex);

		for (const FArcTacticsBinding& Binding : Config.EditorBindings.GetBindings())
		{
			if (Binding.IsOutputBinding())
			{
				continue;
			}
			const int32 BindingTargetIndex = OrderedIDs.IndexOfByKey(Binding.GetTargetPath().GetStructID());
			if (BindingTargetIndex == INDEX_NONE)
			{
				continue;
			}
			const FGuid SourceID = Binding.GetSourcePath().GetStructID();
			if (SourceID == Config.ContextID || SourceID == Config.GlobalParametersID)
			{
				continue;
			}
			const int32 BindingSourceIndex = OrderedIDs.IndexOfByKey(SourceID);
			if (BindingSourceIndex == INDEX_NONE || BindingSourceIndex >= BindingTargetIndex)
			{
				return false;
			}
		}
		return true;
	}

	template <typename ItemType>
	void MoveArrayItem(
		TArray<ItemType>& Items,
		const int32 SourceIndex,
		const int32 TargetIndex,
		const EArcTacticsEditorInsertLocation Location)
	{
		ItemType Moving = MoveTemp(Items[SourceIndex]);
		Items.RemoveAt(SourceIndex);
		int32 InsertIndex = TargetIndex + (Location == EArcTacticsEditorInsertLocation::After ? 1 : 0);
		if (SourceIndex < InsertIndex)
		{
			--InsertIndex;
		}
		Items.Insert(MoveTemp(Moving), InsertIndex);
	}
}

TSharedRef<FArcTacticsEditorViewModel> FArcTacticsEditorViewModel::Create(UArcTacticsConfig* InConfig)
{
	check(InConfig);
	TSharedRef<FArcTacticsEditorViewModel> Result = MakeShareable(new FArcTacticsEditorViewModel(InConfig));
	Result->Initialize();
	return Result;
}

FArcTacticsEditorViewModel::FArcTacticsEditorViewModel(UArcTacticsConfig* InConfig)
	: Config(InConfig)
{
}

void FArcTacticsEditorViewModel::Initialize()
{
	if (UArcTacticsConfig* ConfigObject = Config.Get())
	{
		AuthoringChangedHandle = ConfigObject->OnAuthoringChanged().AddRaw(
			this,
			&FArcTacticsEditorViewModel::HandleAuthoringChanged);
	}
	if (GEditor)
	{
		GEditor->RegisterForUndo(this);
	}
}

FArcTacticsEditorViewModel::~FArcTacticsEditorViewModel()
{
	if (UArcTacticsConfig* ConfigObject = Config.Get())
	{
		ConfigObject->OnAuthoringChanged().Remove(AuthoringChangedHandle);
	}
	if (GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}
}

bool FArcTacticsEditorViewModel::IsSelectionValid(
	const FArcTacticsEditorSelection& InSelection) const
{
	const UArcTacticsConfig* ConfigObject = Config.Get();
	if (!ConfigObject)
	{
		return false;
	}
	if (InSelection.IsConfig())
	{
		return !InSelection.ID.IsValid();
	}
	switch (InSelection.Kind)
	{
	case EArcTacticsEditorItemKind::Context:
		return InSelection.ID.IsValid() && InSelection.ID == ConfigObject->ContextID;
	case EArcTacticsEditorItemKind::GlobalTask:
		return ArcTacticsEditorViewModelPrivate::FindNodeIndex(
			ConfigObject->GlobalTasks, InSelection.ID) != INDEX_NONE;
	case EArcTacticsEditorItemKind::Action:
		return ArcTacticsEditorViewModelPrivate::FindActionIndex(*ConfigObject, InSelection.ID) != INDEX_NONE;
	case EArcTacticsEditorItemKind::Selector:
		return ConfigObject->FindSelectorByID(InSelection.ID) != nullptr;
	case EArcTacticsEditorItemKind::SelectorAction:
		if (const FArcTacticsSelectorDefinition* Selector = ConfigObject->FindOwningSelector(InSelection.ID))
		{
			return Selector->Action.ID == InSelection.ID;
		}
		return false;
	case EArcTacticsEditorItemKind::Step:
	case EArcTacticsEditorItemKind::ActiveStep:
	case EArcTacticsEditorItemKind::Consideration:
	{
		ArcTacticsEditorViewModelPrivate::FConstBranchNodeLocation Location;
		return ArcTacticsEditorViewModelPrivate::FindBranchNode(
			*ConfigObject, InSelection.Kind, InSelection.ID, Location);
	}
	default:
		return false;
	}
}

bool FArcTacticsEditorViewModel::FindSelectionForStructID(
	const FGuid& InStructID,
	FArcTacticsEditorSelection& OutSelection) const
{
	const UArcTacticsConfig* ConfigObject = Config.Get();
	if (!ConfigObject || !InStructID.IsValid())
	{
		return false;
	}
	const FGuid StructID = ConfigObject->ResolveFunctionRoot(InStructID);
	if (StructID == ConfigObject->ContextID)
	{
		OutSelection = {EArcTacticsEditorItemKind::Context, StructID};
		return true;
	}
	if (StructID == ConfigObject->DecisionSettingsID
		|| StructID == ConfigObject->GlobalParametersID)
	{
		OutSelection = FArcTacticsEditorSelection::Config();
		return true;
	}
	for (const FArcTacticsNodeDefinition& Task : ConfigObject->GlobalTasks)
	{
		if (Task.ID == StructID)
		{
			OutSelection = {EArcTacticsEditorItemKind::GlobalTask, StructID};
			return true;
		}
	}
	for (const FArcTacticsActionDefinition& Action : ConfigObject->Actions)
	{
		if (Action.Action.ID == StructID
			|| Action.ParametersID == StructID
			|| Action.ParameterBagID == StructID)
		{
			OutSelection = {EArcTacticsEditorItemKind::Action, Action.Action.ID};
			return true;
		}
		if (Action.Steps.ContainsByPredicate(
			[&StructID](const FArcTacticsNodeDefinition& Node)
			{
				return Node.ID == StructID;
			}))
		{
			OutSelection = {EArcTacticsEditorItemKind::Step, StructID};
			return true;
		}
		if (Action.ActiveSteps.ContainsByPredicate(
			[&StructID](const FArcTacticsNodeDefinition& Node)
			{
				return Node.ID == StructID;
			}))
		{
			OutSelection = {EArcTacticsEditorItemKind::ActiveStep, StructID};
			return true;
		}
		if (Action.Considerations.ContainsByPredicate(
			[&StructID](const FArcTacticsNodeDefinition& Node)
			{
				return Node.ID == StructID;
			}))
		{
			OutSelection = {EArcTacticsEditorItemKind::Consideration, StructID};
			return true;
		}
	}
	for (const FArcTacticsSelectorDefinition& Selector : ConfigObject->Selectors)
	{
		if (Selector.Selector.ID == StructID)
		{
			OutSelection = {EArcTacticsEditorItemKind::Selector, StructID};
			return true;
		}
		if (Selector.Action.ID == StructID
			|| Selector.ParametersID == StructID
			|| Selector.ParameterBagID == StructID)
		{
			OutSelection = {EArcTacticsEditorItemKind::SelectorAction, Selector.Action.ID};
			return true;
		}
		if (Selector.Steps.ContainsByPredicate(
			[&StructID](const FArcTacticsNodeDefinition& Node)
			{
				return Node.ID == StructID;
			}))
		{
			OutSelection = {EArcTacticsEditorItemKind::Step, StructID};
			return true;
		}
		if (Selector.ActiveSteps.ContainsByPredicate(
			[&StructID](const FArcTacticsNodeDefinition& Node)
			{
				return Node.ID == StructID;
			}))
		{
			OutSelection = {EArcTacticsEditorItemKind::ActiveStep, StructID};
			return true;
		}
		if (Selector.Considerations.ContainsByPredicate(
			[&StructID](const FArcTacticsNodeDefinition& Node)
			{
				return Node.ID == StructID;
			}))
		{
			OutSelection = {EArcTacticsEditorItemKind::Consideration, StructID};
			return true;
		}
	}
	return false;
}

bool FArcTacticsEditorViewModel::Compile()
{
	UArcTacticsConfig* ConfigObject = Config.Get();
	if (!ConfigObject)
	{
		return false;
	}
	const FScopedTransaction Transaction(
		LOCTEXT("CompileTransaction", "Compile Arc Tactics Configuration"));
	ConfigObject->Modify();
	CompileDiagnostics.Reset();
	ConfigObject->CompileEditorData(CompileDiagnostics);
	ConfigObject->MarkPackageDirty();
	CompileResultsChanged.Broadcast();
	return ConfigObject->CompiledData.IsExecutable();
}

bool FArcTacticsEditorViewModel::IsCompiledArtifactValid() const
{
	const UArcTacticsConfig* ConfigObject = Config.Get();
	return ConfigObject && ConfigObject->CompiledData.IsExecutable();
}

void FArcTacticsEditorViewModel::SetCurrentSelection(
	const FArcTacticsEditorSelection& InSelection,
	const bool bUpdatePreferred)
{
	if (Selection == InSelection)
	{
		if (bUpdatePreferred)
		{
			PreferredSelection = InSelection;
			PreferredFallbackSelection = FArcTacticsEditorSelection::Config();
		}
		return;
	}
	Selection = InSelection;
	if (bUpdatePreferred)
	{
		PreferredSelection = InSelection;
		PreferredFallbackSelection = FArcTacticsEditorSelection::Config();
	}
	SelectionChanged.Broadcast(Selection);
}

void FArcTacticsEditorViewModel::SetSelection(const FArcTacticsEditorSelection& InSelection)
{
	SetCurrentSelection(
		IsSelectionValid(InSelection) ? InSelection : FArcTacticsEditorSelection::Config(),
		true);
}

void FArcTacticsEditorViewModel::ReconcileSelection()
{
	if (IsSelectionValid(PreferredSelection))
	{
		SetCurrentSelection(PreferredSelection, false);
	}
	else if (!IsSelectionValid(Selection))
	{
		SetCurrentSelection(
			IsSelectionValid(PreferredFallbackSelection)
				? PreferredFallbackSelection
				: FArcTacticsEditorSelection::Config(),
			false);
	}
}

void FArcTacticsEditorViewModel::HandleAuthoringChanged(const EArcTacticsAuthoringChange Change)
{
	ReconcileSelection();
	CompileDiagnostics.Reset();
	ModelChanged.Broadcast(Change);
	CompileResultsChanged.Broadcast();
}

void FArcTacticsEditorViewModel::PostUndo(const bool bSuccess)
{
	if (bSuccess)
	{
		ReconcileSelection();
	}
}

void FArcTacticsEditorViewModel::PostRedo(const bool bSuccess)
{
	if (bSuccess)
	{
		ReconcileSelection();
	}
}

bool FArcTacticsEditorViewModel::CanAdd(
	const EArcTacticsEditorItemKind Kind,
	const FArcTacticsEditorSelection& Parent,
	const UScriptStruct* NodeType,
	FText* OutError) const
{
	const UArcTacticsConfig* ConfigObject = Config.Get();
	if (!ConfigObject || !IsSelectionValid(Parent))
	{
		ArcTacticsEditorViewModelPrivate::SetError(OutError, LOCTEXT("InvalidAddParent", "The destination no longer exists."));
		return false;
	}

	const bool bCompatibleParent =
		(Kind == EArcTacticsEditorItemKind::GlobalTask
			&& (Parent.IsConfig() || Parent.Kind == EArcTacticsEditorItemKind::GlobalTask))
		|| (Kind == EArcTacticsEditorItemKind::Action
			&& (Parent.IsConfig() || Parent.Kind == EArcTacticsEditorItemKind::Action))
		|| (Kind == EArcTacticsEditorItemKind::Selector
			&& (Parent.IsConfig() || Parent.Kind == EArcTacticsEditorItemKind::Selector))
		|| (Kind == EArcTacticsEditorItemKind::Consideration
			&& (Parent.Kind == EArcTacticsEditorItemKind::Action
				|| Parent.Kind == EArcTacticsEditorItemKind::Selector
				|| Parent.Kind == EArcTacticsEditorItemKind::SelectorAction
				|| Parent.Kind == EArcTacticsEditorItemKind::Consideration))
		|| ((Kind == EArcTacticsEditorItemKind::Step || Kind == EArcTacticsEditorItemKind::ActiveStep)
			&& (Parent.Kind == EArcTacticsEditorItemKind::Action
				|| Parent.Kind == EArcTacticsEditorItemKind::SelectorAction
				|| Parent.Kind == Kind));
	if (!bCompatibleParent)
	{
		ArcTacticsEditorViewModelPrivate::SetError(OutError, LOCTEXT("IncompatibleAddParent", "This item type cannot be added at the selected destination."));
		return false;
	}

	FArcTacticsNodeDefinition PreparedNode;
	if (!ArcTacticsAuthoring::PrepareNode(
		*ConfigObject, PreparedNode, NodeType, ArcTacticsEditorViewModelPrivate::GetExpectedRole(Kind)))
	{
		ArcTacticsEditorViewModelPrivate::SetError(OutError, LOCTEXT("InvalidAddType", "The selected struct is not valid for this node role."));
		return false;
	}
	return true;
}

bool FArcTacticsEditorViewModel::Add(
	const EArcTacticsEditorItemKind Kind,
	const FArcTacticsEditorSelection& Parent,
	const UScriptStruct* NodeType)
{
	if (!CanAdd(Kind, Parent, NodeType))
	{
		return false;
	}
	UArcTacticsConfig* ConfigObject = Config.Get();
	check(ConfigObject);
	FArcTacticsNodeDefinition PreparedNode;
	ArcTacticsAuthoring::PrepareNode(
		*ConfigObject, PreparedNode, NodeType, ArcTacticsEditorViewModelPrivate::GetExpectedRole(Kind));

	const FScopedTransaction Transaction(LOCTEXT("AddTransaction", "Add Arc Tactics Item"));
	ConfigObject->Modify();
	int32 NewIndex = INDEX_NONE;
	FArcTacticsEditorSelection NewSelection;
	switch (Kind)
	{
	case EArcTacticsEditorItemKind::GlobalTask:
		NewIndex = Parent.Kind == EArcTacticsEditorItemKind::GlobalTask
			? ArcTacticsEditorViewModelPrivate::FindNodeIndex(ConfigObject->GlobalTasks, Parent.ID) + 1
			: ConfigObject->GlobalTasks.Num();
		ConfigObject->GlobalTasks.Insert(MoveTemp(PreparedNode), NewIndex);
		ConfigObject->NotifyAuthoringChanged(EArcTacticsAuthoringChange::Structure);
		NewSelection = {Kind, ConfigObject->GlobalTasks[NewIndex].ID};
		break;
	case EArcTacticsEditorItemKind::Action:
	{
		NewIndex = Parent.Kind == EArcTacticsEditorItemKind::Action
			? ArcTacticsEditorViewModelPrivate::FindActionIndex(*ConfigObject, Parent.ID) + 1
			: ConfigObject->Actions.Num();
		FArcTacticsActionDefinition Action;
		Action.Action = MoveTemp(PreparedNode);
		ConfigObject->Actions.Insert(MoveTemp(Action), NewIndex);
		ConfigObject->NotifyAuthoringChanged(EArcTacticsAuthoringChange::Structure);
		NewSelection = {Kind, ConfigObject->Actions[NewIndex].Action.ID};
		break;
	}
	case EArcTacticsEditorItemKind::Selector:
	{
		NewIndex = Parent.Kind == EArcTacticsEditorItemKind::Selector
			? ArcTacticsEditorViewModelPrivate::FindSelectorIndex(*ConfigObject, Parent.ID) + 1
			: ConfigObject->Selectors.Num();
		FArcTacticsSelectorDefinition Selector;
		Selector.Selector = MoveTemp(PreparedNode);
		ConfigObject->Selectors.Insert(MoveTemp(Selector), NewIndex);
		ConfigObject->NotifyAuthoringChanged(EArcTacticsAuthoringChange::Structure);
		NewSelection = {Kind, ConfigObject->Selectors[NewIndex].Selector.ID};
		break;
	}
	case EArcTacticsEditorItemKind::Step:
	case EArcTacticsEditorItemKind::ActiveStep:
	case EArcTacticsEditorItemKind::Consideration:
	{
		ArcTacticsEditorViewModelPrivate::FBranchNodeLocation Location;
		ArcTacticsEditorViewModelPrivate::ResolveBranchNodeOwner(*ConfigObject, Kind, Parent, Location);
		check(Location.Items);
		NewIndex = Parent.Kind == Kind
			? Location.Index + 1
			: Location.Items->Num();
		Location.Items->Insert(MoveTemp(PreparedNode), NewIndex);
		ConfigObject->NotifyAuthoringChanged(EArcTacticsAuthoringChange::Structure);
		NewSelection = {Kind, (*Location.Items)[NewIndex].ID};
		break;
	}
	default:
		return false;
	}
	SetSelection(NewSelection);
	return true;
}

bool FArcTacticsEditorViewModel::CanRename(
	const FArcTacticsEditorSelection& Item,
	FText* OutError) const
{
	if (!IsSelectionValid(Item)
		|| !ArcTacticsEditorViewModelPrivate::ResolveNode(*Config.Get(), Item))
	{
		ArcTacticsEditorViewModelPrivate::SetError(OutError, LOCTEXT("InvalidRename", "The selected item cannot be renamed."));
		return false;
	}
	return true;
}

bool FArcTacticsEditorViewModel::Rename(
	const FArcTacticsEditorSelection& Item,
	const FName NewName)
{
	if (!CanRename(Item))
	{
		return false;
	}
	UArcTacticsConfig* ConfigObject = Config.Get();
	check(ConfigObject);
	FName* Name = &ArcTacticsEditorViewModelPrivate::ResolveNode(*ConfigObject, Item)->Name;
	if (*Name == NewName)
	{
		return true;
	}
	const FScopedTransaction Transaction(LOCTEXT("RenameTransaction", "Rename Arc Tactics Item"));
	ConfigObject->Modify();
	*Name = NewName;
	ConfigObject->NotifyAuthoringChanged(EArcTacticsAuthoringChange::Value);
	return true;
}

bool FArcTacticsEditorViewModel::CanReplaceType(
	const FArcTacticsEditorSelection& Item,
	const UScriptStruct* NodeType,
	FText* OutError) const
{
	const UArcTacticsConfig* ConfigObject = Config.Get();
	const FArcTacticsNodeDefinition* Existing = ConfigObject && IsSelectionValid(Item)
		? ArcTacticsEditorViewModelPrivate::ResolveNode(*ConfigObject, Item)
		: nullptr;
	if (!Existing)
	{
		ArcTacticsEditorViewModelPrivate::SetError(OutError, LOCTEXT("InvalidReplaceSelection", "The selected item has no replaceable node type."));
		return false;
	}
	FArcTacticsNodeDefinition Prepared = *Existing;
	if (!ArcTacticsAuthoring::PrepareNode(
		*ConfigObject,
		Prepared,
		NodeType,
		ArcTacticsEditorViewModelPrivate::GetExpectedRole(Item.Kind)))
	{
		ArcTacticsEditorViewModelPrivate::SetError(OutError, LOCTEXT("InvalidReplacementType", "The selected struct is not valid for this node role."));
		return false;
	}
	return true;
}

bool FArcTacticsEditorViewModel::ReplaceType(
	const FArcTacticsEditorSelection& Item,
	const UScriptStruct* NodeType)
{
	if (!CanReplaceType(Item, NodeType))
	{
		return false;
	}
	UArcTacticsConfig* ConfigObject = Config.Get();
	check(ConfigObject);
	FArcTacticsNodeDefinition* Existing = ArcTacticsEditorViewModelPrivate::ResolveNode(*ConfigObject, Item);
	FArcTacticsNodeDefinition Prepared = *Existing;
	ArcTacticsAuthoring::PrepareNode(
		*ConfigObject,
		Prepared,
		NodeType,
		ArcTacticsEditorViewModelPrivate::GetExpectedRole(Item.Kind));
	const FScopedTransaction Transaction(LOCTEXT("ReplaceTypeTransaction", "Replace Arc Tactics Node Type"));
	ConfigObject->Modify();
	*Existing = MoveTemp(Prepared);
	ConfigObject->NotifyAuthoringChanged(EArcTacticsAuthoringChange::Structure);
	return true;
}

bool FArcTacticsEditorViewModel::CanDelete(
	const FArcTacticsEditorSelection& Item,
	FText* OutError) const
{
	if (!IsSelectionValid(Item)
		|| Item.IsConfig()
		|| Item.Kind == EArcTacticsEditorItemKind::Context
		|| Item.Kind == EArcTacticsEditorItemKind::SelectorAction)
	{
		ArcTacticsEditorViewModelPrivate::SetError(OutError, LOCTEXT("InvalidDelete", "The selected item cannot be deleted."));
		return false;
	}
	return true;
}

bool FArcTacticsEditorViewModel::Delete(const FArcTacticsEditorSelection& Item)
{
	if (!CanDelete(Item))
	{
		return false;
	}
	UArcTacticsConfig* ConfigObject = Config.Get();
	check(ConfigObject);
	FArcTacticsEditorSelection Fallback = FArcTacticsEditorSelection::Config();
	const FScopedTransaction Transaction(LOCTEXT("DeleteTransaction", "Delete Arc Tactics Item"));
	ConfigObject->Modify();
	switch (Item.Kind)
	{
	case EArcTacticsEditorItemKind::GlobalTask:
		ConfigObject->GlobalTasks.RemoveAt(
			ArcTacticsEditorViewModelPrivate::FindNodeIndex(ConfigObject->GlobalTasks, Item.ID));
		break;
	case EArcTacticsEditorItemKind::Action:
		ConfigObject->Actions.RemoveAt(
			ArcTacticsEditorViewModelPrivate::FindActionIndex(*ConfigObject, Item.ID));
		break;
	case EArcTacticsEditorItemKind::Selector:
		ConfigObject->Selectors.RemoveAt(
			ArcTacticsEditorViewModelPrivate::FindSelectorIndex(*ConfigObject, Item.ID));
		break;
	case EArcTacticsEditorItemKind::Step:
	case EArcTacticsEditorItemKind::ActiveStep:
	case EArcTacticsEditorItemKind::Consideration:
	{
		ArcTacticsEditorViewModelPrivate::FBranchNodeLocation Location;
		ArcTacticsEditorViewModelPrivate::FindBranchNode(*ConfigObject, Item.Kind, Item.ID, Location);
		Fallback = Location.Owner;
		Location.Items->RemoveAt(Location.Index);
		break;
	}
	default:
		return false;
	}
	PreferredFallbackSelection = Fallback;
	SetCurrentSelection(Fallback, false);
	ConfigObject->NotifyAuthoringChanged(EArcTacticsAuthoringChange::Structure);
	return true;
}

bool FArcTacticsEditorViewModel::CanDuplicate(
	const FArcTacticsEditorSelection& Item,
	FText* OutError) const
{
	const UArcTacticsConfig* ConfigObject = Config.Get();
	if (!IsSelectionValid(Item)
		|| Item.IsConfig()
		|| Item.Kind == EArcTacticsEditorItemKind::Context
		|| Item.Kind == EArcTacticsEditorItemKind::SelectorAction)
	{
		ArcTacticsEditorViewModelPrivate::SetError(OutError, LOCTEXT("InvalidDuplicate", "The selected item cannot be duplicated."));
		return false;
	}

	bool bValidAuthoring = false;
	switch (Item.Kind)
	{
	case EArcTacticsEditorItemKind::GlobalTask:
		bValidAuthoring = ArcTacticsEditorViewModelPrivate::ValidatePayloadNode(
			*ConfigObject,
			ConfigObject->GlobalTasks[ArcTacticsEditorViewModelPrivate::FindNodeIndex(
				ConfigObject->GlobalTasks, Item.ID)],
			EArcTacticsNodeRole::GlobalTask);
		break;
	case EArcTacticsEditorItemKind::Action:
	{
		const int32 ActionIndex = ArcTacticsEditorViewModelPrivate::FindActionIndex(
			*ConfigObject, Item.ID);
		const FArcTacticsActionDefinition& Action = ConfigObject->Actions[ActionIndex];
		bValidAuthoring = FMath::IsFinite(Action.Parameters.Weight)
			&& Action.Parameters.Weight >= 0.0f
			&& FMath::IsFinite(Action.Parameters.Cooldown)
			&& Action.Parameters.Cooldown >= 0.0f
			&& ArcTacticsEditorViewModelPrivate::ValidateActionNodes(
				*ConfigObject, Action);
		break;
	}
	case EArcTacticsEditorItemKind::Selector:
	{
		const FArcTacticsSelectorDefinition& Selector = *ConfigObject->FindSelectorByID(Item.ID);
		bValidAuthoring = FMath::IsFinite(Selector.Parameters.Weight)
			&& Selector.Parameters.Weight >= 0.0f
			&& FMath::IsFinite(Selector.Parameters.Cooldown)
			&& Selector.Parameters.Cooldown >= 0.0f
			&& ArcTacticsEditorViewModelPrivate::ValidateSelectorNodes(
				*ConfigObject, Selector);
		break;
	}
	case EArcTacticsEditorItemKind::Step:
	case EArcTacticsEditorItemKind::ActiveStep:
	case EArcTacticsEditorItemKind::Consideration:
		bValidAuthoring = ArcTacticsEditorViewModelPrivate::ValidatePayloadNode(
			*ConfigObject,
			*ConfigObject->FindNodeByID(Item.ID),
			ArcTacticsEditorViewModelPrivate::GetExpectedRole(Item.Kind));
		break;
	default:
		break;
	}
	if (!bValidAuthoring)
	{
		ArcTacticsEditorViewModelPrivate::SetError(OutError, LOCTEXT("InvalidDuplicateAuthoring", "The selected item contains an invalid node role or instance type."));
		return false;
	}

	FArcTacticsEditorClipboardPayload ScopePayload;
	ScopePayload.Kind = Item.Kind;
	if (Item.Kind == EArcTacticsEditorItemKind::GlobalTask)
	{
		ScopePayload.Node = ConfigObject->GlobalTasks[
			ArcTacticsEditorViewModelPrivate::FindNodeIndex(ConfigObject->GlobalTasks, Item.ID)];
	}
	else if (Item.Kind == EArcTacticsEditorItemKind::Action)
	{
		ScopePayload.Action = ConfigObject->Actions[
			ArcTacticsEditorViewModelPrivate::FindActionIndex(*ConfigObject, Item.ID)];
	}
	else if (Item.Kind == EArcTacticsEditorItemKind::Selector)
	{
		ScopePayload.Selector = *ConfigObject->FindSelectorByID(Item.ID);
	}
	else
	{
		ScopePayload.Node = *ConfigObject->FindNodeByID(Item.ID);
	}
	TSet<FGuid> TargetIDs;
	ArcTacticsEditorViewModelPrivate::AddPayloadTargetIDs(ScopePayload, TargetIDs);
	UArcTacticsConfig::AddOwnedFunctionIDs(ConfigObject->EditorBindings.GetBindings(), TargetIDs);
	if (!ArcTacticsEditorViewModelPrivate::ValidateBindingScopes(*ConfigObject, TargetIDs))
	{
		ArcTacticsEditorViewModelPrivate::SetError(OutError, LOCTEXT("InvalidDuplicateBindingScope", "The selected item contains a binding source outside its accessible scope."));
		return false;
	}
	return true;
}

bool FArcTacticsEditorViewModel::Duplicate(const FArcTacticsEditorSelection& Item)
{
	if (!CanDuplicate(Item))
	{
		return false;
	}
	UArcTacticsConfig* ConfigObject = Config.Get();
	check(ConfigObject);
	FArcTacticsEditorSelection NewSelection;
	const FScopedTransaction Transaction(LOCTEXT("DuplicateTransaction", "Duplicate Arc Tactics Item"));
	ConfigObject->Modify();
	switch (Item.Kind)
	{
	case EArcTacticsEditorItemKind::GlobalTask:
	{
		const int32 Index = ArcTacticsEditorViewModelPrivate::FindNodeIndex(ConfigObject->GlobalTasks, Item.ID) + 1;
		FArcTacticsNodeDefinition DuplicateNode = ConfigObject->GlobalTasks[Index - 1];
		ConfigObject->GlobalTasks.Insert(MoveTemp(DuplicateNode), Index);
		ConfigObject->DuplicateAuthoringData(ConfigObject->GlobalTasks[Index]);
		NewSelection = {Item.Kind, ConfigObject->GlobalTasks[Index].ID};
		break;
	}
	case EArcTacticsEditorItemKind::Action:
	{
		const int32 Index = ArcTacticsEditorViewModelPrivate::FindActionIndex(*ConfigObject, Item.ID) + 1;
		FArcTacticsActionDefinition DuplicateAction = ConfigObject->Actions[Index - 1];
		ConfigObject->Actions.Insert(MoveTemp(DuplicateAction), Index);
		ConfigObject->DuplicateAuthoringData(ConfigObject->Actions[Index]);
		NewSelection = {Item.Kind, ConfigObject->Actions[Index].Action.ID};
		break;
	}
	case EArcTacticsEditorItemKind::Selector:
	{
		const int32 Index = ArcTacticsEditorViewModelPrivate::FindSelectorIndex(*ConfigObject, Item.ID) + 1;
		FArcTacticsSelectorDefinition DuplicateSelector = ConfigObject->Selectors[Index - 1];
		ConfigObject->Selectors.Insert(MoveTemp(DuplicateSelector), Index);
		ConfigObject->DuplicateAuthoringData(ConfigObject->Selectors[Index]);
		NewSelection = {Item.Kind, ConfigObject->Selectors[Index].Selector.ID};
		break;
	}
	case EArcTacticsEditorItemKind::Step:
	case EArcTacticsEditorItemKind::ActiveStep:
	case EArcTacticsEditorItemKind::Consideration:
	{
		ArcTacticsEditorViewModelPrivate::FBranchNodeLocation Location;
		ArcTacticsEditorViewModelPrivate::FindBranchNode(*ConfigObject, Item.Kind, Item.ID, Location);
		const int32 Index = Location.Index + 1;
		FArcTacticsNodeDefinition DuplicateNode = (*Location.Items)[Location.Index];
		Location.Items->Insert(MoveTemp(DuplicateNode), Index);
		ConfigObject->DuplicateAuthoringData((*Location.Items)[Index]);
		NewSelection = {Item.Kind, (*Location.Items)[Index].ID};
		break;
	}
	default:
		return false;
	}
	ConfigObject->NotifyAuthoringChanged(EArcTacticsAuthoringChange::Structure);
	SetSelection(NewSelection);
	return true;
}

bool FArcTacticsEditorViewModel::CanCopy(
	const FArcTacticsEditorSelection& Item,
	FText* OutError) const
{
	if (!IsSelectionValid(Item)
		|| (Item.Kind != EArcTacticsEditorItemKind::GlobalTask
			&& Item.Kind != EArcTacticsEditorItemKind::Action
			&& Item.Kind != EArcTacticsEditorItemKind::Selector
			&& !ArcTacticsEditorViewModelPrivate::IsBranchNodeKind(Item.Kind)))
	{
		ArcTacticsEditorViewModelPrivate::SetError(OutError, LOCTEXT("InvalidCopy", "The selected item cannot be copied."));
		return false;
	}
	return true;
}

bool FArcTacticsEditorViewModel::Copy(const FArcTacticsEditorSelection& Item)
{
	if (!CanCopy(Item))
	{
		return false;
	}
	FText Error;
	return FArcTacticsEditorClipboard::Copy(*Config.Get(), Item, Error);
}

bool FArcTacticsEditorViewModel::CanPaste(
	const FArcTacticsEditorSelection& Destination,
	FText* OutError) const
{
	UArcTacticsConfig* ConfigObject = Config.Get();
	if (!ConfigObject || !IsSelectionValid(Destination))
	{
		ArcTacticsEditorViewModelPrivate::SetError(OutError, LOCTEXT("InvalidPasteDestination", "The paste destination no longer exists."));
		return false;
	}
	FArcTacticsEditorClipboardPayload Payload;
	FText ClipboardError;
	if (!FArcTacticsEditorClipboard::Read(Payload, ClipboardError))
	{
		ArcTacticsEditorViewModelPrivate::SetError(OutError, ClipboardError);
		return false;
	}
	return ArcTacticsEditorViewModelPrivate::ValidatePastePayload(
		*ConfigObject, Payload, Destination, OutError);
}

bool FArcTacticsEditorViewModel::Paste(const FArcTacticsEditorSelection& Destination)
{
	FArcTacticsEditorClipboardPayload Payload;
	FText Error;
	UArcTacticsConfig* ConfigObject = Config.Get();
	if (!ConfigObject
		|| !IsSelectionValid(Destination)
		|| !FArcTacticsEditorClipboard::Read(Payload, Error)
		|| !ArcTacticsEditorViewModelPrivate::ValidatePastePayload(
			*ConfigObject, Payload, Destination, &Error))
	{
		return false;
	}
	const int32 PasteIndex = ArcTacticsEditorViewModelPrivate::GetPasteIndex(
		*ConfigObject, Payload, Destination);
	ArcTacticsEditorViewModelPrivate::FScopeIDs DestinationScope;
	if (ArcTacticsEditorViewModelPrivate::IsBranchNodeKind(Payload.Kind))
	{
		ArcTacticsEditorViewModelPrivate::FConstBranchNodeLocation DestinationOwner;
		ArcTacticsEditorViewModelPrivate::ResolveBranchNodeOwner(
			*ConfigObject, Payload.Kind, Destination, DestinationOwner);
		ArcTacticsEditorViewModelPrivate::ResolveScopeIDs(
			*ConfigObject, DestinationOwner.Owner, DestinationScope);
	}
	TMap<FGuid, FGuid> Remap;
	FArcTacticsEditorSelection NewSelection;

	const FScopedTransaction Transaction(LOCTEXT("PasteTransaction", "Paste Arc Tactics Item"));
	ConfigObject->Modify();
	switch (Payload.Kind)
	{
	case EArcTacticsEditorItemKind::GlobalTask:
		ConfigObject->GlobalTasks.Insert(Payload.Node, PasteIndex);
		Remap = ConfigObject->DuplicateAuthoringData(ConfigObject->GlobalTasks[PasteIndex]);
		NewSelection = {Payload.Kind, ConfigObject->GlobalTasks[PasteIndex].ID};
		break;
	case EArcTacticsEditorItemKind::Action:
		ConfigObject->Actions.Insert(Payload.Action, PasteIndex);
		Remap = ConfigObject->DuplicateAuthoringData(ConfigObject->Actions[PasteIndex]);
		NewSelection = {Payload.Kind, ConfigObject->Actions[PasteIndex].Action.ID};
		break;
	case EArcTacticsEditorItemKind::Selector:
		ConfigObject->Selectors.Insert(Payload.Selector, PasteIndex);
		Remap = ConfigObject->DuplicateAuthoringData(ConfigObject->Selectors[PasteIndex]);
		NewSelection = {Payload.Kind, ConfigObject->Selectors[PasteIndex].Selector.ID};
		break;
	case EArcTacticsEditorItemKind::Step:
	case EArcTacticsEditorItemKind::ActiveStep:
	case EArcTacticsEditorItemKind::Consideration:
	{
		ArcTacticsEditorViewModelPrivate::FBranchNodeLocation Location;
		ArcTacticsEditorViewModelPrivate::ResolveBranchNodeOwner(
			*ConfigObject, Payload.Kind, Destination, Location);
		Location.Items->Insert(Payload.Node, PasteIndex);
		Remap = ConfigObject->DuplicateAuthoringData((*Location.Items)[PasteIndex]);
		NewSelection = {Payload.Kind, (*Location.Items)[PasteIndex].ID};
		break;
	}
	default:
		return false;
	}

	Remap.Add(Payload.SourceContextID, ConfigObject->ContextID);
	Remap.Add(Payload.SourceGlobalParametersID, ConfigObject->GlobalParametersID);
	if (ArcTacticsEditorViewModelPrivate::IsBranchNodeKind(Payload.Kind))
	{
		Remap.Add(Payload.SourceScopeParametersID, DestinationScope.ParametersID);
		Remap.Add(Payload.SourceScopeParameterBagID, DestinationScope.ParameterBagID);
		if (Payload.SourceScopeSelectorID.IsValid() && DestinationScope.SelectorID.IsValid())
		{
			Remap.Add(Payload.SourceScopeSelectorID, DestinationScope.SelectorID);
		}
	}
	ArcTacticsEditorViewModelPrivate::InstallRemappedBindings(*ConfigObject, Payload, Remap);
	ConfigObject->NotifyAuthoringChanged(EArcTacticsAuthoringChange::Structure);
	SetSelection(NewSelection);
	return true;
}

bool FArcTacticsEditorViewModel::CanMove(
	const FArcTacticsEditorSelection& Item,
	const FArcTacticsEditorSelection& Target,
	const EArcTacticsEditorInsertLocation Location,
	FText* OutError) const
{
	const UArcTacticsConfig* ConfigObject = Config.Get();
	if (!ConfigObject || !IsSelectionValid(Item) || !IsSelectionValid(Target) || Item == Target)
	{
		ArcTacticsEditorViewModelPrivate::SetError(OutError, LOCTEXT("InvalidMove", "The move source or destination no longer exists."));
		return false;
	}
	if (Item.Kind == EArcTacticsEditorItemKind::GlobalTask
		|| Item.Kind == EArcTacticsEditorItemKind::Action
		|| Item.Kind == EArcTacticsEditorItemKind::Selector)
	{
		if (Target.Kind != Item.Kind || Location == EArcTacticsEditorInsertLocation::Into)
		{
			ArcTacticsEditorViewModelPrivate::SetError(OutError, LOCTEXT("IncompatibleMove", "Top-level items can only move before or after the same item kind."));
			return false;
		}
		if (Item.Kind == EArcTacticsEditorItemKind::GlobalTask)
		{
			const int32 SourceIndex = ArcTacticsEditorViewModelPrivate::FindNodeIndex(ConfigObject->GlobalTasks, Item.ID);
			const int32 TargetIndex = ArcTacticsEditorViewModelPrivate::FindNodeIndex(ConfigObject->GlobalTasks, Target.ID);
			if (!ArcTacticsEditorViewModelPrivate::ValidateGlobalTaskOrder(
				*ConfigObject, SourceIndex, TargetIndex, Location))
			{
				ArcTacticsEditorViewModelPrivate::SetError(OutError, LOCTEXT("IllegalTaskMove", "The move would create a forward Global Task binding dependency."));
				return false;
			}
		}
		return true;
	}
	if (!ArcTacticsEditorViewModelPrivate::IsBranchNodeKind(Item.Kind))
	{
		ArcTacticsEditorViewModelPrivate::SetError(OutError, LOCTEXT("UnsupportedMove", "The selected item cannot be moved."));
		return false;
	}

	ArcTacticsEditorViewModelPrivate::FConstBranchNodeLocation SourceLocation;
	ArcTacticsEditorViewModelPrivate::FConstBranchNodeLocation TargetLocation;
	if (!ArcTacticsEditorViewModelPrivate::FindBranchNode(*ConfigObject, Item.Kind, Item.ID, SourceLocation)
		|| !ArcTacticsEditorViewModelPrivate::ResolveBranchNodeOwner(
			*ConfigObject, Item.Kind, Target, TargetLocation))
	{
		ArcTacticsEditorViewModelPrivate::SetError(OutError, LOCTEXT("MissingMoveContainer", "The source or destination action no longer exists."));
		return false;
	}
	if (Target.Kind == Item.Kind
		&& Location == EArcTacticsEditorInsertLocation::Into)
	{
		ArcTacticsEditorViewModelPrivate::SetError(OutError, LOCTEXT("CannotNestBranchNode", "Steps and considerations cannot contain other nodes."));
		return false;
	}
	if (Target.Kind != Item.Kind
		&& Location != EArcTacticsEditorInsertLocation::Into)
	{
		ArcTacticsEditorViewModelPrivate::SetError(OutError, LOCTEXT("MoveRequiresContainer", "Move into an action or before or after a node of the same kind."));
		return false;
	}
	if ((Item.Kind == EArcTacticsEditorItemKind::Step || Item.Kind == EArcTacticsEditorItemKind::ActiveStep)
		&& SourceLocation.Items != TargetLocation.Items)
	{
		ArcTacticsEditorViewModelPrivate::SetError(OutError, LOCTEXT("CrossActionStepMove", "Steps can only be reordered within their action. Copy and paste a step to use it in another action."));
		return false;
	}
	if (SourceLocation.Items != TargetLocation.Items)
	{
		ArcTacticsEditorViewModelPrivate::FScopeIDs SourceScope;
		ArcTacticsEditorViewModelPrivate::FScopeIDs DestinationScope;
		if (!ArcTacticsEditorViewModelPrivate::ResolveScopeIDs(
				*ConfigObject, SourceLocation.Owner, SourceScope)
			|| !ArcTacticsEditorViewModelPrivate::ResolveScopeIDs(
				*ConfigObject, TargetLocation.Owner, DestinationScope))
		{
			ArcTacticsEditorViewModelPrivate::SetError(OutError, LOCTEXT("MissingMoveScope", "The source or destination binding scope no longer exists."));
			return false;
		}
		FArcTacticsBindingEndpoint DestinationOwnerEndpoint;
		FArcTacticsBindingEndpoint ProspectiveTargetEndpoint;
		if (!ConfigObject->ResolveBindingEndpoint(
				TargetLocation.Owner.ID, DestinationOwnerEndpoint)
			|| !ConfigObject->ResolveBindingEndpoint(Item.ID, ProspectiveTargetEndpoint))
		{
			ArcTacticsEditorViewModelPrivate::SetError(OutError, LOCTEXT("MissingMoveEndpoint", "The source or destination binding endpoint no longer exists."));
			return false;
		}
		ProspectiveTargetEndpoint.Owner = DestinationOwnerEndpoint.Owner;
		ProspectiveTargetEndpoint.OwnerIndex = DestinationOwnerEndpoint.OwnerIndex;
		ProspectiveTargetEndpoint.ExecutionIndex =
			Target.Kind == Item.Kind
				? TargetLocation.Index
					+ (Location == EArcTacticsEditorInsertLocation::After ? 1 : 0)
				: TargetLocation.Items->Num();
		TSet<FGuid> MovingIDs;
		MovingIDs.Add(Item.ID);
		UArcTacticsConfig::AddOwnedFunctionIDs(ConfigObject->EditorBindings.GetBindings(), MovingIDs);
		for (const FArcTacticsBinding& Binding : ConfigObject->EditorBindings.GetBindings())
		{
			const FGuid TargetID = Binding.GetTargetPath().GetStructID();
			if (!MovingIDs.Contains(TargetID))
			{
				continue;
			}
			FArcTacticsBindingEndpoint TargetEndpoint = ProspectiveTargetEndpoint;
			if (TargetID != Item.ID)
			{
				TargetEndpoint.ID = TargetID;
				TargetEndpoint.Kind = EArcTacticsBindingEndpointKind::Function;
			}
			if (!ArcTacticsEditorViewModelPrivate::ValidateMovedConsiderationBinding(
				*ConfigObject,
				Binding,
				SourceScope,
				DestinationScope,
				TargetEndpoint))
			{
				ArcTacticsEditorViewModelPrivate::SetError(OutError, LOCTEXT("IllegalConsiderationMove", "The consideration has a binding that is invalid in the destination action."));
				return false;
			}
		}
	}
	return true;
}

bool FArcTacticsEditorViewModel::Move(
	const FArcTacticsEditorSelection& Item,
	const FArcTacticsEditorSelection& Target,
	const EArcTacticsEditorInsertLocation Location)
{
	if (!CanMove(Item, Target, Location))
	{
		return false;
	}
	UArcTacticsConfig* ConfigObject = Config.Get();
	check(ConfigObject);
	const FScopedTransaction Transaction(LOCTEXT("MoveTransaction", "Move Arc Tactics Item"));
	ConfigObject->Modify();

	if (Item.Kind == EArcTacticsEditorItemKind::GlobalTask)
	{
		ArcTacticsEditorViewModelPrivate::MoveArrayItem(
			ConfigObject->GlobalTasks,
			ArcTacticsEditorViewModelPrivate::FindNodeIndex(ConfigObject->GlobalTasks, Item.ID),
			ArcTacticsEditorViewModelPrivate::FindNodeIndex(ConfigObject->GlobalTasks, Target.ID),
			Location);
	}
	else if (Item.Kind == EArcTacticsEditorItemKind::Action)
	{
		ArcTacticsEditorViewModelPrivate::MoveArrayItem(
			ConfigObject->Actions,
			ArcTacticsEditorViewModelPrivate::FindActionIndex(*ConfigObject, Item.ID),
			ArcTacticsEditorViewModelPrivate::FindActionIndex(*ConfigObject, Target.ID),
			Location);
	}
	else if (Item.Kind == EArcTacticsEditorItemKind::Selector)
	{
		ArcTacticsEditorViewModelPrivate::MoveArrayItem(
			ConfigObject->Selectors,
			ArcTacticsEditorViewModelPrivate::FindSelectorIndex(*ConfigObject, Item.ID),
			ArcTacticsEditorViewModelPrivate::FindSelectorIndex(*ConfigObject, Target.ID),
			Location);
	}
	else
	{
		ArcTacticsEditorViewModelPrivate::FBranchNodeLocation SourceLocation;
		ArcTacticsEditorViewModelPrivate::FBranchNodeLocation TargetLocation;
		ArcTacticsEditorViewModelPrivate::FindBranchNode(*ConfigObject, Item.Kind, Item.ID, SourceLocation);
		ArcTacticsEditorViewModelPrivate::ResolveBranchNodeOwner(
			*ConfigObject, Item.Kind, Target, TargetLocation);
		if (SourceLocation.Items == TargetLocation.Items)
		{
			const int32 TargetIndex = Target.Kind == Item.Kind
				? TargetLocation.Index
				: TargetLocation.Items->Num() - 1;
			const EArcTacticsEditorInsertLocation EffectiveLocation =
				Target.Kind == Item.Kind
					? Location
					: EArcTacticsEditorInsertLocation::After;
			ArcTacticsEditorViewModelPrivate::MoveArrayItem(
				*SourceLocation.Items, SourceLocation.Index, TargetIndex, EffectiveLocation);
		}
		else
		{
			ArcTacticsEditorViewModelPrivate::FScopeIDs SourceScope;
			ArcTacticsEditorViewModelPrivate::FScopeIDs DestinationScope;
			ArcTacticsEditorViewModelPrivate::ResolveScopeIDs(
				*ConfigObject, SourceLocation.Owner, SourceScope);
			ArcTacticsEditorViewModelPrivate::ResolveScopeIDs(
				*ConfigObject, TargetLocation.Owner, DestinationScope);
			TSet<FGuid> MovingIDs;
			MovingIDs.Add(Item.ID);
			UArcTacticsConfig::AddOwnedFunctionIDs(ConfigObject->EditorBindings.GetBindings(), MovingIDs);
			TArray<FArcTacticsBinding> MovingBindings;
			for (const FArcTacticsBinding& Binding : ConfigObject->EditorBindings.GetBindings())
			{
				if (MovingIDs.Contains(Binding.GetTargetPath().GetStructID()))
				{
					MovingBindings.Add(Binding);
				}
			}
			ConfigObject->EditorBindings.RemoveBindings(
				[&MovingIDs](FPropertyBindingBinding& Binding)
				{
					return MovingIDs.Contains(Binding.GetTargetPath().GetStructID());
				});
			FArcTacticsNodeDefinition Moving = MoveTemp((*SourceLocation.Items)[SourceLocation.Index]);
			SourceLocation.Items->RemoveAt(SourceLocation.Index);
			const int32 InsertIndex = Target.Kind == Item.Kind
				? TargetLocation.Index + (Location == EArcTacticsEditorInsertLocation::After ? 1 : 0)
				: TargetLocation.Items->Num();
			TargetLocation.Items->Insert(MoveTemp(Moving), InsertIndex);
			TMap<FGuid, FGuid> ScopeRemap;
			ScopeRemap.Add(SourceScope.ParametersID, DestinationScope.ParametersID);
			ScopeRemap.Add(SourceScope.ParameterBagID, DestinationScope.ParameterBagID);
			if (SourceScope.SelectorID.IsValid())
			{
				ScopeRemap.Add(SourceScope.SelectorID, DestinationScope.SelectorID);
			}
			ConfigObject->AddRemappedBindings(MovingBindings, ScopeRemap);
		}
	}

	ConfigObject->NotifyAuthoringChanged(EArcTacticsAuthoringChange::Structure);
	return true;
}

#undef LOCTEXT_NAMESPACE
