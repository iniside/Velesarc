// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCPToolset.h"
#include "ArcMCPToolsetUtils.h"

#include "ArcMCP/StateTree/ArcStateTreeNodeRegistry.h"
#include "ArcMCP/StateTree/ArcStateTreeJsonParser.h"
#include "ArcMCP/StateTree/ArcStateTreeBuilder.h"
#include "ArcMCP/StateTree/ArcStateTreeTypes.h"

#include "StateTreeExecutionTypes.h"
#include "StateTreeSchema.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Class.h"

// ---------------------------------------------------------------------------
// ListStateTreeSchemas
// ---------------------------------------------------------------------------

FString UArcMCPToolset::ListStateTreeSchemas()
{
	TArray<TSharedPtr<FJsonValue>> SchemasArray;

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class->IsChildOf(UStateTreeSchema::StaticClass()) || Class == UStateTreeSchema::StaticClass())
		{
			continue;
		}
		if (Class->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}

		const UStateTreeSchema* CDO = Class->GetDefaultObject<UStateTreeSchema>();

		TSharedPtr<FJsonObject> SchemaObj = MakeShared<FJsonObject>();
		SchemaObj->SetStringField(TEXT("class_name"), Class->GetName());
		SchemaObj->SetStringField(TEXT("display_name"), Class->GetDisplayNameText().ToString());
		SchemaObj->SetStringField(TEXT("class_path"), Class->GetPathName());

		// Context data
		TArray<TSharedPtr<FJsonValue>> ContextArray;
		for (const FStateTreeExternalDataDesc& DataDesc : CDO->GetContextDataDescs())
		{
			TSharedPtr<FJsonObject> CtxObj = MakeShared<FJsonObject>();
			CtxObj->SetStringField(TEXT("name"), DataDesc.Name.ToString());
			if (DataDesc.Struct)
			{
				CtxObj->SetStringField(TEXT("type"), DataDesc.Struct->GetName());
			}
			ContextArray.Add(MakeShared<FJsonValueObject>(CtxObj));
		}
		SchemaObj->SetArrayField(TEXT("context_data"), ContextArray);

		// Features
		TSharedPtr<FJsonObject> Features = MakeShared<FJsonObject>();
#if WITH_EDITOR
		Features->SetBoolField(TEXT("allow_evaluators"), CDO->AllowEvaluators());
		Features->SetBoolField(TEXT("allow_utility_considerations"), CDO->AllowUtilityConsiderations());
		Features->SetBoolField(TEXT("allow_multiple_tasks"), CDO->AllowMultipleTasks());
		Features->SetBoolField(TEXT("allow_enter_conditions"), CDO->AllowEnterConditions());
		Features->SetBoolField(TEXT("allow_global_parameters"), CDO->AllowGlobalParameters());
#endif
		SchemaObj->SetObjectField(TEXT("features"), Features);

		SchemasArray.Add(MakeShared<FJsonValueObject>(SchemaObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("schemas"), SchemasArray);
	Result->SetNumberField(TEXT("count"), SchemasArray.Num());

	return ArcMCPToolsetPrivate::JsonObjToString(Result);
}

// ---------------------------------------------------------------------------
// ListStateTreeNodes
// ---------------------------------------------------------------------------

FString UArcMCPToolset::ListStateTreeNodes(const FString& Schema, const FString& Category, const FString& Filter)
{
	FArcStateTreeNodeRegistry& Registry = FArcStateTreeNodeRegistry::Get();
	if (!Registry.IsInitialized())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Node registry not yet initialized"));
	}

	if (Schema.IsEmpty())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Missing required parameter: Schema"));
	}

	UClass* SchemaClass = FArcStateTreeNodeRegistry::FindSchemaClass(Schema);
	if (!SchemaClass)
	{
		return ArcMCPToolsetPrivate::MakeError(FString::Printf(TEXT("Schema class not found: %s"), *Schema));
	}

	const UStateTreeSchema* SchemaCDO = SchemaClass->GetDefaultObject<UStateTreeSchema>();

	TArray<const FArcSTNodeTypeDesc*> Nodes = Registry.GetNodes(SchemaCDO, Category, Filter);

	TArray<TSharedPtr<FJsonValue>> NodesArray;
	for (const FArcSTNodeTypeDesc* Desc : Nodes)
	{
		TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
		NodeObj->SetStringField(TEXT("alias"), Desc->Alias);
		NodeObj->SetStringField(TEXT("struct_name"), Desc->StructName);
		NodeObj->SetStringField(TEXT("category"), Desc->Category);

		// Metadata: prefer cached descriptor fields, fall back to live reflection
		if (!Desc->DisplayName.IsEmpty())
		{
			NodeObj->SetStringField(TEXT("display_name"), Desc->DisplayName);
		}
		if (!Desc->Description.IsEmpty())
		{
			NodeObj->SetStringField(TEXT("description"), Desc->Description);
		}
		else if (Desc->Struct)
		{
			const FString Tooltip = Desc->Struct->GetToolTipText().ToString();
			if (!Tooltip.IsEmpty())
			{
				NodeObj->SetStringField(TEXT("description"), Tooltip);
			}
		}

		// Node properties
		TArray<TSharedPtr<FJsonValue>> PropsArray;
		for (const FArcSTPropertyDesc& Prop : Desc->NodeProperties)
		{
			TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
			PropObj->SetStringField(TEXT("name"), Prop.Name);
			PropObj->SetStringField(TEXT("type"), Prop.TypeName);
			PropObj->SetStringField(TEXT("default"), Prop.DefaultValue);
			if (!Prop.DisplayName.IsEmpty())
			{
				PropObj->SetStringField(TEXT("display_name"), Prop.DisplayName);
			}
			if (!Prop.Description.IsEmpty())
			{
				PropObj->SetStringField(TEXT("description"), Prop.Description);
			}
			PropsArray.Add(MakeShared<FJsonValueObject>(PropObj));
		}
		NodeObj->SetArrayField(TEXT("node_properties"), PropsArray);

		// Instance data properties
		TArray<TSharedPtr<FJsonValue>> InstancePropsArray;
		for (const FArcSTPropertyDesc& Prop : Desc->InstanceDataProperties)
		{
			TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
			PropObj->SetStringField(TEXT("name"), Prop.Name);
			PropObj->SetStringField(TEXT("type"), Prop.TypeName);
			PropObj->SetStringField(TEXT("default"), Prop.DefaultValue);
			if (!Prop.DisplayName.IsEmpty())
			{
				PropObj->SetStringField(TEXT("display_name"), Prop.DisplayName);
			}
			if (!Prop.Description.IsEmpty())
			{
				PropObj->SetStringField(TEXT("description"), Prop.Description);
			}
			if (!Prop.Role.IsEmpty())
			{
				PropObj->SetStringField(TEXT("role"), Prop.Role);
			}
			InstancePropsArray.Add(MakeShared<FJsonValueObject>(PropObj));
		}
		NodeObj->SetArrayField(TEXT("instance_data_properties"), InstancePropsArray);

		NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("nodes"), NodesArray);
	Result->SetNumberField(TEXT("count"), NodesArray.Num());

	return ArcMCPToolsetPrivate::JsonObjToString(Result);
}

