// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "imgui.h"

class UArcMassSpatialHashSubsystem;

class FArcSpatialHashMinimapDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	static UWorld* GetDebugWorld();
	UArcMassSpatialHashSubsystem* GetSpatialHashSubsystem() const;

	// --- Canvas drawing ---
	void DrawGrid();
	void DrawEntities();
	void DrawHUD();
	void HandleInput();

	// --- Coordinate conversion ---
	ImVec2 WorldToScreen(float WorldX, float WorldY) const;
	FVector2D ScreenToWorld(const ImVec2& ScreenPos) const;

	// --- View state ---
	FVector2D ViewOffset = FVector2D::ZeroVector; // Center of view in world units
	float Zoom = 0.05f; // Pixels per world unit
	float MinZoom = 0.001f;
	float MaxZoom = 1.0f;

	// --- Canvas state (set each frame) ---
	ImVec2 CanvasPos = {0, 0};   // Top-left of canvas in screen space
	ImVec2 CanvasSize = {0, 0};  // Size of canvas area
	ImVec2 CanvasCenter = {0, 0};

	// --- Interaction state ---
	bool bIsPanning = false;
	ImVec2 PanStartMouse = {0, 0};
	FVector2D PanStartOffset = FVector2D::ZeroVector;

	// --- Stats ---
	int32 TotalEntities = 0;
	int32 OccupiedCells = 0;
	float CellSize = 0.0f;

	// --- Hovered entity ---
	bool bHasHoveredEntity = false;
	bool bHoveredIsPlayer = false;
	FVector HoveredEntityPos = FVector::ZeroVector;
	int32 HoveredEntityIndex = INDEX_NONE;
};
