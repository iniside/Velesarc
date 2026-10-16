// Copyright Lukasz Baran. All Rights Reserved.

#include "Decision/ArcTacticsConfig.h"

#include "ArcTQSPropertyFunction.h"
#include "ArcTQSQueryReference.h"
#include "Decision/ArcTacticsCompiler.h"
#include "Misc/DataValidation.h"
#include "PropertyBindingDataView.h"
#include "PropertyBindingTypes.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsConfig)

void UArcTacticsConfig::PostInitProperties()
{
	Super::PostInitProperties();
#if WITH_EDITOR
	InitializeEditorBindingsOwner();
#endif
}

void UArcTacticsConfig::PostLoad()
{
	Super::PostLoad();
	CompiledData.ResolvePaths();
#if WITH_EDITOR
	InitializeEditorBindingsOwner();
	EnsureValidNodeIDs();
	RefreshEditorBindingData();
#endif
}

#if WITH_EDITOR
namespace ArcTacticsConfigPrivate
{
	uint32 NextRevision(const uint32 Revision)
	{
		const uint32 Next = Revision + 1;
		return Next == 0 ? 1 : Next;
	}

	void RemapID(FGuid& ID, TMap<FGuid, FGuid>& Remap)
	{
		const FGuid OldID = ID;
		ID = FGuid::NewGuid();
		if (OldID.IsValid())
		{
			Remap.Add(OldID, ID);
		}
	}

	void RemapNode(FArcTacticsNodeDefinition& Node, TMap<FGuid, FGuid>& Remap)
	{
		RemapID(Node.ID, Remap);
	}

	void RemapAction(FArcTacticsActionDefinition& Action, TMap<FGuid, FGuid>& Remap)
	{
		RemapID(Action.ParametersID, Remap);
		RemapID(Action.ParameterBagID, Remap);
		RemapNode(Action.Action, Remap);
		for (FArcTacticsNodeDefinition& Step : Action.Steps)
		{
			RemapNode(Step, Remap);
		}
		for (FArcTacticsNodeDefinition& ActiveStep : Action.ActiveSteps)
		{
			RemapNode(ActiveStep, Remap);
		}
		for (FArcTacticsNodeDefinition& Consideration : Action.Considerations)
		{
			RemapNode(Consideration, Remap);
		}
	}

	void RemapSelector(FArcTacticsSelectorDefinition& Selector, TMap<FGuid, FGuid>& Remap)
	{
		RemapID(Selector.ParametersID, Remap);
		RemapID(Selector.ParameterBagID, Remap);
		RemapNode(Selector.Selector, Remap);
		RemapNode(Selector.Action, Remap);
		for (FArcTacticsNodeDefinition& Step : Selector.Steps)
		{
			RemapNode(Step, Remap);
		}
		for (FArcTacticsNodeDefinition& ActiveStep : Selector.ActiveSteps)
		{
			RemapNode(ActiveStep, Remap);
		}
		for (FArcTacticsNodeDefinition& Consideration : Selector.Considerations)
		{
			RemapNode(Consideration, Remap);
		}
	}

	void CloneRemappedBindings(UArcTacticsConfig& Config, const TMap<FGuid, FGuid>& Remap)
	{
		TSet<FGuid> TargetIDs;
		for (const TPair<FGuid, FGuid>& Pair : Remap)
		{
			TargetIDs.Add(Pair.Key);
		}
		const TArray<FArcTacticsBinding>& ExistingBindings = Config.EditorBindings.GetBindings();
		UArcTacticsConfig::AddOwnedFunctionIDs(ExistingBindings, TargetIDs);
		TArray<FArcTacticsBinding> ClonedBindings;
		for (const FArcTacticsBinding& Binding : ExistingBindings)
		{
			if (TargetIDs.Contains(Binding.GetTargetPath().GetStructID()))
			{
				ClonedBindings.Add(Binding);
			}
		}
		Config.AddRemappedBindings(ClonedBindings, Remap);
	}

	void EnsureUniqueID(FGuid& ID, TSet<FGuid>& UsedIDs)
	{
		if (!ID.IsValid())
		{
			ID = FGuid::NewGuid();
		}
		else if (UsedIDs.Contains(ID))
		{
			ID = FGuid::NewGuid();
		}
		UsedIDs.Add(ID);
	}

	void EnsureNodeID(
		const UArcTacticsConfig& Config,
		FArcTacticsNodeDefinition& Node,
		const EArcTacticsNodeRole ExpectedRole,
		TSet<FGuid>& UsedIDs)
	{
		EnsureUniqueID(Node.ID, UsedIDs);
		Config.SynchronizeNode(Node, ExpectedRole);
	}

	void EnsureActionIDs(
		const UArcTacticsConfig& Config,
		FArcTacticsActionDefinition& Action,
		TSet<FGuid>& UsedIDs)
	{
		EnsureUniqueID(Action.ParametersID, UsedIDs);
		EnsureUniqueID(Action.ParameterBagID, UsedIDs);
		EnsureNodeID(Config, Action.Action, EArcTacticsNodeRole::Action, UsedIDs);
		for (FArcTacticsNodeDefinition& Step : Action.Steps)
		{
			EnsureNodeID(Config, Step, EArcTacticsNodeRole::Step, UsedIDs);
		}
		for (FArcTacticsNodeDefinition& ActiveStep : Action.ActiveSteps)
		{
			EnsureNodeID(Config, ActiveStep, EArcTacticsNodeRole::ActiveStep, UsedIDs);
		}
		for (FArcTacticsNodeDefinition& Consideration : Action.Considerations)
		{
			EnsureNodeID(Config, Consideration, EArcTacticsNodeRole::Consideration, UsedIDs);
		}
	}

	int32 FindGlobalTaskIndex(const UArcTacticsConfig& Config, const FGuid ID)
	{
		return Config.GlobalTasks.IndexOfByPredicate([ID](const FArcTacticsNodeDefinition& Task)
		{
			return Task.ID == ID;
		});
	}

	FName GetNodeName(const FArcTacticsNodeDefinition& Node)
	{
		if (!Node.Name.IsNone())
		{
			return Node.Name;
		}
		const FArcTacticsNode* NodeValue = Node.Node.GetPtr<FArcTacticsNode>();
		return NodeValue ? FName(*NodeValue->GetDisplayName().ToString()) : FName(TEXT("Node"));
	}

	TInstancedStruct<FPropertyBindingBindableStructDescriptor> MakeDescriptor(
		const FName Name,
		const UStruct* Struct,
		const FGuid ID)
	{
		return TInstancedStruct<FPropertyBindingBindableStructDescriptor>::Make(Name, Struct, ID);
	}

	void ForEachNodeDefinition(UArcTacticsConfig& Config, const TFunctionRef<void(FArcTacticsNodeDefinition&)> Function)
	{
		for (FArcTacticsNodeDefinition& Task : Config.GlobalTasks)
		{
			Function(Task);
		}
		for (FArcTacticsActionDefinition& Action : Config.Actions)
		{
			Function(Action.Action);
			for (FArcTacticsNodeDefinition& Step : Action.Steps)
			{
				Function(Step);
			}
			for (FArcTacticsNodeDefinition& ActiveStep : Action.ActiveSteps)
			{
				Function(ActiveStep);
			}
			for (FArcTacticsNodeDefinition& Consideration : Action.Considerations)
			{
				Function(Consideration);
			}
		}
		for (FArcTacticsSelectorDefinition& Selector : Config.Selectors)
		{
			Function(Selector.Selector);
			Function(Selector.Action);
			for (FArcTacticsNodeDefinition& Step : Selector.Steps)
			{
				Function(Step);
			}
			for (FArcTacticsNodeDefinition& ActiveStep : Selector.ActiveSteps)
			{
				Function(ActiveStep);
			}
			for (FArcTacticsNodeDefinition& Consideration : Selector.Considerations)
			{
				Function(Consideration);
			}
		}
	}

