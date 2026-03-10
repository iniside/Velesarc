// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCP/StateTree/ArcStateTreeJsonParser.h"

#include "Dom/JsonObject.h"

bool FArcStateTreeJsonParser::Parse(const TSharedPtr<FJsonObject>& Json, FArcSTDescription& OutDesc)
{
	Errors.Empty();
	Warnings.Empty();

	if (!Json.IsValid())
	{
		AddError(TEXT("Invalid JSON object"));
		return false;
	}

	// Required top-level fields
	if (!Json->TryGetStringField(TEXT("name"), OutDesc.Name) || OutDesc.Name.IsEmpty())
	{
		AddError(TEXT("Missing required field: 'name'"));
	}
	if (!Json->TryGetStringField(TEXT("package_path"), OutDesc.PackagePath) || OutDesc.PackagePath.IsEmpty())
	{
		AddError(TEXT("Missing required field: 'package_path'"));
	}
	if (!Json->TryGetStringField(TEXT("schema"), OutDesc.Schema) || OutDesc.Schema.IsEmpty())
	{
		AddError(TEXT("Missing required field: 'schema'"));
	}

	// Optional parameters
	if (Json->HasField(TEXT("parameters")))
	{
		OutDesc.Parameters = Json->GetObjectField(TEXT("parameters"));
	}

	// Evaluators
	if (Json->HasField(TEXT("evaluators")))
	{
		const TArray<TSharedPtr<FJsonValue>>& EvalArray = Json->GetArrayField(TEXT("evaluators"));
		for (const auto& Val : EvalArray)
		{
			FArcSTNodeDesc Node;
			if (ParseNode(Val->AsObject(), Node, TEXT("evaluator")))
			{
				OutDesc.Evaluators.Add(MoveTemp(Node));
			}
		}
	}

	// Global tasks
	if (Json->HasField(TEXT("global_tasks")))
	{
		const TArray<TSharedPtr<FJsonValue>>& TaskArray = Json->GetArrayField(TEXT("global_tasks"));
		for (const auto& Val : TaskArray)
		{
			FArcSTNodeDesc Node;
			if (ParseNode(Val->AsObject(), Node, TEXT("global_task")))
			{
				OutDesc.GlobalTasks.Add(MoveTemp(Node));
			}
		}
	}

	// States (required)
	if (!Json->HasField(TEXT("states")))
	{
		AddError(TEXT("Missing required field: 'states'"));
	}
	else
	{
		const TArray<TSharedPtr<FJsonValue>>& StatesArray = Json->GetArrayField(TEXT("states"));
		if (StatesArray.IsEmpty())
		{
			AddError(TEXT("'states' array must not be empty"));
		}
		for (const auto& Val : StatesArray)
		{
			FArcSTStateDesc State;
			if (ParseState(Val->AsObject(), State))
			{
				OutDesc.States.Add(MoveTemp(State));
			}
		}
	}

	// Bindings
	if (Json->HasField(TEXT("bindings")))
	{
		const TArray<TSharedPtr<FJsonValue>>& BindArray = Json->GetArrayField(TEXT("bindings"));
		for (const auto& Val : BindArray)
		{
			FArcSTBindingDesc Binding;
			if (ParseBinding(Val->AsObject(), Binding))
			{
				OutDesc.Bindings.Add(MoveTemp(Binding));
			}
		}
	}

	// Validate cross-references
	if (Errors.IsEmpty())
	{
		ValidateReferences(OutDesc);
	}

	return Errors.IsEmpty();
}

bool FArcStateTreeJsonParser::ParseNode(const TSharedPtr<FJsonObject>& Json, FArcSTNodeDesc& OutNode, const FString& Context)
{
	if (!Json.IsValid())
	{
		AddError(FString::Printf(TEXT("%s: invalid JSON object"), *Context));
		return false;
	}

	if (!Json->TryGetStringField(TEXT("id"), OutNode.Id) || OutNode.Id.IsEmpty())
	{
		AddError(FString::Printf(TEXT("%s: missing required field 'id'"), *Context));
		return false;
	}
	if (!Json->TryGetStringField(TEXT("type"), OutNode.Type) || OutNode.Type.IsEmpty())
	{
		AddError(FString::Printf(TEXT("%s '%s': missing required field 'type'"), *Context, *OutNode.Id));
		return false;
	}

	if (Json->HasField(TEXT("properties")))
	{
		OutNode.Properties = Json->GetObjectField(TEXT("properties"));
	}
	if (Json->HasField(TEXT("instance_data")))
	{
		OutNode.InstanceData = Json->GetObjectField(TEXT("instance_data"));
	}
	Json->TryGetStringField(TEXT("operand"), OutNode.Operand);

	return true;
}

