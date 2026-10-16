// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkitHost.h"

class IDetailsView;
class SArcTQSPreviewViewport;
class SArcTQSResultsView;
class SDockTab;
class UArcTQSQueryDefinition;

class FArcTQSQueryEditor
	: public FAssetEditorToolkit
	, public FEditorUndoClient
{
public:
	static const FName ViewportTabId;
	static const FName DetailsTabId;
	static const FName PreviewTabId;
	static const FName ResultsTabId;
	static const FName ToolkitFName;
	static const FName AppIdentifier;

	static TSharedRef<FArcTQSQueryEditor> CreateEditor(
		const EToolkitMode::Type Mode,
		const TSharedPtr<IToolkitHost>& InitToolkitHost,
		UArcTQSQueryDefinition* DefinitionToEdit);

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
	void InitQueryEditor(
		const EToolkitMode::Type Mode,
		const TSharedPtr<IToolkitHost>& InitToolkitHost,
		UArcTQSQueryDefinition* DefinitionToEdit);

	void RefreshDetails();
	void HandleResultChanged();
	void HandleResultItemSelected(int32 ItemIndex);

	TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Details(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Preview(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Results(const FSpawnTabArgs& Args);

	TSharedPtr<SArcTQSPreviewViewport> PreviewViewport;
	TSharedPtr<SArcTQSResultsView> ResultsView;
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<IDetailsView> PreviewDetailsView;
};
