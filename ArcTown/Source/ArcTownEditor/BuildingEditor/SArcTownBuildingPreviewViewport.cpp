// Copyright Lukasz Baran. All Rights Reserved.

#include "BuildingEditor/SArcTownBuildingPreviewViewport.h"

#include "AdvancedPreviewScene.h"
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "ArcMass/Visualization/ArcUnifiedMeshVisualizationTrait.h"
#include "Catalog/ArcTownBuildingDefinition.h"
#include "Catalog/ArcTownBuildingVisual.h"
#include "Components/StaticMeshComponent.h"
#include "EditorModeManager.h"
#include "EditorViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"
#include "PrimitiveDrawingUtils.h"
#include "SceneView.h"
#include "Styling/AppStyle.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "UObject/UObjectGlobals.h"
#include "ViewportInteractions/ViewportInteractionsBehaviorSource.h"
#include "ViewportInteractions/ViewportInteractionsUtilities.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SArcTownBuildingPreviewViewport"

DEFINE_LOG_CATEGORY_STATIC(LogArcTownBuildingPreview, Log, All);

namespace ArcTownBuildingPreviewViewportLocal
{
	constexpr float LineThickness = 2.0f;
	constexpr int32 CircleSides = 64;
	constexpr float ArrowLengthFractionOfFootprint = 0.75f;
	constexpr float ArrowHeadFractionOfLength = 0.1f;
	constexpr double AuthoredDoorMarkerRadiusCm = 15.0;
	constexpr int32 AuthoredDoorMarkerSides = 16;
	constexpr double LegendSwatchExtent = 12.0;

	const FLinearColor VisualBoundsColor(1.0f, 0.2f, 0.9f);
	const FLinearColor FootprintColor(FLinearColor::White);
	const FLinearColor PlotMinColor(0.1f, 0.9f, 0.1f);
	const FLinearColor PlotMaxColor(0.0f, 0.35f, 0.0f);
	const FLinearColor InfluenceColor(1.0f, 0.9f, 0.1f);
	const FLinearColor DenyColor(0.9f, 0.1f, 0.1f);
	const FLinearColor DoorColor(0.2f, 0.45f, 1.0f);

	FBox MakeCenteredBox(const FVector2D& SizeXY, const double MinZ, const double MaxZ)
	{
		const double HalfX = 0.5 * FMath::Abs(SizeXY.X);
		const double HalfY = 0.5 * FMath::Abs(SizeXY.Y);
		return FBox(FVector(-HalfX, -HalfY, MinZ), FVector(HalfX, HalfY, MaxZ));
	}

	TSharedRef<SWidget> MakeLegendRow(const FLinearColor& SwatchColor, const FText& Label)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SColorBlock)
				.Color(SwatchColor)
				.AlphaDisplayMode(EColorBlockAlphaDisplayMode::Ignore)
				.Size(FVector2D(LegendSwatchExtent, LegendSwatchExtent))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Label)
			];
	}

	void ApplyMaterialOverrides(UStaticMeshComponent& Component, const TArray<TSoftObjectPtr<UMaterialInterface>>& Overrides)
	{
		for (int32 Index = 0; Index < Overrides.Num(); ++Index)
		{
			UMaterialInterface* const Material = Overrides[Index].LoadSynchronous();
			if (Material != nullptr)
			{
				Component.SetMaterial(Index, Material);
			}
		}
	}

	const UArcUnifiedMeshVisualizationTrait* FindUnifiedMeshTrait(const FSoftObjectPath& ConfigPath)
	{
		const UArcMassEntityConfigAsset* const Config = Cast<UArcMassEntityConfigAsset>(ConfigPath.TryLoad());
		if (Config == nullptr)
		{
			return nullptr;
		}

		return Cast<UArcUnifiedMeshVisualizationTrait>(
			Config->FindTrait(UArcUnifiedMeshVisualizationTrait::StaticClass()));
	}

	FVector MeshOffset(const UArcTownBuildingDefinition& Definition)
	{
		// The emitter seats the pivot FloorHeightAbovePivot below the sampled ground, so z=0 here is the
		// building's floor and the plane every drawn bound lies on.
		return FVector(0.0, 0.0, -Definition.FloorHeightAbovePivot);
	}
}

