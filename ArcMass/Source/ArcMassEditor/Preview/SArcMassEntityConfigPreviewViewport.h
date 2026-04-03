// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SEditorViewport.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "UObject/GCObject.h"
#include "EditorUndoClient.h"

class AActor;
class FArcMassEntityConfigPreviewClient;
class FAdvancedPreviewScene;
#include "ArcMassPreviewContributor.h"
class UMassEntityConfigAsset;

class SArcMassEntityConfigPreviewViewport
	: public SEditorViewport
	, public FGCObject
	, public ICommonEditorViewportToolbarInfoProvider
	, public FEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SArcMassEntityConfigPreviewViewport) {}
		SLATE_ARGUMENT(UMassEntityConfigAsset*, ConfigAsset)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SArcMassEntityConfigPreviewViewport() override;

	void RebuildPreview();
	AActor* GetPreviewActor() const { return PreviewActor; }

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SArcMassEntityConfigPreviewViewport"); }

	// ICommonEditorViewportToolbarInfoProvider
	virtual TSharedRef<SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;

	// FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override { if (bSuccess) { RebuildPreview(); } }
	virtual void PostRedo(bool bSuccess) override { if (bSuccess) { RebuildPreview(); } }

protected:
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> BuildViewportToolbar() override;

private:
	void FocusCameraOnPreview();
	void OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent);

	TObjectPtr<UMassEntityConfigAsset> ConfigAsset = nullptr;
	TSharedPtr<FAdvancedPreviewScene> PreviewScene;
	TSharedPtr<FArcMassEntityConfigPreviewClient> ViewportClient;
	TObjectPtr<AActor> PreviewActor = nullptr;

	TArray<TUniquePtr<IArcMassPreviewContributor>> Contributors;
	FDelegateHandle ObjectPropertyChangedHandle;
};
