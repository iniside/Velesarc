// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcVisualizationMinimapDebugger.h"

#include "imgui.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "ArcMass/Visualization/ArcMassEntityVisualization.h"
#include "ArcMass/Physics/ArcMassPhysicsBody.h"
#include "ArcMass/MobileVisualization/ArcMobileVisualization.h"
#include "MassActorSubsystem.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityFragments.h"
#include "MassEntityQuery.h"
#include "MassExecutionContext.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

// ====================================================================
// Colors
// ====================================================================

namespace Arcx::GameplayDebugger::VisEntity::Minimap
{
	// Mesh grid
	static const ImU32 MeshGridCellFillColor = IM_COL32(30, 45, 100, 60);
	static const ImU32 MeshGridCellBorderColor = IM_COL32(45, 45, 85, 90);
	static const ImU32 MeshGridLineColor = IM_COL32(40, 40, 70, 70);
	static const ImU32 MeshActivationRadiusColor = IM_COL32(80, 160, 220, 180);
	static const ImU32 MeshDeactivationRadiusColor = IM_COL32(80, 160, 220, 100);

	// Physics grid
	static const ImU32 PhysicsGridCellFillColor = IM_COL32(30, 80, 45, 60);
	static const ImU32 PhysicsGridCellBorderColor = IM_COL32(55, 55, 55, 90);
	static const ImU32 PhysicsGridLineColor = IM_COL32(50, 50, 50, 70);
	static const ImU32 PhysicsActivationRadiusColor = IM_COL32(100, 200, 120, 180);
	static const ImU32 PhysicsDeactivationRadiusColor = IM_COL32(100, 200, 120, 100);

	// Mobile visualization
	static const ImU32 MobileCellFillColor = IM_COL32(30, 45, 100, 60);
	static const ImU32 MobileCellBorderColor = IM_COL32(45, 45, 85, 90);
	static const ImU32 MobileGridLineColor = IM_COL32(40, 40, 70, 70);

	// Mobile LOD entity colors
	static const ImU32 MobileLODActorColor = IM_COL32(80, 200, 100, 200);
	static const ImU32 MobileLODActorHoveredColor = IM_COL32(120, 240, 140, 255);
	static const ImU32 MobileLODISMColor = IM_COL32(70, 130, 190, 200);
	static const ImU32 MobileLODISMHoveredColor = IM_COL32(110, 175, 230, 255);
	static const ImU32 MobileLODNoneColor = IM_COL32(100, 100, 100, 140);
	static const ImU32 MobileLODNoneHoveredColor = IM_COL32(150, 150, 150, 200);

	// Mobile radius circles
	static const ImU32 MobileActorRadiusColor = IM_COL32(80, 200, 100, 160);
	static const ImU32 MobileISMRadiusColor = IM_COL32(70, 130, 190, 120);

	// Mobile regions
	static const ImU32 MobileRegionBorderColor = IM_COL32(160, 100, 60, 120);

	// Entity dots
	static const ImU32 StaticEntityColor = IM_COL32(150, 140, 40, 180);
	static const ImU32 StaticEntityHoveredColor = IM_COL32(200, 195, 80, 255);
	static const ImU32 MobileEntityColor = IM_COL32(70, 130, 190, 180);
	static const ImU32 MobileEntityHoveredColor = IM_COL32(110, 175, 230, 255);
	static const ImU32 PhysicsEntityColor = IM_COL32(100, 200, 120, 180);
	static const ImU32 MeshEntityColor = IM_COL32(80, 160, 220, 180);

	// Active cell highlight
	static const ImU32 ActiveCellColor = IM_COL32(45, 140, 70, 40);
	static const ImU32 PlayerCellColor = IM_COL32(200, 60, 60, 40);

	// Player entities
	static const ImU32 PlayerColor = IM_COL32(200, 60, 60, 220);
	static const ImU32 PlayerHoveredColor = IM_COL32(240, 120, 120, 255);

	// Source entities
	static const ImU32 SourceColor = IM_COL32(190, 120, 30, 200);

	// General
	static const ImU32 BackgroundColor = IM_COL32(15, 15, 20, 240);
	static const ImU32 OriginColor = IM_COL32(180, 45, 45, 120);
	static const ImU32 HUDTextColor = IM_COL32(160, 160, 160, 220);

	ImU32 CellColorByCount(int32 Count, ImU32 BaseColor)
	{
		const float Alpha = FMath::Clamp(0.15f + 0.1f * Count, 0.15f, 0.8f);
		const uint8 A = static_cast<uint8>(Alpha * 255.f);
		return (BaseColor & 0x00FFFFFF) | (static_cast<ImU32>(A) << 24);
	}
}

namespace VisColors = Arcx::GameplayDebugger::VisEntity::Minimap;

// ====================================================================
// Lifecycle
// ====================================================================

void FArcVisualizationMinimapDebugger::Initialize()
{
	ViewOffset = FVector2D::ZeroVector;
	Zoom = 0.05f;
	bIsPanning = false;
	bHasHoveredEntity = false;
	MeshGridEntityCount = 0;
	MeshGridCellCount = 0;
	PhysicsGridEntityCount = 0;
	PhysicsGridCellCount = 0;
	MobileEntityCount = 0;
	MobileCellCount = 0;
	PhysicsEntityCount = 0;
	MeshEntityCount = 0;
	SourceEntityCount = 0;
	SourceEntityPositions.Reset();
	MobileActorCount = 0;
	MobileISMCount = 0;
	MobileNoneCount = 0;
	MobileActorRadius = 0.0f;
	MobileISMRadius = 0.0f;
}

