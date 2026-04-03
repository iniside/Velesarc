// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Debug/DebugDrawComponent.h"
#include "DebugRenderSceneProxy.h"
#include "ArcTQSDebuggerDrawComponent.generated.h"

struct FArcTQSDebugQueryData;

struct FArcTQSDebugDrawDelegateHelper_Debugger : public FDebugDrawDelegateHelper
{
	typedef FDebugDrawDelegateHelper Super;

public:
	FArcTQSDebugDrawDelegateHelper_Debugger()
		: bDrawLabels(true)
	{
	}

	void SetupFromProxy(const FDebugRenderSceneProxy* InSceneProxy, bool bInDrawLabels);
	void Reset() { ResetTexts(); }

protected:
	virtual void DrawDebugLabels(UCanvas* Canvas, APlayerController* PC) override;

private:
	bool bDrawLabels;
};

UCLASS(ClassGroup = Debug)
class UArcTQSDebuggerDrawComponent : public UDebugDrawComponent
{
	GENERATED_BODY()

public:
	UArcTQSDebuggerDrawComponent(const FObjectInitializer& ObjectInitializer);

	void UpdateQueryData(
		const FArcTQSDebugQueryData& InData,
		bool bInDrawAllItems,
		bool bInDrawLabels,
		bool bInDrawScores,
		bool bInDrawFilteredItems,
		int32 InSelectedItemIndex,
		int32 InHoveredItemIndex);
	void ClearQueryData();

protected:
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

#if UE_ENABLE_DEBUG_DRAWING
	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
	virtual FDebugDrawDelegateHelper& GetDebugDrawDelegateHelper() override { return DebugDrawDelegateHelper; }
	FArcTQSDebugDrawDelegateHelper_Debugger DebugDrawDelegateHelper;
#endif

private:
	void CollectShapes(
		const FArcTQSDebugQueryData& Data,
		bool bDrawAllItems,
		bool bDrawLabels,
		bool bDrawScores,
		bool bDrawFilteredItems,
		int32 SelectedIndex,
		int32 HoveredIndex,
		TArray<FDebugRenderSceneProxy::FSphere>& OutSpheres,
		TArray<FDebugRenderSceneProxy::FText3d>& OutTexts,
		TArray<FDebugRenderSceneProxy::FArrowLine>& OutArrows) const;

	TArray<FDebugRenderSceneProxy::FSphere> StoredSpheres;
	TArray<FDebugRenderSceneProxy::FText3d> StoredTexts;
	TArray<FDebugRenderSceneProxy::FArrowLine> StoredArrows;
	bool bStoredDrawLabels = true;
};