class FArcTownBuildingPreviewClient : public FEditorViewportClient
{
public:
	FArcTownBuildingPreviewClient(
		const TSharedRef<FAdvancedPreviewScene>& InPreviewScene,
		const TSharedRef<SArcTownBuildingPreviewViewport>& InViewport,
		const UArcTownBuildingDefinition* InDefinition)
		: FEditorViewportClient(nullptr, &InPreviewScene.Get(), StaticCastSharedRef<SEditorViewport>(InViewport))
		, Definition(InDefinition)
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
			[](UViewportInteractionsBehaviorSource* InSource) -> void
			{
				UE::Editor::ViewportInteractions::AddDefaultCameraMovementInteractions(InSource);
			});
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		SetRealtime(true);
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

		const UArcTownBuildingDefinition* const EditedDefinition = Definition.Get();
		if (EditedDefinition == nullptr || PDI == nullptr)
		{
			return;
		}

		using namespace ArcTownBuildingPreviewViewportLocal;

		double FootprintTopZ = 0.0;
		if (VisualBounds.IsValid != 0)
		{
			DrawWireBox(PDI, VisualBounds, VisualBoundsColor, SDPG_Foreground, LineThickness);
			FootprintTopZ = FMath::Max(VisualBounds.Max.Z, 0.0);
		}

		DrawWireBox(
			PDI,
			MakeCenteredBox(EditedDefinition->FootprintXY, 0.0, FootprintTopZ),
			FootprintColor,
			SDPG_Foreground,
			LineThickness);

		FVector2D PlotSizeMinXY = FVector2D::ZeroVector;
		FVector2D PlotSizeMaxXY = FVector2D::ZeroVector;
		ArcTownBuildingDefinition::ResolvePlotSize(*EditedDefinition, PlotSizeMinXY, PlotSizeMaxXY);

		DrawWireBox(PDI, MakeCenteredBox(PlotSizeMinXY, 0.0, 0.0), PlotMinColor, SDPG_Foreground, LineThickness);
		DrawWireBox(PDI, MakeCenteredBox(PlotSizeMaxXY, 0.0, 0.0), PlotMaxColor, SDPG_Foreground, LineThickness);

		DrawCircle(
			PDI,
			FVector::ZeroVector,
			FVector::XAxisVector,
			FVector::YAxisVector,
			InfluenceColor,
			EditedDefinition->InfluenceRadius,
			CircleSides,
			SDPG_Foreground,
			LineThickness);

		if (EditedDefinition->DenyMinDistance > 0.0)
		{
			DrawCircle(
				PDI,
				FVector::ZeroVector,
				FVector::XAxisVector,
				FVector::YAxisVector,
				DenyColor,
				EditedDefinition->DenyMinDistance,
				CircleSides,
				SDPG_Foreground,
				LineThickness);
		}

		if (EditedDefinition->bOverrideDoorLocation)
		{
			DrawCircle(
				PDI,
				FVector(EditedDefinition->DoorLocationXY.X, EditedDefinition->DoorLocationXY.Y, 0.0),
				FVector::XAxisVector,
				FVector::YAxisVector,
				DoorColor,
				AuthoredDoorMarkerRadiusCm,
				AuthoredDoorMarkerSides,
				SDPG_Foreground,
				LineThickness);
		}

		if (bHasDoorForward)
		{
			const float ArrowLength = ArrowLengthFractionOfFootprint
				* static_cast<float>(EditedDefinition->FootprintXY.GetMax());

			const FVector ArrowOrigin = EditedDefinition->bOverrideDoorLocation
				? FVector(EditedDefinition->DoorLocationXY.X, EditedDefinition->DoorLocationXY.Y, 0.0)
				: FVector::ZeroVector;

			DrawDirectionalArrow(
				PDI,
				FRotationMatrix::MakeFromX(FVector(DoorForwardXY.X, DoorForwardXY.Y, 0.0))
					.ConcatTranslation(ArrowOrigin),
				DoorColor,
				ArrowLength,
				ArrowHeadFractionOfLength * ArrowLength,
				SDPG_Foreground,
				LineThickness);
		}
	}

	void SetDoorForward(const FVector2D& InDoorForwardXY)
	{
		DoorForwardXY = InDoorForwardXY;
		bHasDoorForward = true;
	}

	void ClearDoorForward()
	{
		bHasDoorForward = false;
	}

	void SetVisualBounds(const FBox& InVisualBounds)
	{
		VisualBounds = InVisualBounds;
	}

	void ClearVisualBounds()
	{
		VisualBounds = FBox(ForceInit);
	}

private:
	TWeakObjectPtr<const UArcTownBuildingDefinition> Definition;
	FBox VisualBounds = FBox(ForceInit);
	FVector2D DoorForwardXY = FVector2D(0.0, -1.0);
	bool bHasDoorForward = false;
	bool bITFInitialized = false;
};

void SArcTownBuildingPreviewViewport::Construct(const FArguments& InArgs)
{
	Definition = InArgs._Definition;

	FAdvancedPreviewScene::ConstructionValues SceneArgs;
	SceneArgs.bCreatePhysicsScene = false;
	PreviewScene = MakeShared<FAdvancedPreviewScene>(SceneArgs);

	// The scene floor is left visible: unlike the species preview, here it IS the ground plane the drawn
	// bounds describe.

	SEditorViewport::Construct(SEditorViewport::FArguments());

	ObjectPropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(
		this, &SArcTownBuildingPreviewViewport::HandleObjectPropertyChanged);

	RebuildPreview();
	FocusCameraOnPreview();
}

