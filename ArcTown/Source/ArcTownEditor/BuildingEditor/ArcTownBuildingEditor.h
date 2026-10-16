// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkitHost.h"

class IDetailsView;
class SArcTownBuildingPreviewViewport;
class SDockTab;
class UArcTownBuildingDefinition;

class FArcTownBuildingEditor
	: public FAssetEditorToolkit
	, public FEditorUndoClient
{
public:
	static const FName ViewportTabId;
	static const FName DetailsTabId;
	static const FName ToolkitFName;
	static const FName AppIdentifier;

	static TSharedRef<FArcTownBuildingEditor> CreateEditor(
		const EToolkitMode::Type Mode,
		const TSharedPtr<IToolkitHost>& InitToolkitHost,
		UArcTownBuildingDefinition* DefinitionToEdit);

	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	// FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

private:
	void RefreshAfterUndoRedo();

	void InitBuildingEditor(
		const EToolkitMode::Type Mode,
		const TSharedPtr<IToolkitHost>& InitToolkitHost,
		UArcTownBuildingDefinition* DefinitionToEdit);

	TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Details(const FSpawnTabArgs& Args);

	TSharedPtr<SArcTownBuildingPreviewViewport> PreviewViewport;
	TSharedPtr<IDetailsView> DetailsView;
};