	void SyncQueryReferences(UArcTacticsConfig& Config)
	{
		ForEachNodeDefinition(Config, [](FArcTacticsNodeDefinition& Definition)
		{
			if (const UScriptStruct* InstanceType = Definition.InstanceData.GetScriptStruct())
			{
				ArcTacticsNode::ForEachQueryReference(*InstanceType, Definition.InstanceData.GetMutableMemory(),
					[](FArcTQSQueryReference& Reference)
				{
					Reference.SyncParameters();
				});
			}
		});
	}

	bool UpdateBindingPathSegments(const UArcTacticsConfig& Config, FPropertyBindingPath& Path)
	{
		FPropertyBindingDataView DataView;
		Config.GetBindingDataViewByID(Path.GetStructID(), DataView);
		return ArcTacticsBindings::UpdatePathSegments(Path, DataView);
	}

	void AddDescriptor(
		const UArcTacticsConfig& Config,
		const FGuid ID,
		TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>>& OutStructDescs)
	{
		TInstancedStruct<FPropertyBindingBindableStructDescriptor> Descriptor;
		if (Config.GetBindableStructByID(ID, Descriptor))
		{
			OutStructDescs.Add(MoveTemp(Descriptor));
		}
	}

	bool RefreshEditorBinding(UArcTacticsConfig& Config, FArcTacticsBinding& Binding)
	{
		if (!UpdateBindingPathSegments(Config, Binding.GetMutableSourcePath())
			|| !UpdateBindingPathSegments(Config, Binding.GetMutableTargetPath()))
		{
			return false;
		}
		const FGuid SourceID = Binding.GetSourcePath().GetStructID();
		const FGuid TargetID = Binding.GetTargetPath().GetStructID();
		TInstancedStruct<FPropertyBindingBindableStructDescriptor> SourceDescriptor;
		TInstancedStruct<FPropertyBindingBindableStructDescriptor> TargetDescriptor;
		if (!Config.GetBindableStructByID(SourceID, SourceDescriptor)
			|| !Config.GetBindableStructByID(TargetID, TargetDescriptor))
		{
			return false;
		}

		FArcTacticsBindingEndpoint SourceEndpoint;
		FArcTacticsBindingEndpoint TargetEndpoint;
		if (!Config.ResolveBindingEndpoint(SourceID, SourceEndpoint)
			|| !Config.ResolveBindingEndpoint(TargetID, TargetEndpoint))
		{
			return false;
		}
		const FConstStructView FunctionNode = Binding.GetPropertyFunctionNode();
		if (FunctionNode.IsValid())
		{
			const FArcTQSPropertyFunction* Function = FunctionNode.GetPtr<const FArcTQSPropertyFunction>();
			if (!Function || Function->ID != SourceID || Binding.IsOutputBinding())
			{
				return false;
			}
		}
		else if (SourceEndpoint.Kind == EArcTacticsBindingEndpointKind::Function)
		{
			return false;
		}
		return Config.EditorBindings.ValidateEditorBinding(
			Binding,
			SourceDescriptor.Get().Struct,
			SourceEndpoint,
			TargetDescriptor.Get().Struct,
			TargetEndpoint,
			Config.IsBindingSourceAccessible(SourceEndpoint, TargetEndpoint, Binding.IsOutputBinding()))
			== EArcTacticsBindingValidationResult::Valid;
	}

}

void UArcTacticsConfig::InitializeEditorBindingsOwner()
{
	TScriptInterface<IPropertyBindingBindingCollectionOwner> Owner;
	Owner.SetObject(this);
	Owner.SetInterface(this);
	EditorBindings.SetBindingsOwner(Owner);
}

bool UArcTacticsConfig::SynchronizeNode(
	FArcTacticsNodeDefinition& Definition,
	const EArcTacticsNodeRole ExpectedRole) const
{
	const FArcTacticsNode* Node = Definition.Node.GetPtr<FArcTacticsNode>();
	if (!Node)
	{
		Definition.InstanceData.Reset();
		return true;
	}
	if (Node->GetRole() != ExpectedRole)
	{
		return false;
	}
	const UScriptStruct* InstanceType = Node->GetInstanceDataType();
	if (!InstanceType)
	{
		Definition.InstanceData.Reset();
		return false;
	}
	if (Definition.InstanceData.GetScriptStruct() != InstanceType)
	{
		Definition.InstanceData.InitializeAs(InstanceType);
	}
	return true;
}

FArcTacticsNodeDefinition* UArcTacticsConfig::FindNodeByID(const FGuid& ID)
{
	return const_cast<FArcTacticsNodeDefinition*>(
		static_cast<const UArcTacticsConfig*>(this)->FindNodeByID(ID));
}

const FArcTacticsNodeDefinition* UArcTacticsConfig::FindNodeByID(const FGuid& ID) const
{
	for (const FArcTacticsNodeDefinition& Task : GlobalTasks)
	{
		if (Task.ID == ID)
		{
			return &Task;
		}
	}
	for (const FArcTacticsActionDefinition& Action : Actions)
	{
		if (Action.Action.ID == ID)
		{
			return &Action.Action;
		}
		for (const FArcTacticsNodeDefinition& Step : Action.Steps)
		{
			if (Step.ID == ID)
			{
				return &Step;
			}
		}
		for (const FArcTacticsNodeDefinition& ActiveStep : Action.ActiveSteps)
		{
			if (ActiveStep.ID == ID)
			{
				return &ActiveStep;
			}
		}
		for (const FArcTacticsNodeDefinition& Consideration : Action.Considerations)
		{
			if (Consideration.ID == ID)
			{
				return &Consideration;
			}
		}
	}
	for (const FArcTacticsSelectorDefinition& Selector : Selectors)
	{
		if (Selector.Selector.ID == ID)
		{
			return &Selector.Selector;
		}
		if (Selector.Action.ID == ID)
		{
			return &Selector.Action;
		}
		for (const FArcTacticsNodeDefinition& Step : Selector.Steps)
		{
			if (Step.ID == ID)
			{
				return &Step;
			}
		}
		for (const FArcTacticsNodeDefinition& ActiveStep : Selector.ActiveSteps)
		{
			if (ActiveStep.ID == ID)
			{
				return &ActiveStep;
			}
		}
		for (const FArcTacticsNodeDefinition& Consideration : Selector.Considerations)
		{
			if (Consideration.ID == ID)
			{
				return &Consideration;
			}
		}
	}
	return nullptr;
}

FArcTacticsSelectorDefinition* UArcTacticsConfig::FindSelectorByID(const FGuid& ID)
{
	return const_cast<FArcTacticsSelectorDefinition*>(
		static_cast<const UArcTacticsConfig*>(this)->FindSelectorByID(ID));
}

const FArcTacticsSelectorDefinition* UArcTacticsConfig::FindSelectorByID(const FGuid& ID) const
{
	return Selectors.FindByPredicate([ID](const FArcTacticsSelectorDefinition& Selector)
	{
		return Selector.Selector.ID == ID;
	});
}

