// Copyright Lukasz Baran. All Rights Reserved.

#include "SpeciesEditor/SArcEcotopeBandPreviewViewport.h"

#include "AdvancedPreviewScene.h"
#include "Animation/TransformProviderData.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "EditorViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Species/ArcEcotopeSpeciesModel.h"
#include "Species/ArcForestStageBand.h"
#include "SpeciesEditor/ArcEcotopeSpeciesEditor.h"
#include "Styling/AppStyle.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "UObject/UObjectGlobals.h"
#include "ViewportInteractions/ViewportInteractionsBehaviorSource.h"
#include "ViewportInteractions/ViewportInteractionsUtilities.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SArcEcotopeBandPreviewViewport"

namespace ArcEcotopeBandPreviewViewportLocal
{
	// /Engine/BasicShapes/Plane is authored as a 100 cm square, so a ground span in centimetres divides by it to become a scale.
	constexpr double PlaneAssetSizeCm = 100.0;
	constexpr double GroundMarginFraction = 0.5;
	constexpr double VariantGapFraction = 0.25;
	constexpr double MinVariantExtentCm = 1.0;
	constexpr float MaxGroundSlopeDeg = 60.0f;
	constexpr double CentimetresPerMetre = 100.0;
	constexpr double EmptyRowGroundSpansOfFullGrownHeight = 3.0;

	double MakeEmptyRowGroundSpanCm(const UArcEcotopeSpeciesModel* Species)
	{
		const UArcEcotopeSpeciesModel* SizeSource =
			Species != nullptr ? Species : GetDefault<UArcEcotopeSpeciesModel>();
		const double TreeSizeM = FMath::Clamp(
			static_cast<double>(SizeSource->TreeSizeM),
			ArcForestSpeciesModel::MinimumTreeSizeM,
			ArcForestSpeciesModel::MaximumTreeSizeM);
		return EmptyRowGroundSpansOfFullGrownHeight * TreeSizeM * CentimetresPerMetre;
	}

	struct FBandLayout
	{
		FQuat Align = FQuat::Identity;
		FVector Up = FVector::UpVector;
		double UniformScale = 1.0;
	};

	FBandLayout MakeBandLayout(const FArcForestStageBand& Band, const FRotator& GroundRotation)
	{
		FBandLayout Layout;
		const FVector Normal = GroundRotation.RotateVector(FVector::UpVector);
		Layout.Align = UE::ArcForest::MakeTerrainAlign(Normal, Band.Rotation.MaxAlignDeg);
		Layout.Up = Layout.Align.RotateVector(FVector::UpVector);
		Layout.UniformScale =
			0.5 * (static_cast<double>(Band.Scale.MinScale) + static_cast<double>(Band.Scale.MaxScale));
		return Layout;
	}

	FTransform MakeVariantTransform(
		const FArcForestStageBand& Band,
		const FBandLayout& Layout,
		double MeshExtentY,
		const FQuat& MeshAxisFix,
		double& InOutNextSlotEdgeY)
	{
		const double ExtentY = FMath::Max(MeshExtentY * Layout.UniformScale, MinVariantExtentCm);

		const FVector SlotPosition(0.0, InOutNextSlotEdgeY + ExtentY, 0.0);
		InOutNextSlotEdgeY += (2.0 + VariantGapFraction) * ExtentY;

		const FVector Seated =
			SlotPosition + Layout.Up * (static_cast<double>(Band.OffsetAlongUpCm) * Layout.UniformScale);
		return FTransform(Layout.Align * MeshAxisFix, Seated, FVector(Layout.UniformScale));
	}
}

class FArcEcotopeBandPreviewClient : public FEditorViewportClient
{
public:
	FArcEcotopeBandPreviewClient(
		const TSharedRef<FAdvancedPreviewScene>& InPreviewScene,
		const TSharedRef<SArcEcotopeBandPreviewViewport>& InViewport)
		: FEditorViewportClient(nullptr, &InPreviewScene.Get(), StaticCastSharedRef<SEditorViewport>(InViewport))
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

private:
	bool bITFInitialized = false;
};

void SArcEcotopeBandPreviewViewport::Construct(const FArguments& InArgs)
{
	Species = InArgs._Species;
	Selection = InArgs._Selection;

	check(Selection.IsValid());
	Selection->OnChanged.AddSP(this, &SArcEcotopeBandPreviewViewport::HandleSelectionChanged);

	Settings = NewObject<UArcEcotopeBandPreviewSettings>(GetTransientPackage(), NAME_None, RF_Transient);

	FAdvancedPreviewScene::ConstructionValues SceneArgs;
	SceneArgs.bCreatePhysicsScene = false;
	PreviewScene = MakeShared<FAdvancedPreviewScene>(SceneArgs);
	PreviewScene->SetFloorVisibility(false);

	SEditorViewport::Construct(SEditorViewport::FArguments());

	ObjectPropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(
		this, &SArcEcotopeBandPreviewViewport::HandleObjectPropertyChanged);

	RebuildPreview();
	FocusCameraOnPreview();
}

