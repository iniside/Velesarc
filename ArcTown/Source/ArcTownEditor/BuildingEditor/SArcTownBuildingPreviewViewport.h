// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "SEditorViewport.h"
#include "UObject/GCObject.h"
#include "UObject/ObjectPtr.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class AActor;
class FAdvancedPreviewScene;
class FArcTownBuildingPreviewClient;
class UArcTownBuildingDefinition;
class UStaticMeshComponent;

class SArcTownBuildingPreviewViewport
	: public SEditorViewport
	, public FGCObject
	, public ICommonEditorViewportToolbarInfoProvider
{
public:
	SLATE_BEGIN_ARGS(SArcTownBuildingPreviewViewport) {}
		SLATE_ARGUMENT_DEFAULT(UArcTownBuildingDefinition*, Definition) { nullptr };
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SArcTownBuildingPreviewViewport() override;

	void RebuildPreview();

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SArcTownBuildingPreviewViewport"); }

	// ICommonEditorViewportToolbarInfoProvider
	virtual TSharedRef<SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;

protected:
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> BuildViewportToolbar() override;
	virtual void PopulateViewportOverlays(TSharedRef<SOverlay> Overlay) override;

private:
	void HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent);

	void ClearPreviewComponents();
	void FocusCameraOnPreview();

	TObjectPtr<UArcTownBuildingDefinition> Definition = nullptr;
	TObjectPtr<AActor> PreviewActor = nullptr;
	TArray<TObjectPtr<UStaticMeshComponent>> MeshComponents;

	FBox PreviewBounds = FBox(ForceInit);

	TSharedPtr<FAdvancedPreviewScene> PreviewScene;
	TSharedPtr<FArcTownBuildingPreviewClient> ViewportClient;

	FDelegateHandle ObjectPropertyChangedHandle;
};
