// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCP/Commands/ArcMCPCommand_ListStateTreeSchemas.h"

#include "StateTreeExecutionTypes.h"
#include "StateTreeSchema.h"
#include "UObject/UObjectIterator.h"

REGISTER_ECA_COMMAND(FArcMCPCommand_ListStateTreeSchemas)

FECACommandResult FArcMCPCommand_ListStateTreeSchemas::Execute(const TSharedPtr<FJsonObject>& Params)
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

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetArrayField(TEXT("schemas"), SchemasArray);
	Result->SetNumberField(TEXT("count"), SchemasArray.Num());

	return FECACommandResult::Success(Result);
}
