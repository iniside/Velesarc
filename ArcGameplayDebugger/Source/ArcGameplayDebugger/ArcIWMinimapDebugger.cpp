// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWMinimapDebugger.h"

#include "imgui.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "ArcInstancedWorld/Visualization/ArcIWVisualizationSubsystem.h"
#include "ArcInstancedWorld/ArcIWTypes.h"
#include "ArcInstancedWorld/ArcIWSettings.h"
#include "ArcInstancedWorld/Visualization/ArcIWMassISMPartitionActor.h"
#include "ArcMass/Physics/ArcMassPhysicsBody.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityFragments.h"
#include "MassEntityView.h"
#include "MassEntityQuery.h"
#include "MassExecutionContext.h"
#include "EngineUtils.h"
#include "Mesh/MassEngineMeshFragments.h"
#include "ArcMass/Visualization/ArcMassEntityVisualization.h"

// ====================================================================
// Colors
// ====================================================================

namespace Arcx::GameplayDebugger::IW::Minimap
{
	// World Partition cells
	static const ImU32 WPCellBorderColor = IM_COL32(200, 80, 80, 140);
	static const ImU32 WPCellFillColor = IM_COL32(200, 80, 80, 20);

	// MeshAdd grid — bright green
	static const ImU32 MeshAddGridBorderColor = IM_COL32(60, 200, 60, 90);
	static const ImU32 MeshAddActiveFillColor = IM_COL32(50, 180, 50, 45);

	// MeshRemove grid — dark olive green
	static const ImU32 MeshRemoveGridBorderColor = IM_COL32(80, 140, 40, 70);
	static const ImU32 MeshRemoveActiveFillColor = IM_COL32(70, 120, 30, 35);

	// PhysicsAdd grid — bright cyan/teal
	static const ImU32 PhysicsAddGridBorderColor = IM_COL32(40, 180, 180, 90);
	static const ImU32 PhysicsAddActiveFillColor = IM_COL32(30, 160, 160, 45);

	// PhysicsRemove grid — dark cyan
	static const ImU32 PhysicsRemoveGridBorderColor = IM_COL32(30, 120, 130, 70);
	static const ImU32 PhysicsRemoveActiveFillColor = IM_COL32(25, 100, 110, 35);

	// Actor grid — blue
	static const ImU32 ActorGridBorderColor = IM_COL32(70, 110, 200, 90);
	static const ImU32 ActorActiveFillColor = IM_COL32(50, 90, 200, 50);

	// Dehydration grid — orange
	static const ImU32 DehydrationGridBorderColor = IM_COL32(180, 120, 50, 80);
	static const ImU32 DehydrationActiveFillColor = IM_COL32(160, 110, 40, 35);

	// Player cell highlight
	static const ImU32 PlayerCellColor = IM_COL32(255, 80, 80, 60);

	// Entity states — 9 states
	static const ImU32 EntityGameColor = IM_COL32(220, 200, 50, 255);
	static const ImU32 EntityGameHoveredColor = IM_COL32(255, 255, 100, 255);
	static const ImU32 EntityMeshColor = IM_COL32(80, 220, 80, 255);
	static const ImU32 EntityMeshHoveredColor = IM_COL32(120, 255, 120, 255);
	static const ImU32 EntitySimpleMeshColor = IM_COL32(230, 150, 50, 255);
	static const ImU32 EntitySimpleMeshHoveredColor = IM_COL32(255, 190, 80, 255);
	static const ImU32 EntityPhysicsColor = IM_COL32(40, 200, 200, 255);
	static const ImU32 EntityPhysicsHoveredColor = IM_COL32(80, 240, 240, 255);
	static const ImU32 EntityActorColor = IM_COL32(80, 140, 255, 255);
	static const ImU32 EntityActorHoveredColor = IM_COL32(120, 180, 255, 255);
	static const ImU32 EntityKeepHydratedColor = IM_COL32(255, 100, 200, 255);
	static const ImU32 EntityKeepHydratedHoveredColor = IM_COL32(255, 150, 230, 255);
	// Skinned mesh variants — purple tones to distinguish from green static meshes
	static const ImU32 EntitySkinnedMeshColor = IM_COL32(180, 80, 220, 255);
	static const ImU32 EntitySkinnedMeshHoveredColor = IM_COL32(210, 120, 255, 255);
	static const ImU32 EntitySkinnedPhysicsColor = IM_COL32(140, 60, 200, 255);
	static const ImU32 EntitySkinnedPhysicsHoveredColor = IM_COL32(170, 100, 240, 255);
	static const ImU32 EntitySimpleSkinnedMeshColor = IM_COL32(200, 100, 180, 255);
	static const ImU32 EntitySimpleSkinnedMeshHoveredColor = IM_COL32(230, 140, 210, 255);

	// Source entity (streaming origin)
	static const ImU32 SourceEntityColor = IM_COL32(255, 255, 255, 255);

	// General
	static const ImU32 BackgroundColor = IM_COL32(20, 20, 25, 240);
	static const ImU32 OriginColor = IM_COL32(255, 60, 60, 180);
	static const ImU32 HUDTextColor = IM_COL32(200, 200, 200, 255);

	ImU32 CellColorByCount(int32 Count, ImU32 BaseColor)
	{
		const float Alpha = FMath::Clamp(0.15f + 0.1f * Count, 0.15f, 0.8f);
		const uint8 A = static_cast<uint8>(Alpha * 255.f);
		return (BaseColor & 0x00FFFFFF) | (static_cast<ImU32>(A) << 24);
	}
}

// ====================================================================
// Lifecycle
// ====================================================================

