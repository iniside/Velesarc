#pragma once
#include "ArcItemDebuggerCreateItemSpec.h"

class FArcDebuggerItems
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;
	int32 SelectedItemStoreIndex = -1;

	TArray<FAssetData> ItemDefinitions;
	TArray<FString> ItemList;

	FArcItemDebuggerItemSpecCreator ItemSpecCreator;
};

class FArcItemDebuggerItems
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();
};