SArcTownBuildingPreviewViewport::~SArcTownBuildingPreviewViewport()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(ObjectPropertyChangedHandle);

	ClearPreviewComponents();

	if (PreviewActor)
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}

	if (ViewportClient.IsValid())
	{
		ViewportClient->Viewport = nullptr;
	}

	ViewportClient.Reset();
	Client.Reset();
}

void SArcTownBuildingPreviewViewport::RebuildPreview()
{
	if (!PreviewScene.IsValid())
	{
		return;
	}

	ClearPreviewComponents();

	if (PreviewActor)
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}

	PreviewBounds = FBox(ForceInit);

	if (ViewportClient.IsValid())
	{
		ViewportClient->ClearDoorForward();
		ViewportClient->ClearVisualBounds();
	}

	if (!Definition)
	{
		return;
	}

	const FArcTownBuildingVisual* const Visual = Definition->Visual.GetPtr();
	if (Visual == nullptr)
	{
		return;
	}

	UWorld* const PreviewWorld = PreviewScene->GetWorld();
	const FTransform PreviewTransform(ArcTownBuildingPreviewViewportLocal::MeshOffset(*Definition));

	if (const FArcTownBuildingVisual_StaticMesh* const StaticMeshVisual =
		Definition->Visual.GetPtr<FArcTownBuildingVisual_StaticMesh>())
	{
		UStaticMesh* const Mesh = StaticMeshVisual->Mesh.LoadSynchronous();
		if (Mesh == nullptr)
		{
			return;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = MakeUniqueObjectName(
			PreviewWorld->GetCurrentLevel(), AActor::StaticClass(), TEXT("ArcTownBuildingPreviewActor"));
		SpawnParams.ObjectFlags = RF_Transient;
		PreviewActor = PreviewWorld->SpawnActor<AActor>(SpawnParams);
		if (PreviewActor == nullptr)
		{
			return;
		}

		UStaticMeshComponent* const MeshComponent = NewObject<UStaticMeshComponent>(PreviewActor);
		PreviewActor->AddOwnedComponent(MeshComponent);
		MeshComponent->SetStaticMesh(Mesh);
		PreviewScene->AddComponent(MeshComponent, PreviewTransform);
		MeshComponents.Add(MeshComponent);

		// Each variant reads its own bounds source: component bounds carry bounds extension and bounds
		// scale, so reading them for a static mesh would move a picture that is already correct.
		PreviewBounds = Mesh->GetBoundingBox().TransformBy(PreviewTransform);
	}
	else if (const FArcTownBuildingVisual_Actor* const ActorVisual =
		Definition->Visual.GetPtr<FArcTownBuildingVisual_Actor>())
	{
		UClass* const ActorClass = ActorVisual->ActorClass.LoadSynchronous();
		if (ActorClass == nullptr)
		{
			return;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags = RF_Transient;
		PreviewActor = PreviewWorld->SpawnActor<AActor>(ActorClass, PreviewTransform, SpawnParams);
		if (PreviewActor == nullptr)
		{
			return;
		}

		PreviewBounds = PreviewActor->GetComponentsBoundingBox(/*bNonColliding*/ true);
	}
	else if (const FArcTownBuildingVisual_MassEntity* const EntityVisual =
		Definition->Visual.GetPtr<FArcTownBuildingVisual_MassEntity>())
	{
		const UArcUnifiedMeshVisualizationTrait* const Trait =
			ArcTownBuildingPreviewViewportLocal::FindUnifiedMeshTrait(EntityVisual->EntityConfig);
		if (Trait == nullptr)
		{
			return;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = MakeUniqueObjectName(
			PreviewWorld->GetCurrentLevel(), AActor::StaticClass(), TEXT("ArcTownBuildingPreviewActor"));
		SpawnParams.ObjectFlags = RF_Transient;
		PreviewActor = PreviewWorld->SpawnActor<AActor>(SpawnParams);
		if (PreviewActor == nullptr)
		{
			return;
		}

		const TArray<FArcUnifiedMeshEntry> Entries = Trait->GetMeshEntries();
		for (const FArcUnifiedMeshEntry& Entry : Entries)
		{
			if (!Entry.IsValidEntry())
			{
				continue;
			}

			if (Entry.IsSkinnedEntry())
			{
				UE_LOGF(LogArcTownBuildingPreview, Warning,"Building '%ls' selects entity config '%ls' carrying skinned entry '%ls'; the preview draws static meshes only.",
					*Definition->GetName(),
					*EntityVisual->EntityConfig.ToString(),
					*Entry.SkinnedAsset.ToString());
				continue;
			}

			UStaticMesh* const EntryMesh = Entry.StaticMesh.LoadSynchronous();
			if (EntryMesh == nullptr)
			{
				continue;
			}

			const FTransform EntryTransform = Entry.RelativeTransform * PreviewTransform;

			UStaticMeshComponent* const EntryComponent = NewObject<UStaticMeshComponent>(PreviewActor);
			PreviewActor->AddOwnedComponent(EntryComponent);
			EntryComponent->SetStaticMesh(EntryMesh);
			ArcTownBuildingPreviewViewportLocal::ApplyMaterialOverrides(*EntryComponent, Entry.MaterialOverrides);
			PreviewScene->AddComponent(EntryComponent, EntryTransform);
			MeshComponents.Add(EntryComponent);

			PreviewBounds += EntryMesh->GetBoundingBox().TransformBy(EntryTransform);
		}
	}
	else
	{
		return;
	}

	if (ViewportClient.IsValid())
	{
		ViewportClient->SetVisualBounds(PreviewBounds);
		ViewportClient->SetDoorForward(ArcTownBuildingDefinition::ResolveDoorForwardXY(*Definition, *Visual));
	}
}

void SArcTownBuildingPreviewViewport::ClearPreviewComponents()
{
	if (!PreviewScene.IsValid())
	{
		return;
	}

	for (const TObjectPtr<UStaticMeshComponent>& MeshComponent : MeshComponents)
	{
		if (MeshComponent)
		{
			PreviewScene->RemoveComponent(MeshComponent);
		}
	}
	MeshComponents.Reset();
}

void SArcTownBuildingPreviewViewport::FocusCameraOnPreview()
{
	if (!ViewportClient.IsValid())
	{
		return;
	}

	FBox FocusBox = PreviewBounds;
	if (Definition)
	{
		FVector2D PlotSizeMinXY = FVector2D::ZeroVector;
		FVector2D PlotSizeMaxXY = FVector2D::ZeroVector;
		ArcTownBuildingDefinition::ResolvePlotSize(*Definition, PlotSizeMinXY, PlotSizeMaxXY);
		FocusBox += ArcTownBuildingPreviewViewportLocal::MakeCenteredBox(PlotSizeMaxXY, 0.0, 0.0);
	}

	const FBoxSphereBounds FocusBounds(FocusBox);
	if (FocusBounds.SphereRadius > KINDA_SMALL_NUMBER)
	{
		ViewportClient->FocusViewportOnBox(FocusBox);
	}
}

void SArcTownBuildingPreviewViewport::HandleObjectPropertyChanged(
	UObject* Object,
	FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Object != nullptr && Object == Definition)
	{
		RebuildPreview();
	}
}

void SArcTownBuildingPreviewViewport::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Definition);
	Collector.AddReferencedObject(PreviewActor);
	Collector.AddReferencedObjects(MeshComponents);
}

