#if 0

// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

class FArcMCPCommand_ListStateTreeSchemas : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("list_statetree_schemas"); }
	virtual FString GetDescription() const override
	{
		return TEXT("List all available StateTree schema classes with their context data and allowed features.");
	}
	virtual FString GetCategory() const override { return TEXT("StateTree"); }
	virtual TArray<FECACommandParam> GetParameters() const override { return {}; }
	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

#endif
