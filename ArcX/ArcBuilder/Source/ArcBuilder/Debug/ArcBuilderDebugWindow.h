// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "SlateIMWidgetBase.h"
#include "AssetRegistry/AssetData.h"

class UArcBuilderComponent;
class UArcBuildingDefinition;

class FArcBuilderDebugWindow : public FSlateIMWindowBase
{
public:
	FArcBuilderDebugWindow();

	mutable TWeakObjectPtr<UWorld> World;

protected:
	virtual void DrawWindow(float DeltaTime) override;

private:
	void DrawBuildingDefinitionsList();
	void DrawBuildingDefinitionDetail(UArcBuildingDefinition* BuildDef);

	UArcBuilderComponent* FindLocalPlayerBuilderComponent() const;
	void RefreshBuildingDefinitions();

	// State
	TArray<FAssetData> CachedBuildingDefinitions;
	TArray<FString> CachedBuildingDefinitionNames;
	int32 SelectedDefinitionIndex = -1;

	// Filter
	FString FilterText;
	TArray<FString> FilteredDefinitionNames;
	TArray<int32> FilteredToSourceIndex;
};
