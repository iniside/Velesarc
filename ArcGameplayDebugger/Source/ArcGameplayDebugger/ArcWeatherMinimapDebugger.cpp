// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcWeatherMinimapDebugger.h"

#include "imgui.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "ArcWeather/ArcWeatherSubsystem.h"
#include "ArcWeather/ArcClimateRegionVolume.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

// ====================================================================
// Colors
// ====================================================================

namespace Arcx::GameplayDebugger::Weather::Minimap
{
	static const ImU32 BackgroundColor = IM_COL32(20, 20, 25, 240);
	static const ImU32 OriginColor = IM_COL32(255, 60, 60, 180);
	static const ImU32 HUDTextColor = IM_COL32(200, 200, 200, 255);
	static const ImU32 CellBorderColor = IM_COL32(80, 80, 80, 120);
	static const ImU32 SelectedCellBorderColor = IM_COL32(255, 255, 0, 255);
	static const ImU32 HoveredCellBorderColor = IM_COL32(255, 255, 255, 160);
	static const ImU32 PlayerCellColor = IM_COL32(255, 80, 80, 60);
	static const ImU32 PlayerCellBorderColor = IM_COL32(255, 80, 80, 200);
	static const ImU32 RegionOutlineColor = IM_COL32(180, 140, 255, 180);
	static const ImU32 FreezeOverlayColor = IM_COL32(0, 200, 255, 80);
	static const ImU32 RainOverlayColor = IM_COL32(60, 80, 255, 80);
	static const ImU32 NoCellsTextColor = IM_COL32(150, 150, 150, 200);
}

// ====================================================================
// Color helpers (file-scoped)
// ====================================================================

namespace ArcWeatherMinimapInternal
{
	ImU32 TemperatureToColor(float Temp, float RangeMin, float RangeMax)
	{
		const float T = FMath::Clamp((Temp - RangeMin) / (RangeMax - RangeMin), 0.f, 1.f);
		// Blue (cold) -> Red (hot)
		const uint8 R = static_cast<uint8>(T * 255.f);
		const uint8 B = static_cast<uint8>((1.f - T) * 255.f);
		const uint8 G = static_cast<uint8>(FMath::Clamp((0.5f - FMath::Abs(T - 0.5f)) * 2.f, 0.f, 1.f) * 120.f);
		return IM_COL32(R, G, B, 180);
	}

	ImU32 HumidityToColor(float Humidity)
	{
		const float T = FMath::Clamp(Humidity / 100.f, 0.f, 1.f);
		// Yellow (dry) -> Blue (wet)
		const uint8 R = static_cast<uint8>((1.f - T) * 220.f);
		const uint8 G = static_cast<uint8>((1.f - T) * 200.f);
		const uint8 B = static_cast<uint8>(80 + T * 175.f);
		return IM_COL32(R, G, B, 180);
	}

	ImU32 CombinedColor(float Temp, float Humidity, float RangeMin, float RangeMax)
	{
		const float TempT = FMath::Clamp((Temp - RangeMin) / (RangeMax - RangeMin), 0.f, 1.f);
		const float HumT = FMath::Clamp(Humidity / 100.f, 0.f, 1.f);
		// Temperature as hue (blue->red), humidity as brightness
		const uint8 R = static_cast<uint8>(TempT * 255.f);
		const uint8 B = static_cast<uint8>((1.f - TempT) * 255.f);
		const uint8 G = static_cast<uint8>(FMath::Clamp((0.5f - FMath::Abs(TempT - 0.5f)) * 2.f, 0.f, 1.f) * 120.f);
		const uint8 A = static_cast<uint8>(80 + HumT * 160.f);
		return IM_COL32(R, G, B, A);
	}
}

// ====================================================================
// Lifecycle
// ====================================================================

void FArcWeatherMinimapDebugger::Initialize()
{
	ViewOffset = FVector2D::ZeroVector;
	Zoom = 0.05f;
	bIsPanning = false;
	bHasSelectedCell = false;
	bHasHoveredCell = false;
	CellCount = 0;
	PaintTempOffset = 0.f;
	PaintHumOffset = 0.f;
}

