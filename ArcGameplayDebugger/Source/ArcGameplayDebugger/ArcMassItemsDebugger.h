// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcMassItemsDebuggerShared.h"
#include "ArcItemDebuggerCreateItemSpec.h"
#include "AssetRegistry/AssetData.h"
#include "Bags/ArcBagId.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemId.h"
#include "Mass/EntityHandle.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "UObject/PrimaryAssetId.h"

struct FArcItemDefinitionData;
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
		bool bHasQuickBar = false;
		int32 AttachmentCount = 0;
		int32 EquippedCount = 0;
	};

	enum class EActiveTab : uint8 { Items, QuickBars, Attachments, Equipment, Bags };

	void RefreshEntityList();
	void RefreshItemDefinitions();
	void RefreshBagDefinitions();
	void DrawEntityListPanel();
	void DrawDetailPanel(FMassEntityManager& Manager, FMassEntityHandle Entity);
	void DrawItemsTab(FMassEntityManager& Manager, FMassEntityHandle Entity);
	void DrawQuickBarTab(FMassEntityManager& Manager, FMassEntityHandle Entity);
	void DrawAttachmentsTab(FMassEntityManager& Manager, FMassEntityHandle Entity);
	void DrawEquipmentTab(FMassEntityManager& Manager, FMassEntityHandle Entity);
	void DrawBagsTab(FMassEntityManager& Manager, FMassEntityHandle Entity);
	void RequestJumpToItem(FArcItemId ItemId);
	void DrawAddItemPanel(FMassEntityManager& Manager, FMassEntityHandle Entity);
	void DrawItemsTable(FMassEntityManager& Manager, FMassEntityHandle Entity, FArcMassItemStoreFragment& Store);
	void DrawSelectedItemDetail(FMassEntityManager& Manager, FMassEntityHandle Entity, FArcMassItemStoreFragment& Store);
	void DrawItemDetail(FMassEntityManager& Manager, FMassEntityHandle Entity, const FArcItemData& Item);
	void DrawInstancedDataTree(const FArcItemData& Item) const;
	void DrawDefinitionFragments(const FArcItemDefinitionData* Definition) const;

	TArray<FEntityEntry> CachedEntities;
	int32 SelectedEntityIndex = INDEX_NONE;
	int32 SelectedItemIndex = INDEX_NONE;
	char EntityFilterBuf[256] = {};
	char ItemDefinitionFilterBuf[256] = {};
	char SlotTagBuf[256] = {};
	float LastRefreshTime = 0.f;

	EArcMassAddItemMode AddItemMode = EArcMassAddItemMode::None;
	TArray<FPrimaryAssetId> CachedItemDefinitions;
	TArray<FString> CachedItemDefinitionNames;
	int32 SelectedDefinitionIndex = INDEX_NONE;
	FArcItemDebuggerItemSpecCreator ItemSpecCreator;

	EActiveTab ActiveTab = EActiveTab::Items;
	bool bPauseAutoRefresh = false;
	EArcMassItemsDebuggerJumpTarget PendingJump = EArcMassItemsDebuggerJumpTarget::None;
	FArcItemId PendingJumpItemId;

	// --- Bags tab state ---
	// Cached UArcBagDefinition asset list for the "Add Bag" picker (refreshed on demand).
	TArray<FAssetData> CachedBagDefinitions;
	TArray<FString> CachedBagDefinitionNames;
	int32 SelectedBagDefinitionIndex = INDEX_NONE;
	// Pending two-click move source (bag + slot); set on the first "Move" click, consumed on the second.
	TOptional<TPair<FArcBagId, int32>> PendingBagMove;
	// "Add item" controls shared by the per-slot "Add new here" and the "first free slot" panel.
	char BagItemDefinitionFilterBuf[256] = {};
	int32 BagAddItemSelectedDefIndex = INDEX_NONE;
	int32 BagAddItemLevel = 1;
	int32 BagAddItemAmount = 1;
};