void FArcIWMinimapDebugger::Initialize()
{
	ViewOffset = FVector2D::ZeroVector;
	Zoom = 0.01f;
	bIsPanning = false;
	bHasHoveredEntity = false;
	EntityCount = 0;
	GameEntityCount = 0;
	MeshEntityCount = 0;
	PhysicsEntityCount = 0;
	ActorEntityCount = 0;
	KeepHydratedCount = 0;
	SimpleMeshEntityCount = 0;
	SkinnedMeshEntityCount = 0;
	SkinnedPhysicsEntityCount = 0;
	SimpleSkinnedMeshEntityCount = 0;
	VisHolderCount = 0;
	OccupiedCellCount = 0;
	SourceEntityCount = 0;
}

void FArcIWMinimapDebugger::Uninitialize()
{
}

// ====================================================================
// Helpers
// ====================================================================

UWorld* FArcIWMinimapDebugger::GetDebugWorld()
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return nullptr;
	}
	return GEngine->GameViewport->GetWorld();
}

UArcIWVisualizationSubsystem* FArcIWMinimapDebugger::GetVisualizationSubsystem() const
{
	UWorld* World = GetDebugWorld();
	return World ? World->GetSubsystem<UArcIWVisualizationSubsystem>() : nullptr;
}

FMassEntityManager* FArcIWMinimapDebugger::GetEntityManager() const
{
	UWorld* World = GetDebugWorld();
	if (!World)
	{
		return nullptr;
	}
	UMassEntitySubsystem* MassSub = World->GetSubsystem<UMassEntitySubsystem>();
	return MassSub ? &MassSub->GetMutableEntityManager() : nullptr;
}

ImVec2 FArcIWMinimapDebugger::WorldToScreen(float WorldX, float WorldY) const
{
	return ImVec2(
		CanvasCenter.x + (WorldX - static_cast<float>(ViewOffset.X)) * Zoom,
		CanvasCenter.y - (WorldY - static_cast<float>(ViewOffset.Y)) * Zoom
	);
}

FVector2D FArcIWMinimapDebugger::ScreenToWorld(const ImVec2& ScreenPos) const
{
	return FVector2D(
		ViewOffset.X + (ScreenPos.x - CanvasCenter.x) / Zoom,
		ViewOffset.Y - (ScreenPos.y - CanvasCenter.y) / Zoom
	);
}

// ====================================================================
// Main Draw
// ====================================================================

void FArcIWMinimapDebugger::Draw()
{
	ImGui::SetNextWindowSize(ImVec2(700, 700), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("ArcIW Minimap", &bShow, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::End();
		return;
	}

	// Update cell sizes from subsystem and world
	UArcIWVisualizationSubsystem* VisSub = GetVisualizationSubsystem();
	{
		UWorld* WPWorld = GetDebugWorld();
		const UArcIWSettings* Settings = UArcIWSettings::Get();
		WPCellSize = static_cast<float>(WPWorld
			? AArcIWMassISMPartitionActor::GetGridCellSize(WPWorld, Settings->DefaultGridName)
			: Settings->DefaultGridCellSize);
	}

	// Count vis holder entities
	VisHolderCount = 0;
	if (bShowVisEntities)
	{
		UWorld* CountWorld = GetDebugWorld();
		if (CountWorld)
		{
			for (TActorIterator<AArcIWMassISMPartitionActor> It(CountWorld); It; ++It)
			{
				const TArray<FArcIWActorClassData>& Entries = It->GetActorClassEntries();
				for (const FArcIWActorClassData& Entry : Entries)
				{
					VisHolderCount += Entry.MeshDescriptors.Num();
				}
			}
		}
	}

	DrawHUD();
	ImGui::Separator();

	// Canvas area = remaining window space
	CanvasPos = ImGui::GetCursorScreenPos();
	CanvasSize = ImGui::GetContentRegionAvail();

	if (CanvasSize.x < 1.0f || CanvasSize.y < 1.0f)
	{
		ImGui::End();
		return;
	}

	CanvasCenter = ImVec2(
		CanvasPos.x + CanvasSize.x * 0.5f,
		CanvasPos.y + CanvasSize.y * 0.5f
	);

	ImGui::InvisibleButton("##iwcanvas", CanvasSize, ImGuiButtonFlags_MouseButtonMiddle);
	const bool bCanvasHovered = ImGui::IsItemHovered();

	HandleInput();

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->PushClipRect(CanvasPos, ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y), true);

	// Background
	DrawList->AddRectFilled(CanvasPos, ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y),
		Arcx::GameplayDebugger::IW::Minimap::BackgroundColor);

	// Origin crosshair
	{
		const ImVec2 Origin = WorldToScreen(0.0f, 0.0f);
		DrawList->AddLine(ImVec2(CanvasPos.x, Origin.y), ImVec2(CanvasPos.x + CanvasSize.x, Origin.y),
			Arcx::GameplayDebugger::IW::Minimap::OriginColor, 1.0f);
		DrawList->AddLine(ImVec2(Origin.x, CanvasPos.y), ImVec2(Origin.x, CanvasPos.y + CanvasSize.y),
			Arcx::GameplayDebugger::IW::Minimap::OriginColor, 1.0f);
	}

	DrawGrid();

	if (bShowEntities)
	{
		DrawEntities();
	}

	DrawList->PopClipRect();

	// Tooltip for hovered entity
	if (bHasHoveredEntity && bCanvasHovered)
	{
		ImGui::BeginTooltip();
		const char* StateLabels[] = { "[Game]", "[Mesh]", "[Mesh+Physics]", "[Actor]", "[Actor, KeepHydrated]", "[SimpleMesh]", "[SkinnedMesh]", "[SkinnedMesh+Physics]", "[SimpleSkinnedMesh]" };
		const char* StateLabel = (HoveredEntityState >= 0 && HoveredEntityState <= 8) ? StateLabels[HoveredEntityState] : "[Unknown]";
		ImGui::Text("Entity: E%d %s", HoveredEntityIndex, StateLabel);
		ImGui::Text("Position: (%.0f, %.0f, %.0f)", HoveredEntityPos.X, HoveredEntityPos.Y, HoveredEntityPos.Z);
		ImGui::EndTooltip();
	}

	ImGui::End();

	// Viewport overlay
	if (bDrawInViewport)
	{
		UWorld* World = GetDebugWorld();
		if (World)
		{
			DrawViewportOverlay(World);
		}
	}
}

