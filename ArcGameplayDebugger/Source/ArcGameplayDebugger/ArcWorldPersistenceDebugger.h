// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityHandle.h"
#include "imgui.h"

class UArcMassEntityPersistenceSubsystem;
struct FMassEntityManager;

class FArcWorldPersistenceDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	static UWorld* GetDebugWorld();
	UArcMassEntityPersistenceSubsystem* GetPersistenceSubsystem() const;
	FMassEntityManager* GetEntityManager() const;

	// --- Panel drawing ---
	void DrawToolbar();
	void DrawEntityList(float Width);
	void DrawMinimap(float Width);
	void DrawEntityDetails(float Width);
	void DrawWipeConfirmationPopup();

	// --- Minimap helpers ---
	void HandleMinimapInput();
	ImVec2 WorldToScreen(float WorldX, float WorldY) const;
	FVector2D ScreenToWorld(const ImVec2& ScreenPos) const;

	// --- Operations ---
	void SaveAll();
	void LoadAll();
	void WipeSaveData();

	// --- Cached data (rebuilt each frame) ---
	struct FEntityEntry
	{
		FMassEntityHandle Handle;
		FVector Position;
		bool bIsSource = false;
	};
	TArray<FEntityEntry> CachedEntities;
	void RebuildEntityCache();

	// --- Selection ---
	FMassEntityHandle SelectedEntity;
	bool bHasSelection = false;

	// --- Entity list state ---
	char FilterBuf[128] = {};

	// --- Minimap view state ---
	FVector2D ViewOffset = FVector2D::ZeroVector;
	float Zoom = 0.05f;
	float MinZoom = 0.001f;
	float MaxZoom = 1.0f;

	// --- Minimap canvas state (set each frame) ---
	ImVec2 CanvasPos = {0, 0};
	ImVec2 CanvasSize = {0, 0};
	ImVec2 CanvasCenter = {0, 0};

	// --- Minimap interaction state ---
	bool bIsPanning = false;
	ImVec2 PanStartMouse = {0, 0};
	FVector2D PanStartOffset = FVector2D::ZeroVector;

	// --- Status ---
	FString StatusMessage;
	double StatusMessageExpireTime = 0.0;

	// --- Wipe confirmation ---
	bool bShowWipeConfirmation = false;
};
