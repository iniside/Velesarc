// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

class FArcMCPCommand_ValidateStateTree : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("validate_statetree"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Validate a StateTree JSON description without creating the asset. Returns errors and warnings.");
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