void FArcWeatherMinimapDebugger::Uninitialize()
{
}

// ====================================================================
// Helpers
// ====================================================================

UWorld* FArcWeatherMinimapDebugger::GetDebugWorld()
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return nullptr;
	}
	return GEngine->GameViewport->GetWorld();
}

UArcWeatherSubsystem* FArcWeatherMinimapDebugger::GetWeatherSubsystem() const
{
	UWorld* World = GetDebugWorld();
	return World ? World->GetSubsystem<UArcWeatherSubsystem>() : nullptr;
}

ImVec2 FArcWeatherMinimapDebugger::WorldToScreen(float WorldX, float WorldY) const
{
	return ImVec2(
		CanvasCenter.x + (WorldX - static_cast<float>(ViewOffset.X)) * Zoom,
		CanvasCenter.y - (WorldY - static_cast<float>(ViewOffset.Y)) * Zoom
	);
}

FVector2D FArcWeatherMinimapDebugger::ScreenToWorld(const ImVec2& ScreenPos) const
{
	return FVector2D(
		ViewOffset.X + (ScreenPos.x - CanvasCenter.x) / Zoom,
		ViewOffset.Y - (ScreenPos.y - CanvasCenter.y) / Zoom
	);
}

FArcWeatherMinimapDebugger::FEffectiveClimate FArcWeatherMinimapDebugger::ComputeEffective(
	const FArcWeatherCell& Cell, const UArcWeatherSubsystem* Sub) const
{
	FArcClimateParams CellEffective = Cell.GetEffective();
	FEffectiveClimate Result;
	Result.Temperature = CellEffective.Temperature;
	Result.Humidity = CellEffective.Humidity;

	const int32 SeasonIdx = Sub->GetCurrentSeason();
	if (Sub->Seasons.IsValidIndex(SeasonIdx))
	{
		const FArcClimateParams& Season = Sub->Seasons[SeasonIdx];
		Result.Temperature += Season.Temperature;
		Result.Humidity = FMath::Clamp(Result.Humidity + Season.Humidity, 0.f, 100.f);
	}

	return Result;
}

// ====================================================================
// Main Draw
// ====================================================================

void FArcWeatherMinimapDebugger::Draw()
{
	ImGui::SetNextWindowSize(ImVec2(700, 650), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Weather Minimap", &bShow, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::End();
		return;
	}

	UArcWeatherSubsystem* WeatherSub = GetWeatherSubsystem();
	if (!WeatherSub)
	{
		ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "No UArcWeatherSubsystem available.");
		ImGui::End();
		return;
	}

	DrawHUD();
	DrawSelectedCellPanel();
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

	ImGui::InvisibleButton("##weathercanvas", CanvasSize,
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);
	const bool bCanvasHovered = ImGui::IsItemHovered();

	HandleInput();

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->PushClipRect(CanvasPos, ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y), true);

	// Background
	DrawList->AddRectFilled(CanvasPos, ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y),
		Arcx::GameplayDebugger::Weather::Minimap::BackgroundColor);

	// Origin crosshair
	{
		const ImVec2 Origin = WorldToScreen(0.0f, 0.0f);
		DrawList->AddLine(ImVec2(CanvasPos.x, Origin.y), ImVec2(CanvasPos.x + CanvasSize.x, Origin.y),
			Arcx::GameplayDebugger::Weather::Minimap::OriginColor, 1.0f);
		DrawList->AddLine(ImVec2(Origin.x, CanvasPos.y), ImVec2(Origin.x, CanvasPos.y + CanvasSize.y),
			Arcx::GameplayDebugger::Weather::Minimap::OriginColor, 1.0f);
	}

	DrawCells();

	if (bShowRegions)
	{
		DrawRegionVolumes();
	}

	DrawPlayerCell();

	// "No weather cells" label (inside clip rect)
	if (CellCount == 0)
	{
		const ImVec2 TextSize = ImGui::CalcTextSize("No weather cells");
		DrawList->AddText(
			ImVec2(CanvasCenter.x - TextSize.x * 0.5f, CanvasCenter.y - TextSize.y * 0.5f),
			Arcx::GameplayDebugger::Weather::Minimap::NoCellsTextColor, "No weather cells");
	}

	DrawList->PopClipRect();

	// Hovered cell tooltip (outside clip rect so tooltip renders normally)
	if (bHasHoveredCell && bCanvasHovered)
	{
		const TMap<FIntVector, FArcWeatherCell>& Grid = WeatherSub->GetGrid();
		if (const FArcWeatherCell* Cell = Grid.Find(HoveredCellCoords))
		{
			const FEffectiveClimate Eff = ComputeEffective(*Cell, WeatherSub);
			ImGui::BeginTooltip();
			ImGui::Text("Cell (%d, %d, %d)", HoveredCellCoords.X, HoveredCellCoords.Y, HoveredCellCoords.Z);
			ImGui::Text("Effective: %.1f C, %.1f%%", Eff.Temperature, Eff.Humidity);
			ImGui::Text("Base: %.1f C, %.1f%%", Cell->BaseClimate.Temperature, Cell->BaseClimate.Humidity);
			ImGui::Text("Offset: %+.1f C, %+.1f%%", Cell->TemperatureOffset, Cell->HumidityOffset);
			ImGui::EndTooltip();
		}
	}

	ImGui::End();
}