// ====================================================================
// Input Handling
// ====================================================================

void FArcIWMinimapDebugger::HandleInput()
{
	const ImGuiIO& IO = ImGui::GetIO();
	const bool bCanvasHovered = ImGui::IsItemHovered();

	// Panning (middle mouse)
	if (bCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
	{
		bIsPanning = true;
		PanStartMouse = IO.MousePos;
		PanStartOffset = ViewOffset;
	}

	if (bIsPanning)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
		{
			const float DeltaX = IO.MousePos.x - PanStartMouse.x;
			const float DeltaY = IO.MousePos.y - PanStartMouse.y;
			ViewOffset.X = PanStartOffset.X - DeltaX / Zoom;
			ViewOffset.Y = PanStartOffset.Y + DeltaY / Zoom;
		}
		else
		{
			bIsPanning = false;
		}
	}

	// Zooming (scroll wheel, centered on mouse)
	if (bCanvasHovered && FMath::Abs(IO.MouseWheel) > 0.0f)
	{
		const FVector2D WorldBeforeZoom = ScreenToWorld(IO.MousePos);

		const float ZoomFactor = 1.15f;
		if (IO.MouseWheel > 0.0f)
		{
			Zoom = FMath::Min(Zoom * ZoomFactor, MaxZoom);
		}
		else
		{
			Zoom = FMath::Max(Zoom / ZoomFactor, MinZoom);
		}

		const FVector2D WorldAfterZoom = ScreenToWorld(IO.MousePos);
		ViewOffset.X -= (WorldAfterZoom.X - WorldBeforeZoom.X);
		ViewOffset.Y -= (WorldAfterZoom.Y - WorldBeforeZoom.Y);
	}
}

// ====================================================================
// Grid Drawing
// ====================================================================

namespace Arcx::GameplayDebugger::IW::MinimapDetail
{
	void DrawGridCells(ImDrawList* DrawList, float CellSize, ImU32 FillColor, ImU32 BorderColor,
		const ImVec2& CanvasPos, const ImVec2& CanvasSize,
		const TFunction<ImVec2(float, float)>& WorldToScreenFn,
		const TFunction<FVector2D(const ImVec2&)>& ScreenToWorldFn)
	{
		// Compute visible world bounds
		const FVector2D WorldMin = ScreenToWorldFn(CanvasPos);
		const FVector2D WorldMax = ScreenToWorldFn(ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y));

		const int32 MinCellX = FMath::FloorToInt(FMath::Min(WorldMin.X, WorldMax.X) / CellSize);
		const int32 MaxCellX = FMath::CeilToInt(FMath::Max(WorldMin.X, WorldMax.X) / CellSize);
		const int32 MinCellY = FMath::FloorToInt(FMath::Min(WorldMin.Y, WorldMax.Y) / CellSize);
		const int32 MaxCellY = FMath::CeilToInt(FMath::Max(WorldMin.Y, WorldMax.Y) / CellSize);

		// Limit draw count to prevent performance issues at extreme zoom-out
		const int32 MaxCellsToDraw = 200;
		const int32 CellCountX = MaxCellX - MinCellX;
		const int32 CellCountY = MaxCellY - MinCellY;
		if (CellCountX * CellCountY > MaxCellsToDraw)
		{
			return;
		}

		for (int32 CX = MinCellX; CX <= MaxCellX; ++CX)
		{
			for (int32 CY = MinCellY; CY <= MaxCellY; ++CY)
			{
				const float WorldMinX = CX * CellSize;
				const float WorldMinY = CY * CellSize;
				const float WorldMaxX = WorldMinX + CellSize;
				const float WorldMaxY = WorldMinY + CellSize;

				const ImVec2 ScreenMin = WorldToScreenFn(WorldMinX, WorldMaxY);
				const ImVec2 ScreenMax = WorldToScreenFn(WorldMaxX, WorldMinY);

				DrawList->AddRectFilled(ScreenMin, ScreenMax, FillColor);
				DrawList->AddRect(ScreenMin, ScreenMax, BorderColor, 0.0f, 0, 1.0f);
			}
		}
	}
}

