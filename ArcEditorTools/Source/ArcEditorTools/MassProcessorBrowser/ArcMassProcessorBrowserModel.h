// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessorDependencySolver.h"
#include "UObject/WeakObjectPtr.h"

class UMassProcessor;
class UArcMassProcessorBrowserConfig;

enum class EArcProcessorTreeItemType : uint8
{
	Plugin,
	Module,
	Category,
	Processor
};

struct FArcProcessorTreeItem : TSharedFromThis<FArcProcessorTreeItem>
{
	FText DisplayName;
	EArcProcessorTreeItemType Type;
	TWeakObjectPtr<UMassProcessor> ProcessorCDO;
	TArray<TSharedPtr<FArcProcessorTreeItem>> Children;
	TWeakPtr<FArcProcessorTreeItem> Parent;

	bool IsLeaf() const { return Type == EArcProcessorTreeItemType::Processor; }
};

struct FArcProcessorRequirementsInfo
{
	TArray<FName> FragmentsRead;
	TArray<FName> FragmentsWrite;
	TArray<FName> SharedFragmentsRead;
	TArray<FName> SharedFragmentsWrite;
	TArray<FName> ConstSharedFragmentsRead;
	TArray<FName> ChunkFragmentsRead;
	TArray<FName> ChunkFragmentsWrite;
	TArray<FName> SubsystemsRead;
	TArray<FName> SubsystemsWrite;
	TArray<FName> SparseRead;
	TArray<FName> SparseWrite;
	TArray<FName> TagsAll;
	TArray<FName> TagsAny;
	TArray<FName> TagsNone;
};

class FArcMassProcessorBrowserModel
{
public:
	void Build(const UArcMassProcessorBrowserConfig* Config);
	const TArray<TSharedPtr<FArcProcessorTreeItem>>& GetRootItems() const { return RootItems; }
	TSharedPtr<FArcProcessorTreeItem> FindItemForProcessor(const UClass* ProcessorClass) const;
	static FArcProcessorRequirementsInfo ExtractRequirements(UMassProcessor* ProcessorCDO);
	static bool HasModifiedConfigProperties(UMassProcessor* ProcessorCDO);

private:
	FString ResolveCategory(const UClass* ProcessorClass, const UArcMassProcessorBrowserConfig* Config) const;
	FString ResolveModuleName(const UClass* ProcessorClass) const;
	FString ResolvePluginName(const FString& ModuleName) const;
	TSharedPtr<FArcProcessorTreeItem> FindOrCreateGroup(TArray<TSharedPtr<FArcProcessorTreeItem>>& Items, const FString& Name, EArcProcessorTreeItemType Type);

	TArray<TSharedPtr<FArcProcessorTreeItem>> RootItems;
	TMap<const UClass*, TSharedPtr<FArcProcessorTreeItem>> ProcessorItemMap;
};
