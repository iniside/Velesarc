// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCP/StateTree/ArcStateTreeBuilder.h"
#include "ArcMCP/StateTree/ArcStateTreeNodeRegistry.h"
#include "ArcMCP/StateTree/ArcStateTreeTypes.h"

#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "StateTreeCompiler.h"
#include "StateTreeSchema.h"
#include "StateTreeNodeBase.h"
#include "StateTreeTaskBase.h"
#include "StateTreeConditionBase.h"
#include "StateTreeTypes.h"
#include "PropertyBindingPath.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectIterator.h"
#include "StructUtils/InstancedStruct.h"
#include "JsonObjectConverter.h"
#include "StateTreeCompilerLog.h"
#include "Misc/PackageName.h"

FArcStateTreeBuildResult FArcStateTreeBuilder::Build(const FArcSTDescription& Desc, bool bSave)
{
	Result = FArcStateTreeBuildResult();
	NodeIdToGuid.Empty();
	StateMap.Empty();

	if (!ResolveSchemaClass(Desc.Schema)) { Result.bSuccess = false; return Result; }
	if (!CreateAssetAndEditorData(Desc, !bSave)) { Result.bSuccess = false; return Result; }

	// Warn about unsupported parameters field (v1 limitation)
	if (Desc.Parameters.IsValid() && Desc.Parameters->Values.Num() > 0)
	{
		AddWarning(TEXT("'parameters' field is not yet supported in v1 and will be ignored"));
	}

	if (!AddEvaluatorsAndGlobalTasks(Desc)) { Result.bSuccess = false; return Result; }
	if (!BuildStateHierarchy(Desc)) { Result.bSuccess = false; return Result; }
	if (!AddStateNodes(Desc)) { Result.bSuccess = false; return Result; }
	if (!AddTransitions(Desc)) { Result.bSuccess = false; return Result; }
	if (!AddBindings(Desc)) { /* bindings are best-effort -- warnings, not errors */ }
	if (!CompileTree()) { Result.bSuccess = false; return Result; }

	if (bSave)
	{
		if (!SaveAsset(Desc)) { Result.bSuccess = false; return Result; }
		Result.AssetPath = StateTree->GetPathName();
	}
	else
	{
		// Validate mode: clean up transient objects per spec requirements
		if (EditorData)
		{
			EditorData->MarkAsGarbage();
		}
		StateTree->MarkAsGarbage();
		StateTree = nullptr;
		EditorData = nullptr;
		Package = nullptr;
	}

	Result.bSuccess = Result.Errors.IsEmpty();
	return Result;
}

bool FArcStateTreeBuilder::ResolveSchemaClass(const FString& SchemaName)
{
	SchemaClass = FArcStateTreeNodeRegistry::FindSchemaClass(SchemaName);
	if (!SchemaClass)
	{
		AddError(FString::Printf(TEXT("Schema class not found: '%s'"), *SchemaName));
		return false;
	}
	return true;
}

bool FArcStateTreeBuilder::CreateAssetAndEditorData(const FArcSTDescription& Desc, bool bTransient)
{
	if (bTransient)
	{
		Package = GetTransientPackage();
		StateTree = NewObject<UStateTree>(Package, *Desc.Name, RF_Transient);
	}
	else
	{
		const FString FullPath = Desc.PackagePath / Desc.Name;
		Package = CreatePackage(*FullPath);
		StateTree = NewObject<UStateTree>(Package, *Desc.Name, RF_Public | RF_Standalone);
	}

	if (!StateTree)
	{
		AddError(TEXT("Failed to create UStateTree object"));
		return false;
	}

	EditorData = NewObject<UStateTreeEditorData>(StateTree);
	StateTree->EditorData = EditorData;

	UStateTreeSchema* Schema = Cast<UStateTreeSchema>(NewObject<UObject>(EditorData, SchemaClass));
	EditorData->Schema = Schema;

	return true;
}

