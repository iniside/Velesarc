// Copyright Lukasz Baran. All Rights Reserved.

#include "SArcTQSPreviewViewport.h"

#include "AdvancedPreviewScene.h"
#include "ArcMass/Spatial/ArcMassSpatialHashSubsystem.h"
#include "ArcTQSEditorUtils.h"
#include "ArcTQSPreviewTypes.h"
#include "ArcTQSQueryDefinition.h"
#include "ArcTQSRenderingComponent.h"
#include "BaseGizmos/GizmoElementHitTargets.h"
#include "BaseGizmos/TransformProxy.h"
#include "EditorGizmos/EditorTransformGizmoUtil.h"
#include "EditorGizmos/TransformGizmo.h"
#include "EditorModeManager.h"
#include "EditorViewportClient.h"
#include "Engine/World.h"
#include "HitProxies.h"
#include "InteractiveGizmoManager.h"
#include "MassEntitySubsystem.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "ViewportInteractions/ViewportInteractionsBehaviorSource.h"
#include "ViewportInteractions/ViewportInteractionsUtilities.h"

namespace ArcTQSPreviewViewportPrivate
{
	constexpr float MockRadius = 30.0f;
	constexpr float HighlightRadius = 45.0f;
	constexpr float FocusPadding = 800.0f;

	const FLinearColor QuerierColor(FLinearColor::White);
	const FLinearColor TargetColor(1.0f, 0.0f, 1.0f);
	const FLinearColor SelectedColor(1.0f, 0.9f, 0.1f);
	const FLinearColor HighlightColor(0.1f, 0.9f, 1.0f);

	constexpr EArcTQSWorldRequirements UnavailableRequirements =
		EArcTQSWorldRequirements::Navigation
		| EArcTQSWorldRequirements::Collision
		| EArcTQSWorldRequirements::SmartObjects
		| EArcTQSWorldRequirements::Areas
		| EArcTQSWorldRequirements::Knowledge
		| EArcTQSWorldRequirements::Perception
		| EArcTQSWorldRequirements::Actors
		| EArcTQSWorldRequirements::WorldConditions;

	FString GetMockLabel(const FArcTQSPreviewMock& Mock, const int32 MockIndex)
	{
		if (!Mock.Name.IsNone())
		{
			return Mock.Name.ToString();
		}
		return MockIndex == 0 ? FString(TEXT("Querier")) : FString::Printf(TEXT("Target %d"), MockIndex - 1);
	}
}

struct HArcTQSMockHitProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	int32 MockIndex = INDEX_NONE;

	explicit HArcTQSMockHitProxy(const int32 InMockIndex)
		: HHitProxy(HPP_UI)
		, MockIndex(InMockIndex)
	{
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}
};

IMPLEMENT_HIT_PROXY(HArcTQSMockHitProxy, HHitProxy);

class FArcTQSPreviewClient : public FEditorViewportClient
{
public:
	FArcTQSPreviewClient(
		const TSharedRef<FAdvancedPreviewScene>& InPreviewScene,
		const TSharedRef<SArcTQSPreviewViewport>& InViewport)
		: FEditorViewportClient(nullptr, &InPreviewScene.Get(), StaticCastSharedRef<SEditorViewport>(InViewport))
		, ViewportWidget(InViewport)
	{
		SetViewMode(VMI_Lit);
		EngineShowFlags.SetCompositeEditorPrimitives(true);
		EngineShowFlags.SetDebugAI(true);
		OverrideNearClipPlane(1.0f);

		DrawHelper.bDrawGrid = true;
		DrawHelper.bDrawPivot = false;
		DrawHelper.bDrawWorldBox = false;
		DrawHelper.bDrawKillZ = false;

		UModeManagerInteractiveToolsContext* ToolsContext = ModeTools->GetInteractiveToolsContext();
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		ToolsContext->OnBuildViewportInteractions().AddLambda(
			[](UViewportInteractionsBehaviorSource* InSource) -> void
			{
				UE::Editor::ViewportInteractions::AddDefaultCameraMovementInteractions(InSource);
			});
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		SetRealtime(true);
		SetViewLocation(FVector(-1200.0, 0.0, 900.0));
		SetViewRotation(FRotator(-35.0, 0.0, 0.0));
	}

