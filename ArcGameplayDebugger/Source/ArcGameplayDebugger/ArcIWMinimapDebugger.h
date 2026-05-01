// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "imgui.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AActor;
class AArcIWMassISMPartitionActor;
class UArcIWMinimapDebuggerDrawComponent;
class UArcIWVisualizationSubsystem;
struct FMassEntityManager;

/**
 * ImGui minimap debugger for the ArcInstancedWorld plugin.
 *
 * Six grid overlay modes:
 *   - World Partition cells (large cells where partition actors live)
 *   - Mesh-add radius (visualization grid cells where mesh instances are created)
 *   - Mesh-remove radius (hysteresis band for mesh instance removal)
 *   - Physics-add radius (cells where physics bodies are attached)
 *   - Physics-remove radius (hysteresis band for physics body removal)
 *   - Dehydration radius (cells beyond which entities are dehydrated)
 *
 * Nine entity states: Game (0), Mesh (1), MeshPhysics (2), Actor (3), ActorKeepHydrated (4),
 * SimpleMesh (5), SkinnedMesh (6), SkinnedMeshPhysics (7), SimpleSkinnedMesh (8).
 *
 * Also supports drawing entity positions in the 3D viewport.
 */
class FArcIWMinimapDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	/** When true, draws entity debug shapes in the 3D viewport. Called from Tick. */
	void DrawViewportOverlay(UWorld* World);

	bool bShow = false;

private:
	static UWorld* GetDebugWorld();
	UArcIWVisualizationSubsystem* GetVisualizationSubsystem() const;
	FMassEntityManager* GetEntityManager() const;

	// --- Canvas drawing ---
	void DrawGrid();
	void DrawEntities();
	void DrawHUD();
	void HandleInput();

	// --- Coordinate conversion ---
	ImVec2 WorldToScreen(float WorldX, float WorldY) const;
	FVector2D ScreenToWorld(const ImVec2& ScreenPos) const;

	// --- View state ---
	FVector2D ViewOffset = FVector2D::ZeroVector;
	float Zoom = 0.01f;
	float MinZoom = 0.0005f;
	float MaxZoom = 1.0f;

	// --- Canvas state (set each frame) ---
	ImVec2 CanvasPos = {0, 0};
	ImVec2 CanvasSize = {0, 0};
	ImVec2 CanvasCenter = {0, 0};

	// --- Interaction state ---
	bool bIsPanning = false;
	ImVec2 PanStartMouse = {0, 0};
	FVector2D PanStartOffset = FVector2D::ZeroVector;

	// --- Display toggles ---
	bool bShowWPCells = true;
	bool bShowMeshAddGrid = true;
	bool bShowMeshRemoveGrid = false;
	bool bShowActorGrid = true;
	bool bShowPhysicsAddGrid = true;
	bool bShowPhysicsRemoveGrid = false;
	bool bShowDehydrationGrid = false;
	bool bShowEntities = true;
	bool bShowVisEntities = false;
	bool bDrawInViewport = false;
	bool bShowStaticMeshEntities = true;
	bool bShowSkinnedMeshEntities = true;
	bool bShowSourceEntities = true;

	// --- Config (updated from subsystem each frame) ---
	float WPCellSize = 12800.0f;

	// --- Stats ---
	int32 EntityCount = 0;
	int32 GameEntityCount = 0;
	int32 MeshEntityCount = 0;
	int32 PhysicsEntityCount = 0;
	int32 ActorEntityCount = 0;
	int32 KeepHydratedCount = 0;
	int32 SimpleMeshEntityCount = 0;
	int32 SkinnedMeshEntityCount = 0;
	int32 SkinnedPhysicsEntityCount = 0;
	int32 SimpleSkinnedMeshEntityCount = 0;
	int32 VisHolderCount = 0;
	int32 OccupiedCellCount = 0;
	int32 SourceEntityCount = 0;

	// --- Hovered entity ---
	bool bHasHoveredEntity = false;
	FVector HoveredEntityPos = FVector::ZeroVector;
	int32 HoveredEntityIndex = INDEX_NONE;
	int32 HoveredEntityState = 0; // 0=Game, 1=Mesh, 2=MeshPhysics, 3=Actor, 4=ActorKeepHydrated, 5=SimpleMesh, 6=SkinnedMesh, 7=SkinnedMeshPhysics, 8=SimpleSkinnedMesh

	// --- Draw component ---
	void EnsureDrawActor(UWorld* World);
	void DestroyDrawActor();

	TWeakObjectPtr<AActor> DrawActor;
	TWeakObjectPtr<UArcIWMinimapDebuggerDrawComponent> DrawComponent;
};