bool FArcStateTreeBuilder::AddEvaluatorsAndGlobalTasks(const FArcSTDescription& Desc)
{
	const FArcStateTreeNodeRegistry& Registry = FArcStateTreeNodeRegistry::Get();

	// Evaluators
	for (const FArcSTNodeDesc& NodeDesc : Desc.Evaluators)
	{
		const UScriptStruct* NodeStruct = Registry.ResolveNodeType(NodeDesc.Type);
		if (!NodeStruct)
		{
			AddError(FString::Printf(TEXT("Unknown evaluator type: '%s'"), *NodeDesc.Type));
			continue;
		}

		FStateTreeEditorNode& EdNode = EditorData->Evaluators.AddDefaulted_GetRef();
		if (!InitializeEditorNode(EdNode, NodeStruct, NodeDesc))
		{
			continue;
		}
		NodeIdToGuid.Add(NodeDesc.Id, EdNode.ID);
	}

	// Global tasks
	for (const FArcSTNodeDesc& NodeDesc : Desc.GlobalTasks)
	{
		const UScriptStruct* NodeStruct = Registry.ResolveNodeType(NodeDesc.Type);
		if (!NodeStruct)
		{
			AddError(FString::Printf(TEXT("Unknown global task type: '%s'"), *NodeDesc.Type));
			continue;
		}

		FStateTreeEditorNode& EdNode = EditorData->GlobalTasks.AddDefaulted_GetRef();
		if (!InitializeEditorNode(EdNode, NodeStruct, NodeDesc))
		{
			continue;
		}
		NodeIdToGuid.Add(NodeDesc.Id, EdNode.ID);
	}

	return Result.Errors.IsEmpty();
}

bool FArcStateTreeBuilder::BuildStateHierarchy(const FArcSTDescription& Desc)
{
	using namespace UE::ArcMCP::StateTree;

	// First pass: identify root states (no parent)
	TArray<const FArcSTStateDesc*> Roots;
	TMap<FString, const FArcSTStateDesc*> DescMap;

	for (const FArcSTStateDesc& State : Desc.States)
	{
		DescMap.Add(State.Id, &State);
		if (State.Parent.IsEmpty())
		{
			Roots.Add(&State);
		}
	}

	if (Roots.IsEmpty())
	{
		AddError(TEXT("No root states found (states with no 'parent')"));
		return false;
	}

	// Build hierarchy recursively
	TFunction<bool(const FArcSTStateDesc*, UStateTreeState*)> BuildChildren;
	BuildChildren = [&](const FArcSTStateDesc* StateDesc, UStateTreeState* ParentUState) -> bool
	{
		for (const FString& ChildId : StateDesc->Children)
		{
			const FArcSTStateDesc** ChildDescPtr = DescMap.Find(ChildId);
			if (!ChildDescPtr)
			{
				AddError(FString::Printf(TEXT("State '%s' child '%s' not found"), *StateDesc->Id, *ChildId));
				continue;
			}

			const EStateTreeStateType ChildType = ParseStateType((*ChildDescPtr)->Type);
			UStateTreeState& ChildState = ParentUState->AddChildState(FName(*(*ChildDescPtr)->Name), ChildType);

			if (!(*ChildDescPtr)->Selection.IsEmpty())
			{
				ChildState.SelectionBehavior = ParseSelectionBehavior((*ChildDescPtr)->Selection);
			}
			if (!(*ChildDescPtr)->Tag.IsEmpty())
			{
				ChildState.Tag = FGameplayTag::RequestGameplayTag(FName(*(*ChildDescPtr)->Tag));
			}

			StateMap.Add((*ChildDescPtr)->Id, &ChildState);
			BuildChildren(*ChildDescPtr, &ChildState);
		}
		return true;
	};

	// Create root states
	for (const FArcSTStateDesc* RootDesc : Roots)
	{
		const EStateTreeStateType RootType = ParseStateType(RootDesc->Type);
		UStateTreeState& RootState = EditorData->AddSubTree(FName(*RootDesc->Name), RootType);

		if (!RootDesc->Selection.IsEmpty())
		{
			RootState.SelectionBehavior = ParseSelectionBehavior(RootDesc->Selection);
		}
		if (!RootDesc->Tag.IsEmpty())
		{
			RootState.Tag = FGameplayTag::RequestGameplayTag(FName(*RootDesc->Tag));
		}

		StateMap.Add(RootDesc->Id, &RootState);
		BuildChildren(RootDesc, &RootState);
	}

	return Result.Errors.IsEmpty();
}