FArcTacticsSelectorDefinition* UArcTacticsConfig::FindOwningSelector(const FGuid& ID)
{
	return const_cast<FArcTacticsSelectorDefinition*>(
		static_cast<const UArcTacticsConfig*>(this)->FindOwningSelector(ID));
}

const FArcTacticsSelectorDefinition* UArcTacticsConfig::FindOwningSelector(const FGuid& ID) const
{
	for (const FArcTacticsSelectorDefinition& Selector : Selectors)
	{
		if (Selector.Selector.ID == ID
			|| Selector.ParametersID == ID
			|| Selector.ParameterBagID == ID
			|| Selector.Action.ID == ID)
		{
			return &Selector;
		}
		for (const FArcTacticsNodeDefinition& Step : Selector.Steps)
		{
			if (Step.ID == ID)
			{
				return &Selector;
			}
		}
		for (const FArcTacticsNodeDefinition& ActiveStep : Selector.ActiveSteps)
		{
			if (ActiveStep.ID == ID)
			{
				return &Selector;
			}
		}
		for (const FArcTacticsNodeDefinition& Consideration : Selector.Considerations)
		{
			if (Consideration.ID == ID)
			{
				return &Selector;
			}
		}
	}
	return nullptr;
}

const FArcTacticsBinding* UArcTacticsConfig::FindFunctionBinding(const FGuid& FunctionID) const
{
	if (!FunctionID.IsValid())
	{
		return nullptr;
	}
	return EditorBindings.GetBindings().FindByPredicate([&FunctionID](const FArcTacticsBinding& Binding)
	{
		const FArcTQSPropertyFunction* Function = Binding.GetPropertyFunctionNode().GetPtr<const FArcTQSPropertyFunction>();
		return Function && Function->ID == FunctionID;
	});
}

FGuid UArcTacticsConfig::ResolveFunctionRoot(const FGuid& ID) const
{
	FGuid CurrentID = ID;
	for (int32 Hop = 0; Hop <= EditorBindings.GetNumBindings(); ++Hop)
	{
		const FArcTacticsBinding* FunctionBinding = FindFunctionBinding(CurrentID);
		if (!FunctionBinding)
		{
			return CurrentID;
		}
		CurrentID = FunctionBinding->GetTargetPath().GetStructID();
	}
	return FGuid();
}

void UArcTacticsConfig::AddOwnedFunctionIDs(
	const TConstArrayView<FArcTacticsBinding> Bindings,
	TSet<FGuid>& InOutTargetIDs)
{
	bool bAdded = true;
	while (bAdded)
	{
		bAdded = false;
		for (const FArcTacticsBinding& Binding : Bindings)
		{
			const FArcTQSPropertyFunction* Function = Binding.GetPropertyFunctionNode().GetPtr<const FArcTQSPropertyFunction>();
			if (Function
				&& Function->ID.IsValid()
				&& InOutTargetIDs.Contains(Binding.GetTargetPath().GetStructID())
				&& !InOutTargetIDs.Contains(Function->ID))
			{
				InOutTargetIDs.Add(Function->ID);
				bAdded = true;
			}
		}
	}
}

void UArcTacticsConfig::AddRemappedBindings(
	const TConstArrayView<FArcTacticsBinding> Bindings,
	const TMap<FGuid, FGuid>& Remap)
{
	TMap<FGuid, FGuid> FullRemap = Remap;
	for (const FArcTacticsBinding& Binding : Bindings)
	{
		if (const FArcTQSPropertyFunction* Function = Binding.GetPropertyFunctionNode().GetPtr<const FArcTQSPropertyFunction>())
		{
			FullRemap.Add(Function->ID, FGuid::NewGuid());
		}
	}
	for (const FArcTacticsBinding& Binding : Bindings)
	{
		const FGuid OldSourceID = Binding.GetSourcePath().GetStructID();
		const FGuid OldTargetID = Binding.GetTargetPath().GetStructID();
		const FGuid* NewSourceID = FullRemap.Find(OldSourceID);
		const FGuid* NewTargetID = FullRemap.Find(OldTargetID);
		const FPropertyBindingPath SourcePath(NewSourceID ? *NewSourceID : OldSourceID, Binding.GetSourcePath().GetSegments());
		const FPropertyBindingPath TargetPath(NewTargetID ? *NewTargetID : OldTargetID, Binding.GetTargetPath().GetSegments());
		const FConstStructView FunctionNode = Binding.GetPropertyFunctionNode();
		if (!FunctionNode.IsValid())
		{
			EditorBindings.AddBinding(SourcePath, TargetPath);
			EditorBindings.ForEachMutableBinding(
				[&SourcePath, &TargetPath, &Binding](FPropertyBindingBinding& Candidate)
			{
				if (Candidate.GetSourcePath() == SourcePath && Candidate.GetTargetPath() == TargetPath)
				{
					static_cast<FArcTacticsBinding&>(Candidate).SetIsOutputBinding(Binding.IsOutputBinding());
				}
			});
			continue;
		}
		const FArcTQSPropertyFunction* OldFunction = FunctionNode.GetPtr<const FArcTQSPropertyFunction>();
		if (!OldFunction)
		{
			continue;
		}
		EditorBindings.RemoveBindings(TargetPath, FPropertyBindingBindingCollection::ESearchMode::Exact);
		FArcTacticsBinding& Added = EditorBindings.AddCompiledBinding(FArcTacticsBinding(FunctionNode, SourcePath, TargetPath));
		const FStructView AddedNode = Added.GetMutablePropertyFunctionNode();
		AddedNode.Get<FArcTQSPropertyFunction>().ID = FullRemap.FindChecked(OldFunction->ID);
		Added.GetMutableSourcePath().UpdateSegmentsFromValue(
			FPropertyBindingDataView(AddedNode.GetScriptStruct(), AddedNode.GetMemory()));
		FPropertyBindingDataView TargetDataView;
		if (GetBindingDataViewByID(TargetPath.GetStructID(), TargetDataView))
		{
			Added.GetMutableTargetPath().UpdateSegmentsFromValue(TargetDataView);
		}
	}
}