	virtual ~FArcTQSPreviewClient() override
	{
		DestroyGizmo();
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
		using namespace ArcTQSPreviewViewportPrivate;

		FEditorViewportClient::Draw(View, PDI);

		const TSharedPtr<SArcTQSPreviewViewport> PreviewWidget = ViewportWidget.Pin();
		if (!PreviewWidget.IsValid() || PDI == nullptr)
		{
			return;
		}

		const int32 MockCount = PreviewWidget->GetMockCount();
		for (int32 MockIndex = 0; MockIndex < MockCount; ++MockIndex)
		{
			const FArcTQSPreviewMock* Mock = PreviewWidget->GetMock(MockIndex);
			const FVector Location = Mock->Transform.GetLocation();
			const FLinearColor Color = MockIndex == SelectedMock ? SelectedColor : (MockIndex == 0 ? QuerierColor : TargetColor);

			PDI->SetHitProxy(new HArcTQSMockHitProxy(MockIndex));
			DrawWireSphere(PDI, Location, Color, MockRadius, 16, SDPG_World, 2.0f);
			PDI->DrawPoint(Location, Color, 16.0f, SDPG_World);
			if (MockIndex == 0)
			{
				DrawDirectionalArrow(
					PDI,
					FQuatRotationTranslationMatrix(Mock->Transform.GetRotation(), Location),
					Color,
					120.0f,
					12.0f,
					SDPG_World,
					2.0f);
			}
			PDI->SetHitProxy(nullptr);
		}

		const FArcTQSQueryResult& Result = PreviewWidget->GetResult();
		const int32 HighlightedItem = PreviewWidget->GetHighlightedItem();
		if (Result.Items.IsValidIndex(HighlightedItem))
		{
			const FVector ItemLocation = Result.Items[HighlightedItem].Location;
			DrawWireSphere(PDI, ItemLocation, HighlightColor, HighlightRadius, 24, SDPG_Foreground, 3.0f);
			PDI->DrawLine(Result.QuerierLocation, ItemLocation, HighlightColor, SDPG_Foreground, 1.5f);
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
		if (HitProxy && HitProxy->IsA(HArcTQSMockHitProxy::StaticGetType()))
		{
			SelectedMock = static_cast<HArcTQSMockHitProxy*>(HitProxy)->MockIndex;
			CreateGizmo();
			Invalidate();
			return;
		}

		ClearSelection();
		Invalidate();
	}

	void RefreshGizmo()
	{
		const TSharedPtr<SArcTQSPreviewViewport> PreviewWidget = ViewportWidget.Pin();
		if (SelectedMock == INDEX_NONE)
		{
			return;
		}
		if (!PreviewWidget.IsValid() || SelectedMock >= PreviewWidget->GetMockCount())
		{
			ClearSelection();
			return;
		}
		CreateGizmo();
	}

private:
	void CreateGizmo()
	{
		DestroyGizmo();

		if (SelectedMock == INDEX_NONE)
		{
			return;
		}

		const TWeakPtr<SArcTQSPreviewViewport> WeakWidget = ViewportWidget;
		const int32 MockIndex = SelectedMock;

		ActiveProxy = NewObject<UTransformProxy>();
		ActiveProxy->AddCustom(
			[WeakWidget, MockIndex]() -> FTransform
			{
				const TSharedPtr<SArcTQSPreviewViewport> PreviewWidget = WeakWidget.Pin();
				const FArcTQSPreviewMock* Mock = PreviewWidget.IsValid() ? PreviewWidget->GetMock(MockIndex) : nullptr;
				return Mock ? Mock->Transform : FTransform::Identity;
			},
			[WeakWidget, MockIndex](const FTransform& NewTransform) -> void
			{
				if (const TSharedPtr<SArcTQSPreviewViewport> PreviewWidget = WeakWidget.Pin())
				{
					PreviewWidget->SetMockTransform(MockIndex, NewTransform);
				}
			},
			[WeakWidget, MockIndex]() -> bool
			{
				const TSharedPtr<SArcTQSPreviewViewport> PreviewWidget = WeakWidget.Pin();
				return PreviewWidget.IsValid() && PreviewWidget->GetMock(MockIndex) != nullptr;
			},
			[WeakWidget]() -> void
			{
				const TSharedPtr<SArcTQSPreviewViewport> PreviewWidget = WeakWidget.Pin();
				if (UArcTQSQueryDefinition* Definition = PreviewWidget.IsValid() ? PreviewWidget->GetDefinition() : nullptr)
				{
					Definition->Modify();
				}
			});

		ActiveProxy->OnBeginTransformEdit.AddLambda([WeakWidget](UTransformProxy*) -> void
		{
			if (const TSharedPtr<SArcTQSPreviewViewport> PreviewWidget = WeakWidget.Pin())
			{
				PreviewWidget->BeginMockTransform();
			}
		});
		ActiveProxy->OnEndTransformEdit.AddLambda([WeakWidget](UTransformProxy*) -> void
		{
			if (const TSharedPtr<SArcTQSPreviewViewport> PreviewWidget = WeakWidget.Pin())
			{
				PreviewWidget->EndMockTransform();
			}
		});

		UInteractiveToolManager* ToolManager = ModeTools->GetInteractiveToolsContext()->ToolManager;
		ActiveGizmo = UE::EditorTransformGizmoUtil::CreateTransformGizmo(
			ToolManager,
			FString::Printf(TEXT("ArcTQSPreviewGizmo_%d"), MockIndex),
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

	void DestroyGizmo()
	{
		if (ActiveGizmo)
		{
			if (UInteractiveGizmoManager* GizmoManager = ModeTools->GetInteractiveToolsContext()->GizmoManager)
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

		if (const TSharedPtr<SArcTQSPreviewViewport> PreviewWidget = ViewportWidget.Pin())
		{
			PreviewWidget->EndMockTransform();
		}
	}

	void ClearSelection()
	{
		SelectedMock = INDEX_NONE;
		DestroyGizmo();
	}

	TWeakPtr<SArcTQSPreviewViewport> ViewportWidget;
	int32 SelectedMock = INDEX_NONE;
	bool bITFInitialized = false;
	TObjectPtr<UTransformProxy> ActiveProxy = nullptr;
	TObjectPtr<UTransformGizmo> ActiveGizmo = nullptr;
};

void SArcTQSPreviewViewport::Construct(const FArguments& InArgs)
{
	Definition = InArgs._Definition;
	OnResultChanged = InArgs._OnResultChanged;

	{
		UMassEntitySubsystem::FScopedAllowCreationForWorld AllowEntitySubsystem;
		UArcMassSpatialHashSubsystem::FScopedAllowCreationForWorld AllowSpatialHash;
		FAdvancedPreviewScene::ConstructionValues SceneArgs;
		SceneArgs.bCreatePhysicsScene = false;
		PreviewScene = MakeShared<FAdvancedPreviewScene>(SceneArgs);
	}
	PreviewScene->SetFloorVisibility(true);

	RenderingComponent = NewObject<UArcTQSRenderingComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	PreviewScene->AddComponent(RenderingComponent, FTransform::Identity);

	SEditorViewport::Construct(SEditorViewport::FArguments());

	if (Definition && Definition->PreviewSetup.Reference.GetDefinition() != Definition)
	{
		Definition->PreviewSetup.Reference.SetDefinition(Definition);
	}

	PostCompileHandle = ArcTQS::Delegates::OnPostCompile.AddSP(this, &SArcTQSPreviewViewport::HandleDefinitionChanged);
	PreviewSetupChangedHandle = ArcTQS::Delegates::OnPreviewSetupChanged.AddSP(this, &SArcTQSPreviewViewport::HandleDefinitionChanged);

	RebuildMocks();
	RunQuery();
	FocusCameraOnMocks();
}

SArcTQSPreviewViewport::~SArcTQSPreviewViewport()
{
	ArcTQS::Delegates::OnPostCompile.Remove(PostCompileHandle);
	ArcTQS::Delegates::OnPreviewSetupChanged.Remove(PreviewSetupChangedHandle);

	PreviewEntities.Release();
	MockEntities.Reset();

	if (PreviewScene.IsValid() && RenderingComponent)
	{
		PreviewScene->RemoveComponent(RenderingComponent);
	}
	RenderingComponent = nullptr;

	if (ViewportClient.IsValid())
	{
		ViewportClient->Viewport = nullptr;
	}

	ViewportClient.Reset();
	Client.Reset();
}

void SArcTQSPreviewViewport::RebuildMocks()
{
	PreviewEntities.Release();
	MockEntities.Reset();

	if (!Definition || !PreviewScene.IsValid())
	{
		return;
	}

	UWorld* PreviewWorld = PreviewScene->GetWorld();
	const FArcTQSPreviewSetup& Setup = Definition->PreviewSetup;
	MockEntities.Add(PreviewEntities.Create(*PreviewWorld, Setup.Querier));
	for (const FArcTQSPreviewMock& Target : Setup.Targets)
	{
		MockEntities.Add(PreviewEntities.Create(*PreviewWorld, Target));
	}

	if (ViewportClient.IsValid())
	{
		ViewportClient->RefreshGizmo();
	}
}

void SArcTQSPreviewViewport::RunQuery()
{
	if (!Definition || !PreviewScene.IsValid())
	{
		return;
	}

	const FArcTQSPreviewSetup& Setup = Definition->PreviewSetup;
	UWorld* PreviewWorld = PreviewScene->GetWorld();

	TMap<FGuid, FMassEntityHandle> EntityParameters;
	for (const TPair<FGuid, FName>& Source : Setup.EntityParameterSources)
	{
		const FMassEntityHandle Entity = PreviewEntities.FindByName(Source.Value);
		if (Entity.IsSet())
		{
			EntityParameters.Add(Source.Key, Entity);
		}
	}
	const FArcTQSQueryReference RunReference = ArcTQS::MakeReferenceWithEntityParameters(Setup.Reference, EntityParameters);

	FArcTQSQueryContext Context;
	Context.QuerierEntity = MockEntities.IsValidIndex(0) ? MockEntities[0] : FMassEntityHandle();
	Context.QuerierLocation = Setup.Querier.Transform.GetLocation();
	Context.QuerierForward = Setup.Querier.Transform.GetRotation().GetForwardVector();
	Context.World = PreviewWorld;
	if (UMassEntitySubsystem* EntitySubsystem = PreviewWorld->GetSubsystem<UMassEntitySubsystem>())
	{
		Context.EntityManager = &EntitySubsystem->GetMutableEntityManager();
	}

	FArcTQSRunOptions Options;
	Options.Seed = Setup.Seed;
	Options.Unavailable = ArcTQSPreviewViewportPrivate::UnavailableRequirements;

	Failure = ArcTQS::RunQuerySync(RunReference, Context, Options, Result);
	if (Failure != EArcTQSCreateFailure::None)
	{
		Result.QuerierLocation = Context.QuerierLocation;
	}

	UpdateRendering();
	OnResultChanged.ExecuteIfBound();
}

void SArcTQSPreviewViewport::SetHighlightedItem(const int32 ItemIndex)
{
	HighlightedItem = ItemIndex;
	if (ViewportClient.IsValid())
	{
		ViewportClient->Invalidate();
	}
}

int32 SArcTQSPreviewViewport::GetMockCount() const
{
	return Definition ? 1 + Definition->PreviewSetup.Targets.Num() : 0;
}

const FArcTQSPreviewMock* SArcTQSPreviewViewport::GetMock(const int32 MockIndex) const
{
	if (!Definition)
	{
		return nullptr;
	}
	if (MockIndex == 0)
	{
		return &Definition->PreviewSetup.Querier;
	}
	const TArray<FArcTQSPreviewMock>& Targets = Definition->PreviewSetup.Targets;
	return Targets.IsValidIndex(MockIndex - 1) ? &Targets[MockIndex - 1] : nullptr;
}

FArcTQSPreviewMock* SArcTQSPreviewViewport::GetMutableMock(const int32 MockIndex)
{
	return const_cast<FArcTQSPreviewMock*>(GetMock(MockIndex));
}

void SArcTQSPreviewViewport::BeginMockTransform()
{
	bTransformingMock = true;
}

void SArcTQSPreviewViewport::SetMockTransform(const int32 MockIndex, const FTransform& Transform)
{
	FArcTQSPreviewMock* Mock = GetMutableMock(MockIndex);
	if (!Mock)
	{
		return;
	}

	Mock->Transform = Transform;
	if (MockEntities.IsValidIndex(MockIndex))
	{
		PreviewEntities.Move(MockEntities[MockIndex], Transform);
	}
	RunQuery();
}

void SArcTQSPreviewViewport::EndMockTransform()
{
	if (!bTransformingMock)
	{
		return;
	}
	bTransformingMock = false;
	if (Definition)
	{
		ArcTQSEditor::NotifyPreviewSetupChanged(*Definition);
	}
}

void SArcTQSPreviewViewport::HandleDefinitionChanged(const UArcTQSQueryDefinition& ChangedDefinition)
{
	if (&ChangedDefinition != Definition || bTransformingMock)
	{
		return;
	}
	RebuildMocks();
	RunQuery();
}

void SArcTQSPreviewViewport::UpdateRendering()
{
	if (!RenderingComponent)
	{
		return;
	}

	TArray<FArcTQSRenderMarker> Markers;
	const int32 MockCount = GetMockCount();
	Markers.Reserve(MockCount);
	for (int32 MockIndex = 0; MockIndex < MockCount; ++MockIndex)
	{
		const FArcTQSPreviewMock* Mock = GetMock(MockIndex);
		Markers.Add({ArcTQSPreviewViewportPrivate::GetMockLabel(*Mock, MockIndex), Mock->Transform.GetLocation()});
	}
	RenderingComponent->UpdateQueryData(Result, Markers, true, true);
}

void SArcTQSPreviewViewport::FocusCameraOnMocks()
{
	if (!ViewportClient.IsValid())
	{
		return;
	}

	FBox FocusBox(ForceInit);
	const int32 MockCount = GetMockCount();
	for (int32 MockIndex = 0; MockIndex < MockCount; ++MockIndex)
	{
		FocusBox += GetMock(MockIndex)->Transform.GetLocation();
	}
	if (FocusBox.IsValid)
	{
		ViewportClient->FocusViewportOnBox(FocusBox.ExpandBy(ArcTQSPreviewViewportPrivate::FocusPadding), true);
	}
}

void SArcTQSPreviewViewport::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Definition);
	Collector.AddReferencedObject(RenderingComponent);
}

TSharedRef<SEditorViewport> SArcTQSPreviewViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SArcTQSPreviewViewport::GetExtenders() const
{
	return MakeShared<FExtender>();
}

void SArcTQSPreviewViewport::OnFloatingButtonClicked()
{
}

TSharedRef<FEditorViewportClient> SArcTQSPreviewViewport::MakeEditorViewportClient()
{
	ViewportClient = MakeShareable(new FArcTQSPreviewClient(PreviewScene.ToSharedRef(), SharedThis(this)));
	return ViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SArcTQSPreviewViewport::BuildViewportToolbar()
{
	return SNew(SCommonEditorViewportToolbarBase, SharedThis(this));
}