void FArcVisualizationMinimapDebugger::Uninitialize()
{
}

// ====================================================================
// Helpers
// ====================================================================

UWorld* FArcVisualizationMinimapDebugger::GetDebugWorld()
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return nullptr;
	}
	return GEngine->GameViewport->GetWorld();
}

UArcEntityVisualizationSubsystem* FArcVisualizationMinimapDebugger::GetStaticVisSubsystem() const
{
	UWorld* World = GetDebugWorld();
	return World ? World->GetSubsystem<UArcEntityVisualizationSubsystem>() : nullptr;
}

UArcMobileVisSubsystem* FArcVisualizationMinimapDebugger::GetMobileVisSubsystem() const
{
	UWorld* World = GetDebugWorld();
	return World ? World->GetSubsystem<UArcMobileVisSubsystem>() : nullptr;
}

FMassEntityManager* FArcVisualizationMinimapDebugger::GetEntityManager() const
{
	UWorld* World = GetDebugWorld();
	if (!World)
	{
		return nullptr;
	}
	UMassEntitySubsystem* MassSub = World->GetSubsystem<UMassEntitySubsystem>();
	return MassSub ? &MassSub->GetMutableEntityManager() : nullptr;
}

ImVec2 FArcVisualizationMinimapDebugger::WorldToScreen(float WorldX, float WorldY) const
{
	return ImVec2(
		CanvasCenter.x + (WorldX - static_cast<float>(ViewOffset.X)) * Zoom,
		CanvasCenter.y - (WorldY - static_cast<float>(ViewOffset.Y)) * Zoom
	);
}

FVector2D FArcVisualizationMinimapDebugger::ScreenToWorld(const ImVec2& ScreenPos) const
{
	return FVector2D(
		ViewOffset.X + (ScreenPos.x - CanvasCenter.x) / Zoom,
		ViewOffset.Y - (ScreenPos.y - CanvasCenter.y) / Zoom
	);
}

// ====================================================================
// Main Draw
// ====================================================================

void FArcVisualizationMinimapDebugger::Draw()
{
	ImGui::SetNextWindowSize(ImVec2(600, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Visualization Minimap", &bShow, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::End();
		return;
	}

	DrawHUD();
	ImGui::Separator();

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

	ImGui::InvisibleButton("##viscanvas", CanvasSize, ImGuiButtonFlags_MouseButtonMiddle);
	const bool bCanvasHovered = ImGui::IsItemHovered();

	HandleInput();

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->PushClipRect(CanvasPos, ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y), true);

	DrawList->AddRectFilled(CanvasPos, ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y), VisColors::BackgroundColor);

	{
		const ImVec2 Origin = WorldToScreen(0.0f, 0.0f);
		DrawList->AddLine(ImVec2(CanvasPos.x, Origin.y), ImVec2(CanvasPos.x + CanvasSize.x, Origin.y), VisColors::OriginColor, 1.0f);
		DrawList->AddLine(ImVec2(Origin.x, CanvasPos.y), ImVec2(Origin.x, CanvasPos.y + CanvasSize.y), VisColors::OriginColor, 1.0f);
	}

	DrawGrid();
	DrawSourceEntities();
	DrawRadiusCircles();
	DrawEntities();
	if (bShowMobileEntities)
	{
		DrawMobileEntities();
	}
	if (bShowMobileRadii)
	{
		DrawMobileRadiusCircles();
	}
	if (bShowMobileRegions)
	{
		DrawMobileRegions();
	}

	DrawList->PopClipRect();

	if (bHasHoveredEntity && bCanvasHovered)
	{
		ImGui::BeginTooltip();
		ImGui::Text("Entity: E%d%s%s", HoveredEntityIndex,
			bHoveredIsPlayer ? " [Player]" : "",
			bHoveredIsMobile ? " (Mobile)" : " (Static)");
		ImGui::Text("Position: (%.0f, %.0f, %.0f)", HoveredEntityPos.X, HoveredEntityPos.Y, HoveredEntityPos.Z);

		FMassEntityManager* EM = GetEntityManager();
		if (EM)
		{
			FMassEntityHandle HoveredHandle;
			HoveredHandle.Index = HoveredEntityIndex;
			if (EM->IsEntityValid(HoveredHandle))
			{
				if (bHoveredIsMobile)
				{
					const FArcMobileVisLODFragment* LODFrag = EM->GetFragmentDataPtr<FArcMobileVisLODFragment>(HoveredHandle);
					const FArcMobileVisRepFragment* RepFrag = EM->GetFragmentDataPtr<FArcMobileVisRepFragment>(HoveredHandle);
					if (LODFrag)
					{
						const char* LODStr = LODFrag->CurrentLOD == EArcMobileVisLOD::Actor ? "Actor"
							: LODFrag->CurrentLOD == EArcMobileVisLOD::InstancedMesh ? "ISM" : "None";
						ImGui::Text("LOD: %s", LODStr);
					}
					if (RepFrag)
					{
						ImGui::Text("Cell: (%d, %d, %d)", RepFrag->GridCoords.X, RepFrag->GridCoords.Y, RepFrag->GridCoords.Z);
						ImGui::Text("Region: (%d, %d, %d)", RepFrag->RegionCoord.X, RepFrag->RegionCoord.Y, RepFrag->RegionCoord.Z);
						if (RepFrag->bIsActorRepresentation)
						{
							ImGui::TextColored(ImVec4(0.31f, 0.78f, 0.39f, 1.0f), "Hydrated Actor");
						}
						else if (RepFrag->ISMInstanceId != INDEX_NONE)
						{
							ImGui::Text("ISM Instance: %d", RepFrag->ISMInstanceId);
						}
					}
				}
				else
				{
					const FArcVisRepresentationFragment* VisRep = EM->GetFragmentDataPtr<FArcVisRepresentationFragment>(HoveredHandle);
					if (VisRep)
					{
						const FArcMassPhysicsBodyFragment* HovPhysBody = EM->GetFragmentDataPtr<FArcMassPhysicsBodyFragment>(HoveredHandle);
						ImGui::Text("Physics: %s | Mesh: %s",
							(HovPhysBody && HovPhysBody->Body) ? "Yes" : "No",
							VisRep->bHasMeshRendering ? "Yes" : "No");
					}
				}
			}
		}

		ImGui::EndTooltip();
	}

	ImGui::End();
}