bool FArcStateTreeBuilder::AddStateNodes(const FArcSTDescription& Desc)
{
	const FArcStateTreeNodeRegistry& Registry = FArcStateTreeNodeRegistry::Get();
	using namespace UE::ArcMCP::StateTree;

	for (const FArcSTStateDesc& StateDesc : Desc.States)
	{
		UStateTreeState** StatePtr = StateMap.Find(StateDesc.Id);
		if (!StatePtr) continue;
		UStateTreeState* State = *StatePtr;

		// Enter conditions
		for (const FArcSTNodeDesc& NodeDesc : StateDesc.EnterConditions)
		{
			const UScriptStruct* NodeStruct = Registry.ResolveNodeType(NodeDesc.Type);
			if (!NodeStruct) { AddError(FString::Printf(TEXT("Unknown condition type: '%s'"), *NodeDesc.Type)); continue; }

			FStateTreeEditorNode& EdNode = State->EnterConditions.AddDefaulted_GetRef();
			if (!InitializeEditorNode(EdNode, NodeStruct, NodeDesc)) continue;
			EdNode.ExpressionOperand = ParseOperand(NodeDesc.Operand);
			NodeIdToGuid.Add(NodeDesc.Id, EdNode.ID);
		}

		// Tasks
		for (const FArcSTNodeDesc& NodeDesc : StateDesc.Tasks)
		{
			const UScriptStruct* NodeStruct = Registry.ResolveNodeType(NodeDesc.Type);
			if (!NodeStruct) { AddError(FString::Printf(TEXT("Unknown task type: '%s'"), *NodeDesc.Type)); continue; }

			FStateTreeEditorNode& EdNode = State->Tasks.AddDefaulted_GetRef();
			if (!InitializeEditorNode(EdNode, NodeStruct, NodeDesc)) continue;
			NodeIdToGuid.Add(NodeDesc.Id, EdNode.ID);
		}

		// Considerations
		for (const FArcSTNodeDesc& NodeDesc : StateDesc.Considerations)
		{
			const UScriptStruct* NodeStruct = Registry.ResolveNodeType(NodeDesc.Type);
			if (!NodeStruct) { AddError(FString::Printf(TEXT("Unknown consideration type: '%s'"), *NodeDesc.Type)); continue; }

			FStateTreeEditorNode& EdNode = State->Considerations.AddDefaulted_GetRef();
			if (!InitializeEditorNode(EdNode, NodeStruct, NodeDesc)) continue;
			EdNode.ExpressionOperand = ParseOperand(NodeDesc.Operand);
			NodeIdToGuid.Add(NodeDesc.Id, EdNode.ID);
		}
	}

	return Result.Errors.IsEmpty();
}

bool FArcStateTreeBuilder::AddTransitions(const FArcSTDescription& Desc)
{
	const FArcStateTreeNodeRegistry& Registry = FArcStateTreeNodeRegistry::Get();
	using namespace UE::ArcMCP::StateTree;

	for (const FArcSTStateDesc& StateDesc : Desc.States)
	{
		UStateTreeState** StatePtr = StateMap.Find(StateDesc.Id);
		if (!StatePtr) continue;
		UStateTreeState* State = *StatePtr;

		for (const FArcSTTransitionDesc& TransDesc : StateDesc.Transitions)
		{
			const EStateTreeTransitionTrigger Trigger = ParseTransitionTrigger(TransDesc.Trigger);
			const EStateTreeTransitionType Type = ParseTransitionType(TransDesc.Type);

			// Resolve target state for goto_state
			const UStateTreeState* TargetState = nullptr;
			if (Type == EStateTreeTransitionType::GotoState && !TransDesc.Target.IsEmpty())
			{
				UStateTreeState** TargetPtr = StateMap.Find(TransDesc.Target);
				if (TargetPtr)
				{
					TargetState = *TargetPtr;
				}
				else
				{
					AddError(FString::Printf(TEXT("State '%s' transition target not found: '%s'"), *StateDesc.Id, *TransDesc.Target));
				}
			}

			// Use the event-tag overload for on_event transitions
			FStateTreeTransition* TransPtr = nullptr;
			if (Trigger == EStateTreeTransitionTrigger::OnEvent && !TransDesc.Event.IsEmpty())
			{
				const FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(FName(*TransDesc.Event));
				TransPtr = &State->AddTransition(Trigger, EventTag, Type, TargetState);
			}
			else
			{
				TransPtr = &State->AddTransition(Trigger, Type, TargetState);
			}
			FStateTreeTransition& Trans = *TransPtr;

			// Ensure transition has a valid ID (AddTransition may or may not assign one)
			if (!Trans.ID.IsValid())
			{
				Trans.ID = FGuid::NewGuid();
			}

			if (!TransDesc.Priority.IsEmpty())
			{
				Trans.Priority = ParseTransitionPriority(TransDesc.Priority);
			}

			if (TransDesc.Delay > 0.f)
			{
				Trans.bDelayTransition = true;
				Trans.DelayDuration = TransDesc.Delay;
			}

			// Transition conditions
			for (const FArcSTNodeDesc& CondDesc : TransDesc.Conditions)
			{
				const UScriptStruct* CondStruct = Registry.ResolveNodeType(CondDesc.Type);
				if (!CondStruct) { AddError(FString::Printf(TEXT("Unknown condition type: '%s'"), *CondDesc.Type)); continue; }

				FStateTreeEditorNode& EdNode = Trans.Conditions.AddDefaulted_GetRef();
				if (!InitializeEditorNode(EdNode, CondStruct, CondDesc)) continue;
				EdNode.ExpressionOperand = ParseOperand(CondDesc.Operand);
				NodeIdToGuid.Add(CondDesc.Id, EdNode.ID);
			}
		}
	}

	return Result.Errors.IsEmpty();
}

