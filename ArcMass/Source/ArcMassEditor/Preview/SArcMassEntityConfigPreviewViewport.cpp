// Copyright Lukasz Baran. All Rights Reserved.

#include "Preview/SArcMassEntityConfigPreviewViewport.h"
#include "Preview/ArcMassPreviewContributor.h"
#include "Preview/ArcCompositeMeshPreviewContributor.h"
#include "Preview/ArcNiagaraVisPreviewContributor.h"
#include "Preview/ArcPhysicsBodyTraitPreviewContributor.h"

#include "AdvancedPreviewScene.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "EditorViewportClient.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityTraitBase.h"
#include "SceneView.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"

#include "BaseGizmos/TransformProxy.h"
#include "BaseGizmos/GizmoElementHitTargets.h"
#include "EditorGizmos/TransformGizmo.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "ViewportInteractions/ViewportInteractionsBehaviorSource.h"
#include "ViewportInteractions/ViewportInteraction.h"
#include "ViewportInteractions/ViewportInteractionsUtilities.h"
#include "EditorGizmos/EditorTransformGizmoUtil.h"

// ---- Viewport Client (internal to this TU) ----

class FArcMassEntityConfigPreviewClient : public FEditorViewportClient
{
public:
	FArcMassEntityConfigPreviewClient(
		const TSharedRef<FAdvancedPreviewScene>& InPreviewScene,
		const TSharedRef<SArcMassEntityConfigPreviewViewport>& InViewport,
		TArray<TUniquePtr<IArcMassPreviewContributor>>* InContributors)
		: FEditorViewportClient(nullptr, &InPreviewScene.Get(), StaticCastSharedRef<SEditorViewport>(InViewport))
		, Contributors(InContributors)
		, ViewportWidget(InViewport)
	{
		SetViewMode(VMI_Lit);
		EngineShowFlags.SetCompositeEditorPrimitives(true);
		OverrideNearClipPlane(1.0f);

		DrawHelper.bDrawGrid = false;
		DrawHelper.bDrawPivot = false;
		DrawHelper.bDrawWorldBox = false;
		DrawHelper.bDrawKillZ = false;

		UModeManagerInteractiveToolsContext* ToolsContext = ModeTools->GetInteractiveToolsContext();
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		ToolsContext->OnBuildViewportInteractions().AddLambda(
			[](UViewportInteractionsBehaviorSource* InSource)
			{
				UE::Editor::ViewportInteractions::AddDefaultCameraMovementInteractions(InSource);
			});
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		SetRealtime(true);
	}

	virtual ~FArcMassEntityConfigPreviewClient() override
	{
		DestroyActiveGizmo();
	}

	virtual void Tick(float DeltaSeconds) override
	{
		if (!bITFInitialized && Viewport)
		{
			bITFInitialized = true;
			ModeTools->ReceivedFocus(this, Viewport);
			ModeTools->SetSupportsViewportITF(true);
			ModeTools->SetWidgetMode(UE::Widget::WM_Translate);
		}

		FEditorViewportClient::Tick(DeltaSeconds);
		PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	}

	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override
	{
		FEditorViewportClient::Draw(View, PDI);

		if (Contributors)
		{
			for (int32 ContribIdx = 0; ContribIdx < Contributors->Num(); ++ContribIdx)
			{
				(*Contributors)[ContribIdx]->Draw(PDI, ContribIdx);
			}
		}
	}

	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override
	{
		if (EventArgs.Event == IE_Pressed)
		{
			if (EventArgs.Key == EKeys::W)
			{
				SetWidgetMode(UE::Widget::WM_Translate);
				return true;
			}
			if (EventArgs.Key == EKeys::E)
			{
				SetWidgetMode(UE::Widget::WM_Rotate);
				return true;
			}
			if (EventArgs.Key == EKeys::R)
			{
				SetWidgetMode(UE::Widget::WM_Scale);
				return true;
			}
			if (EventArgs.Key == EKeys::Escape)
			{
				ClearSelection();
				Invalidate();
				return true;
			}
		}

		return FEditorViewportClient::InputKey(EventArgs);
	}

