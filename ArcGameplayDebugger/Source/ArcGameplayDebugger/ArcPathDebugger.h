// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/EntityHandle.h"
#include "MassEntityTypes.h"
#include "UObject/WeakObjectPtr.h"

class AActor;
class UWorld;
class UArcPathDebuggerDrawComponent;
struct FArcPathDebugDrawData;

class FArcPathDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	void DrawEntityListPanel();
	void DrawEntityDetailPanel();
	void DrawPathInWorld();
	void DrawNavMeshOverlay(UWorld* World, FArcPathDebugDrawData& OutData);

	void RefreshEntityList();
	void EnsureDrawActor(UWorld* World);
	void DestroyDrawActor();

	struct FEntityEntry
	{
		FMassEntityHandle Entity;
		FString Label;
		FVector Location = FVector::ZeroVector;
	};

	TArray<FEntityEntry> CachedEntities;
	int32 SelectedEntityIndex = INDEX_NONE;
	char EntityFilterBuf[256] = {};
	float LastRefreshTime = 0.f;

	bool bDrawMoveTarget = true;
	bool bDrawVelocity = true;
	bool bDrawSteering = true;
	bool bDrawForces = true;
	bool bDrawShortPath = true;
	bool bDrawAvoidance = true;
	bool bDrawNavMesh = false;
	float NavMeshZOffset = 10.f;

	TWeakObjectPtr<AActor> DrawActor;
	TWeakObjectPtr<UArcPathDebuggerDrawComponent> DrawComponent;

};