// ---------------------------------------------------------------------------
// CreateStateTree
// ---------------------------------------------------------------------------

FString UArcMCPToolset::CreateStateTree(const FString& Description)
{
	TSharedPtr<FJsonObject> DescJson = ArcMCPToolsetPrivate::StringToJsonObj(Description);
	if (!DescJson.IsValid())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Failed to parse Description as JSON"));
	}

	// Parse JSON into description struct
	FArcStateTreeJsonParser Parser;
	FArcSTDescription Desc;

	if (!Parser.Parse(DescJson, Desc))
	{
		TArray<FString> AllErrors = Parser.GetErrors();
		return ArcMCPToolsetPrivate::MakeError(FString::Join(AllErrors, TEXT("; ")));
	}

	// Build and save
	FArcStateTreeBuilder Builder;
	FArcStateTreeBuildResult BuildResult = Builder.Build(Desc, /*bSave=*/ true);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	if (!BuildResult.bSuccess)
	{
		Result->SetBoolField(TEXT("success"), false);

		TArray<TSharedPtr<FJsonValue>> ErrorsArray;
		for (const FString& Err : BuildResult.Errors)
		{
			ErrorsArray.Add(MakeShared<FJsonValueString>(Err));
		}
		Result->SetArrayField(TEXT("errors"), ErrorsArray);

		return ArcMCPToolsetPrivate::JsonObjToString(Result);
	}

	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), BuildResult.AssetPath);

	TArray<TSharedPtr<FJsonValue>> WarningsArray;
	for (const FString& Warn : BuildResult.Warnings)
	{
		WarningsArray.Add(MakeShared<FJsonValueString>(Warn));
	}
	Result->SetArrayField(TEXT("warnings"), WarningsArray);

	return ArcMCPToolsetPrivate::JsonObjToString(Result);
}

// ---------------------------------------------------------------------------
// ValidateStateTree
// ---------------------------------------------------------------------------

FString UArcMCPToolset::ValidateStateTree(const FString& Description)
{
	TSharedPtr<FJsonObject> DescJson = ArcMCPToolsetPrivate::StringToJsonObj(Description);
	if (!DescJson.IsValid())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("Failed to parse Description as JSON"));
	}

	// Parse JSON into description struct
	FArcStateTreeJsonParser Parser;
	FArcSTDescription Desc;

	if (!Parser.Parse(DescJson, Desc))
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("valid"), false);

		TArray<TSharedPtr<FJsonValue>> ErrorsArray;
		for (const FString& Err : Parser.GetErrors())
		{
			ErrorsArray.Add(MakeShared<FJsonValueString>(Err));
		}
		Result->SetArrayField(TEXT("errors"), ErrorsArray);

		TArray<TSharedPtr<FJsonValue>> WarningsArray;
		for (const FString& Warn : Parser.GetWarnings())
		{
			WarningsArray.Add(MakeShared<FJsonValueString>(Warn));
		}
		Result->SetArrayField(TEXT("warnings"), WarningsArray);

		return ArcMCPToolsetPrivate::JsonObjToString(Result);
	}

	// Build in validate mode (no save)
	FArcStateTreeBuilder Builder;
	FArcStateTreeBuildResult BuildResult = Builder.Build(Desc, /*bSave=*/ false);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("valid"), BuildResult.bSuccess);

	TArray<TSharedPtr<FJsonValue>> ErrorsArray;
	for (const FString& Err : BuildResult.Errors)
	{
		ErrorsArray.Add(MakeShared<FJsonValueString>(Err));
	}
	Result->SetArrayField(TEXT("errors"), ErrorsArray);

	TArray<TSharedPtr<FJsonValue>> WarningsArray;
	for (const FString& Warn : BuildResult.Warnings)
	{
		WarningsArray.Add(MakeShared<FJsonValueString>(Warn));
	}
	Result->SetArrayField(TEXT("warnings"), WarningsArray);

	return ArcMCPToolsetPrivate::JsonObjToString(Result);
}
