// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Export a Widget Blueprint's widget tree (or a named subtree) to the clipboard T3D text format.
 * Returns the serialized text which can be modified and re-imported.
 */
class FArcMCPCommand_ExportWidgetToText : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("export_widget_to_text"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Export a Widget Blueprint's widget tree to T3D clipboard text format. "
			"Returns serialized text that can be modified and re-imported via import_widget_from_text.");
	}
	virtual FString GetCategory() const override { return TEXT("UMG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint (e.g., /Game/UI/WBP_MyWidget)"), true },
			{ TEXT("widget_name"), TEXT("string"), TEXT("Name of a specific widget to export as subtree root. If omitted, exports the entire root widget tree."), false },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Import T3D clipboard text into a Widget Blueprint.
 * Can replace the full widget tree, inject under a parent, or create a new asset.
 */
class FArcMCPCommand_ImportWidgetFromText : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("import_widget_from_text"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Import T3D clipboard text into a Widget Blueprint. "
			"Can create new assets, replace the full widget tree, or inject under a named parent widget.");
	}
	virtual FString GetCategory() const override { return TEXT("UMG"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("widget_path"), TEXT("string"), TEXT("Path to the Widget Blueprint. Created with CanvasPanel root if it doesn't exist."), true },
			{ TEXT("widget_text"), TEXT("string"), TEXT("T3D text describing the widget hierarchy (Begin Object / End Object blocks)"), true },
			{ TEXT("parent_name"), TEXT("string"), TEXT("Name of the parent widget to insert under. If omitted, replaces the entire root widget tree."), false },
			{ TEXT("clear_parent"), TEXT("boolean"), TEXT("Remove existing children of parent before importing (default: false)"), false },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