bool UArcTacticsConfig::ResolveBindingEndpoint(
	const FGuid& ID,
	FArcTacticsBindingEndpoint& OutEndpoint) const
{
	OutEndpoint = FArcTacticsBindingEndpoint();
	if (!ID.IsValid())
	{
		return false;
	}
	if (ID == ContextID)
	{
		OutEndpoint = FArcTacticsBindingEndpoint(
			ID,
			EArcTacticsBindingEndpointKind::Context,
			EArcTacticsBindingOwner::Root,
			INDEX_NONE,
			EArcTacticsBindingPhase::Context);
		return true;
	}
	if (ID == DecisionSettingsID)
	{
		OutEndpoint = FArcTacticsBindingEndpoint(
			ID,
			EArcTacticsBindingEndpointKind::DecisionSettings,
			EArcTacticsBindingOwner::Root,
			INDEX_NONE,
			EArcTacticsBindingPhase::DecisionSettings);
		return true;
	}
	if (ID == GlobalParametersID)
	{
		OutEndpoint = FArcTacticsBindingEndpoint(
			ID,
			EArcTacticsBindingEndpointKind::GlobalParameters,
			EArcTacticsBindingOwner::Root,
			INDEX_NONE,
			EArcTacticsBindingPhase::GlobalInitialization);
		return true;
	}

	for (int32 TaskIndex = 0; TaskIndex < GlobalTasks.Num(); ++TaskIndex)
	{
		if (GlobalTasks[TaskIndex].ID == ID)
		{
			OutEndpoint = FArcTacticsBindingEndpoint(
				ID,
				EArcTacticsBindingEndpointKind::Node,
				EArcTacticsBindingOwner::GlobalTask,
				TaskIndex,
				EArcTacticsBindingPhase::GlobalTask,
				TaskIndex);
			return true;
		}
	}

	for (int32 ActionIndex = 0; ActionIndex < Actions.Num(); ++ActionIndex)
	{
		const FArcTacticsActionDefinition& Action = Actions[ActionIndex];
		if (Action.ParametersID == ID)
		{
			OutEndpoint = FArcTacticsBindingEndpoint(
				ID,
				EArcTacticsBindingEndpointKind::BranchParameters,
				EArcTacticsBindingOwner::Action,
				ActionIndex,
				EArcTacticsBindingPhase::BranchInitialization);
			return true;
		}
		if (Action.ParameterBagID == ID)
		{
			OutEndpoint = FArcTacticsBindingEndpoint(
				ID,
				EArcTacticsBindingEndpointKind::BranchParameterBag,
				EArcTacticsBindingOwner::Action,
				ActionIndex,
				EArcTacticsBindingPhase::BranchInitialization);
			return true;
		}
		if (Action.Action.ID == ID)
		{
			OutEndpoint = FArcTacticsBindingEndpoint(
				ID,
				EArcTacticsBindingEndpointKind::Node,
				EArcTacticsBindingOwner::Action,
				ActionIndex,
				EArcTacticsBindingPhase::Action,
				Action.Considerations.Num());
			return true;
		}
		for (int32 StepIndex = 0; StepIndex < Action.Steps.Num(); ++StepIndex)
		{
			if (Action.Steps[StepIndex].ID == ID)
			{
				OutEndpoint = FArcTacticsBindingEndpoint(
					ID,
					EArcTacticsBindingEndpointKind::Node,
					EArcTacticsBindingOwner::Action,
					ActionIndex,
					EArcTacticsBindingPhase::Step,
					Action.Considerations.Num() + 1 + StepIndex);
				return true;
			}
		}
		for (int32 ActiveStepIndex = 0; ActiveStepIndex < Action.ActiveSteps.Num(); ++ActiveStepIndex)
		{
			if (Action.ActiveSteps[ActiveStepIndex].ID == ID)
			{
				OutEndpoint = FArcTacticsBindingEndpoint(
					ID,
					EArcTacticsBindingEndpointKind::Node,
					EArcTacticsBindingOwner::Action,
					ActionIndex,
					EArcTacticsBindingPhase::Step,
					Action.Considerations.Num() + 1 + Action.Steps.Num() + ActiveStepIndex);
				return true;
			}
		}
		for (int32 ConsiderationIndex = 0; ConsiderationIndex < Action.Considerations.Num(); ++ConsiderationIndex)
		{
			if (Action.Considerations[ConsiderationIndex].ID == ID)
			{
				OutEndpoint = FArcTacticsBindingEndpoint(
					ID,
					EArcTacticsBindingEndpointKind::Node,
					EArcTacticsBindingOwner::Action,
					ActionIndex,
					EArcTacticsBindingPhase::Consideration,
					ConsiderationIndex);
				return true;
			}
		}
	}

	for (int32 SelectorIndex = 0; SelectorIndex < Selectors.Num(); ++SelectorIndex)
	{
		const FArcTacticsSelectorDefinition& Selector = Selectors[SelectorIndex];
		if (Selector.Selector.ID == ID)
		{
			OutEndpoint = FArcTacticsBindingEndpoint(
				ID,
				EArcTacticsBindingEndpointKind::Node,
				EArcTacticsBindingOwner::Selector,
				SelectorIndex,
				EArcTacticsBindingPhase::Selector);
			return true;
		}
		if (Selector.ParametersID == ID)
		{
			OutEndpoint = FArcTacticsBindingEndpoint(
				ID,
				EArcTacticsBindingEndpointKind::BranchParameters,
				EArcTacticsBindingOwner::Selector,
				SelectorIndex,
				EArcTacticsBindingPhase::BranchInitialization);
			return true;
		}
		if (Selector.ParameterBagID == ID)
		{
			OutEndpoint = FArcTacticsBindingEndpoint(
				ID,
				EArcTacticsBindingEndpointKind::BranchParameterBag,
				EArcTacticsBindingOwner::Selector,
				SelectorIndex,
				EArcTacticsBindingPhase::BranchInitialization);
			return true;
		}
		if (Selector.Action.ID == ID)
		{
			OutEndpoint = FArcTacticsBindingEndpoint(
				ID,
				EArcTacticsBindingEndpointKind::Node,
				EArcTacticsBindingOwner::Selector,
				SelectorIndex,
				EArcTacticsBindingPhase::Action,
				Selector.Considerations.Num());
			return true;
		}
		for (int32 StepIndex = 0; StepIndex < Selector.Steps.Num(); ++StepIndex)
		{
			if (Selector.Steps[StepIndex].ID == ID)
			{
				OutEndpoint = FArcTacticsBindingEndpoint(
					ID,
					EArcTacticsBindingEndpointKind::Node,
					EArcTacticsBindingOwner::Selector,
					SelectorIndex,
					EArcTacticsBindingPhase::Step,
					Selector.Considerations.Num() + 1 + StepIndex);
				return true;
			}
		}
		for (int32 ActiveStepIndex = 0; ActiveStepIndex < Selector.ActiveSteps.Num(); ++ActiveStepIndex)
		{
			if (Selector.ActiveSteps[ActiveStepIndex].ID == ID)
			{
				OutEndpoint = FArcTacticsBindingEndpoint(
					ID,
					EArcTacticsBindingEndpointKind::Node,
					EArcTacticsBindingOwner::Selector,
					SelectorIndex,
					EArcTacticsBindingPhase::Step,
					Selector.Considerations.Num() + 1 + Selector.Steps.Num() + ActiveStepIndex);
				return true;
			}
		}
		for (int32 ConsiderationIndex = 0; ConsiderationIndex < Selector.Considerations.Num(); ++ConsiderationIndex)
		{
			if (Selector.Considerations[ConsiderationIndex].ID == ID)
			{
				OutEndpoint = FArcTacticsBindingEndpoint(
					ID,
					EArcTacticsBindingEndpointKind::Node,
					EArcTacticsBindingOwner::Selector,
					SelectorIndex,
					EArcTacticsBindingPhase::Consideration,
					ConsiderationIndex);
				return true;
			}
		}
	}

	const FGuid RootID = ResolveFunctionRoot(ID);
	FArcTacticsBindingEndpoint RootEndpoint;
	if (!RootID.IsValid()
		|| RootID == ID
		|| !ResolveBindingEndpoint(RootID, RootEndpoint)
		|| RootEndpoint.Kind == EArcTacticsBindingEndpointKind::Context)
	{
		return false;
	}
	OutEndpoint = RootEndpoint;
	OutEndpoint.ID = ID;
	OutEndpoint.Kind = EArcTacticsBindingEndpointKind::Function;
	return true;
}

bool UArcTacticsConfig::IsBindingSourceAccessible(
	const FGuid& SourceID,
	const FGuid& TargetID,
	const bool bOutputBinding) const
{
	FArcTacticsBindingEndpoint Source;
	FArcTacticsBindingEndpoint Target;
	if (!ResolveBindingEndpoint(SourceID, Source) || !ResolveBindingEndpoint(TargetID, Target))
	{
		return false;
	}
	return IsBindingSourceAccessible(Source, Target, bOutputBinding);
}