SArcEcotopeBandPreviewViewport::~SArcEcotopeBandPreviewViewport()
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

void SArcEcotopeBandPreviewViewport::RebuildPreview()
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

	UWorld* PreviewWorld = PreviewScene->GetWorld();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(
		PreviewWorld->GetCurrentLevel(), AActor::StaticClass(), TEXT("ArcEcotopeBandPreviewActor"));
	SpawnParams.ObjectFlags = RF_Transient;
	PreviewActor = PreviewWorld->SpawnActor<AActor>(SpawnParams);

	if (!GroundComponent)
	{
		// Own ground instead of the scene floor: FAdvancedPreviewScene exposes only a vertical offset and hands out a const floor component, so its floor cannot be tilted.
		GroundComponent = NewObject<UStaticMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
		GroundComponent->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
	}

	PreviewScene->AddComponent(GroundComponent, FTransform::Identity);

	PreviewRowSpanY = BuildVariantComponents();
	ApplyGroundTransform();
}

void SArcEcotopeBandPreviewViewport::ClearPreviewComponents()
{
	if (!PreviewScene.IsValid())
	{
		return;
	}

	for (const TObjectPtr<UMeshComponent>& VariantComponent : VariantComponents)
	{
		if (VariantComponent)
		{
			PreviewScene->RemoveComponent(VariantComponent);
		}
	}
	VariantComponents.Reset();

	if (GroundComponent)
	{
		PreviewScene->RemoveComponent(GroundComponent);
	}
}

double SArcEcotopeBandPreviewViewport::BuildVariantComponents()
{
	PreviewBounds = FBox(ForceInit);

	const int32 BandIndex = Selection.IsValid() ? Selection->GetBandIndex() : INDEX_NONE;
	if (!Species || !Species->Bands.IsValidIndex(BandIndex))
	{
		return 0.0;
	}

	const FArcForestStageBand& Band = Species->Bands[BandIndex];
	const FArcForestBandVisual_StaticMesh* StaticVisual = Band.Visual.GetPtr<FArcForestBandVisual_StaticMesh>();
	const FArcForestBandVisual_SkinnedMesh* SkinnedVisual = Band.Visual.GetPtr<FArcForestBandVisual_SkinnedMesh>();
	if (StaticVisual == nullptr && SkinnedVisual == nullptr)
	{
		return 0.0;
	}

	const ArcEcotopeBandPreviewViewportLocal::FBandLayout Layout =
		ArcEcotopeBandPreviewViewportLocal::MakeBandLayout(Band, GetGroundRotation());

	double NextSlotEdgeY = 0.0;

	if (StaticVisual != nullptr)
	{
		for (const FArcForestStaticMeshVariant& Variant : StaticVisual->Variants)
		{
			if (Variant.StaticMesh == nullptr)
			{
				continue;
			}

			const FBoxSphereBounds MeshBounds = Variant.StaticMesh->GetBounds();
			const FTransform Transform = ArcEcotopeBandPreviewViewportLocal::MakeVariantTransform(
				Band,
				Layout,
				MeshBounds.BoxExtent.Y,
				UE::ArcForest::MakeMeshAxisFix(Variant.LengthAxis),
				NextSlotEdgeY);

			UInstancedStaticMeshComponent* VariantComponent = NewObject<UInstancedStaticMeshComponent>(PreviewActor);
			PreviewActor->AddOwnedComponent(VariantComponent);
			// InitComponent clears the mesh and the override materials, so the variant is stamped after it.
			Species->StaticMeshDescriptor.InitComponent(VariantComponent);
			VariantComponent->SetStaticMesh(Variant.StaticMesh);
			for (int32 MaterialIndex = 0; MaterialIndex < Variant.MaterialOverrides.Num(); ++MaterialIndex)
			{
				if (Variant.MaterialOverrides[MaterialIndex])
				{
					VariantComponent->SetMaterial(MaterialIndex, Variant.MaterialOverrides[MaterialIndex]);
				}
			}

			PreviewScene->AddComponent(VariantComponent, FTransform::Identity);
			VariantComponent->AddInstance(Transform);
			VariantComponent->MarkRenderStateDirty();
			VariantComponents.Add(VariantComponent);

			PreviewBounds += MeshBounds.GetBox().TransformBy(Transform);
		}

		return NextSlotEdgeY;
	}

	for (const FArcForestSkeletalMeshVariant& Variant : SkinnedVisual->Variants)
	{
		if (Variant.SkeletalMesh == nullptr)
		{
			continue;
		}

		const FBoxSphereBounds MeshBounds = Variant.SkeletalMesh->GetBounds();
		const FTransform Transform = ArcEcotopeBandPreviewViewportLocal::MakeVariantTransform(
			Band,
			Layout,
			MeshBounds.BoxExtent.Y,
			FQuat::Identity,
			NextSlotEdgeY);

		UInstancedSkinnedMeshComponent* VariantComponent = NewObject<UInstancedSkinnedMeshComponent>(PreviewActor);
		PreviewActor->AddOwnedComponent(VariantComponent);
		// InitComponent clears the skinned asset, the provider and the override materials, so the variant is stamped after it.
		Species->SkinnedMeshDescriptor.InitComponent(VariantComponent);
		VariantComponent->SetSkinnedAssetAndUpdate(Variant.SkeletalMesh);
		VariantComponent->SetTransformProvider(Variant.TransformProvider);
		for (int32 MaterialIndex = 0; MaterialIndex < Variant.MaterialOverrides.Num(); ++MaterialIndex)
		{
			if (Variant.MaterialOverrides[MaterialIndex])
			{
				VariantComponent->SetMaterial(MaterialIndex, Variant.MaterialOverrides[MaterialIndex]);
			}
		}

		PreviewScene->AddComponent(VariantComponent, FTransform::Identity);
		VariantComponent->AddInstance(Transform, Variant.AnimationIndex, false);
		VariantComponent->MarkRenderStateDirty();
		VariantComponents.Add(VariantComponent);

		PreviewBounds += MeshBounds.GetBox().TransformBy(Transform);
	}

	return NextSlotEdgeY;
}

