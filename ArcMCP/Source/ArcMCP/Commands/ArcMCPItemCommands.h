#if 0

// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Export an ArcItemDefinition to JSON format.
 */
class FArcMCPCommand_ExportItemToJson : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("export_item_to_json"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Export an ArcItemDefinition to JSON. Returns all fragments with their properties. "
			"Use with import_item_from_json for round-trip editing.");
	}
	virtual FString GetCategory() const override { return TEXT("Items"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("item_path"), TEXT("string"), TEXT("Content path to the item definition asset"), true },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Import/update an ArcItemDefinition from JSON.
 * Creates the asset if it doesn't exist. Replaces all fragments.
 */
class FArcMCPCommand_ImportItemFromJson : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("import_item_from_json"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Import JSON into an ArcItemDefinition, replacing all fragments and properties. "
			"Creates the asset if it doesn't exist. Use export_item_to_json first to get the current state.");
	}
	virtual FString GetCategory() const override { return TEXT("Items"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("item_path"), TEXT("string"), TEXT("Content path to the item definition (created if doesn't exist)"), true },
			{ TEXT("item_json"), TEXT("object"), TEXT("JSON object describing the item (item_type, fragments array, etc.)"), true },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

#endif
