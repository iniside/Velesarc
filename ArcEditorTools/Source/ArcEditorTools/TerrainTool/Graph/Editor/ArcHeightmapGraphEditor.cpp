// Copyright Lukasz Baran. All Rights Reserved.

#include "TerrainTool/Graph/Editor/ArcHeightmapGraphEditor.h"

#include "TerrainTool/Graph/Editor/SArcHeightmapGraphPreview.h"
#include "TerrainTool/Graph/Editor/SArcHeightmapGraphView.h"
#include "TerrainTool/Graph/ArcHeightmapGraph.h"
#include "TerrainTool/Graph/ArcHeightmapGraphEval.h"

#include "Async/Async.h"
#include "ScopedTransaction.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Styling/AppStyle.h"
#include "UObject/Package.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "ArcHeightmapGraphEditor"

const FName FArcHeightmapGraphEditor::GraphTabId(TEXT("ArcHeightmapGraphEditor_Graph"));
const FName FArcHeightmapGraphEditor::PreviewTabId(TEXT("ArcHeightmapGraphEditor_Preview"));
const FName FArcHeightmapGraphEditor::DetailsTabId(TEXT("ArcHeightmapGraphEditor_Details"));

void FArcHeightmapGraphEditor::InitHeightmapGraphEditor(
	const EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	UArcHeightmapGraph* InGraph)
{
	EditedGraph = InGraph;

	GraphView = SNew(SArcHeightmapGraphView, InGraph);
	PreviewWidget = SNew(SArcHeightmapGraphPreview);

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("ArcHeightmapGraphEditor_Layout_v3")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack()
				->AddTab(GraphTabId, ETabState::OpenedTab)
				->SetSizeCoefficient(0.7f)
			)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.3f)
				->Split
				(
					FTabManager::NewStack()
					->AddTab(DetailsTabId, ETabState::OpenedTab)
					->SetSizeCoefficient(0.6f)
				)
				->Split
				(
					FTabManager::NewStack()
					->AddTab(PreviewTabId, ETabState::OpenedTab)
					->SetSizeCoefficient(0.4f)
				)
			)
		);

	InitAssetEditor(
		Mode,
		InitToolkitHost,
		FName("ArcHeightmapGraphEditorApp"),
		Layout,
		/*bCreateDefaultStandaloneMenu=*/true,
		/*bCreateDefaultToolbar=*/true,
		InGraph);

	ToolbarExtender = MakeShareable(new FExtender);
	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP(this, &FArcHeightmapGraphEditor::FillToolbar));
	AddToolbarExtender(ToolbarExtender);
	RegenerateMenusAndToolbars();
}

