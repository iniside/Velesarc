// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UStateTreeSchema;
struct FStateTreeNodeBase;

// Describes a single reflected property
struct FArcSTPropertyDesc
{
	FString Name;
	FString TypeName;
	FString DefaultValue;
	FString DisplayName;  // Human-readable name from reflection metadata
	FString Description;  // Tooltip text from reflection metadata
	FString Role;         // "input", "output", or empty. Used for property function params.
};

// Describes a required external data handle
struct FArcSTExternalDataDesc
{
	FString Name;
	FString TypeName;
	bool bRequired = true;
};

// Full descriptor for a registered node type
struct FArcSTNodeTypeDesc
{
	FString Alias;
	FString StructName;
	FString Category; // "task", "condition", "evaluator", "consideration", "property_function"
	FString DisplayName;  // Human-readable name from struct metadata (e.g., "Add", "Break Transform")
	FString Description;  // Tooltip text from struct reflection
	const UScriptStruct* Struct = nullptr;
	const UScriptStruct* InstanceDataStruct = nullptr;
	const UScriptStruct* ExecutionRuntimeDataStruct = nullptr;
	TArray<FArcSTPropertyDesc> NodeProperties;
	TArray<FArcSTPropertyDesc> InstanceDataProperties;
	TArray<FArcSTExternalDataDesc> ExternalData;
};

/**
 * Singleton registry that discovers all StateTree node types at startup,
 * generates snake_case aliases, and caches reflection data.
 */
class FArcStateTreeNodeRegistry
{
public:
	static FArcStateTreeNodeRegistry& Get();

	/** Initialize the registry. Must be called after all modules are loaded. */
	void Initialize();

	/** Resolve an alias or struct name to UScriptStruct. Returns nullptr if not found. */
	const UScriptStruct* ResolveNodeType(const FString& AliasOrStructName) const;

	/** Find a UStateTreeSchema subclass by name (with or without U prefix). */
	static UClass* FindSchemaClass(const FString& SchemaName);

	/** Get the category string for a node struct. */
	FString GetCategory(const UScriptStruct* Struct) const;

	/** Get all node descriptors, optionally filtered by schema and category. */
	TArray<const FArcSTNodeTypeDesc*> GetNodes(
		const UStateTreeSchema* Schema = nullptr,
		const FString& CategoryFilter = FString(),
		const FString& TextFilter = FString()) const;

	/** Get the full descriptor for a node struct. */
	const FArcSTNodeTypeDesc* GetDescriptor(const UScriptStruct* Struct) const;

	bool IsInitialized() const { return bInitialized; }

private:
	FArcStateTreeNodeRegistry() = default;

	void RegisterStruct(const UScriptStruct* Struct, const UScriptStruct* BaseStruct, const FString& Category);
	static FString GenerateAlias(const UScriptStruct* Struct, const FString& Category);
	static TArray<FArcSTPropertyDesc> ReflectProperties(const UScriptStruct* Struct, bool bClassifyRoles = false);
	static FString GetPropertyDefaultString(const FProperty* Property, const void* ContainerPtr);

	TMap<FString, FArcSTNodeTypeDesc> ByAlias; // alias -> descriptor
	TMap<FString, FString> StructNameToAlias;  // FName string -> alias
	bool bInitialized = false;
};
