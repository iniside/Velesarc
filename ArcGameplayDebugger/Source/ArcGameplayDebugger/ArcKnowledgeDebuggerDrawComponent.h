// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Debug/DebugDrawComponent.h"
#include "DebugRenderSceneProxy.h"
#include "ArcKnowledgeDebuggerDrawComponent.generated.h"

struct FArcKnowledgeDebugDrawEntry
{
	FVector Location = FVector::ZeroVector;
	FString Label;
	float Relevance = 0.f;
	float BroadcastRadius = 0.f;
	bool bClaimed = false;
};

struct FArcKnowledgeDebugDrawDelegateHelper : public FDebugDrawDelegateHelper
{
	typedef FDebugDrawDelegateHelper Super;

public:
	FArcKnowledgeDebugDrawDelegateHelper()
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
class UArcKnowledgeDebuggerDrawComponent : public UDebugDrawComponent
{
	GENERATED_BODY()

public:
	UArcKnowledgeDebuggerDrawComponent(const FObjectInitializer& ObjectInitializer);

	void UpdateEntryData(
		TArrayView<const FArcKnowledgeDebugDrawEntry> InEntries,
		int32 InSelectedIndex,
		bool bInDrawAllEntries,
		bool bInDrawLabels,
		bool bInDrawSelectedRadius);
	void ClearEntryData();

protected:
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

#if UE_ENABLE_DEBUG_DRAWING
	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
	virtual FDebugDrawDelegateHelper& GetDebugDrawDelegateHelper() override { return DebugDrawDelegateHelper; }
	FArcKnowledgeDebugDrawDelegateHelper DebugDrawDelegateHelper;
#endif

private:
	void CollectShapes(
		TArrayView<const FArcKnowledgeDebugDrawEntry> Entries,
		int32 SelectedIndex,
		bool bDrawAllEntries,
		bool bDrawLabels,
		bool bDrawSelectedRadius,
		TArray<FDebugRenderSceneProxy::FSphere>& OutSpheres,
		TArray<FDebugRenderSceneProxy::FText3d>& OutTexts,
		TArray<FDebugRenderSceneProxy::FArrowLine>& OutArrows) const;

	TArray<FDebugRenderSceneProxy::FSphere> StoredSpheres;
	TArray<FDebugRenderSceneProxy::FText3d> StoredTexts;
	TArray<FDebugRenderSceneProxy::FArrowLine> StoredArrows;
	bool bStoredDrawLabels = true;
};
