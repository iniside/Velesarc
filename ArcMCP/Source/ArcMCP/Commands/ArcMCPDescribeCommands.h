#if 0

// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Query rich descriptions of all item fragment types.
 *
 * Returns auto-reflected property metadata plus manually authored
 * common pairings, prerequisites, and usage notes for each fragment.
 * Optionally filter by struct name or base type.
 */
class FArcMCPCommand_DescribeFragments : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("describe_fragments"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Get rich descriptions of item fragment types including properties, common pairings, prerequisites, and usage notes.");
	}
	virtual FString GetCategory() const override { return TEXT("Items"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("struct_name"), TEXT("string"), TEXT("Filter to a single fragment by struct name (e.g., ArcItemFragment_GrantedAbilities). F prefix is accepted but optional."), false },
			{ TEXT("base_type"), TEXT("string"), TEXT("Filter by base: 'fragment' (FArcItemFragment) or 'scalable_float' (FArcScalableFloatItemFragment). Omit for all."), false },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

#endif