void FArcIWMinimapDebugger::DrawGrid()
{
	using namespace Arcx::GameplayDebugger::IW::Minimap;

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	EntityCount = 0;
	OccupiedCellCount = 0;

	TFunction<ImVec2(float, float)> W2S = [this](float X, float Y) { return WorldToScreen(X, Y); };
	TFunction<FVector2D(const ImVec2&)> S2W = [this](const ImVec2& P) { return ScreenToWorld(P); };

	UArcIWVisualizationSubsystem* VisSub = GetVisualizationSubsystem();
	const UArcIWSettings* IWSettings = UArcIWSettings::Get();
	const float MeshCellSz = VisSub ? VisSub->GetMeshCellSize() : IWSettings->MeshCellSize;
	const float PhysicsCellSz = VisSub ? VisSub->GetPhysicsCellSize() : IWSettings->PhysicsCellSize;
	const float ActorCellSz = VisSub ? VisSub->GetActorCellSize() : IWSettings->ActorCellSize;

	// --- World Partition cells (largest grid) ---
	if (bShowWPCells)
	{
		Arcx::GameplayDebugger::IW::MinimapDetail::DrawGridCells(DrawList, WPCellSize, WPCellFillColor, WPCellBorderColor, CanvasPos, CanvasSize, W2S, S2W);
	}

	// Helper: convert cell coords to screen rect, returns false if offscreen
	auto CellToScreenRect = [this](const FIntVector& Cell, float InCellSize, ImVec2& OutMin, ImVec2& OutMax) -> bool
	{
		const float WorldMinX = Cell.X * InCellSize;
		const float WorldMinY = Cell.Y * InCellSize;
		OutMin = WorldToScreen(WorldMinX, WorldMinY + InCellSize);
		OutMax = WorldToScreen(WorldMinX + InCellSize, WorldMinY);
		return OutMax.x >= CanvasPos.x && OutMin.x <= CanvasPos.x + CanvasSize.x
			&& OutMax.y >= CanvasPos.y && OutMin.y <= CanvasPos.y + CanvasSize.y;
	};

	// Count total entities from all partition actors
	{
		UWorld* CountWorld = GetDebugWorld();
		if (CountWorld)
		{
			for (TActorIterator<AArcIWMassISMPartitionActor> It(CountWorld); It; ++It)
			{
				EntityCount += It->GetSpawnedEntities().Num();
			}
		}
	}

	// Build set of occupied cells for lookup
	const TMap<FIntVector, TArray<FMassEntityHandle>>* MeshCellEntitiesPtr = nullptr;
	const TMap<FIntVector, TArray<FMassEntityHandle>>* PhysicsCellEntitiesPtr = nullptr;
	const TMap<FIntVector, TArray<FMassEntityHandle>>* ActorCellEntitiesPtr = nullptr;
	if (VisSub)
	{
		MeshCellEntitiesPtr = &VisSub->GetMeshCellEntities();
		PhysicsCellEntitiesPtr = &VisSub->GetPhysicsCellEntities();
		ActorCellEntitiesPtr = &VisSub->GetActorCellEntities();
		OccupiedCellCount = MeshCellEntitiesPtr->Num();
	}

	const FIntVector MeshPlayerCell = VisSub ? VisSub->GetLastMeshPlayerCell() : FIntVector(TNumericLimits<int32>::Max());
	const FIntVector PhysicsPlayerCell = VisSub ? VisSub->GetLastPhysicsPlayerCell() : FIntVector(TNumericLimits<int32>::Max());
	const FIntVector ActorPlayerCell = VisSub ? VisSub->GetLastActorPlayerCell() : FIntVector(TNumericLimits<int32>::Max());

	// --- MeshRemove grid (drawn first = furthest back) ---
	if (bShowMeshRemoveGrid && VisSub && MeshPlayerCell.X != TNumericLimits<int32>::Max())
	{
		TArray<FIntVector> Cells;
		UArcIWVisualizationSubsystem::GetCellsInRadius(MeshPlayerCell, VisSub->GetMeshRemoveRadiusCells(), Cells);
		for (const FIntVector& Cell : Cells)
		{
			ImVec2 ScreenMin, ScreenMax;
			if (!CellToScreenRect(Cell, MeshCellSz, ScreenMin, ScreenMax)) continue;
			if (MeshCellEntitiesPtr && MeshCellEntitiesPtr->Contains(Cell))
			{
				DrawList->AddRectFilled(ScreenMin, ScreenMax, MeshRemoveActiveFillColor);
			}
			DrawList->AddRect(ScreenMin, ScreenMax, MeshRemoveGridBorderColor, 0.0f, 0, 1.0f);
		}
	}

	// --- MeshAdd grid ---
	if (bShowMeshAddGrid && VisSub && MeshPlayerCell.X != TNumericLimits<int32>::Max())
	{
		TArray<FIntVector> Cells;
		UArcIWVisualizationSubsystem::GetCellsInRadius(MeshPlayerCell, VisSub->GetMeshAddRadiusCells(), Cells);
		for (const FIntVector& Cell : Cells)
		{
			ImVec2 ScreenMin, ScreenMax;
			if (!CellToScreenRect(Cell, MeshCellSz, ScreenMin, ScreenMax)) continue;
			if (MeshCellEntitiesPtr && MeshCellEntitiesPtr->Contains(Cell))
			{
				DrawList->AddRectFilled(ScreenMin, ScreenMax, MeshAddActiveFillColor);
			}
			DrawList->AddRect(ScreenMin, ScreenMax, MeshAddGridBorderColor, 0.0f, 0, 1.0f);
		}
	}

	// --- PhysicsRemove grid ---
	if (bShowPhysicsRemoveGrid && VisSub && PhysicsPlayerCell.X != TNumericLimits<int32>::Max())
	{
		TArray<FIntVector> Cells;
		UArcIWVisualizationSubsystem::GetCellsInRadius(PhysicsPlayerCell, VisSub->GetPhysicsRemoveRadiusCells(), Cells);
		for (const FIntVector& Cell : Cells)
		{
			ImVec2 ScreenMin, ScreenMax;
			if (!CellToScreenRect(Cell, PhysicsCellSz, ScreenMin, ScreenMax)) continue;
			if (PhysicsCellEntitiesPtr && PhysicsCellEntitiesPtr->Contains(Cell))
			{
				DrawList->AddRectFilled(ScreenMin, ScreenMax, PhysicsRemoveActiveFillColor);
			}
			DrawList->AddRect(ScreenMin, ScreenMax, PhysicsRemoveGridBorderColor, 0.0f, 0, 1.0f);
		}
	}

	// --- PhysicsAdd grid ---
	if (bShowPhysicsAddGrid && VisSub && PhysicsPlayerCell.X != TNumericLimits<int32>::Max())
	{
		TArray<FIntVector> Cells;
		UArcIWVisualizationSubsystem::GetCellsInRadius(PhysicsPlayerCell, VisSub->GetPhysicsAddRadiusCells(), Cells);
		for (const FIntVector& Cell : Cells)
		{
			ImVec2 ScreenMin, ScreenMax;
			if (!CellToScreenRect(Cell, PhysicsCellSz, ScreenMin, ScreenMax)) continue;
			if (PhysicsCellEntitiesPtr && PhysicsCellEntitiesPtr->Contains(Cell))
			{
				DrawList->AddRectFilled(ScreenMin, ScreenMax, PhysicsAddActiveFillColor);
			}
			DrawList->AddRect(ScreenMin, ScreenMax, PhysicsAddGridBorderColor, 0.0f, 0, 1.0f);
		}
	}

	// --- Dehydration grid ---
	if (bShowDehydrationGrid && VisSub && ActorPlayerCell.X != TNumericLimits<int32>::Max())
	{
		TArray<FIntVector> Cells;
		UArcIWVisualizationSubsystem::GetCellsInRadius(ActorPlayerCell, VisSub->GetActorDehydrationRadiusCells(), Cells);
		for (const FIntVector& Cell : Cells)
		{
			ImVec2 ScreenMin, ScreenMax;
			if (!CellToScreenRect(Cell, ActorCellSz, ScreenMin, ScreenMax)) continue;
			if (ActorCellEntitiesPtr && ActorCellEntitiesPtr->Contains(Cell))
			{
				DrawList->AddRectFilled(ScreenMin, ScreenMax, DehydrationActiveFillColor);
			}
			DrawList->AddRect(ScreenMin, ScreenMax, DehydrationGridBorderColor, 0.0f, 0, 1.0f);
		}
	}

	// --- Actor grid (drawn on top) ---
	if (bShowActorGrid && VisSub && ActorPlayerCell.X != TNumericLimits<int32>::Max())
	{
		TArray<FIntVector> Cells;
		UArcIWVisualizationSubsystem::GetCellsInRadius(ActorPlayerCell, VisSub->GetActorHydrationRadiusCells(), Cells);
		for (const FIntVector& Cell : Cells)
		{
			ImVec2 ScreenMin, ScreenMax;
			if (!CellToScreenRect(Cell, ActorCellSz, ScreenMin, ScreenMax)) continue;
			if (ActorCellEntitiesPtr && ActorCellEntitiesPtr->Contains(Cell))
			{
				DrawList->AddRectFilled(ScreenMin, ScreenMax, ActorActiveFillColor);
			}
			DrawList->AddRect(ScreenMin, ScreenMax, ActorGridBorderColor, 0.0f, 0, 1.0f);
		}
	}

	// --- Player cell highlight ---
	if (VisSub && MeshPlayerCell.X != TNumericLimits<int32>::Max())
	{
		ImVec2 ScreenMin, ScreenMax;
		if (CellToScreenRect(MeshPlayerCell, MeshCellSz, ScreenMin, ScreenMax))
		{
			DrawList->AddRectFilled(ScreenMin, ScreenMax, PlayerCellColor);
			DrawList->AddRect(ScreenMin, ScreenMax, IM_COL32(255, 80, 80, 200), 0.0f, 0, 2.0f);
			DrawList->AddText(ImVec2(ScreenMin.x + 2.0f, ScreenMin.y + 2.0f), IM_COL32(255, 100, 100, 220), "Player");
		}
	}
	else if (!VisSub)
	{
		// No subsystem — just draw raw grid lines
		Arcx::GameplayDebugger::IW::MinimapDetail::DrawGridCells(DrawList, MeshCellSz, MeshAddActiveFillColor, MeshAddGridBorderColor, CanvasPos, CanvasSize, W2S, S2W);
	}
}

