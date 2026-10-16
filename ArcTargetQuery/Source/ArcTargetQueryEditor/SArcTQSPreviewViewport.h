// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSPreviewEntities.h"
#include "ArcTQSQueryExecution.h"
#include "ArcTQSQueryResult.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "SEditorViewport.h"
#include "UObject/GCObject.h"
#include "UObject/ObjectPtr.h"

class FAdvancedPreviewScene;
class FArcTQSPreviewClient;
class UArcTQSQueryDefinition;
class UArcTQSRenderingComponent;
struct FArcTQSPreviewMock;

class SArcTQSPreviewViewport
	: public SEditorViewport
	, public FGCObject
	, public ICommonEditorViewportToolbarInfoProvider
{
public:
	SLATE_BEGIN_ARGS(SArcTQSPreviewViewport) {}
		SLATE_ARGUMENT_DEFAULT(UArcTQSQueryDefinition*, Definition) { nullptr };
		SLATE_EVENT(FSimpleDelegate, OnResultChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SArcTQSPreviewViewport() override;

	void RebuildMocks();
	void RunQuery();

	const FArcTQSQueryResult& GetResult() const { return Result; }
	EArcTQSCreateFailure GetFailure() const { return Failure; }

	void SetHighlightedItem(int32 ItemIndex);
	int32 GetHighlightedItem() const { return HighlightedItem; }

	int32 GetMockCount() const;
	const FArcTQSPreviewMock* GetMock(int32 MockIndex) const;
	void BeginMockTransform();
	void SetMockTransform(int32 MockIndex, const FTransform& Transform);
	void EndMockTransform();
	UArcTQSQueryDefinition* GetDefinition() const { return Definition; }

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SArcTQSPreviewViewport"); }

	// ICommonEditorViewportToolbarInfoProvider
	virtual TSharedRef<SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;

protected:
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> BuildViewportToolbar() override;

private:
	void HandleDefinitionChanged(const UArcTQSQueryDefinition& ChangedDefinition);
	FArcTQSPreviewMock* GetMutableMock(int32 MockIndex);
	void UpdateRendering();
	void FocusCameraOnMocks();

	TObjectPtr<UArcTQSQueryDefinition> Definition = nullptr;
	TObjectPtr<UArcTQSRenderingComponent> RenderingComponent = nullptr;

	TSharedPtr<FAdvancedPreviewScene> PreviewScene;
	TSharedPtr<FArcTQSPreviewClient> ViewportClient;

	FArcTQSPreviewEntities PreviewEntities;
	TArray<FMassEntityHandle> MockEntities;

	FArcTQSQueryResult Result;
	EArcTQSCreateFailure Failure = EArcTQSCreateFailure::None;
	int32 HighlightedItem = INDEX_NONE;
	bool bTransformingMock = false;

	FSimpleDelegate OnResultChanged;
	FDelegateHandle PostCompileHandle;
	FDelegateHandle PreviewSetupChangedHandle;
};