bool UArcTacticsConfig::IsBindingSourceAccessible(
	const FArcTacticsBindingEndpoint& Source,
	const FArcTacticsBindingEndpoint& Target,
	const bool bOutputBinding) const
{
	if (!Source.IsValid() || !Target.IsValid())
	{
		return false;
	}
	const bool bSourceIsFunction = Source.Kind == EArcTacticsBindingEndpointKind::Function;
	if (bOutputBinding && (bSourceIsFunction || Target.Kind == EArcTacticsBindingEndpointKind::Function))
	{
		return false;
	}
	if (bSourceIsFunction)
	{
		return Source.RootID == Target.RootID;
	}

	if (bOutputBinding)
	{
		if (Target.Kind != EArcTacticsBindingEndpointKind::Node
			|| Target.Phase == EArcTacticsBindingPhase::Selector)
		{
			return false;
		}
		if (Target.Owner == EArcTacticsBindingOwner::GlobalTask)
		{
			return Source.Kind == EArcTacticsBindingEndpointKind::GlobalParameters;
		}
		return Source.Kind == EArcTacticsBindingEndpointKind::BranchParameterBag
			&& Source.Owner == Target.Owner
			&& Source.OwnerIndex == Target.OwnerIndex;
	}

	const EArcTacticsBindingEndpointKind TargetKind = Target.RootKind;
	if (TargetKind == EArcTacticsBindingEndpointKind::GlobalParameters)
	{
		return Source.Kind == EArcTacticsBindingEndpointKind::Context;
	}
	const bool bSourceIsSameSelector = Source.Kind == EArcTacticsBindingEndpointKind::Node
		&& Source.Phase == EArcTacticsBindingPhase::Selector
		&& Source.Owner == Target.Owner
		&& Source.OwnerIndex == Target.OwnerIndex;
	if (TargetKind == EArcTacticsBindingEndpointKind::DecisionSettings
		|| (TargetKind == EArcTacticsBindingEndpointKind::Node
			&& Target.Phase == EArcTacticsBindingPhase::Selector))
	{
		return Source.Kind == EArcTacticsBindingEndpointKind::Context
			|| Source.Kind == EArcTacticsBindingEndpointKind::GlobalParameters
			|| Source.Owner == EArcTacticsBindingOwner::GlobalTask;
	}
	if (TargetKind == EArcTacticsBindingEndpointKind::BranchParameterBag)
	{
		return Source.Kind == EArcTacticsBindingEndpointKind::Context
			|| Source.Kind == EArcTacticsBindingEndpointKind::GlobalParameters
			|| Source.Owner == EArcTacticsBindingOwner::GlobalTask
			|| bSourceIsSameSelector;
	}
	if (TargetKind == EArcTacticsBindingEndpointKind::BranchParameters)
	{
		return Source.Kind == EArcTacticsBindingEndpointKind::Context
			|| Source.Kind == EArcTacticsBindingEndpointKind::GlobalParameters
			|| Source.Owner == EArcTacticsBindingOwner::GlobalTask
			|| bSourceIsSameSelector
			|| (Source.Kind == EArcTacticsBindingEndpointKind::BranchParameterBag
				&& Source.Owner == Target.Owner
				&& Source.OwnerIndex == Target.OwnerIndex);
	}
	if (TargetKind != EArcTacticsBindingEndpointKind::Node)
	{
		return false;
	}
	if (Source.Kind == EArcTacticsBindingEndpointKind::Context)
	{
		return true;
	}
	if (Target.Owner == EArcTacticsBindingOwner::GlobalTask)
	{
		return Source.Kind == EArcTacticsBindingEndpointKind::GlobalParameters
			|| (Source.Owner == EArcTacticsBindingOwner::GlobalTask
				&& Source.ExecutionIndex < Target.ExecutionIndex);
	}
	if (Source.Kind == EArcTacticsBindingEndpointKind::GlobalParameters
		|| Source.Owner == EArcTacticsBindingOwner::GlobalTask)
	{
		return true;
	}
	return bSourceIsSameSelector
		|| (Source.Owner == Target.Owner
			&& Source.OwnerIndex == Target.OwnerIndex
			&& (Source.Kind == EArcTacticsBindingEndpointKind::BranchParameters
				|| Source.Kind == EArcTacticsBindingEndpointKind::BranchParameterBag));
}

TMap<FGuid, FGuid> UArcTacticsConfig::DuplicateAuthoringData(FArcTacticsNodeDefinition& Definition)
{
	TMap<FGuid, FGuid> Remap;
	ArcTacticsConfigPrivate::RemapNode(Definition, Remap);
	ArcTacticsConfigPrivate::CloneRemappedBindings(*this, Remap);
	return Remap;
}

TMap<FGuid, FGuid> UArcTacticsConfig::DuplicateAuthoringData(FArcTacticsActionDefinition& Definition)
{
	TMap<FGuid, FGuid> Remap;
	ArcTacticsConfigPrivate::RemapAction(Definition, Remap);
	ArcTacticsConfigPrivate::CloneRemappedBindings(*this, Remap);
	return Remap;
}

TMap<FGuid, FGuid> UArcTacticsConfig::DuplicateAuthoringData(FArcTacticsSelectorDefinition& Definition)
{
	TMap<FGuid, FGuid> Remap;
	ArcTacticsConfigPrivate::RemapSelector(Definition, Remap);
	ArcTacticsConfigPrivate::CloneRemappedBindings(*this, Remap);
	return Remap;
}

void UArcTacticsConfig::EnsureValidNodeIDs()
{
	TSet<FGuid> UsedIDs;
	ArcTacticsConfigPrivate::EnsureUniqueID(ContextID, UsedIDs);
	ArcTacticsConfigPrivate::EnsureUniqueID(DecisionSettingsID, UsedIDs);
	ArcTacticsConfigPrivate::EnsureUniqueID(GlobalParametersID, UsedIDs);
	for (FArcTacticsNodeDefinition& Task : GlobalTasks)
	{
		ArcTacticsConfigPrivate::EnsureNodeID(
			*this, Task, EArcTacticsNodeRole::GlobalTask, UsedIDs);
	}
	for (FArcTacticsActionDefinition& Action : Actions)
	{
		ArcTacticsConfigPrivate::EnsureActionIDs(*this, Action, UsedIDs);
	}
	for (FArcTacticsSelectorDefinition& Selector : Selectors)
	{
		ArcTacticsConfigPrivate::EnsureUniqueID(Selector.ParametersID, UsedIDs);
		ArcTacticsConfigPrivate::EnsureUniqueID(Selector.ParameterBagID, UsedIDs);
		ArcTacticsConfigPrivate::EnsureNodeID(
			*this, Selector.Selector, EArcTacticsNodeRole::Selector, UsedIDs);
		ArcTacticsConfigPrivate::EnsureNodeID(
			*this, Selector.Action, EArcTacticsNodeRole::Action, UsedIDs);
		for (FArcTacticsNodeDefinition& Step : Selector.Steps)
		{
			ArcTacticsConfigPrivate::EnsureNodeID(
				*this, Step, EArcTacticsNodeRole::Step, UsedIDs);
		}
		for (FArcTacticsNodeDefinition& ActiveStep : Selector.ActiveSteps)
		{
			ArcTacticsConfigPrivate::EnsureNodeID(
				*this, ActiveStep, EArcTacticsNodeRole::ActiveStep, UsedIDs);
		}
		for (FArcTacticsNodeDefinition& Consideration : Selector.Considerations)
		{
			ArcTacticsConfigPrivate::EnsureNodeID(
				*this, Consideration, EArcTacticsNodeRole::Consideration, UsedIDs);
		}
	}
}