// ====================================================================
// Entity Drawing
// ====================================================================

void FArcIWMinimapDebugger::DrawEntities()
{
	using namespace Arcx::GameplayDebugger::IW::Minimap;

	UArcIWVisualizationSubsystem* VisSub = GetVisualizationSubsystem();
	FMassEntityManager* EntityManager = GetEntityManager();
	if (!VisSub || !EntityManager)
	{
		return;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImGuiIO& IO = ImGui::GetIO();

	bHasHoveredEntity = false;
	float BestHoverDistSq = 64.0f;
	GameEntityCount = 0;
	MeshEntityCount = 0;
	PhysicsEntityCount = 0;
	ActorEntityCount = 0;
	KeepHydratedCount = 0;
	SimpleMeshEntityCount = 0;
	SkinnedMeshEntityCount = 0;
	SkinnedPhysicsEntityCount = 0;
	SimpleSkinnedMeshEntityCount = 0;

	const float EntityRadius = FMath::Clamp(Zoom * 200.0f, 2.0f, 6.0f);

	UWorld* EntityWorld = GetDebugWorld();
	if (!EntityWorld)
	{
		return;
	}

	// --- Pass 1: Draw all instance transforms as Game-state dots (yellow) ---
	// This covers every loaded entity including ones not yet in any spatial grid.
	for (TActorIterator<AArcIWMassISMPartitionActor> It(EntityWorld); It; ++It)
	{
		const TArray<FArcIWActorClassData>& ClassEntries = It->GetActorClassEntries();
		for (const FArcIWActorClassData& Entry : ClassEntries)
		{
			for (const FTransform& InstanceTransform : Entry.InstanceTransforms)
			{
				const FVector& Position = InstanceTransform.GetLocation();
				const ImVec2 ScreenPos = WorldToScreen(Position.X, Position.Y);

				if (ScreenPos.x < CanvasPos.x - EntityRadius || ScreenPos.x > CanvasPos.x + CanvasSize.x + EntityRadius ||
					ScreenPos.y < CanvasPos.y - EntityRadius || ScreenPos.y > CanvasPos.y + CanvasSize.y + EntityRadius)
				{
					continue;
				}

				++GameEntityCount;
				DrawList->AddCircleFilled(ScreenPos, EntityRadius, EntityGameColor);
			}
		}
	}

	// --- Pass 2: Overdraw active entities with their real state color ---
	// Entities with mesh/physics/actor state get drawn on top of the Game dots.
	for (TActorIterator<AArcIWMassISMPartitionActor> It(EntityWorld); It; ++It)
	{
	for (const FMassEntityHandle& Entity : It->GetSpawnedEntities())
	{
		if (!EntityManager->IsEntityValid(Entity))
		{
			continue;
		}

		const FArcIWInstanceFragment* InstanceFrag = EntityManager->GetFragmentDataPtr<FArcIWInstanceFragment>(Entity);
		if (!InstanceFrag)
		{
			continue;
		}

		// Determine entity state — skip Game (0) since pass 1 already drew those
		// 1=Mesh, 2=MeshPhysics, 3=Actor, 4=ActorKeepHydrated,
		// 5=SimpleMesh, 6=SkinnedMesh, 7=SkinnedMeshPhysics, 8=SimpleSkinnedMesh
		int32 EntityState = 0;
		if (InstanceFrag->bIsActorRepresentation)
		{
			EntityState = InstanceFrag->bKeepHydrated ? 4 : 3;
		}
		else
		{
			bool bHasActiveISM = false;
			for (int32 Id : InstanceFrag->ISMInstanceIds)
			{
				if (Id != INDEX_NONE)
				{
					bHasActiveISM = true;
					break;
				}
			}

			if (bHasActiveISM)
			{
				bool bIsSkinned = false;
				const FArcIWVisConfigFragment* VisConfig =
					EntityManager->GetConstSharedFragmentDataPtr<FArcIWVisConfigFragment>(Entity);
				if (VisConfig)
				{
					for (const FArcIWMeshEntry& MeshEntry : VisConfig->MeshDescriptors)
					{
						if (MeshEntry.IsSkinned())
						{
							bIsSkinned = true;
							break;
						}
					}
				}

				const FArcMassPhysicsBodyFragment* PhysicsBody = EntityManager->GetFragmentDataPtr<FArcMassPhysicsBodyFragment>(Entity);
				bool bHasPhysics = (PhysicsBody && PhysicsBody->Body != nullptr);
				if (bIsSkinned)
				{
					EntityState = bHasPhysics ? 7 : 6;
				}
				else
				{
					EntityState = bHasPhysics ? 2 : 1;
				}
			}
			else
			{
				const FMassRenderPrimitiveFragment* PrimFrag =
					EntityManager->GetFragmentDataPtr<FMassRenderPrimitiveFragment>(Entity);
				if (PrimFrag && PrimFrag->bIsVisible)
				{
					FMassEntityView EntityView(*EntityManager, Entity);
					bool bIsSkinned = EntityView.HasTag<FArcIWSimpleVisSkinnedTag>();
					EntityState = bIsSkinned ? 8 : 5;
				}
			}
		}

		// Skip Game-state entities (already drawn in pass 1)
		if (EntityState == 0)
		{
			continue;
		}

		const FTransformFragment* TransformFrag = EntityManager->GetFragmentDataPtr<FTransformFragment>(Entity);
		if (!TransformFrag)
		{
			continue;
		}

		const FVector& Position = TransformFrag->GetTransform().GetLocation();
		const ImVec2 ScreenPos = WorldToScreen(Position.X, Position.Y);

		if (ScreenPos.x < CanvasPos.x - EntityRadius || ScreenPos.x > CanvasPos.x + CanvasSize.x + EntityRadius ||
			ScreenPos.y < CanvasPos.y - EntityRadius || ScreenPos.y > CanvasPos.y + CanvasSize.y + EntityRadius)
		{
			continue;
		}

		// Filter by mesh type toggles
		bool bIsSkinnedState = (EntityState == 6 || EntityState == 7 || EntityState == 8);
		bool bIsStaticMeshState = (EntityState == 1 || EntityState == 2 || EntityState == 5);
		if (bIsSkinnedState && !bShowSkinnedMeshEntities)
		{
			continue;
		}
		if (bIsStaticMeshState && !bShowStaticMeshEntities)
		{
			continue;
		}

		// Adjust counts: remove the Game count from pass 1, add real state
		--GameEntityCount;

		ImU32 Color;
		ImU32 HColor;
		switch (EntityState)
		{
		case 8: Color = EntitySimpleSkinnedMeshColor; HColor = EntitySimpleSkinnedMeshHoveredColor; ++SimpleSkinnedMeshEntityCount; break;
		case 7: Color = EntitySkinnedPhysicsColor; HColor = EntitySkinnedPhysicsHoveredColor; ++SkinnedPhysicsEntityCount; break;
		case 6: Color = EntitySkinnedMeshColor; HColor = EntitySkinnedMeshHoveredColor; ++SkinnedMeshEntityCount; break;
		case 5: Color = EntitySimpleMeshColor; HColor = EntitySimpleMeshHoveredColor; ++SimpleMeshEntityCount; break;
		case 4: Color = EntityKeepHydratedColor; HColor = EntityKeepHydratedHoveredColor; ++KeepHydratedCount; ++ActorEntityCount; break;
		case 3: Color = EntityActorColor; HColor = EntityActorHoveredColor; ++ActorEntityCount; break;
		case 2: Color = EntityPhysicsColor; HColor = EntityPhysicsHoveredColor; ++PhysicsEntityCount; break;
		case 1: Color = EntityMeshColor; HColor = EntityMeshHoveredColor; ++MeshEntityCount; break;
		default: break;
		}

		const float DX = IO.MousePos.x - ScreenPos.x;
		const float DY = IO.MousePos.y - ScreenPos.y;
		const float DistSq = DX * DX + DY * DY;

		if (DistSq < BestHoverDistSq)
		{
			BestHoverDistSq = DistSq;
			bHasHoveredEntity = true;
			HoveredEntityPos = Position;
			HoveredEntityIndex = Entity.Index;
			HoveredEntityState = EntityState;
			Color = HColor;
		}

		DrawList->AddCircleFilled(ScreenPos, EntityRadius, Color);
	}
	} // TActorIterator

	// --- Pass 3: Draw source (streaming origin) entities ---
	if (bShowSourceEntities)
	{
		SourceEntityCount = 0;
		FMassEntityQuery SourceQuery(EntityManager->AsShared());
		SourceQuery.AddTagRequirement<FArcVisSourceEntityTag>(EMassFragmentPresence::All);
		SourceQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);

		FMassExecutionContext QueryContext(*EntityManager, 0.f);
		SourceQuery.ForEachEntityChunk(QueryContext,
			[this, DrawList, EntityRadius](FMassExecutionContext& Ctx)
			{
				TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
				for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
				{
					const FVector& Pos = Transforms[EntityIt].GetTransform().GetLocation();
					const ImVec2 SP = WorldToScreen(Pos.X, Pos.Y);
					const float SourceRadius = FMath::Clamp(Zoom * 400.0f, 4.0f, 10.0f);
					DrawList->AddCircleFilled(SP, SourceRadius, Arcx::GameplayDebugger::IW::Minimap::SourceEntityColor);
					DrawList->AddCircle(SP, SourceRadius + 2.0f, IM_COL32(255, 255, 255, 120), 0, 2.0f);
					++SourceEntityCount;
				}
			});
	}
}

