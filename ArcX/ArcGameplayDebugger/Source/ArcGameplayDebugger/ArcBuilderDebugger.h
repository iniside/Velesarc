// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetData.h"

class UArcBuilderComponent;
class UArcBuildingDefinition;

class FArcBuilderDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	UArcBuilderComponent* FindLocalPlayerBuilderComponent() const;
	void RefreshBuildingDefinitions();
	void ApplyFilter();

	void DrawDefinitionsList(UArcBuilderComponent* BuilderComp);
	void DrawDefinitionDetail(UArcBuildingDefinition* BuildDef, UArcBuilderComponent* BuilderComp);
	void DrawPlacementControls(UArcBuilderComponent* BuilderComp);

	// Cached asset data
	TArray<FAssetData> CachedBuildingDefinitions;
	TArray<FString> CachedBuildingDefinitionNames;
	int32 SelectedDefinitionIndex = -1;

	// Filter
	char FilterBuf[256] = {};
	TArray<int32> FilteredToSourceIndex;
};
