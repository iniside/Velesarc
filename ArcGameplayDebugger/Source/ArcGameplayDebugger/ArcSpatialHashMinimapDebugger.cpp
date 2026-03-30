// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSpatialHashMinimapDebugger.h"

#include "imgui.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "ArcMass/Spatial/ArcMassSpatialHashSubsystem.h"
#include "MassActorSubsystem.h"
#include "MassEntitySubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

// ====================================================================
// Colors
// ====================================================================

namespace
{
	static const ImU32 GridLineColor = IM_COL32(80, 80, 80, 120);
	static const ImU32 GridLineColorMajor = IM_COL32(120, 120, 120, 160);
	static const ImU32 CellFillColor = IM_COL32(40, 120, 60, 80);
	static const ImU32 EntityColor = IM_COL32(220, 200, 50, 255);
	static const ImU32 EntityHoveredColor = IM_COL32(255, 255, 100, 255);
	static const ImU32 PlayerColor = IM_COL32(50, 140, 255, 255);
	static const ImU32 PlayerHoveredColor = IM_COL32(100, 180, 255, 255);
	static const ImU32 BackgroundColor = IM_COL32(20, 20, 25, 240);
	static const ImU32 OriginColor = IM_COL32(255, 60, 60, 180);
	static const ImU32 HUDTextColor = IM_COL32(200, 200, 200, 255);

	// Lerp cell fill alpha based on entity count
	ImU32 CellColorByCount(int32 Count)
	{
		const float Alpha = FMath::Clamp(0.15f + 0.1f * Count, 0.15f, 0.8f);
		const uint8 A = static_cast<uint8>(Alpha * 255.f);
		return IM_COL32(40, 160, 80, A);
	}
}

// ====================================================================
// Lifecycle
// ====================================================================

void FArcSpatialHashMinimapDebugger::Initialize()
{
	ViewOffset = FVector2D::ZeroVector;
	Zoom = 0.05f;
	bIsPanning = false;
	bHasHoveredEntity = false;
	TotalEntities = 0;
	OccupiedCells = 0;
	CellSize = 0.0f;
}

void FArcSpatialHashMinimapDebugger::Uninitialize()
{
}

// ====================================================================
// Helpers
// ====================================================================

UWorld* FArcSpatialHashMinimapDebugger::GetDebugWorld()
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return nullptr;
	}
	return GEngine->GameViewport->GetWorld();
}

UArcMassSpatialHashSubsystem* FArcSpatialHashMinimapDebugger::GetSpatialHashSubsystem() const
{
	UWorld* World = GetDebugWorld();
	if (!World)
	{
		return nullptr;
	}
	return World->GetSubsystem<UArcMassSpatialHashSubsystem>();
}

ImVec2 FArcSpatialHashMinimapDebugger::WorldToScreen(float WorldX, float WorldY) const
{
	return ImVec2(
		CanvasCenter.x + (WorldX - static_cast<float>(ViewOffset.X)) * Zoom,
		CanvasCenter.y - (WorldY - static_cast<float>(ViewOffset.Y)) * Zoom // Y flipped for top-down
	);
}

FVector2D FArcSpatialHashMinimapDebugger::ScreenToWorld(const ImVec2& ScreenPos) const
{
	return FVector2D(
		ViewOffset.X + (ScreenPos.x - CanvasCenter.x) / Zoom,
		ViewOffset.Y - (ScreenPos.y - CanvasCenter.y) / Zoom // Y flipped
	);
}

// ====================================================================
// Main Draw
// ====================================================================