// ====================================================================
// Input Handling
// ====================================================================

void FArcVisualizationMinimapDebugger::HandleInput()
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
			ViewOffset.Y = PanStartOffset.Y + DeltaY / Zoom;
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

		const FVector2D WorldAfterZoom = ScreenToWorld(IO.MousePos);
		ViewOffset.X -= (WorldAfterZoom.X - WorldBeforeZoom.X);
		ViewOffset.Y -= (WorldAfterZoom.Y - WorldBeforeZoom.Y);
	}
}

// ====================================================================
// Grid Lines
// ====================================================================

void FArcVisualizationMinimapDebugger::DrawGridLines(float CellSize, ImU32 LineColor)
{
	if (CellSize <= 0.0f)
	{
		return;
	}

	// Skip if cells would be too small on screen to be useful
	const float CellScreenSize = CellSize * Zoom;
	if (CellScreenSize < 4.0f)
	{
		return;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	// Visible world bounds from canvas corners
	const FVector2D WorldTopLeft = ScreenToWorld(CanvasPos);
	const FVector2D WorldBottomRight = ScreenToWorld(ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y));

	const float WorldMinX = FMath::Min(WorldTopLeft.X, WorldBottomRight.X);
	const float WorldMaxX = FMath::Max(WorldTopLeft.X, WorldBottomRight.X);
	const float WorldMinY = FMath::Min(WorldTopLeft.Y, WorldBottomRight.Y);
	const float WorldMaxY = FMath::Max(WorldTopLeft.Y, WorldBottomRight.Y);

	// Snap to cell boundaries (one cell beyond visible for clean edges)
	// Clamp to reasonable range to avoid int32 overflow when zoomed out or panned far
	const double RawCellMinX = FMath::FloorToDouble(static_cast<double>(WorldMinX) / CellSize) - 1.0;
	const double RawCellMaxX = FMath::CeilToDouble(static_cast<double>(WorldMaxX) / CellSize) + 1.0;
	const double RawCellMinY = FMath::FloorToDouble(static_cast<double>(WorldMinY) / CellSize) - 1.0;
	const double RawCellMaxY = FMath::CeilToDouble(static_cast<double>(WorldMaxY) / CellSize) + 1.0;

	// Cap line count to avoid perf issues at extreme zoom-out
	const int32 MaxLines = 200;
	if ((RawCellMaxX - RawCellMinX) > MaxLines || (RawCellMaxY - RawCellMinY) > MaxLines)
	{
		return;
	}

	const int32 CellMinX = static_cast<int32>(RawCellMinX);
	const int32 CellMaxX = static_cast<int32>(RawCellMaxX);
	const int32 CellMinY = static_cast<int32>(RawCellMinY);
	const int32 CellMaxY = static_cast<int32>(RawCellMaxY);

	const float CanvasLeft = CanvasPos.x;
	const float CanvasRight = CanvasPos.x + CanvasSize.x;
	const float CanvasTop = CanvasPos.y;
	const float CanvasBottom = CanvasPos.y + CanvasSize.y;

	// Vertical lines
	for (int32 X = CellMinX; X <= CellMaxX; ++X)
	{
		const float WorldX = X * CellSize;
		const ImVec2 ScreenTop = WorldToScreen(WorldX, WorldMaxY);
		const float SX = ScreenTop.x;
		if (SX >= CanvasLeft && SX <= CanvasRight)
		{
			DrawList->AddLine(ImVec2(SX, CanvasTop), ImVec2(SX, CanvasBottom), LineColor, 1.0f);
		}
	}

	// Horizontal lines
	for (int32 Y = CellMinY; Y <= CellMaxY; ++Y)
	{
		const float WorldY = Y * CellSize;
		const ImVec2 ScreenLeft = WorldToScreen(WorldMinX, WorldY);
		const float SY = ScreenLeft.y;
		if (SY >= CanvasTop && SY <= CanvasBottom)
		{
			DrawList->AddLine(ImVec2(CanvasLeft, SY), ImVec2(CanvasRight, SY), LineColor, 1.0f);
		}
	}
}

// ====================================================================
// Grid Drawing
// ====================================================================

