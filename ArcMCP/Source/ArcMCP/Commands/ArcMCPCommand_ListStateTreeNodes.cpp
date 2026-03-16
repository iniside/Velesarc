// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCP/Commands/ArcMCPCommand_ListStateTreeNodes.h"

#include "ArcMCP/StateTree/ArcStateTreeNodeRegistry.h"
#include "StateTreeSchema.h"
#include "UObject/Class.h"

REGISTER_ECA_COMMAND(FArcMCPCommand_ListStateTreeNodes)

FECACommandResult FArcMCPCommand_ListStateTreeNodes::Execute(const TSharedPtr<FJsonObject>& Params)
{
	FArcStateTreeNodeRegistry& Registry = FArcStateTreeNodeRegistry::Get();
	if (!Registry.IsInitialized())
	{
		return FECACommandResult::Error(TEXT("Node registry not yet initialized"));
	}

	FString SchemaName;
	if (!GetStringParam(Params, TEXT("schema"), SchemaName))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: schema"));
	}

	UClass* SchemaClass = FArcStateTreeNodeRegistry::FindSchemaClass(SchemaName);
	if (!SchemaClass)
	{
		return FECACommandResult::Error(FString::Printf(TEXT("Schema class not found: %s"), *SchemaName));
	}

	const UStateTreeSchema* Schema = SchemaClass->GetDefaultObject<UStateTreeSchema>();

	FString CategoryFilter, TextFilter;
	GetStringParam(Params, TEXT("category"), CategoryFilter, false);
	GetStringParam(Params, TEXT("filter"), TextFilter, false);

	TArray<const FArcSTNodeTypeDesc*> Nodes = Registry.GetNodes(Schema, CategoryFilter, TextFilter);

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

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetArrayField(TEXT("nodes"), NodesArray);
	Result->SetNumberField(TEXT("count"), NodesArray.Num());

	return FECACommandResult::Success(Result);
}
