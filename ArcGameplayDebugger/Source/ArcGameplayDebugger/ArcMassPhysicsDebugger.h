// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/EntityHandle.h"
#include "Engine/HitResult.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AActor;
class UArcMassPhysicsDebuggerDrawComponent;
class UWorld;
struct FArcMassPhysicsDebugDrawData;
struct FBodyInstance;

class FArcMassPhysicsDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	void PerformLineTrace();
	void DrawInfoPanel();
	void DrawForceControls();
	void DrawVisualizationToggles();
	void DrawWorldVisualization();
	void CollectBodyShapes(FBodyInstance* Body, FColor Color, FArcMassPhysicsDebugDrawData& OutData);
	void CollectAllPhysicsBodies(FArcMassPhysicsDebugDrawData& OutData);
	void EnsureDrawActor(UWorld* World);
	void DestroyDrawActor();

	FHitResult CachedHitResult;
	FMassEntityHandle DetectedEntity;
	FBodyInstance* DetectedBody = nullptr;
	bool bHasHit = false;

	float ImpulseMagnitude = 10000.f;
	float ForceMagnitude = 50000.f;
	float TraceDistance = 10000.f;

	bool bDrawBounds = true;
	bool bDrawCollisionShapes = true;
	bool bDrawHitLine = true;
	bool bDrawAllBodies = false;

	TWeakObjectPtr<AActor> DrawActor;
	TWeakObjectPtr<UArcMassPhysicsDebuggerDrawComponent> DrawComponent;
};