void FArcVisualizationMinimapDebugger::DrawGrid()
{
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	MeshGridEntityCount = 0;
	MeshGridCellCount = 0;
	PhysicsGridEntityCount = 0;
	PhysicsGridCellCount = 0;

	UArcEntityVisualizationSubsystem* VisSub = GetStaticVisSubsystem();

	// --- Mesh grid ---
	if (bShowMeshGrid && VisSub)
	{
		const FArcVisualizationGrid& Grid = VisSub->GetMeshGrid();
		MeshGridCellSize = Grid.CellSize;
		MeshGridCellCount = Grid.CellEntities.Num();

		if (bShowActiveCells)
		{
			const FIntVector MeshPlayerCell = VisSub->GetLastMeshPlayerCell();
			if (MeshPlayerCell.X != TNumericLimits<int32>::Max())
			{
				const float WorldMinX = MeshPlayerCell.X * MeshGridCellSize;
				const float WorldMinY = MeshPlayerCell.Y * MeshGridCellSize;
				const float WorldMaxX = WorldMinX + MeshGridCellSize;
				const float WorldMaxY = WorldMinY + MeshGridCellSize;

				const ImVec2 ScreenMin = WorldToScreen(WorldMinX, WorldMaxY);
				const ImVec2 ScreenMax = WorldToScreen(WorldMaxX, WorldMinY);

				DrawList->AddRectFilled(ScreenMin, ScreenMax, VisColors::PlayerCellColor);
				DrawList->AddRect(ScreenMin, ScreenMax, IM_COL32(80, 160, 220, 200), 0.0f, 0, 2.0f);
			}
		}

		for (const TPair<FIntVector, TArray<FMassEntityHandle>>& Pair : Grid.CellEntities)
		{
			const FIntVector& Coords = Pair.Key;
			const int32 EntityCount = Pair.Value.Num();
			MeshGridEntityCount += EntityCount;

			const float WorldMinX = Coords.X * MeshGridCellSize;
			const float WorldMinY = Coords.Y * MeshGridCellSize;
			const float WorldMaxX = WorldMinX + MeshGridCellSize;
			const float WorldMaxY = WorldMinY + MeshGridCellSize;

			const ImVec2 ScreenMin = WorldToScreen(WorldMinX, WorldMaxY);
			const ImVec2 ScreenMax = WorldToScreen(WorldMaxX, WorldMinY);

			if (ScreenMax.x < CanvasPos.x || ScreenMin.x > CanvasPos.x + CanvasSize.x ||
				ScreenMax.y < CanvasPos.y || ScreenMin.y > CanvasPos.y + CanvasSize.y)
			{
				continue;
			}

			DrawList->AddRectFilled(ScreenMin, ScreenMax, VisColors::CellColorByCount(EntityCount, VisColors::MeshGridCellFillColor));
			DrawList->AddRect(ScreenMin, ScreenMax, VisColors::MeshGridCellBorderColor, 0.0f, 0, 1.0f);

			const float CellScreenSize = (ScreenMax.x - ScreenMin.x);
			if (CellScreenSize > 24.0f)
			{
				char CountBuf[16];
				FCStringAnsi::Snprintf(CountBuf, sizeof(CountBuf), "%d", EntityCount);
				const ImVec2 TextSize = ImGui::CalcTextSize(CountBuf);
				DrawList->AddText(
					ImVec2(
						(ScreenMin.x + ScreenMax.x - TextSize.x) * 0.5f,
						(ScreenMin.y + ScreenMax.y - TextSize.y) * 0.5f
					),
					VisColors::HUDTextColor, CountBuf
				);
			}
		}

		DrawGridLines(MeshGridCellSize, VisColors::MeshGridLineColor);
	}

	// --- Physics grid ---
	if (bShowPhysicsGrid && VisSub)
	{
		const FArcVisualizationGrid& Grid = VisSub->GetPhysicsGrid();
		PhysicsGridCellSize = Grid.CellSize;
		PhysicsGridCellCount = Grid.CellEntities.Num();

		if (bShowActiveCells)
		{
			const FIntVector PhysicsPlayerCell = VisSub->GetLastPhysicsPlayerCell();
			if (PhysicsPlayerCell.X != TNumericLimits<int32>::Max())
			{
				const float WorldMinX = PhysicsPlayerCell.X * PhysicsGridCellSize;
				const float WorldMinY = PhysicsPlayerCell.Y * PhysicsGridCellSize;
				const float WorldMaxX = WorldMinX + PhysicsGridCellSize;
				const float WorldMaxY = WorldMinY + PhysicsGridCellSize;

				const ImVec2 ScreenMin = WorldToScreen(WorldMinX, WorldMaxY);
				const ImVec2 ScreenMax = WorldToScreen(WorldMaxX, WorldMinY);

				DrawList->AddRectFilled(ScreenMin, ScreenMax, VisColors::PlayerCellColor);
				DrawList->AddRect(ScreenMin, ScreenMax, IM_COL32(100, 200, 120, 200), 0.0f, 0, 2.0f);
			}
		}

		for (const TPair<FIntVector, TArray<FMassEntityHandle>>& Pair : Grid.CellEntities)
		{
			const FIntVector& Coords = Pair.Key;
			const int32 EntityCount = Pair.Value.Num();
			PhysicsGridEntityCount += EntityCount;

			const float WorldMinX = Coords.X * PhysicsGridCellSize;
			const float WorldMinY = Coords.Y * PhysicsGridCellSize;
			const float WorldMaxX = WorldMinX + PhysicsGridCellSize;
			const float WorldMaxY = WorldMinY + PhysicsGridCellSize;

			const ImVec2 ScreenMin = WorldToScreen(WorldMinX, WorldMaxY);
			const ImVec2 ScreenMax = WorldToScreen(WorldMaxX, WorldMinY);

			if (ScreenMax.x < CanvasPos.x || ScreenMin.x > CanvasPos.x + CanvasSize.x ||
				ScreenMax.y < CanvasPos.y || ScreenMin.y > CanvasPos.y + CanvasSize.y)
			{
				continue;
			}

			DrawList->AddRectFilled(ScreenMin, ScreenMax, VisColors::CellColorByCount(EntityCount, VisColors::PhysicsGridCellFillColor));
			DrawList->AddRect(ScreenMin, ScreenMax, VisColors::PhysicsGridCellBorderColor, 0.0f, 0, 1.0f);

			const float CellScreenSize = (ScreenMax.x - ScreenMin.x);
			if (CellScreenSize > 24.0f)
			{
				char CountBuf[16];
				FCStringAnsi::Snprintf(CountBuf, sizeof(CountBuf), "%d", EntityCount);
				const ImVec2 TextSize = ImGui::CalcTextSize(CountBuf);
				DrawList->AddText(
					ImVec2(
						(ScreenMin.x + ScreenMax.x - TextSize.x) * 0.5f,
						(ScreenMin.y + ScreenMax.y - TextSize.y) * 0.5f
					),
					VisColors::HUDTextColor, CountBuf
				);
			}
		}

		DrawGridLines(PhysicsGridCellSize, VisColors::PhysicsGridLineColor);
	}

	// --- Mobile visualization grid ---
	if (bShowMobileGrid)
	{
		UArcMobileVisSubsystem* MobileSub = GetMobileVisSubsystem();
		if (MobileSub)
		{
			MobileCellSize = MobileSub->GetCellSize();

			const TMap<FIntVector, TArray<FMassEntityHandle>>& MobileEntityCells = MobileSub->GetEntityCells();
			MobileCellCount = MobileEntityCells.Num();
			MobileEntityCount = 0;

			for (const TPair<FIntVector, TArray<FMassEntityHandle>>& Pair : MobileEntityCells)
			{
				const FIntVector& Coords = Pair.Key;
				const int32 EntityCount = Pair.Value.Num();
				MobileEntityCount += EntityCount;

				const float WorldMinX = Coords.X * MobileCellSize;
				const float WorldMinY = Coords.Y * MobileCellSize;
				const float WorldMaxX = WorldMinX + MobileCellSize;
				const float WorldMaxY = WorldMinY + MobileCellSize;

				const ImVec2 ScreenMin = WorldToScreen(WorldMinX, WorldMaxY);
				const ImVec2 ScreenMax = WorldToScreen(WorldMaxX, WorldMinY);

				if (ScreenMax.x < CanvasPos.x || ScreenMin.x > CanvasPos.x + CanvasSize.x ||
					ScreenMax.y < CanvasPos.y || ScreenMin.y > CanvasPos.y + CanvasSize.y)
				{
					continue;
				}

				DrawList->AddRectFilled(ScreenMin, ScreenMax, VisColors::CellColorByCount(EntityCount, VisColors::MobileCellFillColor));
				DrawList->AddRect(ScreenMin, ScreenMax, VisColors::MobileCellBorderColor, 0.0f, 0, 1.0f);

				const float CellScreenSize = (ScreenMax.x - ScreenMin.x);
				if (CellScreenSize > 24.0f)
				{
					char CountBuf[16];
					FCStringAnsi::Snprintf(CountBuf, sizeof(CountBuf), "%d", EntityCount);
					const ImVec2 TextSize = ImGui::CalcTextSize(CountBuf);
					DrawList->AddText(
						ImVec2(
							(ScreenMin.x + ScreenMax.x - TextSize.x) * 0.5f,
							(ScreenMin.y + ScreenMax.y - TextSize.y) * 0.5f
						),
						VisColors::HUDTextColor, CountBuf
					);
				}
			}

			DrawGridLines(MobileCellSize, VisColors::MobileGridLineColor);
		}
	}
}

