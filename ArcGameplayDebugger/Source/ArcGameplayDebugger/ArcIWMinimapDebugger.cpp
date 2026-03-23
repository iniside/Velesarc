// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWMinimapDebugger.h"

#include "imgui.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "ArcInstancedWorld/ArcIWVisualizationSubsystem.h"
#include "ArcInstancedWorld/ArcIWTypes.h"
#include "ArcInstancedWorld/ArcIWPartitionActor.h"
#include "ArcInstancedWorld/ArcIWSettings.h"
#include "MassEntitySubsystem.h"
#include "MassEntityFragments.h"
#include "EngineUtils.h"

// ====================================================================
// Colors
// ====================================================================

namespace Arcx::GameplayDebugger::IW::Minimap
{
	// World Partition cells
	static const ImU32 WPCellBorderColor = IM_COL32(200, 80, 80, 140);
	static const ImU32 WPCellFillColor = IM_COL32(200, 80, 80, 20);

	// ISM visualization grid — borders are subtle, occupied cells get brighter fill
	static const ImU32 ISMGridBorderColor = IM_COL32(60, 160, 60, 80);
	static const ImU32 ISMOccupiedFillColor = IM_COL32(40, 140, 40, 60);
	static const ImU32 ISMActiveFillColor = IM_COL32(60, 200, 100, 45);

	// Actor hydration grid — blue tint, borders visible, active cells brighter
	static const ImU32 ActorGridBorderColor = IM_COL32(70, 110, 200, 90);
	static const ImU32 ActorActiveFillColor = IM_COL32(50, 90, 200, 50);

	// Dehydration radius grid — orange tint
	static const ImU32 DehydrationGridBorderColor = IM_COL32(180, 120, 50, 80);
	static const ImU32 DehydrationActiveFillColor = IM_COL32(160, 110, 40, 35);

	// Player cell highlight
	static const ImU32 PlayerCellColor = IM_COL32(255, 80, 80, 60);

	// Entities — 3 states: ISM (green), Actor (blue), Inactive (yellow)
	static const ImU32 EntityInactiveColor = IM_COL32(220, 200, 50, 255);
	static const ImU32 EntityInactiveHoveredColor = IM_COL32(255, 255, 100, 255);
	static const ImU32 EntityISMColor = IM_COL32(80, 220, 80, 255);
	static const ImU32 EntityISMHoveredColor = IM_COL32(120, 255, 120, 255);
	static const ImU32 EntityActorColor = IM_COL32(80, 140, 255, 255);
	static const ImU32 EntityActorHoveredColor = IM_COL32(120, 180, 255, 255);
	static const ImU32 EntityKeepHydratedColor = IM_COL32(255, 100, 200, 255);
	static const ImU32 EntityKeepHydratedHoveredColor = IM_COL32(255, 150, 230, 255);

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
	OccupiedCellCount = 0;
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
	if (VisSub)
	{
		ISMCellSize = VisSub->GetCellSize();
	}