bool FArcStateTreeJsonParser::ParseState(const TSharedPtr<FJsonObject>& Json, FArcSTStateDesc& OutState)
{
	if (!Json.IsValid())
	{
		AddError(TEXT("State: invalid JSON object"));
		return false;
	}

	if (!Json->TryGetStringField(TEXT("id"), OutState.Id) || OutState.Id.IsEmpty())
	{
		AddError(TEXT("State: missing required field 'id'"));
		return false;
	}
	if (!Json->TryGetStringField(TEXT("name"), OutState.Name) || OutState.Name.IsEmpty())
	{
		AddError(FString::Printf(TEXT("State '%s': missing required field 'name'"), *OutState.Id));
	}
	if (!Json->TryGetStringField(TEXT("type"), OutState.Type) || OutState.Type.IsEmpty())
	{
		AddError(FString::Printf(TEXT("State '%s': missing required field 'type'"), *OutState.Id));
	}

	Json->TryGetStringField(TEXT("parent"), OutState.Parent);
	Json->TryGetStringField(TEXT("selection"), OutState.Selection);
	Json->TryGetStringField(TEXT("tag"), OutState.Tag);
	Json->TryGetStringField(TEXT("linked_asset"), OutState.LinkedAsset);

	// Children array
	if (Json->HasField(TEXT("children")))
	{
		const TArray<TSharedPtr<FJsonValue>>& ChildArray = Json->GetArrayField(TEXT("children"));
		for (const auto& Val : ChildArray)
		{
			OutState.Children.Add(Val->AsString());
		}
	}

	// Enter conditions
	if (Json->HasField(TEXT("enter_conditions")))
	{
		for (const auto& Val : Json->GetArrayField(TEXT("enter_conditions")))
		{
			FArcSTNodeDesc Node;
			if (ParseNode(Val->AsObject(), Node, FString::Printf(TEXT("State '%s' enter_condition"), *OutState.Id)))
			{
				OutState.EnterConditions.Add(MoveTemp(Node));
			}
		}
	}

	// Tasks
	if (Json->HasField(TEXT("tasks")))
	{
		for (const auto& Val : Json->GetArrayField(TEXT("tasks")))
		{
			FArcSTNodeDesc Node;
			if (ParseNode(Val->AsObject(), Node, FString::Printf(TEXT("State '%s' task"), *OutState.Id)))
			{
				OutState.Tasks.Add(MoveTemp(Node));
			}
		}
	}

	// Transitions
	if (Json->HasField(TEXT("transitions")))
	{
		for (const auto& Val : Json->GetArrayField(TEXT("transitions")))
		{
			FArcSTTransitionDesc Trans;
			if (ParseTransition(Val->AsObject(), Trans, OutState.Id))
			{
				OutState.Transitions.Add(MoveTemp(Trans));
			}
		}
	}

	// Considerations
	if (Json->HasField(TEXT("considerations")))
	{
		for (const auto& Val : Json->GetArrayField(TEXT("considerations")))
		{
			FArcSTNodeDesc Node;
			if (ParseNode(Val->AsObject(), Node, FString::Printf(TEXT("State '%s' consideration"), *OutState.Id)))
			{
				OutState.Considerations.Add(MoveTemp(Node));
			}
		}
	}

	return true;
}

bool FArcStateTreeJsonParser::ParseTransition(const TSharedPtr<FJsonObject>& Json, FArcSTTransitionDesc& OutTrans, const FString& StateId)
{
	if (!Json.IsValid())
	{
		AddError(FString::Printf(TEXT("State '%s' transition: invalid JSON"), *StateId));
		return false;
	}

	if (!Json->TryGetStringField(TEXT("trigger"), OutTrans.Trigger) || OutTrans.Trigger.IsEmpty())
	{
		AddError(FString::Printf(TEXT("State '%s' transition: missing 'trigger'"), *StateId));
		return false;
	}
	if (!Json->TryGetStringField(TEXT("type"), OutTrans.Type) || OutTrans.Type.IsEmpty())
	{
		AddError(FString::Printf(TEXT("State '%s' transition: missing 'type'"), *StateId));
		return false;
	}

	Json->TryGetStringField(TEXT("target"), OutTrans.Target);
	Json->TryGetStringField(TEXT("priority"), OutTrans.Priority);
	Json->TryGetNumberField(TEXT("delay"), OutTrans.Delay);
	Json->TryGetStringField(TEXT("event"), OutTrans.Event);

	// on_event trigger requires an event tag
	if (OutTrans.Trigger == TEXT("on_event") && OutTrans.Event.IsEmpty())
	{
		AddError(FString::Printf(TEXT("State '%s' transition: 'on_event' trigger requires an 'event' tag"), *StateId));
		return false;
	}

	// Transition conditions
	if (Json->HasField(TEXT("conditions")))
	{
		for (const auto& Val : Json->GetArrayField(TEXT("conditions")))
		{
			FArcSTNodeDesc Node;
			if (ParseNode(Val->AsObject(), Node, FString::Printf(TEXT("State '%s' transition condition"), *StateId)))
			{
				OutTrans.Conditions.Add(MoveTemp(Node));
			}
		}
	}

	return true;
}

