// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

class FArcMCPCommand_CreateStateTree : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_statetree"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Create a compiled StateTree asset from a JSON description. The asset is saved as a .uasset.");
	}
	virtual FString GetCategory() const override { return TEXT("StateTree"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("description"), TEXT("object"), TEXT("Full StateTree JSON description object"), true },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
