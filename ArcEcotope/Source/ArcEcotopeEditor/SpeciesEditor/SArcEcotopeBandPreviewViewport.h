// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "SEditorViewport.h"
#include "UObject/GCObject.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

#include "SArcEcotopeBandPreviewViewport.generated.h"

class AActor;
class FAdvancedPreviewScene;
class FArcEcotopeBandPreviewClient;
class UArcEcotopeSpeciesModel;
class UMeshComponent;
class UStaticMeshComponent;
struct FArcEcotopeBandSelection;

UCLASS(Transient)
class UArcEcotopeBandPreviewSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Preview", meta = (ClampMin = "-60.0", ClampMax = "60.0", Units = "deg"))
	float GroundSlopeDeg = 0.0f;
};

class SArcEcotopeBandPreviewViewport
	: public SEditorViewport
	, public FGCObject
	, public ICommonEditorViewportToolbarInfoProvider
{
public:
	SLATE_BEGIN_ARGS(SArcEcotopeBandPreviewViewport) {}
		SLATE_ARGUMENT_DEFAULT(UArcEcotopeSpeciesModel*, Species) { nullptr };
		SLATE_ARGUMENT(TSharedPtr<FArcEcotopeBandSelection>, Selection)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SArcEcotopeBandPreviewViewport() override;

	void RebuildPreview();

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SArcEcotopeBandPreviewViewport"); }

	// ICommonEditorViewportToolbarInfoProvider
	virtual TSharedRef<SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;

protected:
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> BuildViewportToolbar() override;
	virtual void PopulateViewportOverlays(TSharedRef<SOverlay> Overlay) override;

private:
	void HandleSelectionChanged();
	void HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent);
	float GetGroundSlopeDeg() const;
	void HandleGroundSlopeChanged(float NewSlopeDeg);

	void ClearPreviewComponents();
	double BuildVariantComponents();
	void ApplyGroundTransform();
	void RefreshTransforms();
	void FocusCameraOnPreview();
	FRotator GetGroundRotation() const;

	TObjectPtr<UArcEcotopeSpeciesModel> Species = nullptr;
	TObjectPtr<UArcEcotopeBandPreviewSettings> Settings = nullptr;
	TObjectPtr<AActor> PreviewActor = nullptr;
	TObjectPtr<UStaticMeshComponent> GroundComponent = nullptr;
	TArray<TObjectPtr<UMeshComponent>> VariantComponents;

	double PreviewRowSpanY = 0.0;
	FBox PreviewBounds = FBox(ForceInit);

	TSharedPtr<FArcEcotopeBandSelection> Selection;
	TSharedPtr<FAdvancedPreviewScene> PreviewScene;
	TSharedPtr<FArcEcotopeBandPreviewClient> ViewportClient;

	FDelegateHandle ObjectPropertyChangedHandle;
};
