// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Debug/DebugDrawComponent.h"
#include "DebugRenderSceneProxy.h"
#include "ArcPathDebuggerDrawComponent.generated.h"

struct FArcPathDebugNavPoint
{
	FVector Position = FVector::ZeroVector;
	FVector Tangent = FVector::ZeroVector;
	FVector Left = FVector::ZeroVector;
	FVector Right = FVector::ZeroVector;
	FString Label;
};

struct FArcPathDebugDrawData
{
	FVector BasePos = FVector::ZeroVector;
	FVector EntityForward = FVector::ForwardVector;

	bool bDrawMoveTarget = false;
	FVector MoveTargetPos = FVector::ZeroVector;
	FVector MoveTargetForward = FVector::ForwardVector;
	float MoveTargetSlackRadius = 0.f;
	FString MoveTargetLabel;

	bool bDrawVelocity = false;
	FVector Velocity = FVector::ZeroVector;

	bool bDrawSteering = false;
	FVector DesiredVelocity = FVector::ZeroVector;
	bool bHasStandTarget = false;
	FVector StandTarget = FVector::ZeroVector;

	bool bDrawForces = false;
	FVector ForceValue = FVector::ZeroVector;

	bool bHasGhost = false;
	FVector GhostPos = FVector::ZeroVector;
	FVector GhostVelocity = FVector::ZeroVector;

	bool bDrawAvoidance = false;
	enum class EColliderShape : uint8 { Circle, Pill };
	EColliderShape ColliderShape = EColliderShape::Circle;
	float ColliderRadius = 0.f;
	float PillHalfLength = 0.f;

	bool bDrawShortPath = false;
	TArray<FArcPathDebugNavPoint> PathPoints;
	float PathZOffset = 5.f;

	bool bDrawNavMesh = false;

	struct FNavMeshPoly
	{
		TArray<FVector, TInlineAllocator<6>> Verts;
	};
	TArray<FNavMeshPoly> NavMeshPolys;
};

struct FArcPathDebugDrawDelegateHelper : public FDebugDrawDelegateHelper
{
	typedef FDebugDrawDelegateHelper Super;

public:
	void SetupFromProxy(const FDebugRenderSceneProxy* InSceneProxy);
	void Reset() { ResetTexts(); }

protected:
	virtual void DrawDebugLabels(UCanvas* Canvas, APlayerController* PC) override;
};

UCLASS(ClassGroup = Debug)
class UArcPathDebuggerDrawComponent : public UDebugDrawComponent
{
	GENERATED_BODY()
public:
	UArcPathDebuggerDrawComponent(const FObjectInitializer& ObjectInitializer);

	void UpdatePathData(const FArcPathDebugDrawData& InData);
	void ClearShapes();

protected:
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

#if UE_ENABLE_DEBUG_DRAWING
	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
	virtual FDebugDrawDelegateHelper& GetDebugDrawDelegateHelper() override { return DebugDrawDelegateHelper; }
	FArcPathDebugDrawDelegateHelper DebugDrawDelegateHelper;
#endif

private:
	void CollectShapes();

	TArray<FDebugRenderSceneProxy::FSphere> StoredSpheres;
	TArray<FDebugRenderSceneProxy::FDebugLine> StoredLines;
	TArray<FDebugRenderSceneProxy::FText3d> StoredTexts;
	TArray<FDebugRenderSceneProxy::FArrowLine> StoredArrows;

	FArcPathDebugDrawData Data;
};