// ====================================================================
// HUD
// ====================================================================

void FArcIWMinimapDebugger::DrawHUD()
{
	// Grid toggles — first row
	ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "WP");
	ImGui::SameLine();
	ImGui::Checkbox("##wp", &bShowWPCells);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.24f, 0.78f, 0.24f, 1.0f), "MeshAdd");
	ImGui::SameLine();
	ImGui::Checkbox("##meshadd", &bShowMeshAddGrid);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.31f, 0.55f, 0.16f, 1.0f), "MeshRm");
	ImGui::SameLine();
	ImGui::Checkbox("##meshrm", &bShowMeshRemoveGrid);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.16f, 0.71f, 0.71f, 1.0f), "PhysAdd");
	ImGui::SameLine();
	ImGui::Checkbox("##physadd", &bShowPhysicsAddGrid);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.12f, 0.47f, 0.51f, 1.0f), "PhysRm");
	ImGui::SameLine();
	ImGui::Checkbox("##physrm", &bShowPhysicsRemoveGrid);
	ImGui::SameLine();
	const bool bActorHydrationDisabled = UArcIWSettings::Get()->bDisableActorHydration;
	if (bActorHydrationDisabled)
	{
		bShowActorGrid = false;
		bShowDehydrationGrid = false;
	}
	ImGui::BeginDisabled(bActorHydrationDisabled);
	ImGui::TextColored(ImVec4(0.4f, 0.5f, 0.9f, 1.0f), "Actor");
	ImGui::SameLine();
	ImGui::Checkbox("##actorgrid", &bShowActorGrid);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.8f, 0.55f, 0.25f, 1.0f), "Dehydr");
	ImGui::SameLine();
	ImGui::Checkbox("##dehydrgrid", &bShowDehydrationGrid);
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::Text("|");
	ImGui::SameLine();
	ImGui::Checkbox("Entities", &bShowEntities);
	ImGui::SameLine();
	ImGui::Checkbox("Static", &bShowStaticMeshEntities);
	ImGui::SameLine();
	ImGui::Checkbox("Skinned", &bShowSkinnedMeshEntities);
	ImGui::SameLine();
	ImGui::Checkbox("VisEntities", &bShowVisEntities);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Src");
	ImGui::SameLine();
	ImGui::Checkbox("##source", &bShowSourceEntities);
	ImGui::SameLine();
	ImGui::Checkbox("Viewport", &bDrawInViewport);

	// Zoom & reset
	ImGui::SameLine();
	ImGui::Text("| Zoom: %.5f", Zoom);
	ImGui::SameLine();
	if (ImGui::SmallButton("Reset View"))
	{
		ViewOffset = FVector2D::ZeroVector;
		Zoom = 0.01f;
	}

	// Stats with color-coded entity counts
	ImGui::TextColored(ImVec4(0.86f, 0.78f, 0.2f, 1.0f), "Game:%d", GameEntityCount);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.3f, 0.86f, 0.3f, 1.0f), "Mesh:%d", MeshEntityCount);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.16f, 0.78f, 0.78f, 1.0f), "Phys:%d", PhysicsEntityCount);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.3f, 0.55f, 1.0f, 1.0f), "Actor:%d", ActorEntityCount);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.78f, 1.0f), "KeepHyd:%d", KeepHydratedCount);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.9f, 0.59f, 0.2f, 1.0f), "Simple:%d", SimpleMeshEntityCount);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.71f, 0.31f, 0.86f, 1.0f), "Skinned:%d", SkinnedMeshEntityCount);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.55f, 0.24f, 0.78f, 1.0f), "SknPhys:%d", SkinnedPhysicsEntityCount);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.78f, 0.39f, 0.71f, 1.0f), "SknSimple:%d", SimpleSkinnedMeshEntityCount);
	ImGui::SameLine();
	ImGui::Text("| Total:%d Cells:%d", EntityCount, OccupiedCellCount);
	if (bShowVisEntities)
	{
		ImGui::SameLine();
		ImGui::Text("VisHolders:%d", VisHolderCount);
	}
	if (bShowSourceEntities)
	{
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Src:%d", SourceEntityCount);
	}

	// Radii
	UArcIWVisualizationSubsystem* HUDVisSub = GetVisualizationSubsystem();
	if (HUDVisSub)
	{
		ImGui::Text("MeshAdd:%.0fcm MeshRm:%.0fcm PhysAdd:%.0fcm PhysRm:%.0fcm Actor:%.0fcm Dehydr:%.0fcm",
			HUDVisSub->GetMeshAddRadius(), HUDVisSub->GetMeshRemoveRadius(),
			HUDVisSub->GetPhysicsAddRadius(), HUDVisSub->GetPhysicsRemoveRadius(),
			HUDVisSub->GetActorHydrationRadius(), HUDVisSub->GetActorDehydrationRadius());
	}
	const UArcIWSettings* HUDSettings = UArcIWSettings::Get();
	ImGui::Text("WP:%.0f MeshCell:%.0f PhysCell:%.0f ActorCell:%.0f",
		WPCellSize,
		HUDVisSub ? HUDVisSub->GetMeshCellSize() : HUDSettings->MeshCellSize,
		HUDVisSub ? HUDVisSub->GetPhysicsCellSize() : HUDSettings->PhysicsCellSize,
		HUDVisSub ? HUDVisSub->GetActorCellSize() : HUDSettings->ActorCellSize);
}