// ====================================================================
// Entity Drawing
// ====================================================================

void FArcVisualizationMinimapDebugger::DrawEntities()
{
	FMassEntityManager* EntityManager = GetEntityManager();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImGuiIO& IO = ImGui::GetIO();

	bHasHoveredEntity = false;
	PhysicsEntityCount = 0;
	MeshEntityCount = 0;
	float BestHoverDistSq = 64.0f;

	const float EntityRadius = FMath::Clamp(Zoom * 50.0f, 2.0f, 6.0f);
	const float PlayerRadius = EntityRadius * 1.5f;

	auto DrawEntityDot = [&](FMassEntityHandle Entity, const FVector& Position, bool bIsMobile, ImU32 BaseColor, ImU32 HoveredColor)
	{
		const ImVec2 ScreenPos = WorldToScreen(Position.X, Position.Y);

		if (ScreenPos.x < CanvasPos.x - PlayerRadius || ScreenPos.x > CanvasPos.x + CanvasSize.x + PlayerRadius ||
			ScreenPos.y < CanvasPos.y - PlayerRadius || ScreenPos.y > CanvasPos.y + CanvasSize.y + PlayerRadius)
		{
			return;
		}

		bool bIsPlayer = false;
		if (EntityManager && EntityManager->IsEntityValid(Entity))
		{
			if (FMassActorFragment* ActorFrag = EntityManager->GetFragmentDataPtr<FMassActorFragment>(Entity))
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

		bool bHasPhysics = false;
		bool bHasMesh = false;
		if (EntityManager && EntityManager->IsEntityValid(Entity))
		{
			const FArcVisRepresentationFragment* VisRep = EntityManager->GetFragmentDataPtr<FArcVisRepresentationFragment>(Entity);
			if (VisRep)
			{
				const FArcMassPhysicsBodyFragment* PhysBody = EntityManager->GetFragmentDataPtr<FArcMassPhysicsBodyFragment>(Entity);
				bHasPhysics = PhysBody && PhysBody->Body != nullptr;
				bHasMesh = VisRep->bHasMeshRendering;
				if (bHasPhysics)
				{
					PhysicsEntityCount++;
				}
				if (bHasMesh)
				{
					MeshEntityCount++;
				}
			}
		}

		ImU32 Color = BaseColor;
		ImU32 HColor = HoveredColor;
		if (bIsPlayer)
		{
			Color = VisColors::PlayerColor;
			HColor = VisColors::PlayerHoveredColor;
		}
		else if (bHasPhysics)
		{
			Color = VisColors::PhysicsEntityColor;
			HColor = VisColors::StaticEntityHoveredColor;
		}
		else if (bHasMesh)
		{
			Color = VisColors::MeshEntityColor;
			HColor = VisColors::StaticEntityHoveredColor;
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
			bHoveredIsPlayer = bIsPlayer;
			bHoveredIsMobile = bIsMobile;
			Color = HColor;
		}

		const float Radius = bIsPlayer ? PlayerRadius : EntityRadius;
		DrawList->AddCircleFilled(ScreenPos, Radius, Color);
	};

	// --- Mesh grid entities ---
	if (bShowMeshGrid)
	{
		UArcEntityVisualizationSubsystem* VisSub = GetStaticVisSubsystem();
		if (VisSub && EntityManager)
		{
			const FArcVisualizationGrid& Grid = VisSub->GetMeshGrid();
			for (const TPair<FIntVector, TArray<FMassEntityHandle>>& Pair : Grid.CellEntities)
			{
				for (const FMassEntityHandle& Entity : Pair.Value)
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

					const FVector& Position = TransformFrag->GetTransform().GetLocation();
					DrawEntityDot(Entity, Position, false, VisColors::StaticEntityColor, VisColors::StaticEntityHoveredColor);
				}
			}
		}
	}

}

