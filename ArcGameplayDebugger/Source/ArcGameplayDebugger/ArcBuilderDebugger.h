// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetData.h"

class UArcBuilderSubsystem;
class UArcBuildingDefinition;

class FArcBuilderDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	UArcBuilderSubsystem* FindLocalPlayerBuilderSubsystem() const;
	void RefreshBuildingDefinitions();
	void ApplyFilter();

	void DrawStatus(UArcBuilderSubsystem* BuilderSubsystem);
	void DrawPlacementControls(UArcBuilderSubsystem* BuilderSubsystem);
	void DrawToolBuildings(UArcBuilderSubsystem* BuilderSubsystem);
	void DrawDefinitionsList(UArcBuilderSubsystem* BuilderSubsystem);
	void DrawDefinitionDetail(UArcBuildingDefinition* BuildDef, UArcBuilderSubsystem* BuilderSubsystem);

	// Cached asset data
	TArray<FAssetData> CachedBuildingDefinitions;
	TArray<FString> CachedBuildingDefinitionNames;
	int32 SelectedDefinitionIndex = -1;

	// Filter
	char FilterBuf[256] = {};
	TArray<int32> FilteredToSourceIndex;
};