void FArcHeightmapGraphEditor::FillToolbar(FToolBarBuilder& Builder)
{
	Builder.BeginSection("Terrain");
	Builder.AddToolBarButton(
		FUIAction(
			FExecuteAction::CreateSP(this, &FArcHeightmapGraphEditor::EvaluatePreview),
			FCanExecuteAction::CreateSP(this, &FArcHeightmapGraphEditor::CanEvaluatePreview)),
		NAME_None,
		LOCTEXT("EvaluatePreview", "Evaluate Preview"),
		LOCTEXT("EvaluatePreviewTip", "Evaluate the graph and show its output heightmap in the Preview tab."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.BuildOptions"));
	Builder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateSP(this, &FArcHeightmapGraphEditor::BakeHeightmap)),
		NAME_None,
		LOCTEXT("BakeHeightmap", "Bake"),
		LOCTEXT("BakeHeightmapTip", "Evaluate the graph and store the heightmap in this asset."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.BuildOptions"));
	Builder.EndSection();
}

void FArcHeightmapGraphEditor::EvaluatePreview()
{
	if (bPreviewEvaluating)
	{
		// Worker still running — CanEvaluatePreview disables the button, but guard re-entry defensively.
		return;
	}

	if (GraphView.IsValid())
	{
		GraphView->FlushToAsset();
	}

	if (EditedGraph == nullptr || !PreviewWidget.IsValid())
	{
		return;
	}

	// Snapshot the graph: the worker reads node data while the user may keep editing the live asset
	// on the game thread, so evaluate a transient duplicate (rooted via TStrongObjectPtr) instead.
	PreviewEvalGraph.Reset(DuplicateObject<UArcHeightmapGraph>(EditedGraph, GetTransientPackage()));
	if (!PreviewEvalGraph.IsValid())
	{
		return;
	}

	const FArcHeightmapEvalContext Ctx = FArcHeightmapEvalContext::FromCoverage(
		EditedGraph->PreviewCoverage, EditedGraph->PreviewCoverage, EditedGraph->DefaultResolution);

	bPreviewEvaluating = true;
	PreviewWidget->SetEvaluating(true);

	// Evaluate on a worker thread so the (seconds-long, erosion-heavy) sim never blocks Slate;
	// marshal the result back to the game thread to touch the preview UTexture and notifications.
	UArcHeightmapGraph* EvalGraph = PreviewEvalGraph.Get();
	TWeakPtr<FAssetEditorToolkit> WeakThis = AsShared();
	Async(EAsyncExecution::Thread, [WeakThis, EvalGraph, Ctx]()
	{
		TSharedRef<FArcHeightmapEvalResult> Result = MakeShared<FArcHeightmapEvalResult>();
		FString Err;
		const bool bOk = EvaluateGraph(*EvalGraph, Ctx, *Result, Err);

		AsyncTask(ENamedThreads::GameThread, [WeakThis, bOk, Result, Err]()
		{
			TSharedPtr<FAssetEditorToolkit> Pinned = WeakThis.Pin();
			if (!Pinned.IsValid())
			{
				return;
			}
			static_cast<FArcHeightmapGraphEditor*>(Pinned.Get())->OnPreviewEvalComplete(bOk, Result, Err);
		});
	});
}

void FArcHeightmapGraphEditor::OnPreviewEvalComplete(
	bool bOk, TSharedPtr<FArcHeightmapEvalResult> Result, FString Error)
{
	bPreviewEvaluating = false;
	PreviewEvalGraph.Reset();

	if (PreviewWidget.IsValid())
	{
		PreviewWidget->SetEvaluating(false);
	}

	if (bOk && PreviewWidget.IsValid() && Result.IsValid())
	{
		PreviewWidget->SetResult(*Result);

		FNotificationInfo Info(LOCTEXT("PreviewUpdated", "Preview updated."));
		Info.ExpireDuration = 4.0f;
		FSlateNotificationManager::Get().AddNotification(Info)
			->SetCompletionState(SNotificationItem::CS_Success);
	}
	else
	{
		FNotificationInfo Info(FText::Format(
			LOCTEXT("PreviewFailed", "Preview failed: {0}"),
			FText::FromString(Error)));
		Info.ExpireDuration = 4.0f;
		FSlateNotificationManager::Get().AddNotification(Info)
			->SetCompletionState(SNotificationItem::CS_Fail);
	}
}

void FArcHeightmapGraphEditor::BakeHeightmap()
{
	if (EditedGraph == nullptr)
	{
		return;
	}

	if (GraphView.IsValid())
	{
		GraphView->FlushToAsset();
	}

	// Bake == preview: same coverage square + resolution, evaluated synchronously on the game thread.
	const FArcHeightmapEvalContext Ctx = FArcHeightmapEvalContext::FromCoverage(
		EditedGraph->PreviewCoverage, EditedGraph->PreviewCoverage, EditedGraph->DefaultResolution);

	FArcHeightmapEvalResult EvalResult;
	FString EvalError;
	if (!EvaluateGraph(*EditedGraph, Ctx, EvalResult, EvalError))
	{
		FNotificationInfo Info(FText::Format(
			LOCTEXT("BakeHeightmapFailed", "Bake failed: {0}"),
			FText::FromString(EvalError)));
		Info.ExpireDuration = 4.0f;
		FSlateNotificationManager::Get().AddNotification(Info)
			->SetCompletionState(SNotificationItem::CS_Fail);
		return;
	}

	{
		FScopedTransaction Tx(LOCTEXT("BakeHeightmapTx", "Bake Heightmap"));
		EditedGraph->Modify();
		ArcHeightmapGraph::FromField(EvalResult.Height, EditedGraph->BakedField);
		EditedGraph->BakedLayers.Reset();
		for (const TPair<FName, UE::Geometry::FSampledScalarField2f>& LayerPair : EvalResult.Layers)
		{
			ArcHeightmapGraph::FromField(LayerPair.Value, EditedGraph->BakedLayers.Add(LayerPair.Key));
		}
		EditedGraph->BakedFieldId = FGuid::NewGuid();
		EditedGraph->MarkPackageDirty();
	}

	FNotificationInfo Info(FText::Format(
		LOCTEXT("BakeHeightmapDone", "Baked heightmap ({0}x{1})."),
		FText::AsNumber(EvalResult.Height.Width()),
		FText::AsNumber(EvalResult.Height.Height())));
	Info.ExpireDuration = 4.0f;
	FSlateNotificationManager::Get().AddNotification(Info)
		->SetCompletionState(SNotificationItem::CS_Success);
}

FName FArcHeightmapGraphEditor::GetToolkitFName() const
{
	return FName("ArcHeightmapGraphEditor");
}

FText FArcHeightmapGraphEditor::GetBaseToolkitName() const
{
	return LOCTEXT("BaseToolkitName", "Heightmap Graph Editor");
}

FString FArcHeightmapGraphEditor::GetWorldCentricTabPrefix() const
{
	return TEXT("HeightmapGraph ");
}

FLinearColor FArcHeightmapGraphEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.5f, 0.25f, 0.5f);
}

void FArcHeightmapGraphEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(
		LOCTEXT("WorkspaceMenu_HeightmapGraph", "Heightmap Graph Editor"));

	InTabManager->RegisterTabSpawner(GraphTabId,
		FOnSpawnTab::CreateLambda([this](const FSpawnTabArgs& Args) -> TSharedRef<SDockTab>
		{
			return SNew(SDockTab)
				.Label(LOCTEXT("GraphTabTitle", "Graph"))
				[
					GraphView.ToSharedRef()
				];
		}))
		.SetDisplayName(LOCTEXT("GraphTab", "Graph"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(DetailsTabId,
		FOnSpawnTab::CreateLambda([this](const FSpawnTabArgs& Args) -> TSharedRef<SDockTab>
		{
			// GraphView is SNew'd in InitHeightmapGraphEditor before InitAssetEditor spawns tabs,
			// so it is valid here — guard defensively against a future reorder.
			if (!GraphView.IsValid())
			{
				return SNew(SDockTab).Label(LOCTEXT("DetailsTabTitle", "Details"));
			}
			return SNew(SDockTab)
				.Label(LOCTEXT("DetailsTabTitle", "Details"))
				[
					GraphView->GetDetailsWidget()
				];
		}))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(PreviewTabId,
		FOnSpawnTab::CreateLambda([this](const FSpawnTabArgs& Args) -> TSharedRef<SDockTab>
		{
			return SNew(SDockTab)
				.Label(LOCTEXT("PreviewTabTitle", "Preview"))
				[
					PreviewWidget.ToSharedRef()
				];
		}))
		.SetDisplayName(LOCTEXT("PreviewTab", "Preview"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FArcHeightmapGraphEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(GraphTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	InTabManager->UnregisterTabSpawner(PreviewTabId);
}

void FArcHeightmapGraphEditor::SaveAsset_Execute()
{
	if (GraphView.IsValid())
	{
		GraphView->FlushToAsset();
	}
	FAssetEditorToolkit::SaveAsset_Execute();
}

#undef LOCTEXT_NAMESPACE
