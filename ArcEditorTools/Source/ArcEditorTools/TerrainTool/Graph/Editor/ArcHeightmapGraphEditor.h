// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Spatial/SampledScalarField2.h"
#include "TerrainTool/Graph/ArcHeightmapGraphEval.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkitHost.h"
#include "UObject/StrongObjectPtr.h"

class FExtender;
class FToolBarBuilder;
class SArcHeightmapGraphPreview;
class SArcHeightmapGraphView;
class UArcHeightmapGraph;

/**
 * Asset-editor toolkit owning a UArcHeightmapGraph and hosting the graph view in a single tab.
 *
 * Plain C++ class (no generated.h) derived directly from FAssetEditorToolkit, registering one
 * custom graph tab whose content is an SArcHeightmapGraphView built over the edited asset.
 * SaveAsset_Execute flushes the live mirror state (node drag-moves) back into the asset before
 * the default save runs.
 */
class FArcHeightmapGraphEditor : public FAssetEditorToolkit
{
public:
	void InitHeightmapGraphEditor(
		const EToolkitMode::Type Mode,
		const TSharedPtr<IToolkitHost>& InitToolkitHost,
		UArcHeightmapGraph* InGraph);

	// FAssetEditorToolkit overrides
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void SaveAsset_Execute() override;

private:
	void FillToolbar(FToolBarBuilder& Builder);

	/** Kicks off a background evaluation of the edited graph and pushes the result into the preview tab. */
	void EvaluatePreview();

	void BakeHeightmap();

	/** True while EvaluatePreview's worker is in flight; gates re-entry and drives the preview overlay. */
	bool CanEvaluatePreview() const { return !bPreviewEvaluating; }

	/** Game-thread continuation invoked when the worker eval finishes; updates the preview / notification. */
	void OnPreviewEvalComplete(bool bOk, TSharedPtr<FArcHeightmapEvalResult> Result, FString Error);

	/** The asset being edited. */
	TObjectPtr<UArcHeightmapGraph> EditedGraph = nullptr;

	/** True between dispatching the worker eval and its game-thread completion. */
	bool bPreviewEvaluating = false;

	/** Transient duplicate of EditedGraph evaluated on the worker, so live edits can't race the read. */
	TStrongObjectPtr<UArcHeightmapGraph> PreviewEvalGraph;

	/** The hosted graph view widget. */
	TSharedPtr<SArcHeightmapGraphView> GraphView;

	/** The hosted output-preview widget. */
	TSharedPtr<SArcHeightmapGraphPreview> PreviewWidget;

	TSharedPtr<FExtender> ToolbarExtender;

	/** Identifier of the single graph tab. */
	static const FName GraphTabId;

	/** Identifier of the output-preview tab. */
	static const FName PreviewTabId;

	/** Identifier of the details tab (node struct details + asset resolution/coverage). */
	static const FName DetailsTabId;
};
