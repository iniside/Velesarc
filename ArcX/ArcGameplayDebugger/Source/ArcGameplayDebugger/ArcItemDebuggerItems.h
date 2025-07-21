#pragma once

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
};

class FArcItemDebuggerItems
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();
};