void UArcTacticsConfig::RefreshEditorBindingData()
{
	bool bRemoved = true;
	while (bRemoved)
	{
		const TArray<FArcTacticsBinding>& Bindings = EditorBindings.GetBindings();
		TArray<FArcTacticsBinding> Kept;
		Kept.Reserve(Bindings.Num());
		for (const FArcTacticsBinding& Binding : Bindings)
		{
			FArcTacticsBinding Candidate = Binding;
			if (ArcTacticsConfigPrivate::RefreshEditorBinding(*this, Candidate))
			{
				Kept.Add(MoveTemp(Candidate));
			}
		}
		bRemoved = Kept.Num() != Bindings.Num();
		EditorBindings.RemoveBindings([](FPropertyBindingBinding&)
		{
			return true;
		});
		for (FArcTacticsBinding& Binding : Kept)
		{
			EditorBindings.AddCompiledBinding(MoveTemp(Binding));
		}
	}
}

void UArcTacticsConfig::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
	FProperty* MemberProperty = PropertyChangedEvent.PropertyChain.GetActiveMemberNode()
		? PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue()
		: nullptr;
	FProperty* ChangedProperty = PropertyChangedEvent.Property;
	if (ChangedProperty && PropertyChangedEvent.ChangeType == EPropertyChangeType::Duplicate)
	{
		const FName ChangedPropertyName = ChangedProperty->GetFName();
		const int32 ArrayIndex = PropertyChangedEvent.GetArrayIndex(ChangedProperty);
		if (ChangedPropertyName == GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, GlobalTasks)
			&& GlobalTasks.IsValidIndex(ArrayIndex))
		{
			DuplicateAuthoringData(GlobalTasks[ArrayIndex]);
		}
		else if (ChangedPropertyName == GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, Actions)
			&& Actions.IsValidIndex(ArrayIndex))
		{
			DuplicateAuthoringData(Actions[ArrayIndex]);
		}
		else if (ChangedPropertyName == GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, Selectors)
			&& Selectors.IsValidIndex(ArrayIndex))
		{
			DuplicateAuthoringData(Selectors[ArrayIndex]);
		}
		else if (MemberProperty
			&& (ChangedPropertyName == GET_MEMBER_NAME_CHECKED(FArcTacticsActionDefinition, Considerations)
				|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(FArcTacticsActionDefinition, Steps)
				|| ChangedPropertyName == GET_MEMBER_NAME_CHECKED(FArcTacticsActionDefinition, ActiveSteps)))
		{
			const bool bSteps = ChangedPropertyName == GET_MEMBER_NAME_CHECKED(FArcTacticsActionDefinition, Steps);
			const bool bActiveSteps = ChangedPropertyName == GET_MEMBER_NAME_CHECKED(FArcTacticsActionDefinition, ActiveSteps);
			const int32 OwnerIndex = PropertyChangedEvent.GetArrayIndex(MemberProperty);
			TArray<FArcTacticsNodeDefinition>* Nodes = nullptr;
			if (MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, Actions)
				&& Actions.IsValidIndex(OwnerIndex))
			{
				FArcTacticsActionDefinition& Action = Actions[OwnerIndex];
				Nodes = bSteps ? &Action.Steps : bActiveSteps ? &Action.ActiveSteps : &Action.Considerations;
			}
			else if (MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, Selectors)
				&& Selectors.IsValidIndex(OwnerIndex))
			{
				FArcTacticsSelectorDefinition& Selector = Selectors[OwnerIndex];
				Nodes = bSteps ? &Selector.Steps : bActiveSteps ? &Selector.ActiveSteps : &Selector.Considerations;
			}
			if (Nodes && Nodes->IsValidIndex(ArrayIndex))
			{
				DuplicateAuthoringData((*Nodes)[ArrayIndex]);
			}
		}
	}

	const uint32 StructuralChangeTypes = EPropertyChangeType::ArrayAdd
		| EPropertyChangeType::ArrayRemove
		| EPropertyChangeType::ArrayClear
		| EPropertyChangeType::Duplicate
		| EPropertyChangeType::ArrayMove;
	const FName PropertyName = PropertyChangedEvent.Property
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;
	const bool bStructureChanged = (PropertyChangedEvent.ChangeType & StructuralChangeTypes) != 0
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FArcTacticsNodeDefinition, Node)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FArcTacticsNodeDefinition, InstanceData)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UArcTacticsConfig, GlobalParameters)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FArcTacticsActionDefinition, ParameterBag)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FArcTacticsSelectorDefinition, ParameterBag);
	NotifyAuthoringChanged(
		bStructureChanged ? EArcTacticsAuthoringChange::Structure : EArcTacticsAuthoringChange::Value);
}

void UArcTacticsConfig::PostEditUndo()
{
	Super::PostEditUndo();
	InitializeEditorBindingsOwner();
	NotifyAuthoringChanged(EArcTacticsAuthoringChange::Structure);
}

void UArcTacticsConfig::NotifyAuthoringChanged(const EArcTacticsAuthoringChange Change)
{
	EnsureValidNodeIDs();
	RefreshEditorBindingData();
	CompiledData.bIsValid = false;
	MarkPackageDirty();
	AuthoringChanged.Broadcast(Change);
}

void UArcTacticsConfig::PostDuplicate(const EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	InitializeEditorBindingsOwner();
}

bool UArcTacticsConfig::CompileEditorData(TArray<FArcTacticsCompilerDiagnostic>& OutDiagnostics)
{
	EnsureValidNodeIDs();
	ArcTacticsConfigPrivate::SyncQueryReferences(*this);
	RefreshEditorBindingData();
	FArcTacticsCompiledData NewCompiledData;
	const bool bCompiled = FArcTacticsCompiler::Compile(
		*this,
		ArcTacticsConfigPrivate::NextRevision(CompiledData.Revision),
		NewCompiledData,
		OutDiagnostics);
	CompiledData = MoveTemp(NewCompiledData);
	return bCompiled;
}

void UArcTacticsConfig::PreSave(const FObjectPreSaveContext SaveContext)
{
	TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
	CompileEditorData(Diagnostics);
	Super::PreSave(SaveContext);
}

EDataValidationResult UArcTacticsConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	FArcTacticsCompiledData ValidationData;
	TArray<FArcTacticsCompilerDiagnostic> Diagnostics;
	const bool bCompiled = FArcTacticsCompiler::Compile(
		*this,
		ArcTacticsConfigPrivate::NextRevision(CompiledData.Revision),
		ValidationData,
		Diagnostics);
	for (const FArcTacticsCompilerDiagnostic& Diagnostic : Diagnostics)
	{
		if (Diagnostic.bIsError)
		{
			Context.AddError(Diagnostic.Message);
		}
		else
		{
			Context.AddWarning(Diagnostic.Message);
		}
	}
	if (!bCompiled)
	{
		return EDataValidationResult::Invalid;
	}
	return Result == EDataValidationResult::Invalid ? Result : EDataValidationResult::Valid;
}

