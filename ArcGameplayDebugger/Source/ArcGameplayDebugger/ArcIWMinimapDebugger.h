// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "imgui.h"

class UArcIWVisualizationSubsystem;
struct FMassEntityManager;

/**
 * ImGui minimap debugger for the ArcInstancedWorld plugin.
 *
 * Three grid overlay modes:
 *   - World Partition cells (large cells where partition actors live)
 *   - ISM active radius (visualization grid cells where ISM instances are created)
 *   - Actor active radius (future: cells where actors are hydrated)
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
	bool bShowISMGrid = true;
	bool bShowISMActive = true;
	bool bShowActorGrid = true;
	bool bShowDehydrationGrid = false;
	bool bShowEntities = true;
	bool bDrawInViewport = false;

	// --- Config (updated from subsystem each frame) ---
	float WPCellSize = 12800.0f;
	float ISMCellSize = 2000.0f;

	// --- Stats ---
	int32 EntityCount = 0;
	int32 ISMEntityCount = 0;
	int32 ActorEntityCount = 0;
	int32 InactiveEntityCount = 0;
	int32 OccupiedCellCount = 0;

	// --- Hovered entity ---
	bool bHasHoveredEntity = false;
	FVector HoveredEntityPos = FVector::ZeroVector;
	int32 HoveredEntityIndex = INDEX_NONE;
	bool bHoveredHasISM = false;
	bool bHoveredIsActor = false;
	bool bHoveredKeepHydrated = false;
};
