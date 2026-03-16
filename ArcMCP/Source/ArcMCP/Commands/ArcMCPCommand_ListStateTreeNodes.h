// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

class FArcMCPCommand_ListStateTreeNodes : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_statetree_nodes"); }
	virtual FString GetDescription() const override
	{
		return TEXT("List available StateTree node types (tasks, conditions, evaluators, considerations, property functions) for a given schema, with properties and aliases.");
	}
	virtual FString GetCategory() const override { return TEXT("StateTree"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("schema"), TEXT("string"), TEXT("Schema class name (e.g., 'UMassStateTreeSchema' or 'MassStateTreeSchema')"), true },
			{ TEXT("category"), TEXT("string"), TEXT("Filter: 'task', 'condition', 'evaluator', 'consideration', 'property_function'. Omit for all."), false },
			{ TEXT("filter"), TEXT("string"), TEXT("Substring filter on alias or struct name"), false },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
