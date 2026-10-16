// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Debug/DebugDrawComponent.h"
#include "DebugRenderSceneProxy.h"
#include "ArcTQSQueryResult.h"
#include "ArcTQSRenderingComponent.generated.h"

struct FArcTQSDebugDrawDelegateHelper : public FDebugDrawDelegateHelper
{
	typedef FDebugDrawDelegateHelper Super;

public:
	void SetDrawLabels(bool bInDrawLabels) { bDrawLabels = bInDrawLabels; }
	void Reset() { ResetTexts(); }

protected:
	virtual void DrawDebugLabels(UCanvas* Canvas, APlayerController* PC) override;

private:
	bool bDrawLabels = true;
};

struct FArcTQSRenderMarker
{
	FString Name;
	FVector Location = FVector::ZeroVector;
};

/**
 * Rendering component for the TQS Testing Actor.
 * Uses UDebugDrawComponent / FDebugRenderSceneProxy to draw query results
 * as colored spheres, arrows, and text labels in the editor viewport.
 *
 * Modelled after UEQSRenderingComponent from the engine's EQS system.
 */
UCLASS(ClassGroup = Debug)
class ARCTARGETQUERY_API UArcTQSRenderingComponent : public UDebugDrawComponent
{
	GENERATED_BODY()

public:
	UArcTQSRenderingComponent(const FObjectInitializer& ObjectInitializer);

	/** Update the stored debug data and trigger a visual refresh. */
	void UpdateQueryData(
		const FArcTQSQueryResult& InResult,
		TConstArrayView<FArcTQSRenderMarker> Markers,
		bool bInDrawLabels,
		bool bInDrawFilteredItems);

	/** Clear all debug visuals. */
	void ClearQueryData();

protected:
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

#if UE_ENABLE_DEBUG_DRAWING
	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
	virtual FDebugDrawDelegateHelper& GetDebugDrawDelegateHelper() override { return TQSDebugDrawDelegateHelper; }
	FArcTQSDebugDrawDelegateHelper TQSDebugDrawDelegateHelper;
#endif

private:
	/** Build spheres and texts from query data. */
	void CollectShapes(
		const FArcTQSQueryResult& Result,
		TConstArrayView<FArcTQSRenderMarker> Markers,
		bool bDrawLabels, bool bDrawFilteredItems,
		TArray<FDebugRenderSceneProxy::FSphere>& OutSpheres,
		TArray<FDebugRenderSceneProxy::FText3d>& OutTexts,
		TArray<FDebugRenderSceneProxy::FArrowLine>& OutArrows) const;

	// Pre-computed shapes from last query result
	TArray<FDebugRenderSceneProxy::FSphere> StoredSpheres;
	TArray<FDebugRenderSceneProxy::FText3d> StoredTexts;
	TArray<FDebugRenderSceneProxy::FArrowLine> StoredArrows;
	bool bStoredDrawLabels = true;
};