// ====================================================================
// Viewport Overlay (3D debug draw)
// ====================================================================

void FArcIWMinimapDebugger::DrawViewportOverlay(UWorld* World)
{
	UArcIWVisualizationSubsystem* VisSub = GetVisualizationSubsystem();
	FMassEntityManager* EntityManager = GetEntityManager();
	if (!VisSub || !EntityManager || !World)
	{
		return;
	}

	for (TActorIterator<AArcIWMassISMPartitionActor> It(World); It; ++It)
	{
	for (const FMassEntityHandle& Entity : It->GetSpawnedEntities())
	{
		if (!EntityManager->IsEntityValid(Entity))
		{
			continue;
		}

		const FTransformFragment* TransformFrag = EntityManager->GetFragmentDataPtr<FTransformFragment>(Entity);
		if (!TransformFrag)
		{
			continue;
		}

		const FVector Position = TransformFrag->GetTransform().GetLocation();

		// Determine entity state and color
		FColor DebugColor = FColor::Yellow; // Game entity default
		bool bIsSkinnedEntity = false;
		bool bIsStaticMeshEntity = false;
		const FArcIWInstanceFragment* InstanceFrag = EntityManager->GetFragmentDataPtr<FArcIWInstanceFragment>(Entity);
		if (InstanceFrag)
		{
			if (InstanceFrag->bIsActorRepresentation)
			{
				DebugColor = InstanceFrag->bKeepHydrated ? FColor::Magenta : FColor::Blue;
			}
			else
			{
				bool bHasActiveISM = false;
				for (int32 Id : InstanceFrag->ISMInstanceIds)
				{
					if (Id != INDEX_NONE)
					{
						bHasActiveISM = true;
						break;
					}
				}

				if (bHasActiveISM)
				{
					bool bIsSkinned = false;
					const FArcIWVisConfigFragment* VisConfig =
						EntityManager->GetConstSharedFragmentDataPtr<FArcIWVisConfigFragment>(Entity);
					if (VisConfig)
					{
						for (const FArcIWMeshEntry& MeshEntry : VisConfig->MeshDescriptors)
						{
							if (MeshEntry.IsSkinned())
							{
								bIsSkinned = true;
								break;
							}
						}
					}

					const FArcMassPhysicsBodyFragment* PhysicsBody = EntityManager->GetFragmentDataPtr<FArcMassPhysicsBodyFragment>(Entity);
					bool bHasPhysics = (PhysicsBody && PhysicsBody->Body != nullptr);
					if (bIsSkinned)
					{
						bIsSkinnedEntity = true;
						DebugColor = bHasPhysics ? FColor(140, 60, 200) : FColor(180, 80, 220);
					}
					else
					{
						bIsStaticMeshEntity = true;
						DebugColor = bHasPhysics ? FColor::Cyan : FColor::Green;
					}
				}
				else
				{
					const FMassRenderPrimitiveFragment* PrimFrag =
						EntityManager->GetFragmentDataPtr<FMassRenderPrimitiveFragment>(Entity);
					if (PrimFrag && PrimFrag->bIsVisible)
					{
						FMassEntityView EntityView(*EntityManager, Entity);
						bool bIsSkinned = EntityView.HasTag<FArcIWSimpleVisSkinnedTag>();
						if (bIsSkinned)
						{
							bIsSkinnedEntity = true;
							DebugColor = FColor(200, 100, 180);
						}
						else
						{
							bIsStaticMeshEntity = true;
							DebugColor = FColor(230, 150, 50);
						}
					}
				}
			}
		}

		// Filter by mesh type toggles
		if (bIsSkinnedEntity && !bShowSkinnedMeshEntities)
		{
			continue;
		}
		if (bIsStaticMeshEntity && !bShowStaticMeshEntities)
		{
			continue;
		}

		DrawDebugPoint(World, Position, 8.0f, DebugColor, false, -1.0f);

		// Draw small upward line to make points visible in 3D
		DrawDebugLine(World, Position, Position + FVector(0, 0, 100.0f), DebugColor, false, -1.0f, 0, 1.0f);
	}
	} // TActorIterator
}
