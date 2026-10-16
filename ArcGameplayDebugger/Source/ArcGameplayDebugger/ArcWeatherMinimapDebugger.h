// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "imgui.h"

class UArcWeatherSubsystem;

class FArcWeatherMinimapDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	static UWorld* GetDebugWorld();
	UArcWeatherSubsystem* GetWeatherSubsystem() const;

	// --- Canvas drawing ---
	void DrawCells();
	void DrawRegionVolumes();
	void DrawPlayerCell();
	void DrawHUD();
	void DrawSelectedCellPanel();
	void HandleInput();

	// --- Coordinate conversion ---
	ImVec2 WorldToScreen(float WorldX, float WorldY) const;
	FVector2D ScreenToWorld(const ImVec2& ScreenPos) const;

	// --- Effective climate with season ---
	struct FEffectiveClimate
	{
		float Temperature;
		float Humidity;
	};
	FEffectiveClimate ComputeEffective(const struct FArcWeatherCell& Cell, const UArcWeatherSubsystem* Sub) const;

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
	bool bShowRegions = true;
	bool bShowLabels = true;

	// --- Visualization mode ---
	enum class EVisMode : uint8 { Temperature, Humidity, Combined };
	EVisMode VisMode = EVisMode::Temperature;

	// --- Temperature display range ---
	float TempRangeMin = -20.f;
	float TempRangeMax = 40.f;

	// --- Selected cell ---
	bool bHasSelectedCell = false;
	FIntVector SelectedCellCoords = FIntVector::ZeroValue;

	// --- Paint mode ---
	float PaintTempOffset = 0.f;
	float PaintHumOffset = 0.f;

	// --- Hovered cell (for tooltips) ---
	bool bHasHoveredCell = false;
	FIntVector HoveredCellCoords = FIntVector::ZeroValue;

	// --- Stats (computed each frame) ---
	int32 CellCount = 0;
};