// ====================================================================
// Input Handling
// ====================================================================

void FArcWeatherMinimapDebugger::HandleInput()
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

	// --- Cell selection (left click) and paint mode (shift+left drag) ---
	if (bCanvasHovered)
	{
		UArcWeatherSubsystem* WeatherSub = GetWeatherSubsystem();
		if (WeatherSub)
		{
			const FVector2D WorldPos = ScreenToWorld(IO.MousePos);
			const FIntVector CellCoords(
				FMath::FloorToInt32(WorldPos.X / WeatherSub->CellSize),
				FMath::FloorToInt32(WorldPos.Y / WeatherSub->CellSize),
				0
			);

			if (IO.KeyShift && ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				// Paint mode: apply offsets to hovered cell
				if (WeatherSub->GetGrid().Contains(CellCoords))
				{
					WeatherSub->SetWeatherOffset(CellCoords, PaintTempOffset, PaintHumOffset);
				}
			}
			else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				// Select cell
				if (WeatherSub->GetGrid().Contains(CellCoords))
				{
					bHasSelectedCell = true;
					SelectedCellCoords = CellCoords;
				}
				else
				{
					bHasSelectedCell = false;
				}
			}

			// Right-click: add cell at empty position
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			{
				if (!WeatherSub->GetGrid().Contains(CellCoords))
				{
					FArcWeatherCell& NewCell = WeatherSub->GetMutableGrid().FindOrAdd(CellCoords);
					NewCell.BaseClimate = WeatherSub->DefaultClimate;
					NewCell.HumidityThreshold = WeatherSub->DefaultHumidityThreshold;
					NewCell.FreezeThreshold = WeatherSub->DefaultFreezeThreshold;
					bHasSelectedCell = true;
					SelectedCellCoords = CellCoords;
				}
			}
		}
	}
}

// ====================================================================
// Cell Drawing
// ====================================================================

