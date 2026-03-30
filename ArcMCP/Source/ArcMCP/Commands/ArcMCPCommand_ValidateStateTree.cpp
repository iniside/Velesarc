#if 0

// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCP/Commands/ArcMCPCommand_ValidateStateTree.h"

#include "ArcMCP/StateTree/ArcStateTreeJsonParser.h"
#include "ArcMCP/StateTree/ArcStateTreeBuilder.h"
#include "ArcMCP/StateTree/ArcStateTreeTypes.h"

REGISTER_ECA_COMMAND(FArcMCPCommand_ValidateStateTree)

FECACommandResult FArcMCPCommand_ValidateStateTree::Execute(const TSharedPtr<FJsonObject>& Params)
{
	const TSharedPtr<FJsonObject>* DescJson = nullptr;
	if (!GetObjectParam(Params, TEXT("description"), DescJson))
	{
		return FECACommandResult::Error(TEXT("Missing required parameter: description"));
	}

	// Parse JSON
	FArcStateTreeJsonParser Parser;
	FArcSTDescription Desc;

	if (!Parser.Parse(*DescJson, Desc))
	{
		TSharedPtr<FJsonObject> Result = MakeResult();
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

		return FECACommandResult::Success(Result);
	}

	// Build in validate mode (no save)
	FArcStateTreeBuilder Builder;
	FArcStateTreeBuildResult BuildResult = Builder.Build(Desc, /*bSave=*/ false);

	TSharedPtr<FJsonObject> Result = MakeResult();
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

	return FECACommandResult::Success(Result);
}

#endif