FText UArcTacticsConfig::GetAuthoringName(const FGuid ID) const
{
	if (!ID.IsValid())
	{
		return NSLOCTEXT("ArcTactics", "UnknownAuthoringName", "Unknown Node");
	}
	if (ID == ContextID)
	{
		return NSLOCTEXT("ArcTactics", "ContextAuthoringName", "Context");
	}
	if (ID == DecisionSettingsID)
	{
		return NSLOCTEXT("ArcTactics", "DecisionSettingsAuthoringName", "Decision Settings");
	}
	if (ID == GlobalParametersID)
	{
		return NSLOCTEXT("ArcTactics", "GlobalParametersAuthoringName", "Global Parameters");
	}
	if (const FArcTacticsNodeDefinition* Node = FindNodeByID(ID))
	{
		return FText::FromName(ArcTacticsConfigPrivate::GetNodeName(*Node));
	}
	for (const FArcTacticsActionDefinition& Action : Actions)
	{
		const FText ActionName = FText::FromName(ArcTacticsConfigPrivate::GetNodeName(Action.Action));
		if (Action.ParametersID == ID)
		{
			return FText::Format(
				NSLOCTEXT("ArcTactics", "ActionParametersAuthoringName", "{0} Parameters"),
				ActionName);
		}
		if (Action.ParameterBagID == ID)
		{
			return FText::Format(
				NSLOCTEXT("ArcTactics", "ActionParameterBagAuthoringName", "{0} Parameter Bag"),
				ActionName);
		}
	}
	for (const FArcTacticsSelectorDefinition& Selector : Selectors)
	{
		const FText SelectorName = FText::FromName(ArcTacticsConfigPrivate::GetNodeName(Selector.Selector));
		if (Selector.ParametersID == ID)
		{
			return FText::Format(
				NSLOCTEXT("ArcTactics", "SelectorParametersAuthoringName", "{0} Parameters"),
				SelectorName);
		}
		if (Selector.ParameterBagID == ID)
		{
			return FText::Format(
				NSLOCTEXT("ArcTactics", "SelectorParameterBagAuthoringName", "{0} Parameter Bag"),
				SelectorName);
		}
	}
	return NSLOCTEXT("ArcTactics", "UnknownAuthoringName", "Unknown Node");
}

bool UArcTacticsConfig::GetBindingDataView(
	const FPropertyBindingBinding& InBinding,
	const EBindingSide InSide,
	FPropertyBindingDataView& OutDataView)
{
	const FGuid ID = InSide == EBindingSide::Source
		? InBinding.GetSourcePath().GetStructID()
		: InBinding.GetTargetPath().GetStructID();
	return GetBindingDataViewByID(ID, OutDataView);
}

void UArcTacticsConfig::GetBindableStructs(
	const FGuid InTargetStructID,
	TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>>& OutStructDescs) const
{
	OutStructDescs.Reset();
	FArcTacticsBindingEndpoint TargetEndpoint;
	if (!ResolveBindingEndpoint(InTargetStructID, TargetEndpoint))
	{
		return;
	}
	if (IsBindingSourceAccessible(ContextID, InTargetStructID))
	{
		ArcTacticsConfigPrivate::AddDescriptor(*this, ContextID, OutStructDescs);
	}
	if (IsBindingSourceAccessible(GlobalParametersID, InTargetStructID))
	{
		ArcTacticsConfigPrivate::AddDescriptor(*this, GlobalParametersID, OutStructDescs);
	}
	for (const FArcTacticsNodeDefinition& Task : GlobalTasks)
	{
		if (IsBindingSourceAccessible(Task.ID, InTargetStructID))
		{
			ArcTacticsConfigPrivate::AddDescriptor(*this, Task.ID, OutStructDescs);
		}
	}
	for (const FArcTacticsActionDefinition& Action : Actions)
	{
		if (IsBindingSourceAccessible(Action.ParametersID, InTargetStructID))
		{
			ArcTacticsConfigPrivate::AddDescriptor(*this, Action.ParametersID, OutStructDescs);
		}
		if (IsBindingSourceAccessible(Action.ParameterBagID, InTargetStructID))
		{
			ArcTacticsConfigPrivate::AddDescriptor(*this, Action.ParameterBagID, OutStructDescs);
		}
	}
	for (const FArcTacticsSelectorDefinition& Selector : Selectors)
	{
		if (IsBindingSourceAccessible(Selector.ParametersID, InTargetStructID))
		{
			ArcTacticsConfigPrivate::AddDescriptor(*this, Selector.ParametersID, OutStructDescs);
		}
		if (IsBindingSourceAccessible(Selector.ParameterBagID, InTargetStructID))
		{
			ArcTacticsConfigPrivate::AddDescriptor(*this, Selector.ParameterBagID, OutStructDescs);
		}
		if (IsBindingSourceAccessible(Selector.Selector.ID, InTargetStructID))
		{
			ArcTacticsConfigPrivate::AddDescriptor(*this, Selector.Selector.ID, OutStructDescs);
		}
	}
}

bool UArcTacticsConfig::GetBindableStructByID(
	const FGuid InStructID,
	TInstancedStruct<FPropertyBindingBindableStructDescriptor>& OutStructDesc) const
{
	if (!InStructID.IsValid())
	{
		return false;
	}
	if (InStructID == ContextID)
	{
		OutStructDesc = ArcTacticsConfigPrivate::MakeDescriptor(
			TEXT("Context"), FArcTacticsContextData::StaticStruct(), ContextID);
		return true;
	}
	if (InStructID == DecisionSettingsID)
	{
		OutStructDesc = ArcTacticsConfigPrivate::MakeDescriptor(
			TEXT("DecisionSettings"),
			FArcTacticsDecisionSettingsParameters::StaticStruct(),
			DecisionSettingsID);
		return true;
	}
	if (InStructID == GlobalParametersID && GlobalParameters.GetPropertyBagStruct())
	{
		OutStructDesc = ArcTacticsConfigPrivate::MakeDescriptor(
			TEXT("GlobalParameters"),
			GlobalParameters.GetPropertyBagStruct(),
			GlobalParametersID);
		return true;
	}
	if (const FArcTacticsNodeDefinition* Node = FindNodeByID(InStructID))
	{
		const UScriptStruct* Struct = Node->InstanceData.GetScriptStruct();
		if (!Struct)
		{
			return false;
		}
		OutStructDesc = ArcTacticsConfigPrivate::MakeDescriptor(
			ArcTacticsConfigPrivate::GetNodeName(*Node),
			Struct,
			InStructID);
		return true;
	}
	for (const FArcTacticsActionDefinition& Action : Actions)
	{
		if (Action.ParametersID == InStructID)
		{
			OutStructDesc = ArcTacticsConfigPrivate::MakeDescriptor(
				TEXT("Parameters"),
				FArcTacticsBranchParameters::StaticStruct(),
				InStructID);
			return true;
		}
		if (Action.ParameterBagID == InStructID && Action.ParameterBag.GetPropertyBagStruct())
		{
			OutStructDesc = ArcTacticsConfigPrivate::MakeDescriptor(
				TEXT("ParameterBag"),
				Action.ParameterBag.GetPropertyBagStruct(),
				InStructID);
			return true;
		}
	}
	for (const FArcTacticsSelectorDefinition& Selector : Selectors)
	{
		if (Selector.ParametersID == InStructID)
		{
			OutStructDesc = ArcTacticsConfigPrivate::MakeDescriptor(
				TEXT("Parameters"),
				FArcTacticsBranchParameters::StaticStruct(),
				InStructID);
			return true;
		}
		if (Selector.ParameterBagID == InStructID && Selector.ParameterBag.GetPropertyBagStruct())
		{
			OutStructDesc = ArcTacticsConfigPrivate::MakeDescriptor(
				TEXT("ParameterBag"),
				Selector.ParameterBag.GetPropertyBagStruct(),
				InStructID);
			return true;
		}
	}
	if (const FArcTacticsBinding* FunctionBinding = FindFunctionBinding(InStructID))
	{
		const UScriptStruct* FunctionStruct = FunctionBinding->GetPropertyFunctionNode().GetScriptStruct();
		OutStructDesc = ArcTacticsConfigPrivate::MakeDescriptor(
			FName(*FunctionStruct->GetDisplayNameText().ToString()),
			FunctionStruct,
			InStructID);
		return true;
	}
	return false;
}