void FArcSpatialHashMinimapDebugger::Draw()
{
	ImGui::SetNextWindowSize(ImVec2(600, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Spatial Hash Minimap", &bShow, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::End();
		return;
	}

	UArcMassSpatialHashSubsystem* Subsystem = GetSpatialHashSubsystem();
	if (!Subsystem)
	{
		ImGui::TextDisabled("No UArcMassSpatialHashSubsystem available");
		ImGui::End();
		return;
	}

	// Reserve space for HUD bar at top
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

	// Invisible button to capture input over the canvas area
	ImGui::InvisibleButton("##canvas", CanvasSize, ImGuiButtonFlags_MouseButtonMiddle);
	const bool bCanvasHovered = ImGui::IsItemHovered();

	HandleInput();

	// Clip drawing to canvas area
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->PushClipRect(CanvasPos, ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y), true);

	// Background
	DrawList->AddRectFilled(CanvasPos, ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y), BackgroundColor);

	// Draw origin crosshair
	{
		const ImVec2 Origin = WorldToScreen(0.0f, 0.0f);
		DrawList->AddLine(ImVec2(CanvasPos.x, Origin.y), ImVec2(CanvasPos.x + CanvasSize.x, Origin.y), OriginColor, 1.0f);
		DrawList->AddLine(ImVec2(Origin.x, CanvasPos.y), ImVec2(Origin.x, CanvasPos.y + CanvasSize.y), OriginColor, 1.0f);
	}

	DrawGrid();
	DrawEntities();

	DrawList->PopClipRect();

	// Tooltip for hovered entity
	if (bHasHoveredEntity && bCanvasHovered)
	{
		ImGui::BeginTooltip();
		ImGui::Text("Entity: E%d%s", HoveredEntityIndex, bHoveredIsPlayer ? " [Player]" : "");
		ImGui::Text("Position: (%.0f, %.0f, %.0f)", HoveredEntityPos.X, HoveredEntityPos.Y, HoveredEntityPos.Z);
		ImGui::EndTooltip();
	}

	ImGui::End();
}

// ====================================================================
// Input Handling
// ====================================================================

void FArcSpatialHashMinimapDebugger::HandleInput()
{
	const ImGuiIO& IO = ImGui::GetIO();
	const bool bCanvasHovered = ImGui::IsItemHovered();

	// --- Panning (middle mouse button) ---
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
			ViewOffset.Y = PanStartOffset.Y + DeltaY / Zoom; // Y flipped
		}
		else
		{
			bIsPanning = false;
		}
	}

	// --- Zooming (scroll wheel, centered on mouse) ---
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

		// Adjust offset so the world point under the mouse stays under the mouse
		const FVector2D WorldAfterZoom = ScreenToWorld(IO.MousePos);
		ViewOffset.X -= (WorldAfterZoom.X - WorldBeforeZoom.X);
		ViewOffset.Y -= (WorldAfterZoom.Y - WorldBeforeZoom.Y);
	}
}

// ====================================================================
// Grid Drawing
// ====================================================================

