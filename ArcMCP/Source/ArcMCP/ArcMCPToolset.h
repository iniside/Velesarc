// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ToolsetRegistry/UToolsetRegistry.h"

#include "ArcMCPToolset.generated.h"

/**
 * ArcGame MCP Toolset — exposes editor tools for StateTree, Items, Templates, Widgets, and Assets
 * via the ToolsetRegistry / ModelContextProtocol server.
 */
UCLASS()
class UArcMCPToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	// ---- StateTree ----

	/** List all available StateTree schema classes with their context data and allowed features. */
	UFUNCTION()
	static FString ListStateTreeSchemas();

	/** List StateTree node types (tasks, conditions, evaluators, considerations, property functions).
	 *  Optionally filter by Schema class name, Category ('task','condition','evaluator','consideration','property_function'), or text Filter. */
	UFUNCTION()
	static FString ListStateTreeNodes(const FString& Schema, const FString& Category, const FString& Filter);

	/** Create a compiled StateTree asset from a JSON Description string.
	 *  The Description is a JSON object with name, package_path, schema, states, bindings, etc. */
	UFUNCTION()
	static FString CreateStateTree(const FString& Description);

	/** Validate a StateTree JSON Description without creating an asset. Returns errors and warnings. */
	UFUNCTION()
	static FString ValidateStateTree(const FString& Description);

	// ---- Items ----

	/** Get rich descriptions of item fragment types including properties, pairings, and prerequisites.
	 *  Optionally filter by StructName or BaseType ('fragment' or 'scalable_float'). */
	UFUNCTION()
	static FString DescribeFragments(const FString& StructName, const FString& BaseType);

	/** Export a UArcItemDefinition asset at ItemPath to JSON. */
	UFUNCTION()
	static FString ExportItemToJson(const FString& ItemPath);

	/** Import ItemJson (JSON string) into a UArcItemDefinition at ItemPath. Creates the asset if it doesn't exist. */
	UFUNCTION()
	static FString ImportItemFromJson(const FString& ItemPath, const FString& ItemJson);

	// ---- Templates ----

	/** Create a new item definition from a template at TemplatePath. Saves to PackagePath/AssetName. */
	UFUNCTION()
	static FString CreateItemFromTemplate(const FString& TemplatePath, const FString& AssetName, const FString& PackagePath);

	/** Add a fragment type (FragmentType struct name) to the template at TemplatePath. */
	UFUNCTION()
	static FString TemplateAddFragment(const FString& TemplatePath, const FString& FragmentType);

	/** Remove a fragment type (FragmentType struct name) from the template at TemplatePath. */
	UFUNCTION()
	static FString TemplateRemoveFragment(const FString& TemplatePath, const FString& FragmentType);

	/** Re-sync all item definitions that reference the template at TemplatePath. */
	UFUNCTION()
	static FString UpdateItemsFromTemplate(const FString& TemplatePath);

	// ---- Widgets ----

	/** Export a Widget Blueprint's widget tree at WidgetPath to T3D clipboard text.
	 *  Optionally export only a subtree by specifying WidgetName. */
	UFUNCTION()
	static FString ExportWidgetToText(const FString& WidgetPath, const FString& WidgetName);

	/** Import T3D WidgetText into a Widget Blueprint at WidgetPath.
	 *  Optionally inject under ParentName. Set ClearParent to remove existing children first. */
	UFUNCTION()
	static FString ImportWidgetFromText(const FString& WidgetPath, const FString& WidgetText, const FString& ParentName, bool ClearParent);

	// ---- Assets ----

	/** Create a new UDataAsset subclass instance. AssetClass is the full class path. */
	UFUNCTION()
	static FString CreateDataAsset(const FString& AssetName, const FString& PackagePath, const FString& AssetClass);
};