// ====================================================================
// Mobile Entity Drawing
// ====================================================================

void FArcVisualizationMinimapDebugger::DrawMobileEntities()
{
	FMassEntityManager* EntityManager = GetEntityManager();
	if (!EntityManager)
	{
		return;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImGuiIO& IO = ImGui::GetIO();

	MobileActorCount = 0;
	MobileISMCount = 0;
	MobileNoneCount = 0;
	MobileActorRadius = 0.0f;
	MobileISMRadius = 0.0f;

	float BestHoverDistSq = 64.0f;
	const float EntityRadius = FMath::Clamp(Zoom * 50.0f, 2.0f, 6.0f);

	FMassEntityQuery MobileQuery(EntityManager->AsShared());
	MobileQuery.AddTagRequirement<FArcMobileVisEntityTag>(EMassFragmentPresence::All);
	MobileQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	MobileQuery.AddRequirement<FArcMobileVisLODFragment>(EMassFragmentAccess::ReadOnly);
	MobileQuery.AddRequirement<FArcMobileVisRepFragment>(EMassFragmentAccess::ReadOnly);
	MobileQuery.AddConstSharedRequirement<FArcMobileVisDistanceConfigFragment>();

	FMassExecutionContext QueryContext(*EntityManager, 0.f);
	MobileQuery.ForEachEntityChunk(QueryContext,
		[this, DrawList, &IO, &BestHoverDistSq, EntityRadius](FMassExecutionContext& Ctx)
		{
			TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TConstArrayView<FArcMobileVisLODFragment> LODFragments = Ctx.GetFragmentView<FArcMobileVisLODFragment>();
			const FArcMobileVisDistanceConfigFragment& DistConfig = Ctx.GetConstSharedFragment<FArcMobileVisDistanceConfigFragment>();

			if (MobileActorRadius == 0.0f)
			{
				MobileActorRadius = DistConfig.ActorRadius;
				MobileISMRadius = DistConfig.ISMRadius;
			}

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FVector& Pos = Transforms[EntityIt].GetTransform().GetLocation();
				const EArcMobileVisLOD LOD = LODFragments[EntityIt].CurrentLOD;

				ImU32 BaseColor;
				ImU32 HoveredColor;
				if (LOD == EArcMobileVisLOD::Actor)
				{
					BaseColor = VisColors::MobileLODActorColor;
					HoveredColor = VisColors::MobileLODActorHoveredColor;
					++MobileActorCount;
				}
				else if (LOD == EArcMobileVisLOD::InstancedMesh)
				{
					BaseColor = VisColors::MobileLODISMColor;
					HoveredColor = VisColors::MobileLODISMHoveredColor;
					++MobileISMCount;
				}
				else
				{
					BaseColor = VisColors::MobileLODNoneColor;
					HoveredColor = VisColors::MobileLODNoneHoveredColor;
					++MobileNoneCount;
				}

				const ImVec2 ScreenPos = WorldToScreen(Pos.X, Pos.Y);

				if (ScreenPos.x < CanvasPos.x - EntityRadius || ScreenPos.x > CanvasPos.x + CanvasSize.x + EntityRadius ||
					ScreenPos.y < CanvasPos.y - EntityRadius || ScreenPos.y > CanvasPos.y + CanvasSize.y + EntityRadius)
				{
					continue;
				}

				const float DX = IO.MousePos.x - ScreenPos.x;
				const float DY = IO.MousePos.y - ScreenPos.y;
				const float DistSq = DX * DX + DY * DY;

				ImU32 DrawColor = BaseColor;
				if (DistSq < BestHoverDistSq)
				{
					BestHoverDistSq = DistSq;
					bHasHoveredEntity = true;
					HoveredEntityPos = Pos;
					HoveredEntityIndex = Ctx.GetEntity(EntityIt).Index;
					bHoveredIsPlayer = false;
					bHoveredIsMobile = true;
					DrawColor = HoveredColor;
				}

				DrawList->AddCircleFilled(ScreenPos, EntityRadius, DrawColor);
			}
		});
}