void FArcWeatherMinimapDebugger::DrawCells()
{
	UArcWeatherSubsystem* WeatherSub = GetWeatherSubsystem();
	if (!WeatherSub)
	{
		return;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const TMap<FIntVector, FArcWeatherCell>& Grid = WeatherSub->GetGrid();
	const float CS = WeatherSub->CellSize;
	const ImGuiIO& IO = ImGui::GetIO();

	CellCount = Grid.Num();
	bHasHoveredCell = false;
	float BestHoverDistSq = FLT_MAX;

	for (const auto& Pair : Grid)
	{
		const FIntVector& Coords = Pair.Key;
		const FArcWeatherCell& Cell = Pair.Value;

		const float WorldMinX = Coords.X * CS;
		const float WorldMinY = Coords.Y * CS;
		const float WorldMaxX = WorldMinX + CS;
		const float WorldMaxY = WorldMinY + CS;

		const ImVec2 ScreenMin = WorldToScreen(WorldMinX, WorldMaxY);
		const ImVec2 ScreenMax = WorldToScreen(WorldMaxX, WorldMinY);

		// Frustum cull
		if (ScreenMax.x < CanvasPos.x || ScreenMin.x > CanvasPos.x + CanvasSize.x ||
			ScreenMax.y < CanvasPos.y || ScreenMin.y > CanvasPos.y + CanvasSize.y)
		{
			continue;
		}

		// Compute effective climate
		const FEffectiveClimate Eff = ComputeEffective(Cell, WeatherSub);

		// Cell fill color based on vis mode
		ImU32 FillColor;
		switch (VisMode)
		{
		case EVisMode::Temperature:
			FillColor = ArcWeatherMinimapInternal::TemperatureToColor(Eff.Temperature, TempRangeMin, TempRangeMax);
			break;
		case EVisMode::Humidity:
			FillColor = ArcWeatherMinimapInternal::HumidityToColor(Eff.Humidity);
			break;
		case EVisMode::Combined:
		default:
			FillColor = ArcWeatherMinimapInternal::CombinedColor(Eff.Temperature, Eff.Humidity, TempRangeMin, TempRangeMax);
			break;
		}

		DrawList->AddRectFilled(ScreenMin, ScreenMax, FillColor);
		DrawList->AddRect(ScreenMin, ScreenMax, Arcx::GameplayDebugger::Weather::Minimap::CellBorderColor, 0.0f, 0, 1.0f);

		// Threshold overlays — read from cell's stored thresholds
		if (Eff.Temperature < Cell.FreezeThreshold)
		{
			DrawList->AddRectFilled(ScreenMin, ScreenMax, Arcx::GameplayDebugger::Weather::Minimap::FreezeOverlayColor);
		}
		if (Eff.Humidity >= Cell.HumidityThreshold && Eff.Temperature > Cell.FreezeThreshold)
		{
			DrawList->AddRectFilled(ScreenMin, ScreenMax, Arcx::GameplayDebugger::Weather::Minimap::RainOverlayColor);
		}

		// Selected cell highlight
		if (bHasSelectedCell && Coords == SelectedCellCoords)
		{
			DrawList->AddRect(ScreenMin, ScreenMax, Arcx::GameplayDebugger::Weather::Minimap::SelectedCellBorderColor, 0.0f, 0, 3.0f);
		}

		// Text labels when zoomed in enough
		const float CellScreenSize = ScreenMax.x - ScreenMin.x;
		if (bShowLabels && CellScreenSize > 40.0f)
		{
			char LabelBuf[32];
			FCStringAnsi::Snprintf(LabelBuf, sizeof(LabelBuf), "%.0fC\n%.0f%%", Eff.Temperature, Eff.Humidity);
			const ImVec2 TextSize = ImGui::CalcTextSize(LabelBuf);
			DrawList->AddText(
				ImVec2(
					(ScreenMin.x + ScreenMax.x - TextSize.x) * 0.5f,
					(ScreenMin.y + ScreenMax.y - TextSize.y) * 0.5f
				),
				Arcx::GameplayDebugger::Weather::Minimap::HUDTextColor, LabelBuf
			);
		}

		// Hover detection: check if mouse is inside cell rect
		if (IO.MousePos.x >= ScreenMin.x && IO.MousePos.x <= ScreenMax.x &&
			IO.MousePos.y >= ScreenMin.y && IO.MousePos.y <= ScreenMax.y)
		{
			const float CenterX = (ScreenMin.x + ScreenMax.x) * 0.5f;
			const float CenterY = (ScreenMin.y + ScreenMax.y) * 0.5f;
			const float DX = IO.MousePos.x - CenterX;
			const float DY = IO.MousePos.y - CenterY;
			const float DistSq = DX * DX + DY * DY;
			if (DistSq < BestHoverDistSq)
			{
				BestHoverDistSq = DistSq;
				bHasHoveredCell = true;
				HoveredCellCoords = Coords;
			}

			DrawList->AddRect(ScreenMin, ScreenMax, Arcx::GameplayDebugger::Weather::Minimap::HoveredCellBorderColor, 0.0f, 0, 2.0f);
		}
	}
}

// ====================================================================
// Climate Region Volume Outlines
// ====================================================================

void FArcWeatherMinimapDebugger::DrawRegionVolumes()
{
	UWorld* World = GetDebugWorld();
	if (!World)
	{
		return;
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	for (TActorIterator<AArcClimateRegionVolume> It(World); It; ++It)
	{
		AArcClimateRegionVolume* Volume = *It;
		if (!Volume || !Volume->BoxComponent)
		{
			continue;
		}

		const FVector Origin = Volume->GetActorLocation();
		const FVector Extent = Volume->BoxComponent->GetScaledBoxExtent();

		const float WorldMinX = Origin.X - Extent.X;
		const float WorldMinY = Origin.Y - Extent.Y;
		const float WorldMaxX = Origin.X + Extent.X;
		const float WorldMaxY = Origin.Y + Extent.Y;

		const ImVec2 ScreenMin = WorldToScreen(WorldMinX, WorldMaxY);
		const ImVec2 ScreenMax = WorldToScreen(WorldMaxX, WorldMinY);

		// Frustum cull
		if (ScreenMax.x < CanvasPos.x || ScreenMin.x > CanvasPos.x + CanvasSize.x ||
			ScreenMax.y < CanvasPos.y || ScreenMin.y > CanvasPos.y + CanvasSize.y)
		{
			continue;
		}

		DrawList->AddRect(ScreenMin, ScreenMax, Arcx::GameplayDebugger::Weather::Minimap::RegionOutlineColor, 0.0f, 0, 2.0f);

		// Label when zoomed in
		const float RectWidth = ScreenMax.x - ScreenMin.x;
		if (RectWidth > 60.0f)
		{
			char VolLabel[48];
			FCStringAnsi::Snprintf(VolLabel, sizeof(VolLabel), "%.0fC %.0f%%",
				Volume->BaseClimate.Temperature, Volume->BaseClimate.Humidity);
			DrawList->AddText(ImVec2(ScreenMin.x + 2.f, ScreenMin.y + 2.f),
				Arcx::GameplayDebugger::Weather::Minimap::RegionOutlineColor, VolLabel);
		}

		// Tooltip on hover
		const ImGuiIO& IO = ImGui::GetIO();
		if (IO.MousePos.x >= ScreenMin.x && IO.MousePos.x <= ScreenMax.x &&
			IO.MousePos.y >= ScreenMin.y && IO.MousePos.y <= ScreenMax.y)
		{
			ImGui::BeginTooltip();
			ImGui::Text("Climate Region: %s", TCHAR_TO_ANSI(*Volume->GetActorNameOrLabel()));
			ImGui::Text("Base: %.1f C, %.1f%%", Volume->BaseClimate.Temperature, Volume->BaseClimate.Humidity);
			ImGui::EndTooltip();
		}
	}
}

// ====================================================================
// Player Cell Highlight
// ====================================================================

void FArcWeatherMinimapDebugger::DrawPlayerCell()
{
	UWorld* World = GetDebugWorld();
	UArcWeatherSubsystem* WeatherSub = GetWeatherSubsystem();
	if (!World || !WeatherSub)
	{
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!PlayerPawn)
	{
		return;
	}

	const FVector PlayerLoc = PlayerPawn->GetActorLocation();
	const FIntVector PlayerCell = WeatherSub->WorldToCell(PlayerLoc);
	const float CS = WeatherSub->CellSize;

	const float WorldMinX = PlayerCell.X * CS;
	const float WorldMinY = PlayerCell.Y * CS;
	const float WorldMaxX = WorldMinX + CS;
	const float WorldMaxY = WorldMinY + CS;

	const ImVec2 ScreenMin = WorldToScreen(WorldMinX, WorldMaxY);
	const ImVec2 ScreenMax = WorldToScreen(WorldMaxX, WorldMinY);

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(ScreenMin, ScreenMax, Arcx::GameplayDebugger::Weather::Minimap::PlayerCellColor);
	DrawList->AddRect(ScreenMin, ScreenMax, Arcx::GameplayDebugger::Weather::Minimap::PlayerCellBorderColor, 0.0f, 0, 2.0f);

	// Player dot
	const ImVec2 PlayerScreen = WorldToScreen(PlayerLoc.X, PlayerLoc.Y);
	DrawList->AddCircleFilled(PlayerScreen, FMath::Clamp(Zoom * 80.f, 3.f, 8.f),
		Arcx::GameplayDebugger::Weather::Minimap::PlayerCellBorderColor);
}

// ====================================================================
// HUD
// ====================================================================

void FArcWeatherMinimapDebugger::DrawHUD()
{
	UArcWeatherSubsystem* WeatherSub = GetWeatherSubsystem();
	if (!WeatherSub)
	{
		return;
	}

	// Row 1: Vis mode + toggles + stats + zoom
	{
		ImGui::SetNextItemWidth(110.f);
		const char* ModeNames[] = {"Temperature", "Humidity", "Combined"};
		int ModeIdx = static_cast<int>(VisMode);
		if (ImGui::Combo("##VisMode", &ModeIdx, ModeNames, IM_ARRAYSIZE(ModeNames)))
		{
			VisMode = static_cast<EVisMode>(ModeIdx);
		}

		ImGui::SameLine();
		ImGui::Checkbox("Regions", &bShowRegions);
		ImGui::SameLine();
		ImGui::Checkbox("Labels", &bShowLabels);

		ImGui::SameLine();
		ImGui::Text("| Cells: %d  Size: %.0f", CellCount, WeatherSub->CellSize);

		// Season switcher
		ImGui::SameLine();
		ImGui::Text("Season: %d", WeatherSub->GetCurrentSeason());
		ImGui::SameLine();
		if (ImGui::SmallButton("<##season"))
		{
			const int32 Cur = WeatherSub->GetCurrentSeason();
			if (Cur > 0)
			{
				WeatherSub->SetCurrentSeason(Cur - 1);
			}
		}
		ImGui::SameLine();
		if (ImGui::SmallButton(">##season"))
		{
			WeatherSub->SetCurrentSeason(WeatherSub->GetCurrentSeason() + 1);
		}

		ImGui::SameLine();
		ImGui::Text("| Zoom: %.4f", Zoom);
		ImGui::SameLine();
		if (ImGui::SmallButton("Reset View"))
		{
			ViewOffset = FVector2D::ZeroVector;
			Zoom = 0.05f;
		}
	}

	// Row 2: Player weather + default climate + threshold overrides
	{
		UWorld* World = GetDebugWorld();
		if (World)
		{
			APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
			if (PlayerPawn)
			{
				const FArcClimateParams PlayerWeather = WeatherSub->GetWeatherAtLocation(PlayerPawn->GetActorLocation());
				ImGui::TextColored(ImVec4(1.f, 0.5f, 0.5f, 1.f), "Player: %.1f C, %.1f%%",
					PlayerWeather.Temperature, PlayerWeather.Humidity);
				ImGui::SameLine();
			}
		}

		ImGui::Text("| Default: %.1f C, %.1f%%",
			WeatherSub->DefaultClimate.Temperature, WeatherSub->DefaultClimate.Humidity);
	}

	// Row 3: Paint sliders
	{
		ImGui::Text("Paint:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100.f);
		ImGui::DragFloat("Temp##paint", &PaintTempOffset, 0.5f, -50.f, 50.f, "%+.1f C");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100.f);
		ImGui::DragFloat("Hum##paint", &PaintHumOffset, 0.5f, -100.f, 100.f, "%+.1f%%");
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.f), "(Shift+LMB drag to paint)");
	}
}