	virtual void ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override
	{
		if (!Contributors)
		{
			return;
		}

		if (HitProxy && HitProxy->IsA(HArcMassPreviewHitProxy::StaticGetType()))
		{
			HArcMassPreviewHitProxy* Proxy = static_cast<HArcMassPreviewHitProxy*>(HitProxy);
			UpdateSelectionHighlight(Proxy->ContributorIndex, Proxy->ElementIndex);
			SelectedContributorIndex = Proxy->ContributorIndex;
			SelectedElementIndex = Proxy->ElementIndex;
			CreateGizmoForSelection();
			Invalidate();
			return;
		}

		if (HitProxy && HitProxy->IsA(HActor::StaticGetType()))
		{
			HActor* ActorProxy = static_cast<HActor*>(HitProxy);
			for (int32 ContribIdx = 0; ContribIdx < Contributors->Num(); ++ContribIdx)
			{
				IArcMassPreviewContributor* Contributor = (*Contributors)[ContribIdx].Get();
				int32 PrimitiveCount = Contributor->GetPreviewPrimitiveCount();
				for (int32 PrimIdx = 0; PrimIdx < PrimitiveCount; ++PrimIdx)
				{
					UPrimitiveComponent* Primitive = Contributor->GetPreviewPrimitive(PrimIdx);
					if (Primitive && Primitive == ActorProxy->PrimComponent)
					{
						UpdateSelectionHighlight(ContribIdx, PrimIdx);
						SelectedContributorIndex = ContribIdx;
						SelectedElementIndex = PrimIdx;
						CreateGizmoForSelection();
						Invalidate();
						return;
					}
				}
			}
		}

		ClearSelection();
		Invalidate();
	}

	void RefreshGizmoAfterRebuild()
	{
		if (SelectedContributorIndex != INDEX_NONE)
		{
			CreateGizmoForSelection();
		}
	}

private:
	void CreateGizmoForSelection()
	{
		DestroyActiveGizmo();

		if (SelectedContributorIndex == INDEX_NONE || !Contributors)
		{
			return;
		}

		USceneComponent* DummyRoot = nullptr;
		TSharedPtr<SArcMassEntityConfigPreviewViewport> ViewportPtr = ViewportWidget.Pin();
		if (ViewportPtr.IsValid() && ViewportPtr->GetPreviewActor())
		{
			DummyRoot = ViewportPtr->GetPreviewActor()->GetRootComponent();
			if (!DummyRoot)
			{
				DummyRoot = NewObject<USceneComponent>(ViewportPtr->GetPreviewActor(), TEXT("GizmoProxyRoot"));
				DummyRoot->RegisterComponent();
				ViewportPtr->GetPreviewActor()->SetRootComponent(DummyRoot);
			}
		}

		if (!DummyRoot)
		{
			return;
		}

		int32 ContribIdx = SelectedContributorIndex;
		int32 ElemIdx = SelectedElementIndex;
		TArray<TUniquePtr<IArcMassPreviewContributor>>* ContribPtr = Contributors;

		ActiveProxy = NewObject<UTransformProxy>();
		ActiveProxy->AddComponentCustom(
			DummyRoot,
			[ContribPtr, ContribIdx, ElemIdx]() -> FTransform
			{
				return (*ContribPtr)[ContribIdx]->GetSelectableTransform(ElemIdx);
			},
			[ContribPtr, ContribIdx, ElemIdx](const FTransform& NewTransform)
			{
				(*ContribPtr)[ContribIdx]->SetSelectableTransform(ElemIdx, NewTransform);
			},
			ContribIdx,
			false);

		ActiveProxy->OnBeginTransformEdit.AddLambda(
			[ContribPtr, ContribIdx](UTransformProxy*)
			{
				UObject* TraitObj = (*ContribPtr)[ContribIdx]->GetMutableTraitObject();
				if (TraitObj)
				{
					TraitObj->Modify();
				}
			});

		TWeakPtr<SArcMassEntityConfigPreviewViewport> WeakViewport = ViewportWidget;
		ActiveProxy->OnEndTransformEdit.AddLambda(
			[WeakViewport](UTransformProxy*)
			{
				TSharedPtr<SArcMassEntityConfigPreviewViewport> VP = WeakViewport.Pin();
				if (VP.IsValid())
				{
					VP->RebuildPreview();
				}
			});

		UInteractiveToolManager* ToolManager = ModeTools->GetInteractiveToolsContext()->ToolManager;
		ActiveGizmo = UE::EditorTransformGizmoUtil::CreateTransformGizmo(
			ToolManager,
			FString::Printf(TEXT("ArcMassPreviewGizmo_%d_%d"), ContribIdx, ElemIdx),
			this);
		if (ActiveGizmo)
		{
			ActiveGizmo->TransformGizmoSource = nullptr;
			ActiveGizmo->SetActiveTarget(ActiveProxy, nullptr, ModeTools->GetGizmoStateTarget());
			ActiveGizmo->SetVisibility(true);

			if (UGizmoElementHitMultiTarget* HitMultiTarget = Cast<UGizmoElementHitMultiTarget>(ActiveGizmo->HitTarget))
			{
				HitMultiTarget->GizmoTransformProxy = ActiveProxy;
			}
		}
	}

