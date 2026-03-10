// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Commands/ECACommand.h"

/**
 * Create a new UArcItemDefinition from a UArcItemDefinitionTemplate.
 * Applies the template's fragments to the new item definition.
 */
class FArcMCPCommand_CreateItemFromTemplate : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("create_item_from_template"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Create a new item definition from a template. Applies all template fragments to the new item.");
	}
	virtual FString GetCategory() const override { return TEXT("Items"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("template_path"), TEXT("string"), TEXT("Content path to the template asset (e.g., /Game/Data/Templates/DT_Weapon)"), true },
			{ TEXT("asset_name"), TEXT("string"), TEXT("Name for the new item definition (e.g., DA_SwordItem)"), true },
			{ TEXT("package_path"), TEXT("string"), TEXT("Content folder for the new asset (e.g., /Game/Data/Items)"), true },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Add a default-initialized fragment to a template by struct type name.
 */
class FArcMCPCommand_TemplateAddFragment : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("template_add_fragment"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Add a fragment type to a template. The fragment is default-initialized. Use describe_fragments to see available types.");
	}
	virtual FString GetCategory() const override { return TEXT("Items"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("template_path"), TEXT("string"), TEXT("Content path to the template asset"), true },
			{ TEXT("fragment_type"), TEXT("string"), TEXT("Fragment struct name as returned by describe_fragments (e.g., ArcItemFragment_GrantedAbilities). F prefix is accepted but optional."), true },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Remove a fragment from a template by struct type name.
 */
class FArcMCPCommand_TemplateRemoveFragment : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("template_remove_fragment"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Remove a fragment type from a template by its struct name.");
	}
	virtual FString GetCategory() const override { return TEXT("Items"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("template_path"), TEXT("string"), TEXT("Content path to the template asset"), true },
			{ TEXT("fragment_type"), TEXT("string"), TEXT("Fragment struct name as returned by describe_fragments (e.g., ArcItemFragment_GrantedAbilities). F prefix is accepted but optional."), true },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};

/**
 * Find all item definitions referencing a template and update them.
 * Calls UpdateFromTemplate() on each, which adds new fragments, removes
 * old template-sourced fragments, and preserves custom fragments.
 */
class FArcMCPCommand_UpdateItemsFromTemplate : public IECACommand
{
public:
	virtual FString GetName() const override { return TEXT("update_items_from_template"); }
	virtual FString GetDescription() const override
	{
		return TEXT("Update all item definitions that reference a template. Adds new template fragments, removes old ones, preserves custom fragments.");
	}
	virtual FString GetCategory() const override { return TEXT("Items"); }

	virtual TArray<FECACommandParam> GetParameters() const override
	{
		return {
			{ TEXT("template_path"), TEXT("string"), TEXT("Content path to the template asset"), true },
		};
	}

	virtual FECACommandResult Execute(const TSharedPtr<FJsonObject>& Params) override;
};
