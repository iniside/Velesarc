// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Debug/DebugDrawComponent.h"
#include "DebugRenderSceneProxy.h"
#include "ArcTQSQuerySubsystem.h"
#include "ArcTQSRenderingComponent.generated.h"

/**
 * Custom debug draw delegate helper that controls label visibility
 * based on the owning actor's selection state and label toggle.
 */
class FArcTQSDebugDrawDelegateHelper : public FDebugDrawDelegateHelper
{
	typedef FDebugDrawDelegateHelper Super;

public:
	FArcTQSDebugDrawDelegateHelper()
		: ActorOwner(nullptr)
		, bDrawLabels(true)
	{
	}

	void SetupFromProxy(const FDebugRenderSceneProxy* InSceneProxy, AActor* InActorOwner, bool bInDrawLabels);
	void Reset() { ResetTexts(); }

protected:
	virtual void DrawDebugLabels(UCanvas* Canvas, APlayerController* PC) override;

private:
	AActor* ActorOwner;
	bool bDrawLabels;
};

/**
 * Rendering component for the TQS Testing Actor.
 * Uses UDebugDrawComponent / FDebugRenderSceneProxy to draw query results
 * as colored spheres, arrows, and text labels in the editor viewport.
 *
 * Modelled after UEQSRenderingComponent from the engine's EQS system.
 */
UCLASS(ClassGroup = Debug)
class ARCAI_API UArcTQSRenderingComponent : public UDebugDrawComponent
{
	GENERATED_BODY()

public:
	UArcTQSRenderingComponent(const FObjectInitializer& ObjectInitializer);

	/** Update the stored debug data and trigger a visual refresh. */
	void UpdateQueryData(const FArcTQSDebugQueryData& InData, bool bInDrawLabels, bool bInDrawFilteredItems);

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
		const FArcTQSDebugQueryData& Data,
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
