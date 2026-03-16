// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcVisualizationMinimapDebugger.h"

#include "imgui.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "ArcMass/ArcMassEntityVisualization.h"
#include "ArcMass/MobileVisualization/ArcMobileVisualization.h"
#include "MassActorSubsystem.h"
#include "MassEntitySubsystem.h"
#include "MassEntityFragments.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

// ====================================================================
// Colors
// ====================================================================

namespace Arcx::GameplayDebugger::VisEntity::Minimap
{
	// Static visualization
	static const ImU32 StaticCellFillColor = IM_COL32(40, 120, 60, 80);
	static const ImU32 StaticCellBorderColor = IM_COL32(80, 80, 80, 120);
	static const ImU32 StaticEntityColor = IM_COL32(220, 200, 50, 255);
	static const ImU32 StaticEntityHoveredColor = IM_COL32(255, 255, 100, 255);

	// Mobile visualization
	static const ImU32 MobileCellFillColor = IM_COL32(40, 60, 140, 80);
	static const ImU32 MobileCellBorderColor = IM_COL32(60, 60, 120, 120);
	static const ImU32 MobileEntityColor = IM_COL32(100, 180, 255, 255);
	static const ImU32 MobileEntityHoveredColor = IM_COL32(140, 210, 255, 255);

	// Active cell highlight
	static const ImU32 ActiveCellColor = IM_COL32(60, 200, 100, 60);

	// Player entities
	static const ImU32 PlayerColor = IM_COL32(255, 80, 80, 255);
	static const ImU32 PlayerHoveredColor = IM_COL32(255, 140, 140, 255);

	// Source entities (mobile vis observers)
	static const ImU32 SourceColor = IM_COL32(255, 160, 40, 255);

	// General
	static const ImU32 BackgroundColor = IM_COL32(20, 20, 25, 240);
	static const ImU32 OriginColor = IM_COL32(255, 60, 60, 180);
	static const ImU32 HUDTextColor = IM_COL32(200, 200, 200, 255);
	static const ImU32 PlayerCellColor = IM_COL32(255, 80, 80, 60);

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

void FArcVisualizationMinimapDebugger::Initialize()
{
	ViewOffset = FVector2D::ZeroVector;
	Zoom = 0.05f;
	bIsPanning = false;
	bHasHoveredEntity = false;
	StaticEntityCount = 0;
	StaticCellCount = 0;
	MobileEntityCount = 0;
	MobileCellCount = 0;
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

	ImGui::InvisibleButton("##viscanvas", CanvasSize, ImGuiButtonFlags_MouseButtonMiddle);
	const bool bCanvasHovered = ImGui::IsItemHovered();

	HandleInput();

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->PushClipRect(CanvasPos, ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y), true);