// ====================================================================
// Selected Cell Panel
// ====================================================================

void FArcWeatherMinimapDebugger::DrawSelectedCellPanel()
{
	if (!bHasSelectedCell)
	{
		return;
	}

	UArcWeatherSubsystem* WeatherSub = GetWeatherSubsystem();
	if (!WeatherSub)
	{
		return;
	}

	FArcWeatherCell* Cell = WeatherSub->GetMutableGrid().Find(SelectedCellCoords);
	if (!Cell)
	{
		bHasSelectedCell = false;
		return;
	}

	ImGui::PushID("SelectedCell");

	ImGui::Text("Selected: (%d, %d, %d)", SelectedCellCoords.X, SelectedCellCoords.Y, SelectedCellCoords.Z);
	ImGui::SameLine();

	// Base climate editing
	ImGui::Text("Base:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.f);
	float BaseTemp = Cell->BaseClimate.Temperature;
	if (ImGui::DragFloat("Temp##base", &BaseTemp, 0.5f, -50.f, 80.f, "%.1f C"))
	{
		FArcClimateParams NewBase = Cell->BaseClimate;
		NewBase.Temperature = BaseTemp;
		WeatherSub->SetBaseClimate(SelectedCellCoords, NewBase, Cell->HumidityThreshold, Cell->FreezeThreshold);
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.f);
	float BaseHum = Cell->BaseClimate.Humidity;
	if (ImGui::DragFloat("Hum##base", &BaseHum, 0.5f, 0.f, 100.f, "%.1f%%"))
	{
		FArcClimateParams NewBase = Cell->BaseClimate;
		NewBase.Humidity = BaseHum;
		WeatherSub->SetBaseClimate(SelectedCellCoords, NewBase, Cell->HumidityThreshold, Cell->FreezeThreshold);
	}

	ImGui::SameLine();
	ImGui::Text("Offset:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.f);
	float TempOff = Cell->TemperatureOffset;
	if (ImGui::DragFloat("Temp##off", &TempOff, 0.5f, -50.f, 50.f, "%+.1f C"))
	{
		WeatherSub->SetWeatherOffset(SelectedCellCoords, TempOff, Cell->HumidityOffset);
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.f);
	float HumOff = Cell->HumidityOffset;
	if (ImGui::DragFloat("Hum##off", &HumOff, 0.5f, -100.f, 100.f, "%+.1f%%"))
	{
		WeatherSub->SetWeatherOffset(SelectedCellCoords, Cell->TemperatureOffset, HumOff);
	}

	// Effective readout
	const FEffectiveClimate Eff = ComputeEffective(*Cell, WeatherSub);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f), "Eff: %.1f C, %.1f%%", Eff.Temperature, Eff.Humidity);
	ImGui::SameLine();
	ImGui::Text("Thr:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.f);
	ImGui::DragFloat("Rain##thr", &Cell->HumidityThreshold, 0.5f, 0.f, 100.f, "%.1f%%");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.f);
	ImGui::DragFloat("Freeze##thr", &Cell->FreezeThreshold, 0.5f, -50.f, 50.f, "%.1f C");

	// Action buttons
	ImGui::SameLine();
	if (ImGui::SmallButton("Reset Offsets"))
	{
		WeatherSub->ClearWeatherOffset(SelectedCellCoords);
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Delete Cell"))
	{
		WeatherSub->GetMutableGrid().Remove(SelectedCellCoords);
		bHasSelectedCell = false;
	}

	ImGui::PopID();
}
