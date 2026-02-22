// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "SlateIMWidgetBase.h"
#include "AssetRegistry/AssetData.h"

class UArcItemsStoreComponent;
class UArcItemDefinition;
class UArcQuickBarComponent;
struct FArcItemData;
struct FArcItemId;

class FArcItemsDebugWindow : public FSlateIMWindowBase
{
public:
	FArcItemsDebugWindow();
	
	mutable TWeakObjectPtr<UWorld> World;
	
protected:
	virtual void DrawWindow(float DeltaTime) override;

private:
	void DrawStoreSelector();
	void DrawItemsList();
	void DrawAddItemPanel();
	void DrawItemDetail(const FArcItemData* Item);
	void DrawInstancedDataTree(const FArcItemData* Item);
	void DrawQuickBarPanel();
	void DrawQuickBarSlots(UArcQuickBarComponent* QuickBar, int32 BarIndex);

	TArray<UArcItemsStoreComponent*> GetLocalPlayerStores() const;
	TArray<UArcQuickBarComponent*> GetLocalPlayerQuickBars() const;
	void RefreshItemDefinitions();


	// State
	TWeakObjectPtr<UArcItemsStoreComponent> SelectedStore;
	int32 SelectedStoreIndex = 0;
	int32 SelectedItemIndex = -1;

	// Add item panel
	bool bShowAddItemPanel = false;
	TArray<FAssetData> CachedItemDefinitions;
	TArray<FString> CachedItemDefinitionNames;
	int32 SelectedDefinitionIndex = 0;
	FString FilterText;
	TArray<FString> FilteredDefinitionNames;
	TArray<int32> FilteredToSourceIndex;

	// Slot assignment
	FString SlotTagText;

	// Quick Bar panel
	TWeakObjectPtr<UArcQuickBarComponent> SelectedQuickBar;
	int32 SelectedQuickBarCompIndex = 0;
	int32 SelectedBarIndex = 0;
	int32 SelectedQuickBarItemIndex = -1;
	FString QuickBarAssignItemFilter;
};
