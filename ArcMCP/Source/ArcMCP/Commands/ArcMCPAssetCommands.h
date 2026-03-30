#if 0

// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Create a new DataAsset of a specified UDataAsset subclass.
 *
 * The agent provides the class path (e.g., /Script/ArcCore.ArcItemDefinition)
 * based on conversation context, along with a name and destination folder.
 */
class FArcMCPCommand_CreateDataAsset : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_data_asset"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Create a new DataAsset of a specified UDataAsset subclass in the content browser.");
	}
	virtual FString GetCategory() const override { return TEXT("Asset"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("asset_name"), TEXT("string"), TEXT("Name for the new asset (e.g., DA_SwordItem)"), true },
			{ TEXT("package_path"), TEXT("string"), TEXT("Content folder path (e.g., /Game/Data/Items)"), true },
			{ TEXT("asset_class"), TEXT("string"), TEXT("Full class path of the UDataAsset subclass (e.g., /Script/ArcCore.ArcItemDefinition)"), true },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

#endif
