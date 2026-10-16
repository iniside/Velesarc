// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "Widgets/SCompoundWidget.h"

class FArcRmlModelBinding;
class SArcRmlViewport;
class SBox;
class SVerticalBox;
struct FArcRmlViewModelCall;
struct FArcRmlViewModelDeclaration;

class SArcRmlDocumentPreview final : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SArcRmlDocumentPreview)
	{
	}
	SLATE_END_ARGS()

	virtual ~SArcRmlDocumentPreview() override;

	void Construct(const FArguments& InArgs, const FString& InDocumentPath);
	void Shutdown();

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

private:
	struct FPreviewModel
	{
		FString Name;
		TObjectPtr<UObject> ViewModel;
	};

	void Reload();
	void ReleaseView();
	void DispatchViewModelCall(const FArcRmlViewModelCall& Call);
	void SyncPreviewModels(const TArray<FArcRmlViewModelDeclaration>& Declarations);
	void RebuildBindingsPanel(const TArray<FArcRmlViewModelDeclaration>& Declarations);
	TSharedRef<SWidget> MakeResolutionMenu();
	TSharedRef<SWidget> MakeVariantMenu();
	void SelectResolution(int32 Index);
	void SelectVariant(const FString& Variant);
	FReply HandleOpenInTextEditor();
	FReply HandleReload();
	FIntPoint GetResolution() const;
	float GetGameScale() const;
	FSlateRect GetContentRect() const;

	FString DocumentPath;
	FString FramePath;
	TSharedPtr<SArcRmlViewport> Viewport;
	TSharedPtr<FArcRmlModelBinding> Binding;
	TSharedPtr<FArcRmlModelBinding> TabBinding;
	TSharedPtr<SBox> ViewportSlot;
	TSharedPtr<SVerticalBox> BindingsPanel;
	TArray<FPreviewModel> PreviewModels;
	TArray<FString> VariantNames;
	FString PreviewVariant;
	int32 ResolutionIndex = 0;
	FDelegateHandle CachesClearedHandle;
	bool bShutdown = false;
};
