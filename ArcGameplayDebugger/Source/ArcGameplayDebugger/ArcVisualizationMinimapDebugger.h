// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "imgui.h"
#include "MassEntityQuery.h"

class UArcEntityVisualizationSubsystem;
class UArcMobileVisSubsystem;
struct FMassEntityManager;
struct FMassExecutionContext;

class FArcVisualizationMinimapDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	static UWorld* GetDebugWorld();
	UArcEntityVisualizationSubsystem* GetStaticVisSubsystem() const;
	UArcMobileVisSubsystem* GetMobileVisSubsystem() const;
	FMassEntityManager* GetEntityManager() const;

	// --- Canvas drawing ---
	void DrawGrid();
	void DrawGridLines(float CellSize, ImU32 LineColor);
	void DrawEntities();
	void DrawSourceEntities();
	void DrawRadiusCircles();
	void DrawHUD();
	void HandleInput();

	// --- Coordinate conversion ---
	ImVec2 WorldToScreen(float WorldX, float WorldY) const;
	FVector2D ScreenToWorld(const ImVec2& ScreenPos) const;

	// --- View state ---
	FVector2D ViewOffset = FVector2D::ZeroVector;
	float Zoom = 0.05f;
	float MinZoom = 0.001f;
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
	bool bShowMeshGrid = true;
	bool bShowPhysicsGrid = true;
	bool bShowMobileGrid = true;
	bool bShowActiveCells = true;

	// --- Stats ---
	int32 MeshGridEntityCount = 0;
	int32 MeshGridCellCount = 0;
	int32 PhysicsGridEntityCount = 0;
	int32 PhysicsGridCellCount = 0;
	int32 MobileEntityCount = 0;
	int32 MobileCellCount = 0;
	int32 PhysicsEntityCount = 0;
	int32 MeshEntityCount = 0;
	int32 SourceEntityCount = 0;
	float MeshGridCellSize = 0.0f;
	float PhysicsGridCellSize = 0.0f;
	float MobileCellSize = 0.0f;

	// --- Source entity positions (collected each frame for radius drawing) ---
	TArray<FVector> SourceEntityPositions;

	// --- Hovered entity ---
	bool bHasHoveredEntity = false;
	bool bHoveredIsPlayer = false;
	bool bHoveredIsMobile = false;
	FVector HoveredEntityPos = FVector::ZeroVector;
	int32 HoveredEntityIndex = INDEX_NONE;
};