	void DestroyActiveGizmo()
	{
		if (ActiveGizmo)
		{
			UInteractiveGizmoManager* GizmoManager = ModeTools->GetInteractiveToolsContext()->GizmoManager;
			if (GizmoManager)
			{
				GizmoManager->DestroyGizmo(ActiveGizmo);
			}
			ActiveGizmo = nullptr;
		}

		if (ActiveProxy)
		{
			ActiveProxy->OnBeginTransformEdit.Clear();
			ActiveProxy->OnEndTransformEdit.Clear();
			ActiveProxy = nullptr;
		}
	}

	void ClearSelection()
	{
		UpdateSelectionHighlight(INDEX_NONE, INDEX_NONE);
		SelectedContributorIndex = INDEX_NONE;
		SelectedElementIndex = INDEX_NONE;
		DestroyActiveGizmo();
	}

	void UpdateSelectionHighlight(int32 NewContributorIdx, int32 NewElementIdx)
	{
		if (SelectedContributorIndex != INDEX_NONE && Contributors)
		{
			(*Contributors)[SelectedContributorIndex]->SetSelectionHighlight(INDEX_NONE);
		}

		if (NewContributorIdx != INDEX_NONE && Contributors)
		{
			(*Contributors)[NewContributorIdx]->SetSelectionHighlight(NewElementIdx);
		}
	}

	TArray<TUniquePtr<IArcMassPreviewContributor>>* Contributors = nullptr;
	TWeakPtr<SArcMassEntityConfigPreviewViewport> ViewportWidget;

	int32 SelectedContributorIndex = INDEX_NONE;
	int32 SelectedElementIndex = INDEX_NONE;

	bool bITFInitialized = false;
	TObjectPtr<UTransformProxy> ActiveProxy = nullptr;
	TObjectPtr<UTransformGizmo> ActiveGizmo = nullptr;
};

// ---- Viewport Widget ----

void SArcMassEntityConfigPreviewViewport::Construct(const FArguments& InArgs)
{
	ConfigAsset = InArgs._ConfigAsset;

	FAdvancedPreviewScene::ConstructionValues SceneArgs;
	SceneArgs.bCreatePhysicsScene = false;
	PreviewScene = MakeShared<FAdvancedPreviewScene>(SceneArgs);
	PreviewScene->SetFloorVisibility(true);

	Contributors.Add(MakeUnique<FArcVisualizationPreviewContributor>());
	Contributors.Add(MakeUnique<FArcPhysicsCollisionPreviewContributor>());
	Contributors.Add(MakeUnique<FArcMeshVisualizationPreviewContributor>());
	Contributors.Add(MakeUnique<FArcCompositeMeshPreviewContributor>());
	Contributors.Add(MakeUnique<FArcPhysicsBodyTraitPreviewContributor>());
	Contributors.Add(MakeUnique<FArcNiagaraVisPreviewContributor>());

	SEditorViewport::Construct(SEditorViewport::FArguments());

	GEditor->RegisterForUndo(static_cast<FEditorUndoClient*>(this));

	ObjectPropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(
		this, &SArcMassEntityConfigPreviewViewport::OnObjectPropertyChanged);

	RebuildPreview();
	FocusCameraOnPreview();
}