bool FArcStateTreeJsonParser::ParseBinding(const TSharedPtr<FJsonObject>& Json, FArcSTBindingDesc& OutBinding)
{
	if (!Json.IsValid())
	{
		AddError(TEXT("Binding: invalid JSON object"));
		return false;
	}

	if (!Json->TryGetStringField(TEXT("source"), OutBinding.Source) || OutBinding.Source.IsEmpty())
	{
		AddError(TEXT("Binding: missing 'source'"));
		return false;
	}
	if (!Json->TryGetStringField(TEXT("target"), OutBinding.Target) || OutBinding.Target.IsEmpty())
	{
		AddError(TEXT("Binding: missing 'target'"));
		return false;
	}

	return true;
}

bool FArcStateTreeJsonParser::ValidateReferences(const FArcSTDescription& Desc)
{
	// Build set of all state ids
	TSet<FString> StateIds;
	for (const FArcSTStateDesc& State : Desc.States)
	{
		if (StateIds.Contains(State.Id))
		{
			AddError(FString::Printf(TEXT("Duplicate state id: '%s'"), *State.Id));
		}
		StateIds.Add(State.Id);
	}

	// Build set of all node ids
	TSet<FString> NodeIds;
	auto RegisterNodes = [&](const TArray<FArcSTNodeDesc>& Nodes)
	{
		for (const FArcSTNodeDesc& Node : Nodes)
		{
			if (NodeIds.Contains(Node.Id))
			{
				AddError(FString::Printf(TEXT("Duplicate node id: '%s'"), *Node.Id));
			}
			NodeIds.Add(Node.Id);
		}
	};

	RegisterNodes(Desc.Evaluators);
	RegisterNodes(Desc.GlobalTasks);
	for (const FArcSTStateDesc& State : Desc.States)
	{
		RegisterNodes(State.EnterConditions);
		RegisterNodes(State.Tasks);
		RegisterNodes(State.Considerations);
		for (const FArcSTTransitionDesc& Trans : State.Transitions)
		{
			RegisterNodes(Trans.Conditions);
		}
	}

	// Validate parent references
	for (const FArcSTStateDesc& State : Desc.States)
	{
		if (!State.Parent.IsEmpty() && !StateIds.Contains(State.Parent))
		{
			AddError(FString::Printf(TEXT("State '%s' references unknown parent: '%s'"), *State.Id, *State.Parent));
		}
		for (const FString& ChildId : State.Children)
		{
			if (!StateIds.Contains(ChildId))
			{
				AddError(FString::Printf(TEXT("State '%s' references unknown child: '%s'"), *State.Id, *ChildId));
			}
		}
	}

	// Validate transition targets
	for (const FArcSTStateDesc& State : Desc.States)
	{
		for (const FArcSTTransitionDesc& Trans : State.Transitions)
		{
			if (Trans.Type == TEXT("goto_state"))
			{
				if (Trans.Target.IsEmpty())
				{
					AddError(FString::Printf(TEXT("State '%s' transition: 'goto_state' requires a 'target' state id"), *State.Id));
				}
				else if (!StateIds.Contains(Trans.Target))
				{
					AddError(FString::Printf(TEXT("State '%s' transition targets unknown state: '%s'"), *State.Id, *Trans.Target));
				}
			}
		}
	}

	// Reject unsupported binding prefixes (v1 limitations)
	for (const FArcSTBindingDesc& Binding : Desc.Bindings)
	{
		FString SrcId, SrcPath;
		if (Binding.Source.Split(TEXT("."), &SrcId, &SrcPath))
		{
			if (SrcId == TEXT("parameters"))
			{
				AddError(TEXT("'parameters.*' bindings are not yet supported in v1"));
			}
			if (SrcId == TEXT("context"))
			{
				AddError(TEXT("'context.*' bindings are not yet supported in v1"));
			}
		}
	}

	// Validate binding node references
	TSet<FString> AllIds = NodeIds;

	for (const FArcSTBindingDesc& Binding : Desc.Bindings)
	{
		FString SourceId, SourcePath;
		if (Binding.Source.Split(TEXT("."), &SourceId, &SourcePath))
		{
			if (!AllIds.Contains(SourceId))
			{
				AddError(FString::Printf(TEXT("Binding source references unknown node: '%s'"), *SourceId));
			}
		}
		else
		{
			AddError(FString::Printf(TEXT("Invalid binding source format: '%s' (expected 'node_id.PropertyPath')"), *Binding.Source));
		}

		FString TargetId, TargetPath;
		if (Binding.Target.Split(TEXT("."), &TargetId, &TargetPath))
		{
			if (!AllIds.Contains(TargetId))
			{
				AddError(FString::Printf(TEXT("Binding target references unknown node: '%s'"), *TargetId));
			}
		}
		else
		{
			AddError(FString::Printf(TEXT("Invalid binding target format: '%s' (expected 'node_id.PropertyPath')"), *Binding.Target));
		}
	}

	return Errors.IsEmpty();
}
