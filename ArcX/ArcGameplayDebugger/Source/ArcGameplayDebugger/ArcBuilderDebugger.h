#pragma once

class FArcBuilderDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();
	
	bool bShow = false;

	TArray<FAssetData> BuildDataAssetList;
	int32 SelectedBuildDataAssetIndex = -1;
	float CurrentHeight = 0;
	bool bUsePlacementGrid = false;

	bool bUseRelativeGrid = false;
	bool bAlignGridToSurfaceNormal = true;
};