SArcMassEntityConfigPreviewViewport::~SArcMassEntityConfigPreviewViewport()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(ObjectPropertyChangedHandle);
	GEditor->UnregisterForUndo(static_cast<FEditorUndoClient*>(this));

	if (PreviewActor)
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}

	if (ViewportClient.IsValid())
	{
		ViewportClient->Viewport = nullptr;
	}

	// Release the viewport client before PreviewScene member destructor tears
	// down the preview world. ModeTools (owned by the client) must be destroyed
	// while the world is still alive, otherwise ~FEditorModeTools hits
	// check(PendingDeactivateModes.Num() == 0).
	ViewportClient.Reset();
	Client.Reset();
}

void SArcMassEntityConfigPreviewViewport::RebuildPreview()
{
	if (!ConfigAsset || !PreviewScene.IsValid())
	{
		return;
	}

	for (TUniquePtr<IArcMassPreviewContributor>& Contributor : Contributors)
	{
		Contributor->Clear(*PreviewScene);
	}

	if (PreviewActor)
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}

	UWorld* PreviewWorld = PreviewScene->GetWorld();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(PreviewWorld->GetCurrentLevel(), AActor::StaticClass(), TEXT("ArcMassPreviewActor"));
	SpawnParams.ObjectFlags = RF_Transient;
	PreviewActor = PreviewWorld->SpawnActor<AActor>(SpawnParams);

	TConstArrayView<UMassEntityTraitBase*> Traits = ConfigAsset->GetConfig().GetTraits();
	for (UMassEntityTraitBase* Trait : Traits)
	{
		if (!Trait)
		{
			continue;
		}

		for (TUniquePtr<IArcMassPreviewContributor>& Contributor : Contributors)
		{
			if (Contributor->CanContribute(Trait))
			{
				Contributor->Apply(Trait, *PreviewScene, PreviewActor);
			}
		}
	}

	if (ViewportClient.IsValid())
	{
		ViewportClient->RefreshGizmoAfterRebuild();
	}
}

void SArcMassEntityConfigPreviewViewport::FocusCameraOnPreview()
{
	if (!ViewportClient.IsValid())
	{
		return;
	}

	FBox CombinedBox(ForceInit);

	for (const TUniquePtr<IArcMassPreviewContributor>& Contributor : Contributors)
	{
		FBox ContributorBox = Contributor->GetBounds();
		if (ContributorBox.IsValid)
		{
			CombinedBox += ContributorBox;
		}
	}

	FBoxSphereBounds CombinedBounds(CombinedBox);

	if (CombinedBounds.SphereRadius > KINDA_SMALL_NUMBER)
	{
		ViewportClient->FocusViewportOnBox(CombinedBounds.GetBox());
	}
}

void SArcMassEntityConfigPreviewViewport::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(ConfigAsset);
	Collector.AddReferencedObject(PreviewActor);
}

TSharedRef<SEditorViewport> SArcMassEntityConfigPreviewViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SArcMassEntityConfigPreviewViewport::GetExtenders() const
{
	return MakeShared<FExtender>();
}

void SArcMassEntityConfigPreviewViewport::OnFloatingButtonClicked()
{
}

TSharedRef<FEditorViewportClient> SArcMassEntityConfigPreviewViewport::MakeEditorViewportClient()
{
	ViewportClient = MakeShareable(new FArcMassEntityConfigPreviewClient(
		PreviewScene.ToSharedRef(),
		SharedThis(this),
		&Contributors));

	return ViewportClient.ToSharedRef();
}

void SArcMassEntityConfigPreviewViewport::OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (!ConfigAsset || !Object)
	{
		return;
	}

	UMassEntityTraitBase* ChangedTrait = Cast<UMassEntityTraitBase>(Object);
	if (ChangedTrait == nullptr)
	{
		return;
	}

	TConstArrayView<UMassEntityTraitBase*> Traits = ConfigAsset->GetConfig().GetTraits();
	for (const UMassEntityTraitBase* Trait : Traits)
	{
		if (Trait == ChangedTrait)
		{
			RebuildPreview();
			return;
		}
	}
}

TSharedPtr<SWidget> SArcMassEntityConfigPreviewViewport::BuildViewportToolbar()
{
	return SNew(SCommonEditorViewportToolbarBase, SharedThis(this));
}