// ====================================================================
// Mobile Radius Circle Drawing
// ====================================================================

void FArcVisualizationMinimapDebugger::DrawMobileRadiusCircles()
{
	if (SourceEntityPositions.IsEmpty() || (MobileActorRadius == 0.0f && MobileISMRadius == 0.0f))
	{
		return;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	for (const FVector& Pos : SourceEntityPositions)
	{
		const ImVec2 Center = WorldToScreen(Pos.X, Pos.Y);
		DrawList->AddCircle(Center, MobileActorRadius * Zoom, VisColors::MobileActorRadiusColor, 0, 2.0f);
		DrawList->AddCircle(Center, MobileISMRadius * Zoom, VisColors::MobileISMRadiusColor, 0, 1.5f);
	}
}

// ====================================================================
// Mobile Region Drawing
// ====================================================================

void FArcVisualizationMinimapDebugger::DrawMobileRegions()
{
	UArcMobileVisSubsystem* MobileSub = GetMobileVisSubsystem();
	if (!MobileSub)
	{
		return;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	const float CellSize = MobileSub->GetCellSize();
	const int32 RegionExtent = MobileSub->GetRegionExtent();
	const int32 RegionStride = RegionExtent * 2 + 1;
	const float RegionWorldSize = RegionStride * CellSize;

	if (RegionWorldSize <= 0.0f)
	{
		return;
	}

	const float RegionScreenSize = RegionWorldSize * Zoom;
	if (RegionScreenSize < 8.0f)
	{
		return;
	}

	const FVector2D WorldTopLeft = ScreenToWorld(CanvasPos);
	const FVector2D WorldBottomRight = ScreenToWorld(ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y));

	const float WorldMinX = FMath::Min(WorldTopLeft.X, WorldBottomRight.X);
	const float WorldMaxX = FMath::Max(WorldTopLeft.X, WorldBottomRight.X);
	const float WorldMinY = FMath::Min(WorldTopLeft.Y, WorldBottomRight.Y);
	const float WorldMaxY = FMath::Max(WorldTopLeft.Y, WorldBottomRight.Y);

	const int32 RegionMinX = FMath::FloorToInt32(WorldMinX / RegionWorldSize) - 1;
	const int32 RegionMaxX = FMath::CeilToInt32(WorldMaxX / RegionWorldSize) + 1;
	const int32 RegionMinY = FMath::FloorToInt32(WorldMinY / RegionWorldSize) - 1;
	const int32 RegionMaxY = FMath::CeilToInt32(WorldMaxY / RegionWorldSize) + 1;

	const int32 MaxRegions = 50;
	if ((RegionMaxX - RegionMinX) * (RegionMaxY - RegionMinY) > MaxRegions)
	{
		return;
	}

	for (int32 RX = RegionMinX; RX <= RegionMaxX; ++RX)
	{
		for (int32 RY = RegionMinY; RY <= RegionMaxY; ++RY)
		{
			const float RWorldMinX = RX * RegionWorldSize;
			const float RWorldMinY = RY * RegionWorldSize;
			const float RWorldMaxX = RWorldMinX + RegionWorldSize;
			const float RWorldMaxY = RWorldMinY + RegionWorldSize;

			const ImVec2 ScreenMin = WorldToScreen(RWorldMinX, RWorldMaxY);
			const ImVec2 ScreenMax = WorldToScreen(RWorldMaxX, RWorldMinY);

			DrawList->AddRect(ScreenMin, ScreenMax, VisColors::MobileRegionBorderColor, 0.0f, 0, 2.0f);
		}
	}
}

// ====================================================================
// Source Entity Drawing
// ====================================================================

void FArcVisualizationMinimapDebugger::DrawSourceEntities()
{
	using namespace VisColors;

	FMassEntityManager* EntityManager = GetEntityManager();
	if (!EntityManager)
	{
		return;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const float EntityRadius = FMath::Clamp(Zoom * 50.0f, 2.0f, 6.0f);

	SourceEntityCount = 0;
	SourceEntityPositions.Reset();

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

				if (SP.x >= CanvasPos.x - 12.0f && SP.x <= CanvasPos.x + CanvasSize.x + 12.0f &&
					SP.y >= CanvasPos.y - 12.0f && SP.y <= CanvasPos.y + CanvasSize.y + 12.0f)
				{
					const float S = EntityRadius * 2.5f;
					DrawList->AddQuadFilled(
						ImVec2(SP.x, SP.y - S),
						ImVec2(SP.x + S, SP.y),
						ImVec2(SP.x, SP.y + S),
						ImVec2(SP.x - S, SP.y),
						VisColors::SourceColor
					);
					DrawList->AddQuad(
						ImVec2(SP.x, SP.y - S - 1.0f),
						ImVec2(SP.x + S + 1.0f, SP.y),
						ImVec2(SP.x, SP.y + S + 1.0f),
						ImVec2(SP.x - S - 1.0f, SP.y),
						IM_COL32(255, 255, 255, 120), 1.5f
					);
				}

				SourceEntityPositions.Add(Pos);
				++SourceEntityCount;
			}
		});
}