bool UArcTacticsConfig::GetBindingDataViewByID(
	const FGuid InStructID,
	FPropertyBindingDataView& OutDataView) const
{
	if (!InStructID.IsValid())
	{
		return false;
	}
	if (InStructID == ContextID)
	{
		OutDataView = FPropertyBindingDataView(
			FArcTacticsContextData::StaticStruct(),
			const_cast<FArcTacticsContextData*>(&EditorContextData));
		return true;
	}
	if (InStructID == DecisionSettingsID)
	{
		OutDataView = FPropertyBindingDataView(
			FArcTacticsDecisionSettingsParameters::StaticStruct(),
			const_cast<FArcTacticsDecisionSettingsParameters*>(&DecisionSettings));
		return true;
	}
	if (InStructID == GlobalParametersID && GlobalParameters.IsValid())
	{
		OutDataView = FPropertyBindingDataView(
			const_cast<FInstancedPropertyBag&>(GlobalParameters).GetMutableValue());
		return true;
	}
	if (const FArcTacticsNodeDefinition* Node = FindNodeByID(InStructID))
	{
		if (Node->InstanceData.IsValid())
		{
			OutDataView = FPropertyBindingDataView(
				Node->InstanceData.GetScriptStruct(),
				const_cast<FInstancedStruct&>(Node->InstanceData).GetMutableMemory());
			return true;
		}
		return false;
	}
	for (const FArcTacticsActionDefinition& Action : Actions)
	{
		if (Action.ParametersID == InStructID)
		{
			OutDataView = FPropertyBindingDataView(
				FArcTacticsBranchParameters::StaticStruct(),
				const_cast<FArcTacticsBranchParameters*>(&Action.Parameters));
			return true;
		}
		if (Action.ParameterBagID == InStructID && Action.ParameterBag.IsValid())
		{
			OutDataView = FPropertyBindingDataView(
				const_cast<FInstancedPropertyBag&>(Action.ParameterBag).GetMutableValue());
			return true;
		}
	}
	for (const FArcTacticsSelectorDefinition& Selector : Selectors)
	{
		if (Selector.ParametersID == InStructID)
		{
			OutDataView = FPropertyBindingDataView(
				FArcTacticsBranchParameters::StaticStruct(),
				const_cast<FArcTacticsBranchParameters*>(&Selector.Parameters));
			return true;
		}
		if (Selector.ParameterBagID == InStructID && Selector.ParameterBag.IsValid())
		{
			OutDataView = FPropertyBindingDataView(
				const_cast<FInstancedPropertyBag&>(Selector.ParameterBag).GetMutableValue());
			return true;
		}
	}
	if (const FArcTacticsBinding* FunctionBinding = FindFunctionBinding(InStructID))
	{
		const FConstStructView FunctionNode = FunctionBinding->GetPropertyFunctionNode();
		OutDataView = FPropertyBindingDataView(
			FunctionNode.GetScriptStruct(),
			const_cast<uint8*>(FunctionNode.GetMemory()));
		return true;
	}
	return false;
}

FPropertyBindingBindingCollection* UArcTacticsConfig::GetEditorPropertyBindings()
{
	return &EditorBindings;
}

const FPropertyBindingBindingCollection* UArcTacticsConfig::GetEditorPropertyBindings() const
{
	return &EditorBindings;
}

bool UArcTacticsConfig::CanCreateParameter(const FGuid InStructID) const
{
	FArcTacticsBindingEndpoint Endpoint;
	return ResolveBindingEndpoint(InStructID, Endpoint)
		&& (Endpoint.Kind == EArcTacticsBindingEndpointKind::GlobalParameters
			|| Endpoint.Kind == EArcTacticsBindingEndpointKind::BranchParameterBag);
}

void UArcTacticsConfig::CreateParametersForStruct(
	const FGuid InStructID,
	const TArrayView<UE::PropertyBinding::FPropertyCreationDescriptor> InOutCreationDescs)
{
	if (InOutCreationDescs.IsEmpty())
	{
		return;
	}
	if (InStructID == GlobalParametersID)
	{
		UE::PropertyBinding::CreateUniquelyNamedPropertiesInPropertyBag(
			InOutCreationDescs, GlobalParameters);
		NotifyAuthoringChanged(EArcTacticsAuthoringChange::Structure);
		return;
	}
	for (FArcTacticsActionDefinition& Action : Actions)
	{
		if (Action.ParameterBagID == InStructID)
		{
			UE::PropertyBinding::CreateUniquelyNamedPropertiesInPropertyBag(
				InOutCreationDescs, Action.ParameterBag);
			NotifyAuthoringChanged(EArcTacticsAuthoringChange::Structure);
			return;
		}
	}
	for (FArcTacticsSelectorDefinition& Selector : Selectors)
	{
		if (Selector.ParameterBagID == InStructID)
		{
			UE::PropertyBinding::CreateUniquelyNamedPropertiesInPropertyBag(
				InOutCreationDescs, Selector.ParameterBag);
			NotifyAuthoringChanged(EArcTacticsAuthoringChange::Structure);
			return;
		}
	}
}

FGuid UArcTacticsConfig::GetFallbackStructID() const
{
	return FGuid();
}

void UArcTacticsConfig::AppendBindablePropertyFunctionStructs(
	TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>>& InOutStructs) const
{
	const UScriptStruct* BaseStruct = FArcTQSPropertyFunction::StaticStruct();
	TArray<const UScriptStruct*> FunctionStructs;
	for (TObjectIterator<UScriptStruct> StructIt; StructIt; ++StructIt)
	{
		const UScriptStruct* Struct = *StructIt;
		int32 NumOutputs = 0;
		if (Struct != BaseStruct
			&& Struct->IsChildOf(BaseStruct)
			&& !Struct->HasAnyFlags(RF_NewerVersionExists)
			&& !Struct->HasMetaData(TEXT("Hidden"))
			&& ArcTQS::FindFunctionOutputProperty(Struct, &NumOutputs)
			&& NumOutputs == 1)
		{
			FunctionStructs.Add(Struct);
		}
	}
	FunctionStructs.Sort([](const UScriptStruct& A, const UScriptStruct& B)
	{
		return A.GetPathName() < B.GetPathName();
	});
	for (const UScriptStruct* Struct : FunctionStructs)
	{
		InOutStructs.Add(ArcTacticsConfigPrivate::MakeDescriptor(
			FName(*Struct->GetDisplayNameText().ToString()),
			Struct,
			FGuid::NewDeterministicGuid(Struct->GetPathName())));
	}
}

void UArcTacticsConfig::OnPropertyBindingChanged(
	const FPropertyBindingPath& InSourcePath,
	const FPropertyBindingPath& InTargetPath)
{
	NotifyAuthoringChanged(EArcTacticsAuthoringChange::Value);
}
#endif