bool FArcStateTreeBuilder::AddBindings(const FArcSTDescription& Desc)
{
	for (const FArcSTBindingDesc& BindingDesc : Desc.Bindings)
	{
		FString SourceId, SourcePath;
		if (!BindingDesc.Source.Split(TEXT("."), &SourceId, &SourcePath))
		{
			AddWarning(FString::Printf(TEXT("Invalid binding source: '%s'"), *BindingDesc.Source));
			continue;
		}

		FString TargetId, TargetPath;
		if (!BindingDesc.Target.Split(TEXT("."), &TargetId, &TargetPath))
		{
			AddWarning(FString::Printf(TEXT("Invalid binding target: '%s'"), *BindingDesc.Target));
			continue;
		}

		FPropertyBindingPath Source;
		if (!Source.FromString(*SourcePath))
		{
			AddWarning(FString::Printf(TEXT("Invalid binding source path: '%s'"), *SourcePath));
			continue;
		}
		const FGuid SourceGuid = ResolveBindingSourceGuid(SourceId);
		if (!SourceGuid.IsValid())
		{
			AddWarning(FString::Printf(TEXT("Cannot resolve binding source node: '%s'"), *SourceId));
			continue;
		}
		Source.SetStructID(SourceGuid);

		FPropertyBindingPath Target;
		if (!Target.FromString(*TargetPath))
		{
			AddWarning(FString::Printf(TEXT("Invalid binding target path: '%s'"), *TargetPath));
			continue;
		}
		const FGuid TargetGuid = ResolveBindingSourceGuid(TargetId);
		if (!TargetGuid.IsValid())
		{
			AddWarning(FString::Printf(TEXT("Cannot resolve binding target node: '%s'"), *TargetId));
			continue;
		}
		Target.SetStructID(TargetGuid);

		EditorData->AddPropertyBinding(Source, Target);
	}

	return true; // bindings are best-effort
}

bool FArcStateTreeBuilder::CompileTree()
{
	FStateTreeCompilerLog Log;
	FStateTreeCompiler Compiler(Log);
	const bool bSuccess = Compiler.Compile(*StateTree);

	// Extract messages from compiler log
	TArray<TSharedRef<FTokenizedMessage>> Messages = Log.ToTokenizedMessages();
	for (const TSharedRef<FTokenizedMessage>& Msg : Messages)
	{
		const FString MsgText = Msg->ToText().ToString();
		if (Msg->GetSeverity() == EMessageSeverity::Error)
		{
			AddError(FString::Printf(TEXT("Compile: %s"), *MsgText));
		}
		else
		{
			AddWarning(FString::Printf(TEXT("Compile: %s"), *MsgText));
		}
	}

	return bSuccess;
}

bool FArcStateTreeBuilder::SaveAsset(const FArcSTDescription& Desc)
{
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(
		Package->GetName(), FPackageName::GetAssetPackageExtension());

	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(StateTree);

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	const FSavePackageResultStruct SaveResult = UPackage::Save(Package, StateTree, *PackageFilename, SaveArgs);

	if (SaveResult.Result != ESavePackageResult::Success)
	{
		AddError(FString::Printf(TEXT("Failed to save package: %s"), *PackageFilename));
		return false;
	}

	return true;
}