// ====================================================================
// Radius Circle Drawing
// ====================================================================

void FArcVisualizationMinimapDebugger::DrawRadiusCircles()
{
	if (SourceEntityPositions.IsEmpty())
	{
		return;
	}

	UArcEntityVisualizationSubsystem* VisSub = GetStaticVisSubsystem();
	if (!VisSub)
	{
		return;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	const float MeshActRadius = VisSub->GetMeshActivationRadius();
	const float MeshDeactRadius = VisSub->GetMeshDeactivationRadius();
	const float PhysActRadius = VisSub->GetPhysicsActivationRadius();
	const float PhysDeactRadius = VisSub->GetPhysicsDeactivationRadius();

	for (const FVector& Pos : SourceEntityPositions)
	{
		const ImVec2 Center = WorldToScreen(Pos.X, Pos.Y);

		if (bShowMeshGrid)
		{
			const float MeshActScreenR = MeshActRadius * Zoom;
			const float MeshDeactScreenR = MeshDeactRadius * Zoom;
			DrawList->AddCircle(Center, MeshActScreenR, VisColors::MeshActivationRadiusColor, 0, 2.0f);
			DrawList->AddCircle(Center, MeshDeactScreenR, VisColors::MeshDeactivationRadiusColor, 0, 1.0f);
		}

		if (bShowPhysicsGrid)
		{
			const float PhysActScreenR = PhysActRadius * Zoom;
			const float PhysDeactScreenR = PhysDeactRadius * Zoom;
			DrawList->AddCircle(Center, PhysActScreenR, VisColors::PhysicsActivationRadiusColor, 0, 2.0f);
			DrawList->AddCircle(Center, PhysDeactScreenR, VisColors::PhysicsDeactivationRadiusColor, 0, 1.0f);
		}
	}
}

// ====================================================================
// HUD
// ====================================================================

void FArcVisualizationMinimapDebugger::DrawHUD()
{
	// Row 1: Static visualization toggles
	ImGui::Checkbox("Mesh Grid", &bShowMeshGrid);
	ImGui::SameLine();
	ImGui::Checkbox("Physics Grid", &bShowPhysicsGrid);
	ImGui::SameLine();
	ImGui::Checkbox("Active Cells", &bShowActiveCells);
	ImGui::SameLine();
	ImGui::Text("| Zoom: %.4f", Zoom);
	ImGui::SameLine();
	if (ImGui::SmallButton("Reset View"))
	{
		ViewOffset = FVector2D::ZeroVector;
		Zoom = 0.05f;
	}

	// Row 2: Mobile visualization toggles
	ImGui::Checkbox("Mob Grid", &bShowMobileGrid);
	ImGui::SameLine();
	ImGui::Checkbox("Mob Entities", &bShowMobileEntities);
	ImGui::SameLine();
	ImGui::Checkbox("Mob Radii", &bShowMobileRadii);
	ImGui::SameLine();
	ImGui::Checkbox("Mob Regions", &bShowMobileRegions);

	// Stats row 1: Grid stats
	bool bHasPrevStat = false;
	if (bShowMeshGrid)
	{
		ImGui::TextColored(ImVec4(0.31f, 0.63f, 0.86f, 0.9f), "Mesh: %d entities, %d cells (%.0f)", MeshGridEntityCount, MeshGridCellCount, MeshGridCellSize);
		bHasPrevStat = true;
	}
	if (bShowPhysicsGrid)
	{
		if (bHasPrevStat) { ImGui::SameLine(); ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 0.6f), "|"); ImGui::SameLine(); }
		ImGui::TextColored(ImVec4(0.39f, 0.78f, 0.47f, 0.9f), "Physics: %d entities, %d cells (%.0f)", PhysicsGridEntityCount, PhysicsGridCellCount, PhysicsGridCellSize);
		bHasPrevStat = true;
	}
	if (bShowMobileGrid)
	{
		if (bHasPrevStat) { ImGui::SameLine(); ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 0.6f), "|"); ImGui::SameLine(); }
		ImGui::TextColored(ImVec4(0.27f, 0.51f, 0.75f, 0.9f), "Mobile: %d entities, %d cells (%.0f)", MobileEntityCount, MobileCellCount, MobileCellSize);
	}

	// Stats row 2: Active entity counts + mobile LOD breakdown
	ImGui::TextColored(ImVec4(0.31f, 0.63f, 0.86f, 0.9f), "Mesh Active: %d", MeshEntityCount);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 0.6f), "|");
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.39f, 0.78f, 0.47f, 0.9f), "Physics Active: %d", PhysicsEntityCount);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 0.6f), "|");
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.75f, 0.47f, 0.12f, 0.9f), "Sources: %d", SourceEntityCount);

	if (bShowMobileEntities)
	{
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 0.6f), "|");
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.31f, 0.78f, 0.39f, 0.9f), "Actor:%d", MobileActorCount);
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.27f, 0.51f, 0.75f, 0.9f), "ISM:%d", MobileISMCount);
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 0.9f), "None:%d", MobileNoneCount);
	}
}
