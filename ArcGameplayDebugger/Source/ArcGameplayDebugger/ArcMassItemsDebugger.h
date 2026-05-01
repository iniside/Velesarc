// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcItemDebuggerCreateItemSpec.h"
#include "AssetRegistry/AssetData.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "MassEntityTypes.h"

class UArcItemDefinition;
struct FArcItemData;
struct FArcMassItemStoreFragment;
struct FMassEntityManager;

enum class EArcMassAddItemMode : uint8
{
	None,
	QuickAdd,
	CustomSpec
};

class FArcMassItemsDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	struct FEntityEntry
	{
		FMassEntityHandle Entity;
		FString Label;
		int32 ItemCount = 0;
		int32 SlottedItemCount = 0;
	};

	void RefreshEntityList();
	void RefreshItemDefinitions();
	void DrawEntityListPanel();
	void DrawDetailPanel(FMassEntityManager& Manager, FMassEntityHandle Entity);
	void DrawAddItemPanel(FMassEntityManager& Manager, FMassEntityHandle Entity);
	void DrawItemsTable(FMassEntityManager& Manager, FMassEntityHandle Entity, FArcMassItemStoreFragment& Store);
	void DrawSelectedItemDetail(FMassEntityManager& Manager, FMassEntityHandle Entity, FArcMassItemStoreFragment& Store);
	void DrawItemDetail(FMassEntityManager& Manager, FMassEntityHandle Entity, const FArcItemData& Item);
	void DrawStructProperties(const UScriptStruct* StructType, const void* StructData, const char* TableIdPrefix) const;
	void DrawInstancedDataTree(const FArcItemData& Item) const;
	void DrawDefinitionFragments(const UArcItemDefinition* Definition) const;
	void DrawTags(const char* Header, const FGameplayTagContainer& Tags) const;

	TArray<FEntityEntry> CachedEntities;
	int32 SelectedEntityIndex = INDEX_NONE;
	int32 SelectedItemIndex = INDEX_NONE;
	char EntityFilterBuf[256] = {};
	char ItemDefinitionFilterBuf[256] = {};
	char SlotTagBuf[256] = {};
	float LastRefreshTime = 0.f;

	EArcMassAddItemMode AddItemMode = EArcMassAddItemMode::None;
	TArray<FAssetData> CachedItemDefinitions;
	TArray<FString> CachedItemDefinitionNames;
	int32 SelectedDefinitionIndex = INDEX_NONE;
	FArcItemDebuggerItemSpecCreator ItemSpecCreator;
};