TSharedRef<SEditorViewport> SArcTownBuildingPreviewViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SArcTownBuildingPreviewViewport::GetExtenders() const
{
	return MakeShared<FExtender>();
}

void SArcTownBuildingPreviewViewport::OnFloatingButtonClicked()
{
}

TSharedRef<FEditorViewportClient> SArcTownBuildingPreviewViewport::MakeEditorViewportClient()
{
	ViewportClient = MakeShareable(new FArcTownBuildingPreviewClient(
		PreviewScene.ToSharedRef(),
		SharedThis(this),
		Definition));

	return ViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SArcTownBuildingPreviewViewport::BuildViewportToolbar()
{
	return SNew(SCommonEditorViewportToolbarBase, SharedThis(this));
}

void SArcTownBuildingPreviewViewport::PopulateViewportOverlays(TSharedRef<SOverlay> Overlay)
{
	using namespace ArcTownBuildingPreviewViewportLocal;

	SEditorViewport::PopulateViewportOverlays(Overlay);

	Overlay->AddSlot()
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Bottom)
	.Padding(6.0f)
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.Visibility(EVisibility::HitTestInvisible)
		.Padding(6.0f, 3.0f)
		[
			// The swatches read the same colour constants Draw uses, so the legend cannot drift from the picture.
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeLegendRow(VisualBoundsColor, LOCTEXT("LegendVisualBounds", "Mesh bounds"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeLegendRow(FootprintColor, LOCTEXT("LegendFootprint", "Footprint (walls)"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeLegendRow(PlotMinColor, LOCTEXT("LegendPlotMin", "Plot min"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeLegendRow(PlotMaxColor, LOCTEXT("LegendPlotMax", "Plot max"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeLegendRow(InfluenceColor, LOCTEXT("LegendInfluence", "Influence radius"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeLegendRow(DenyColor, LOCTEXT("LegendDeny", "Deny min distance"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeLegendRow(DoorColor, LOCTEXT("LegendDoor", "Door location and forward"))
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
