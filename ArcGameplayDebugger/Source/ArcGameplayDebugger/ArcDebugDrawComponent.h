// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Debug/DebugDrawComponent.h"
#include "DebugRenderSceneProxy.h"
#include "ArcDebugDrawComponent.generated.h"

// --- Custom shape structs (not natively in FDebugRenderSceneProxy) ---

struct FArcDebugCircle
{
	FVector Center;
	float Radius;
	int32 Segments;
	FVector YAxis;
	FVector ZAxis;
	FColor Color;
	float Thickness;

	FArcDebugCircle() = default;
	FArcDebugCircle(const FVector& InCenter, float InRadius, int32 InSegments, const FVector& InYAxis, const FVector& InZAxis, const FColor& InColor, float InThickness = 1.f)
		: Center(InCenter), Radius(InRadius), Segments(InSegments), YAxis(InYAxis), ZAxis(InZAxis), Color(InColor), Thickness(InThickness)
	{
	}
};

struct FArcDebugPoint
{
	FVector Position;
	float Size;
	FColor Color;

	FArcDebugPoint() = default;
	FArcDebugPoint(const FVector& InPosition, float InSize, const FColor& InColor)
		: Position(InPosition), Size(InSize), Color(InColor)
	{
	}
};

// --- Delegate Helper ---

struct FArcDebugDrawDelegateHelper : public FDebugDrawDelegateHelper
{
	typedef FDebugDrawDelegateHelper Super;

public:
	FArcDebugDrawDelegateHelper()
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

// --- Base Draw Component ---

UCLASS(Abstract, ClassGroup = Debug)
class UArcDebugDrawComponent : public UDebugDrawComponent
{
	GENERATED_BODY()

public:
	UArcDebugDrawComponent(const FObjectInitializer& ObjectInitializer);

	void RefreshShapes();
	void ClearShapes();

	void SetDrawLabels(bool bInDrawLabels) { bStoredDrawLabels = bInDrawLabels; }

protected:
	virtual void CollectShapes() PURE_VIRTUAL(UArcDebugDrawComponent::CollectShapes, );

	// Native FDebugRenderSceneProxy arrays
	TArray<FDebugRenderSceneProxy::FSphere> StoredSpheres;
	TArray<FDebugRenderSceneProxy::FDebugLine> StoredLines;
	TArray<FDebugRenderSceneProxy::FText3d> StoredTexts;
	TArray<FDebugRenderSceneProxy::FArrowLine> StoredArrows;
	TArray<FDebugRenderSceneProxy::FDebugBox> StoredBoxes;
	TArray<FDebugRenderSceneProxy::FCapsule> StoredCapsules;

	// Custom shape arrays
	TArray<FArcDebugCircle> StoredCircles;
	TArray<FArcDebugPoint> StoredPoints;

	bool bStoredDrawLabels = true;

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

#if UE_ENABLE_DEBUG_DRAWING
	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
	virtual FDebugDrawDelegateHelper& GetDebugDrawDelegateHelper() override { return DebugDrawDelegateHelper; }
	FArcDebugDrawDelegateHelper DebugDrawDelegateHelper;
#endif
};