void SArcEcotopeBandPreviewViewport::RefreshTransforms()
{
	if (!PreviewScene.IsValid())
	{
		return;
	}

	const int32 BandIndex = Selection.IsValid() ? Selection->GetBandIndex() : INDEX_NONE;
	const FArcForestBandVisual_StaticMesh* StaticVisual = nullptr;
	if (Species && Species->Bands.IsValidIndex(BandIndex))
	{
		StaticVisual = Species->Bands[BandIndex].Visual.GetPtr<FArcForestBandVisual_StaticMesh>();
	}

	if (StaticVisual == nullptr)
	{
		RebuildPreview();
		return;
	}

	const FArcForestStageBand& Band = Species->Bands[BandIndex];
	const ArcEcotopeBandPreviewViewportLocal::FBandLayout Layout =
		ArcEcotopeBandPreviewViewportLocal::MakeBandLayout(Band, GetGroundRotation());

	FBox RefreshedBounds(ForceInit);
	double NextSlotEdgeY = 0.0;
	int32 ComponentIndex = 0;

	for (const FArcForestStaticMeshVariant& Variant : StaticVisual->Variants)
	{
		if (Variant.StaticMesh == nullptr)
		{
			continue;
		}

		if (!VariantComponents.IsValidIndex(ComponentIndex))
		{
			RebuildPreview();
			return;
		}

		const FBoxSphereBounds MeshBounds = Variant.StaticMesh->GetBounds();
		const FTransform Transform = ArcEcotopeBandPreviewViewportLocal::MakeVariantTransform(
			Band,
			Layout,
			MeshBounds.BoxExtent.Y,
			UE::ArcForest::MakeMeshAxisFix(Variant.LengthAxis),
			NextSlotEdgeY);

		UInstancedStaticMeshComponent* VariantComponent =
			Cast<UInstancedStaticMeshComponent>(VariantComponents[ComponentIndex++]);
		if (VariantComponent == nullptr || VariantComponent->GetInstanceCount() == 0)
		{
			RebuildPreview();
			return;
		}

		VariantComponent->UpdateInstanceTransform(0, Transform, false, true);
		RefreshedBounds += MeshBounds.GetBox().TransformBy(Transform);
	}

	if (ComponentIndex != VariantComponents.Num())
	{
		RebuildPreview();
		return;
	}

	PreviewBounds = RefreshedBounds;
	PreviewRowSpanY = NextSlotEdgeY;
	ApplyGroundTransform();
}