bool FArcStateTreeBuilder::InitializeEditorNode(FStateTreeEditorNode& OutNode, const UScriptStruct* NodeStruct, const FArcSTNodeDesc& NodeDesc)
{
	OutNode.ID = FGuid::NewGuid();
	OutNode.Node.InitializeAs(NodeStruct);

	// Initialize instance data
	const FStateTreeNodeBase& NodeRef = OutNode.Node.Get<const FStateTreeNodeBase>();
	if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(NodeRef.GetInstanceDataType()))
	{
		OutNode.Instance.InitializeAs(InstanceType);
	}
	if (const UScriptStruct* RuntimeType = Cast<const UScriptStruct>(NodeRef.GetExecutionRuntimeDataType()))
	{
		OutNode.ExecutionRuntimeData.InitializeAs(RuntimeType);
	}

	// Set node properties
	if (NodeDesc.Properties.IsValid() && NodeDesc.Properties->Values.Num() > 0)
	{
		if (!SetPropertiesFromJson(
				OutNode.Node.GetMutableMemory(), NodeStruct,
				NodeDesc.Properties, FString::Printf(TEXT("node '%s' properties"), *NodeDesc.Id)))
		{
			return false;
		}
	}

	// Set instance data properties
	if (NodeDesc.InstanceData.IsValid() && NodeDesc.InstanceData->Values.Num() > 0)
	{
		if (OutNode.Instance.IsValid())
		{
			const UScriptStruct* InstanceStruct = OutNode.Instance.GetScriptStruct();
			if (!SetPropertiesFromJson(
					OutNode.Instance.GetMutableMemory(), InstanceStruct,
					NodeDesc.InstanceData, FString::Printf(TEXT("node '%s' instance_data"), *NodeDesc.Id)))
			{
				return false;
			}
		}
		else
		{
			AddWarning(FString::Printf(TEXT("Node '%s': instance_data specified but node has no instance data type"), *NodeDesc.Id));
		}
	}

	return true;
}

bool FArcStateTreeBuilder::SetPropertiesFromJson(void* StructMemory, const UScriptStruct* Struct, const TSharedPtr<FJsonObject>& Props, const FString& Context)
{
	if (!StructMemory || !Struct || !Props.IsValid()) return true;

	for (const TTuple<UE::TSharedString<wchar_t>, TSharedPtr<FJsonValue>>& Pair : Props->Values)
	{
		const FString& PropName = *Pair.Key;
		const TSharedPtr<FJsonValue>& Value = Pair.Value;

		FProperty* Property = Struct->FindPropertyByName(FName(*PropName));
		if (!Property)
		{
			AddWarning(FString::Printf(TEXT("%s: unknown property '%s' on %s"), *Context, *PropName, *Struct->GetName()));
			continue;
		}

		void* PropertyValuePtr = Property->ContainerPtrToValuePtr<void>(StructMemory);

		// Try JSON->UProperty import
		FString ValueString;
		if (Value->TryGetString(ValueString))
		{
			Property->ImportText_Direct(*ValueString, PropertyValuePtr, nullptr, PPF_None);
		}
		else if (Value->Type == EJson::Number)
		{
			// Handle numeric types
			const double NumValue = Value->AsNumber();
			if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
			{
				FloatProp->SetPropertyValue(PropertyValuePtr, static_cast<float>(NumValue));
			}
			else if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
			{
				DoubleProp->SetPropertyValue(PropertyValuePtr, NumValue);
			}
			else if (const FIntProperty* IntProp = CastField<FIntProperty>(Property))
			{
				IntProp->SetPropertyValue(PropertyValuePtr, static_cast<int32>(NumValue));
			}
			else
			{
				// Fall back to text import
				const FString NumStr = FString::SanitizeFloat(NumValue);
				Property->ImportText_Direct(*NumStr, PropertyValuePtr, nullptr, PPF_None);
			}
		}
		else if (Value->Type == EJson::Boolean)
		{
			if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
			{
				BoolProp->SetPropertyValue(PropertyValuePtr, Value->AsBool());
			}
		}
		else if (Value->Type == EJson::Object)
		{
			// For struct properties, try FJsonObjectConverter
			if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
			{
				FJsonObjectConverter::JsonObjectToUStruct(Value->AsObject().ToSharedRef(), StructProp->Struct, PropertyValuePtr);
			}
		}
		else
		{
			AddWarning(FString::Printf(TEXT("%s: unsupported JSON type for property '%s'"), *Context, *PropName));
		}
	}

	return true;
}

FGuid FArcStateTreeBuilder::ResolveBindingSourceGuid(const FString& NodeId) const
{
	if (NodeId == TEXT("parameters"))
	{
		return EditorData->GetRootParametersGuid();
	}
	// TODO: handle "context" prefix by resolving context data descriptors

	const FGuid* Found = NodeIdToGuid.Find(NodeId);
	return Found ? *Found : FGuid();
}
