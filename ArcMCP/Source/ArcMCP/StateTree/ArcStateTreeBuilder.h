// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMCP/StateTree/ArcStateTreeTypes.h"

class UStateTree;
class UStateTreeState;
class UStateTreeEditorData;
class UStateTreeSchema;
struct FStateTreeEditorNode;

struct FArcStateTreeBuildResult
{
	bool bSuccess = false;
	FString AssetPath;
	TArray<FString> Errors;
	TArray<FString> Warnings;
};

/**
 * Builds a UStateTree asset from a parsed FArcSTDescription.
 * Uses the engine's UStateTreeEditorData builder API and FStateTreeCompiler.
 */
class FArcStateTreeBuilder
{
public:
	/**
	 * Build and compile a StateTree, save to disk.
	 * @param Desc     Parsed and validated JSON description
	 * @param bSave    If true, saves the .uasset. If false (validate mode), discards it.
	 */
	FArcStateTreeBuildResult Build(const FArcSTDescription& Desc, bool bSave = true);

private:
	// Pipeline steps
	bool ResolveSchemaClass(const FString& SchemaName);
	bool CreateAssetAndEditorData(const FArcSTDescription& Desc, bool bTransient);
	bool AddEvaluatorsAndGlobalTasks(const FArcSTDescription& Desc);
	bool BuildStateHierarchy(const FArcSTDescription& Desc);
	bool AddStateNodes(const FArcSTDescription& Desc);
	bool AddTransitions(const FArcSTDescription& Desc);
	bool AddBindings(const FArcSTDescription& Desc);
	bool CompileTree();
	bool SaveAsset(const FArcSTDescription& Desc);

	// Helpers
	bool InitializeEditorNode(FStateTreeEditorNode& OutNode, const UScriptStruct* NodeStruct, const FArcSTNodeDesc& NodeDesc);
	bool SetPropertiesFromJson(void* StructMemory, const UScriptStruct* Struct, const TSharedPtr<FJsonObject>& Props, const FString& Context);
	FGuid ResolveBindingSourceGuid(const FString& NodeId) const;

	void AddError(const FString& Msg) { Result.Errors.Add(Msg); }
	void AddWarning(const FString& Msg) { Result.Warnings.Add(Msg); }

	// State
	FArcStateTreeBuildResult Result;
	UClass* SchemaClass = nullptr;
	UPackage* Package = nullptr;
	UStateTree* StateTree = nullptr;
	UStateTreeEditorData* EditorData = nullptr;

	TMap<FString, FGuid> NodeIdToGuid;       // json node id -> editor node GUID
	TMap<FString, UStateTreeState*> StateMap; // json state id -> editor state object
};