void FArcSpatialHashMinimapDebugger::DrawGrid()
{
	UArcMassSpatialHashSubsystem* Subsystem = GetSpatialHashSubsystem();
	if (!Subsystem)
	{
		return;
	}

	const FMassSpatialHashGrid& Grid = Subsystem->GetSpatialHashGrid();
	CellSize = Grid.Settings.CellSize;
	OccupiedCells = Grid.SpatialBuckets.Num();
	TotalEntities = 0;

	if (CellSize <= 0.0f)
	{
		return;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	// Draw occupied cells as filled rectangles
	for (const auto& Pair : Grid.SpatialBuckets)
	{
		const FIntVector& Coords = Pair.Key;
		const TArray<FMassSpatialHashGrid::FEntityWithPosition>& Entities = Pair.Value;
		TotalEntities += Entities.Num();

		const float WorldMinX = Coords.X * CellSize;
		const float WorldMinY = Coords.Y * CellSize;
		const float WorldMaxX = WorldMinX + CellSize;
		const float WorldMaxY = WorldMinY + CellSize;

		const ImVec2 ScreenMin = WorldToScreen(WorldMinX, WorldMaxY); // Max Y -> min screen Y (flipped)
		const ImVec2 ScreenMax = WorldToScreen(WorldMaxX, WorldMinY);

		// Skip cells that are completely off-screen
		if (ScreenMax.x < CanvasPos.x || ScreenMin.x > CanvasPos.x + CanvasSize.x ||
			ScreenMax.y < CanvasPos.y || ScreenMin.y > CanvasPos.y + CanvasSize.y)
		{
			continue;
		}

		// Fill cell
		DrawList->AddRectFilled(ScreenMin, ScreenMax, CellColorByCount(Entities.Num()));

		// Cell border
		DrawList->AddRect(ScreenMin, ScreenMax, GridLineColor, 0.0f, 0, 1.0f);

		// Entity count label (only if cell is big enough on screen)
		const float CellScreenSize = (ScreenMax.x - ScreenMin.x);
		if (CellScreenSize > 24.0f)
		{
			char CountBuf[16];
			FCStringAnsi::Snprintf(CountBuf, sizeof(CountBuf), "%d", Entities.Num());
			const ImVec2 TextSize = ImGui::CalcTextSize(CountBuf);
			DrawList->AddText(
				ImVec2(
					(ScreenMin.x + ScreenMax.x - TextSize.x) * 0.5f,
					(ScreenMin.y + ScreenMax.y - TextSize.y) * 0.5f
				),
				HUDTextColor, CountBuf
			);
		}
	}
}

// ====================================================================
// Entity Drawing
// ====================================================================

void FArcSpatialHashMinimapDebugger::DrawEntities()
{
	UArcMassSpatialHashSubsystem* Subsystem = GetSpatialHashSubsystem();
	if (!Subsystem)
	{
		return;
	}

	const FMassSpatialHashGrid& Grid = Subsystem->GetSpatialHashGrid();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImGuiIO& IO = ImGui::GetIO();

	// Get entity manager for fragment lookups (player detection)
	UWorld* World = GetDebugWorld();
	FMassEntityManager* EntityManager = nullptr;
	if (World)
	{
		if (UMassEntitySubsystem* MassSub = World->GetSubsystem<UMassEntitySubsystem>())
		{
			EntityManager = &MassSub->GetMutableEntityManager();
		}
	}

	bHasHoveredEntity = false;
	float BestHoverDistSq = 64.0f; // 8px radius for hover detection

	const float EntityRadius = FMath::Clamp(Zoom * 50.0f, 2.0f, 6.0f);
	const float PlayerRadius = EntityRadius * 1.5f;

	for (const auto& Pair : Grid.SpatialBuckets)
	{
		for (const FMassSpatialHashGrid::FEntityWithPosition& Entry : Pair.Value)
		{
			const ImVec2 ScreenPos = WorldToScreen(Entry.Position.X, Entry.Position.Y);

			// Skip off-screen entities
			if (ScreenPos.x < CanvasPos.x - PlayerRadius || ScreenPos.x > CanvasPos.x + CanvasSize.x + PlayerRadius ||
				ScreenPos.y < CanvasPos.y - PlayerRadius || ScreenPos.y > CanvasPos.y + CanvasSize.y + PlayerRadius)
			{
				continue;
			}

			// Detect player entities via FMassActorFragment -> APawn -> APlayerState
			bool bIsPlayer = false;
			if (EntityManager && EntityManager->IsEntityValid(Entry.Entity))
			{
				if (FMassActorFragment* ActorFrag = EntityManager->GetFragmentDataPtr<FMassActorFragment>(Entry.Entity))
				{
					if (AActor* Actor = ActorFrag->GetMutable())
					{
						if (const APawn* Pawn = Cast<APawn>(Actor))
						{
							bIsPlayer = Pawn->GetPlayerState() != nullptr;
						}
					}
				}
			}

			// Check hover
			const float DX = IO.MousePos.x - ScreenPos.x;
			const float DY = IO.MousePos.y - ScreenPos.y;
			const float DistSq = DX * DX + DY * DY;

			ImU32 Color = bIsPlayer ? PlayerColor : EntityColor;
			if (DistSq < BestHoverDistSq)
			{
				BestHoverDistSq = DistSq;
				bHasHoveredEntity = true;
				HoveredEntityPos = Entry.Position;
				HoveredEntityIndex = Entry.Entity.Index;
				bHoveredIsPlayer = bIsPlayer;
				Color = bIsPlayer ? PlayerHoveredColor : EntityHoveredColor;
			}

			const float Radius = bIsPlayer ? PlayerRadius : EntityRadius;
			DrawList->AddCircleFilled(ScreenPos, Radius, Color);
		}
	}
}

// ====================================================================
// HUD
// ====================================================================

void FArcSpatialHashMinimapDebugger::DrawHUD()
{
	ImGui::Text("Entities: %d", TotalEntities);
	ImGui::SameLine();
	ImGui::Text("| Cells: %d", OccupiedCells);
	ImGui::SameLine();
	ImGui::Text("| Cell Size: %.0f", CellSize);
	ImGui::SameLine();
	ImGui::Text("| Zoom: %.4f", Zoom);

	ImGui::SameLine();
	if (ImGui::SmallButton("Reset View"))
	{
		ViewOffset = FVector2D::ZeroVector;
		Zoom = 0.05f;
	}
}
