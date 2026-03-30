#if 0

// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCP/Commands/ArcMCPCommand_CreateStateTree.h"

#include "ArcMCP/StateTree/ArcStateTreeJsonParser.h"
#include "ArcMCP/StateTree/ArcStateTreeBuilder.h"
#include "ArcMCP/StateTree/ArcStateTreeTypes.h"

REGISTER_ECA_COMMAND(FArcMCPCommand_CreateStateTree)

FECACommandResult FArcMCPCommand_CreateStateTree::Execute(const TSharedPtr<FJsonObject>& Params)
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
		TArray<FString> AllErrors = Parser.GetErrors();
		return FECACommandResult::Error(FString::Join(AllErrors, TEXT("; ")));
	}

	// Build and save
	FArcStateTreeBuilder Builder;
	FArcStateTreeBuildResult BuildResult = Builder.Build(Desc, /*bSave=*/ true);

	if (!BuildResult.bSuccess)
	{
		TSharedPtr<FJsonObject> Result = MakeResult();
		Result->SetBoolField(TEXT("success"), false);

		TArray<TSharedPtr<FJsonValue>> ErrorsArray;
		for (const FString& Err : BuildResult.Errors)
		{
			ErrorsArray.Add(MakeShared<FJsonValueString>(Err));
		}
		Result->SetArrayField(TEXT("errors"), ErrorsArray);

		return FECACommandResult::Success(Result);
	}

	TSharedPtr<FJsonObject> Result = MakeResult();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("asset_path"), BuildResult.AssetPath);

	TArray<TSharedPtr<FJsonValue>> WarningsArray;
	for (const FString& Warn : BuildResult.Warnings)
	{
		WarningsArray.Add(MakeShared<FJsonValueString>(Warn));
	}
	Result->SetArrayField(TEXT("warnings"), WarningsArray);

	return FECACommandResult::Success(Result);
}

#endif
