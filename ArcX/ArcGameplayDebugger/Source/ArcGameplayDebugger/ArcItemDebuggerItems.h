#pragma once

#include "AssetRegistry/AssetData.h"
#include "GameplayTagContainer.h"

class UArcItemsStoreComponent;
class UArcItemDefinition;
class UArcQuickBarComponent;
struct FArcItemData;
struct FArcItemId;

class FArcDebuggerItems
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	void DrawItemsTab();
	void DrawStoreSelector();
	void DrawItemsList();
	void DrawAddItemPanel();
	void DrawItemDetail(const FArcItemData* Item);
	void DrawInstancedDataTree(const FArcItemData* Item);

	void DrawQuickBarTab();
	void DrawQuickBarSlots(UArcQuickBarComponent* QuickBar, int32 BarIndex);

	TArray<UArcItemsStoreComponent*> GetPlayerStores() const;
	TArray<UArcItemsStoreComponent*> GetAllWorldStores() const;
	TArray<UArcQuickBarComponent*> GetPlayerQuickBars() const;
	void RefreshItemDefinitions();

	// Items tab state
	TWeakObjectPtr<UArcItemsStoreComponent> SelectedStore;
	int32 SelectedStoreIndex = 0;
	int32 SelectedItemIndex = -1;

	// Transfer target state
	TWeakObjectPtr<UArcItemsStoreComponent> TargetStore;
	int32 TargetStoreIndex = -1;

	// Add item panel
	bool bShowAddItemPanel = false;
	TArray<FAssetData> CachedItemDefinitions;
	TArray<FString> CachedItemDefinitionNames;
	int32 SelectedDefinitionIndex = 0;
	char FilterBuf[256] = {};

	// Slot assignment
	char SlotTagBuf[256] = {};

	// QuickBar tab state
	TWeakObjectPtr<UArcQuickBarComponent> SelectedQuickBar;
	int32 SelectedQuickBarCompIndex = 0;
	int32 SelectedBarIndex = 0;
	int32 SelectedQuickBarSlotIndex = -1;
	char QuickBarAssignFilterBuf[256] = {};
};
