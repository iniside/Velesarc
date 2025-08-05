#pragma once
#include "ArcCraft/ArcCraftComponent.h"

class UArcItemsStoreComponent;
class UArcCraftComponent;

class FArcCraftGameplayDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

	struct CraftStations
	{
		TWeakObjectPtr<UArcCraftComponent> CraftComponent;
	};

	TArray<FAssetData> CraftDataAssetList;
	int32 SelectedCratDataAssetIndex = -1;

	int32 CraftAmount = 1;
	
	TArray<CraftStations> CraftStationsList;
	int32 SelectedCraftStationIndex = -1;

	
};