void SArcEcotopeBandPreviewViewport::ApplyGroundTransform()
{
	if (!GroundComponent)
	{
		return;
	}

	const double RowSpanCm = PreviewRowSpanY > 0.0
		? PreviewRowSpanY
		: ArcEcotopeBandPreviewViewportLocal::MakeEmptyRowGroundSpanCm(Species);
	const double GroundSpanCm = RowSpanCm * (1.0 + ArcEcotopeBandPreviewViewportLocal::GroundMarginFraction);

	const FRotator GroundRotation = GetGroundRotation();
	GroundComponent->SetRelativeTransform(FTransform(
		GroundRotation.Quaternion(),
		GroundRotation.RotateVector(FVector(0.0, PreviewRowSpanY * 0.5, 0.0)),
		FVector(GroundSpanCm / ArcEcotopeBandPreviewViewportLocal::PlaneAssetSizeCm)));
}

void SArcEcotopeBandPreviewViewport::FocusCameraOnPreview()
{
	if (!ViewportClient.IsValid())
	{
		return;
	}

	const FBoxSphereBounds CombinedBounds(PreviewBounds);
	if (CombinedBounds.SphereRadius > KINDA_SMALL_NUMBER)
	{
		ViewportClient->FocusViewportOnBox(CombinedBounds.GetBox());
	}
}

FRotator SArcEcotopeBandPreviewViewport::GetGroundRotation() const
{
	return FRotator(Settings ? static_cast<double>(Settings->GroundSlopeDeg) : 0.0, 0.0, 0.0);
}

void SArcEcotopeBandPreviewViewport::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Species);
	Collector.AddReferencedObject(Settings);
	Collector.AddReferencedObject(PreviewActor);
	Collector.AddReferencedObject(GroundComponent);
}

TSharedRef<SEditorViewport> SArcEcotopeBandPreviewViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SArcEcotopeBandPreviewViewport::GetExtenders() const
{
	return MakeShared<FExtender>();
}

void SArcEcotopeBandPreviewViewport::OnFloatingButtonClicked()
{
}

TSharedRef<FEditorViewportClient> SArcEcotopeBandPreviewViewport::MakeEditorViewportClient()
{
	ViewportClient = MakeShareable(new FArcEcotopeBandPreviewClient(
		PreviewScene.ToSharedRef(),
		SharedThis(this)));

	return ViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SArcEcotopeBandPreviewViewport::BuildViewportToolbar()
{
	return SNew(SCommonEditorViewportToolbarBase, SharedThis(this));
}

void SArcEcotopeBandPreviewViewport::PopulateViewportOverlays(TSharedRef<SOverlay> Overlay)
{
	SEditorViewport::PopulateViewportOverlays(Overlay);

	Overlay->AddSlot()
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Bottom)
	.Padding(6.0f)
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.Padding(6.0f, 3.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("GroundSlopeLabel", "Ground Slope"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SSpinBox<float>)
				.MinValue(-ArcEcotopeBandPreviewViewportLocal::MaxGroundSlopeDeg)
				.MaxValue(ArcEcotopeBandPreviewViewportLocal::MaxGroundSlopeDeg)
				.MinSliderValue(-ArcEcotopeBandPreviewViewportLocal::MaxGroundSlopeDeg)
				.MaxSliderValue(ArcEcotopeBandPreviewViewportLocal::MaxGroundSlopeDeg)
				.MinDesiredWidth(64.0f)
				.Value(this, &SArcEcotopeBandPreviewViewport::GetGroundSlopeDeg)
				.OnValueChanged(this, &SArcEcotopeBandPreviewViewport::HandleGroundSlopeChanged)
			]
		]
	];
}

void SArcEcotopeBandPreviewViewport::HandleSelectionChanged()
{
	RebuildPreview();
	FocusCameraOnPreview();
}

void SArcEcotopeBandPreviewViewport::HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Object != nullptr && Object == Species)
	{
		RebuildPreview();
	}
}

float SArcEcotopeBandPreviewViewport::GetGroundSlopeDeg() const
{
	return Settings ? Settings->GroundSlopeDeg : 0.0f;
}

void SArcEcotopeBandPreviewViewport::HandleGroundSlopeChanged(float NewSlopeDeg)
{
	if (!Settings)
	{
		return;
	}

	Settings->GroundSlopeDeg = FMath::Clamp(
		NewSlopeDeg,
		-ArcEcotopeBandPreviewViewportLocal::MaxGroundSlopeDeg,
		ArcEcotopeBandPreviewViewportLocal::MaxGroundSlopeDeg);
	RefreshTransforms();
}

#undef LOCTEXT_NAMESPACE
