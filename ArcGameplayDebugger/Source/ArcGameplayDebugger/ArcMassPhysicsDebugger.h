// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityHandle.h"
#include "Engine/HitResult.h"

class UWorld;
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
	void DrawCollisionShapes(UWorld* World, const FTransform& BodyTransform);

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
};