	// Background
	DrawList->AddRectFilled(CanvasPos, ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y), Arcx::GameplayDebugger::VisEntity::Minimap::BackgroundColor);

	// Origin crosshair
	{
		const ImVec2 Origin = WorldToScreen(0.0f, 0.0f);
		DrawList->AddLine(ImVec2(CanvasPos.x, Origin.y), ImVec2(CanvasPos.x + CanvasSize.x, Origin.y), Arcx::GameplayDebugger::VisEntity::Minimap::OriginColor, 1.0f);
		DrawList->AddLine(ImVec2(Origin.x, CanvasPos.y), ImVec2(Origin.x, CanvasPos.y + CanvasSize.y), Arcx::GameplayDebugger::VisEntity::Minimap::OriginColor, 1.0f);
	}

	DrawGrid();
	DrawEntities();

	DrawList->PopClipRect();

	// Tooltip for hovered entity
	if (bHasHoveredEntity && bCanvasHovered)
	{
		ImGui::BeginTooltip();
		ImGui::Text("Entity: E%d%s%s", HoveredEntityIndex,
			bHoveredIsPlayer ? " [Player]" : "",
			bHoveredIsMobile ? " (Mobile)" : " (Static)");
		ImGui::Text("Position: (%.0f, %.0f, %.0f)", HoveredEntityPos.X, HoveredEntityPos.Y, HoveredEntityPos.Z);
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
// Grid Drawing
// ====================================================================

void FArcVisualizationMinimapDebugger::DrawGrid()
{
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	StaticEntityCount = 0;
	StaticCellCount = 0;
	MobileEntityCount = 0;
	MobileCellCount = 0;

	// --- Static visualization grid ---
	if (bShowStaticGrid)
	{
		UArcEntityVisualizationSubsystem* StaticSub = GetStaticVisSubsystem();
		if (StaticSub)
		{
			const FArcVisualizationGrid& Grid = StaticSub->GetGrid();
			StaticCellSize = Grid.CellSize;
			StaticCellCount = Grid.CellEntities.Num();

			// Draw active cells highlight
			if (bShowActiveCells)
			{
				TArray<FIntVector> ActiveCells;
				StaticSub->GetActiveCells(ActiveCells);
				for (const FIntVector& Cell : ActiveCells)
				{
					const float WorldMinX = Cell.X * StaticCellSize;
					const float WorldMinY = Cell.Y * StaticCellSize;
					const float WorldMaxX = WorldMinX + StaticCellSize;
					const float WorldMaxY = WorldMinY + StaticCellSize;

					const ImVec2 ScreenMin = WorldToScreen(WorldMinX, WorldMaxY);
					const ImVec2 ScreenMax = WorldToScreen(WorldMaxX, WorldMinY);

					if (ScreenMax.x < CanvasPos.x || ScreenMin.x > CanvasPos.x + CanvasSize.x ||
						ScreenMax.y < CanvasPos.y || ScreenMin.y > CanvasPos.y + CanvasSize.y)
					{
						continue;
					}

					DrawList->AddRectFilled(ScreenMin, ScreenMax, Arcx::GameplayDebugger::VisEntity::Minimap::ActiveCellColor);
				}

				// Draw player cell
				const FIntVector PlayerCell = StaticSub->GetLastPlayerCell();
				if (PlayerCell.X != TNumericLimits<int32>::Max())
				{
					const float WorldMinX = PlayerCell.X * StaticCellSize;
					const float WorldMinY = PlayerCell.Y * StaticCellSize;
					const float WorldMaxX = WorldMinX + StaticCellSize;
					const float WorldMaxY = WorldMinY + StaticCellSize;

					const ImVec2 ScreenMin = WorldToScreen(WorldMinX, WorldMaxY);
					const ImVec2 ScreenMax = WorldToScreen(WorldMaxX, WorldMinY);

					DrawList->AddRectFilled(ScreenMin, ScreenMax, Arcx::GameplayDebugger::VisEntity::Minimap::PlayerCellColor);
					DrawList->AddRect(ScreenMin, ScreenMax, IM_COL32(255, 80, 80, 200), 0.0f, 0, 2.0f);
				}
			}

			for (const auto& Pair : Grid.CellEntities)
			{
				const FIntVector& Coords = Pair.Key;
				const int32 EntityCount = Pair.Value.Num();
				StaticEntityCount += EntityCount;

				const float WorldMinX = Coords.X * StaticCellSize;
				const float WorldMinY = Coords.Y * StaticCellSize;
				const float WorldMaxX = WorldMinX + StaticCellSize;
				const float WorldMaxY = WorldMinY + StaticCellSize;

				const ImVec2 ScreenMin = WorldToScreen(WorldMinX, WorldMaxY);
				const ImVec2 ScreenMax = WorldToScreen(WorldMaxX, WorldMinY);

				if (ScreenMax.x < CanvasPos.x || ScreenMin.x > CanvasPos.x + CanvasSize.x ||
					ScreenMax.y < CanvasPos.y || ScreenMin.y > CanvasPos.y + CanvasSize.y)
				{
					continue;
				}

				DrawList->AddRectFilled(ScreenMin, ScreenMax, Arcx::GameplayDebugger::VisEntity::Minimap::CellColorByCount(EntityCount, Arcx::GameplayDebugger::VisEntity::Minimap::StaticCellFillColor));
				DrawList->AddRect(ScreenMin, ScreenMax, Arcx::GameplayDebugger::VisEntity::Minimap::StaticCellBorderColor, 0.0f, 0, 1.0f);

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
						Arcx::GameplayDebugger::VisEntity::Minimap::HUDTextColor, CountBuf
					);
				}
			}
		}
	}

	// --- Mobile visualization grid ---
	if (bShowMobileGrid)
	{
		UArcMobileVisSubsystem* MobileSub = GetMobileVisSubsystem();
		if (MobileSub)
		{
			MobileCellSize = MobileSub->GetCellSize();

			// Mobile subsystem doesn't expose its EntityCells map directly,
			// so we iterate through cells by querying entity positions.
			// For grid visualization, we draw source entity cells instead.
			const auto& SourcePositions = MobileSub->GetSourcePositions();
			for (const auto& Pair : SourcePositions)
			{
				const FIntVector& Cell = Pair.Value;
				const float WorldMinX = Cell.X * MobileCellSize;
				const float WorldMinY = Cell.Y * MobileCellSize;
				const float WorldMaxX = WorldMinX + MobileCellSize;
				const float WorldMaxY = WorldMinY + MobileCellSize;

				const ImVec2 ScreenMin = WorldToScreen(WorldMinX, WorldMaxY);
				const ImVec2 ScreenMax = WorldToScreen(WorldMaxX, WorldMinY);

				if (ScreenMax.x < CanvasPos.x || ScreenMin.x > CanvasPos.x + CanvasSize.x ||
					ScreenMax.y < CanvasPos.y || ScreenMin.y > CanvasPos.y + CanvasSize.y)
				{
					continue;
				}

				DrawList->AddRectFilled(ScreenMin, ScreenMax, Arcx::GameplayDebugger::VisEntity::Minimap::SourceColor & 0x40FFFFFF);
				DrawList->AddRect(ScreenMin, ScreenMax, Arcx::GameplayDebugger::VisEntity::Minimap::SourceColor, 0.0f, 0, 2.0f);
			}
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

		// Player detection
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

		ImU32 Color = bIsPlayer ? Arcx::GameplayDebugger::VisEntity::Minimap::PlayerColor : BaseColor;
		ImU32 HColor = bIsPlayer ? Arcx::GameplayDebugger::VisEntity::Minimap::PlayerHoveredColor : HoveredColor;

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

	// --- Static entities ---
	if (bShowStaticGrid)
	{
		UArcEntityVisualizationSubsystem* StaticSub = GetStaticVisSubsystem();
		if (StaticSub && EntityManager)
		{
			const FArcVisualizationGrid& Grid = StaticSub->GetGrid();
			for (const auto& Pair : Grid.CellEntities)
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
					DrawEntityDot(Entity, Position, false, Arcx::GameplayDebugger::VisEntity::Minimap::StaticEntityColor, Arcx::GameplayDebugger::VisEntity::Minimap::StaticEntityHoveredColor);
				}
			}
		}
	}

	// --- Mobile entities ---
	if (bShowMobileGrid)
	{
		UArcMobileVisSubsystem* MobileSub = GetMobileVisSubsystem();
		if (MobileSub && EntityManager)
		{
			// Iterate all entities that have FArcMobileVisRepFragment to get their positions
			// We can get cell data from the subsystem
			const auto& SourcePositions = MobileSub->GetSourcePositions();

			// Draw source entities (observers/cameras) as diamonds
			for (const auto& Pair : SourcePositions)
			{
				const FMassEntityHandle& Entity = Pair.Key;
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
				const ImVec2 ScreenPos = WorldToScreen(Position.X, Position.Y);

				if (ScreenPos.x >= CanvasPos.x - 8.0f && ScreenPos.x <= CanvasPos.x + CanvasSize.x + 8.0f &&
					ScreenPos.y >= CanvasPos.y - 8.0f && ScreenPos.y <= CanvasPos.y + CanvasSize.y + 8.0f)
				{
					// Draw diamond shape for source entities
					const float S = EntityRadius * 2.0f;
					DrawList->AddQuadFilled(
						ImVec2(ScreenPos.x, ScreenPos.y - S),
						ImVec2(ScreenPos.x + S, ScreenPos.y),
						ImVec2(ScreenPos.x, ScreenPos.y + S),
						ImVec2(ScreenPos.x - S, ScreenPos.y),
						Arcx::GameplayDebugger::VisEntity::Minimap::SourceColor
					);
				}

				MobileEntityCount++;
			}
		}
	}
}

// ====================================================================
// HUD
// ====================================================================

void FArcVisualizationMinimapDebugger::DrawHUD()
{
	ImGui::Checkbox("Static", &bShowStaticGrid);
	ImGui::SameLine();
	ImGui::Checkbox("Mobile", &bShowMobileGrid);
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

	// Stats row
	if (bShowStaticGrid)
	{
		ImGui::TextColored(ImVec4(0.86f, 0.78f, 0.2f, 1.0f), "Static: %d entities, %d cells (%.0f)", StaticEntityCount, StaticCellCount, StaticCellSize);
	}
	if (bShowMobileGrid)
	{
		if (bShowStaticGrid)
		{
			ImGui::SameLine();
			ImGui::Text("|");
			ImGui::SameLine();
		}
		ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Mobile: %d sources (%.0f)", MobileEntityCount, MobileCellSize);
	}
}