	// WP cell size: query from first partition actor's grid name, fall back to settings
	UWorld* DebugWorld = GetDebugWorld();
	WPCellSize = static_cast<float>(UArcIWSettings::Get()->DefaultGridCellSize);
	if (DebugWorld)
	{
		for (TActorIterator<AArcIWPartitionActor> It(DebugWorld); It; ++It)
		{
			WPCellSize = static_cast<float>(AArcIWPartitionActor::GetGridCellSize(DebugWorld, It->GetGridName()));
			break;
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
		const char* StateLabel = bHoveredIsActor
			? (bHoveredKeepHydrated ? "[Actor, KeepHydrated]" : "[Actor]")
			: (bHoveredHasISM ? "[ISM]" : "[Inactive]");
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

namespace
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

	// --- World Partition cells (largest grid) ---
	if (bShowWPCells)
	{
		DrawGridCells(DrawList, WPCellSize, WPCellFillColor, WPCellBorderColor, CanvasPos, CanvasSize, W2S, S2W);
	}

	// Helper: convert cell coords to screen rect, returns false if offscreen
	auto CellToScreenRect = [this](const FIntVector& Cell, ImVec2& OutMin, ImVec2& OutMax) -> bool
	{
		const float WorldMinX = Cell.X * ISMCellSize;
		const float WorldMinY = Cell.Y * ISMCellSize;
		OutMin = WorldToScreen(WorldMinX, WorldMinY + ISMCellSize);
		OutMax = WorldToScreen(WorldMinX + ISMCellSize, WorldMinY);
		return OutMax.x >= CanvasPos.x && OutMin.x <= CanvasPos.x + CanvasSize.x
			&& OutMax.y >= CanvasPos.y && OutMin.y <= CanvasPos.y + CanvasSize.y;
	};

	// Build set of occupied cells for lookup
	const TMap<FIntVector, TArray<FMassEntityHandle>>* CellEntitiesPtr = nullptr;
	if (VisSub)
	{
		CellEntitiesPtr = &VisSub->GetCellEntities();
		OccupiedCellCount = CellEntitiesPtr->Num();
		for (const TPair<FIntVector, TArray<FMassEntityHandle>>& Pair : *CellEntitiesPtr)
		{
			EntityCount += Pair.Value.Num();
		}
	}

	// --- Dehydration radius grid (drawn first = behind everything) ---
	if (bShowDehydrationGrid && VisSub && VisSub->GetLastPlayerCell().X != TNumericLimits<int32>::Max())
	{
		TArray<FIntVector> DehydrationCells;
		VisSub->GetCellsInRadius(VisSub->GetLastPlayerCell(), VisSub->GetDehydrationRadiusCells(), DehydrationCells);
		for (const FIntVector& Cell : DehydrationCells)
		{
			ImVec2 ScreenMin, ScreenMax;
			if (!CellToScreenRect(Cell, ScreenMin, ScreenMax)) continue;

			// Fill occupied cells brighter
			if (CellEntitiesPtr && CellEntitiesPtr->Contains(Cell))
			{
				DrawList->AddRectFilled(ScreenMin, ScreenMax, DehydrationActiveFillColor);
			}
			DrawList->AddRect(ScreenMin, ScreenMax, DehydrationGridBorderColor, 0.0f, 0, 1.0f);
		}
	}

	// --- ISM visualization grid ---
	if (bShowISMGrid && VisSub && VisSub->GetLastPlayerCell().X != TNumericLimits<int32>::Max())
	{
		// Draw grid + active cell fill for all cells in swap radius
		TArray<FIntVector> ISMCells;
		VisSub->GetActiveCells(ISMCells);
		for (const FIntVector& Cell : ISMCells)
		{
			ImVec2 ScreenMin, ScreenMax;
			if (!CellToScreenRect(Cell, ScreenMin, ScreenMax)) continue;

			// Fill active cells that are occupied (have entities registered)
			if (bShowISMActive && CellEntitiesPtr && CellEntitiesPtr->Contains(Cell))
			{
				const TArray<FMassEntityHandle>& Entities = (*CellEntitiesPtr)[Cell];
				DrawList->AddRectFilled(ScreenMin, ScreenMax,
					CellColorByCount(Entities.Num(), ISMOccupiedFillColor));

				// Cell entity count label
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
			else
			{
				// Empty active cell — subtle fill
				DrawList->AddRectFilled(ScreenMin, ScreenMax, ISMActiveFillColor);
			}

			DrawList->AddRect(ScreenMin, ScreenMax, ISMGridBorderColor, 0.0f, 0, 1.0f);
		}

		// Player cell highlight
		const FIntVector PlayerCell = VisSub->GetLastPlayerCell();
		{
			ImVec2 ScreenMin, ScreenMax;
			if (CellToScreenRect(PlayerCell, ScreenMin, ScreenMax))
			{
				DrawList->AddRectFilled(ScreenMin, ScreenMax, PlayerCellColor);
				DrawList->AddRect(ScreenMin, ScreenMax, IM_COL32(255, 80, 80, 200), 0.0f, 0, 2.0f);
			}
		}
	}
	else if (bShowISMGrid)
	{
		// No subsystem — just draw the raw grid lines
		DrawGridCells(DrawList, ISMCellSize, ISMActiveFillColor, ISMGridBorderColor, CanvasPos, CanvasSize, W2S, S2W);
	}

	// --- Actor hydration grid (drawn on top of ISM) ---
	if (bShowActorGrid && VisSub && VisSub->GetLastPlayerCell().X != TNumericLimits<int32>::Max())
	{
		TArray<FIntVector> ActorCells;
		VisSub->GetCellsInRadius(VisSub->GetLastPlayerCell(), VisSub->GetActorRadiusCells(), ActorCells);
		for (const FIntVector& Cell : ActorCells)
		{
			ImVec2 ScreenMin, ScreenMax;
			if (!CellToScreenRect(Cell, ScreenMin, ScreenMax)) continue;

			// Fill occupied cells brighter
			if (CellEntitiesPtr && CellEntitiesPtr->Contains(Cell))
			{
				DrawList->AddRectFilled(ScreenMin, ScreenMax, ActorActiveFillColor);
			}
			DrawList->AddRect(ScreenMin, ScreenMax, ActorGridBorderColor, 0.0f, 0, 1.0f);
		}
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
	ISMEntityCount = 0;
	ActorEntityCount = 0;
	InactiveEntityCount = 0;

	const float EntityRadius = FMath::Clamp(Zoom * 200.0f, 2.0f, 6.0f);

	const TMap<FIntVector, TArray<FMassEntityHandle>>& CellEntities = VisSub->GetCellEntities();
	for (const TPair<FIntVector, TArray<FMassEntityHandle>>& Pair : CellEntities)
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
			const ImVec2 ScreenPos = WorldToScreen(Position.X, Position.Y);

			if (ScreenPos.x < CanvasPos.x - EntityRadius || ScreenPos.x > CanvasPos.x + CanvasSize.x + EntityRadius ||
				ScreenPos.y < CanvasPos.y - EntityRadius || ScreenPos.y > CanvasPos.y + CanvasSize.y + EntityRadius)
			{
				continue;
			}

			// Determine entity visualization state
			bool bHasActiveISM = false;
			bool bIsActor = false;
			bool bKeepHydrated = false;
			const FArcIWInstanceFragment* InstanceFrag = EntityManager->GetFragmentDataPtr<FArcIWInstanceFragment>(Entity);
			if (InstanceFrag)
			{
				bIsActor = InstanceFrag->bIsActorRepresentation;
				bKeepHydrated = InstanceFrag->bKeepHydrated;
				if (!bIsActor)
				{
					for (int32 Id : InstanceFrag->ISMInstanceIds)
					{
						if (Id != INDEX_NONE)
						{
							bHasActiveISM = true;
							break;
						}
					}
				}
			}

			// Pick color based on state
			ImU32 Color;
			ImU32 HColor;
			if (bIsActor && bKeepHydrated)
			{
				Color = EntityKeepHydratedColor;
				HColor = EntityKeepHydratedHoveredColor;
				++ActorEntityCount;
			}
			else if (bIsActor)
			{
				Color = EntityActorColor;
				HColor = EntityActorHoveredColor;
				++ActorEntityCount;
			}
			else if (bHasActiveISM)
			{
				Color = EntityISMColor;
				HColor = EntityISMHoveredColor;
				++ISMEntityCount;
			}
			else
			{
				Color = EntityInactiveColor;
				HColor = EntityInactiveHoveredColor;
				++InactiveEntityCount;
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
				bHoveredHasISM = bHasActiveISM;
				bHoveredIsActor = bIsActor;
				bHoveredKeepHydrated = bKeepHydrated;
				Color = HColor;
			}

			DrawList->AddCircleFilled(ScreenPos, EntityRadius, Color);
		}
	}
}

// ====================================================================
// HUD
// ====================================================================

void FArcIWMinimapDebugger::DrawHUD()
{
	// Grid toggles
	ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "WP");
	ImGui::SameLine();
	ImGui::Checkbox("##wp", &bShowWPCells);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "ISM");
	ImGui::SameLine();
	ImGui::Checkbox("##ismgrid", &bShowISMGrid);
	ImGui::SameLine();
	ImGui::Checkbox("Occupied##ismactive", &bShowISMActive);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.4f, 0.5f, 0.9f, 1.0f), "Actor");
	ImGui::SameLine();
	ImGui::Checkbox("##actorgrid", &bShowActorGrid);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.8f, 0.55f, 0.25f, 1.0f), "Dehydr");
	ImGui::SameLine();
	ImGui::Checkbox("##dehydrgrid", &bShowDehydrationGrid);
	ImGui::SameLine();
	ImGui::Text("|");
	ImGui::SameLine();
	ImGui::Checkbox("Entities", &bShowEntities);
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

	// Stats with color-coded entity counts and cell sizes
	ImGui::TextColored(ImVec4(0.3f, 0.86f, 0.3f, 1.0f), "ISM:%d", ISMEntityCount);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.3f, 0.55f, 1.0f, 1.0f), "Actor:%d", ActorEntityCount);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.86f, 0.78f, 0.2f, 1.0f), "Inactive:%d", InactiveEntityCount);
	ImGui::SameLine();
	ImGui::Text("| Total:%d Cells:%d", EntityCount, OccupiedCellCount);
	ImGui::Text("WP:%.0fcm ISM:%.0fcm", WPCellSize, ISMCellSize);
	UArcIWVisualizationSubsystem* HUDVisSub = GetVisualizationSubsystem();
	if (HUDVisSub)
	{
		ImGui::SameLine();
		ImGui::Text("Actor:%.0fcm Dehydr:%.0fcm", HUDVisSub->GetActorRadius(), HUDVisSub->GetDehydrationRadius());
	}
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

	const TMap<FIntVector, TArray<FMassEntityHandle>>& CellEntities = VisSub->GetCellEntities();
	for (const TPair<FIntVector, TArray<FMassEntityHandle>>& Pair : CellEntities)
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

			const FVector Position = TransformFrag->GetTransform().GetLocation();

			// Determine entity state
			FColor DebugColor = FColor::Yellow;
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
						DebugColor = FColor::Green;
					}
				}
			}

			DrawDebugPoint(World, Position, 8.0f, DebugColor, false, -1.0f);

			// Draw small upward line to make points visible in 3D
			DrawDebugLine(World, Position, Position + FVector(0, 0, 100.0f), DebugColor, false, -1.0f, 0, 1.0f);
		}
	}
}
