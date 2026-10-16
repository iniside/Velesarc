// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGameplayDebuggerSubsystem.h"

#include "imgui.h"
#include "ImGuiConfig.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

TAutoConsoleVariable<bool> CVarArcDebugDraw(
	TEXT("Arc.ToggleDebug"),
	false,
	TEXT("Enable/Disable Arc Gameplay Debugger")
);

static TAutoConsoleVariable<bool> CVarArcToggleGameInput(
	TEXT("Arc.ToggleGameInput"),
	false,
	TEXT("Enable/Disable Arc Gameplay Debugger")
);

static TAutoConsoleVariable<bool> CVarArcDrawPawnOverlay(
	TEXT("Arc.DrawPawnOverlay"),
	false,
	TEXT("Draw a screen-space ImGui overlay marker at the local player pawn's location")
);

static TAutoConsoleVariable<bool> CVarArcDrawPinnedOverlay(
	TEXT("Arc.DrawPinnedOverlay"),
	false,
	TEXT("Draw a screen-space ImGui overlay marker pinned to a fixed viewport location (reference)")
);

void UArcGameplayDebuggerSubsystem::Toggle()
{
	if (bDrawDebug == false)
	{
		bDrawDebug = true;
	}
	else if (bDrawDebug == true)
	{
		bDrawDebug = false;
	}
}

UArcGameplayDebuggerSubsystem::UArcGameplayDebuggerSubsystem()
{
	bDrawDebug = false;
}

void UArcGameplayDebuggerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	CVarArcDebugDraw->OnChangedDelegate().AddUObject(this, &UArcGameplayDebuggerSubsystem::OnCVarChanged);
	CVarArcToggleGameInput->OnChangedDelegate().AddUObject(this, &UArcGameplayDebuggerSubsystem::OnToggleGameInputChanged);
}

void UArcGameplayDebuggerSubsystem::OnCVarChanged(IConsoleVariable* CVar)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);

	if (CVarArcDebugDraw.GetValueOnGameThread())
	{
		if (PC)
		{
			PC->bShowMouseCursor = true;
			UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(PC);;
			//PC->bEnableMouseOverEvents = false;
		}	
	}
	else
	{
		if (PC)
		{
			PC->bShowMouseCursor = false;
			UWidgetBlueprintLibrary::SetInputMode_GameOnly(PC);
			//PC->bEnableMouseOverEvents = true;
		}
	}
}

void UArcGameplayDebuggerSubsystem::OnToggleGameInputChanged(IConsoleVariable* CVar)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);

	if (CVarArcToggleGameInput.GetValueOnGameThread())
	{
		if (PC)
		{
			PC->bShowMouseCursor = true;
			UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(PC);;
			//PC->bEnableMouseOverEvents = false;
		}
	}
	else
	{
		if (PC)
		{
			PC->bShowMouseCursor = false;
			UWidgetBlueprintLibrary::SetInputMode_GameOnly(PC);
			//UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(PC);
			//PC->bEnableMouseOverEvents = true;
		}
	}
}

void UArcGameplayDebuggerSubsystem::Deinitialize()
{
	if (TacticsDebugger.bShow)
	{
		TacticsDebugger.bShow = false;
		TacticsDebugger.Uninitialize();
	}
}

UWorld* UArcGameplayDebuggerSubsystem::GetTickableGameObjectWorld() const
{
	return GetWorld();
}

ETickableTickType UArcGameplayDebuggerSubsystem::GetTickableTickType() const
{
	return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UArcGameplayDebuggerSubsystem::IsTickable() const
{
	return true;
}

namespace
{
	/** Helper to toggle a debugger panel from a menu item. */
	template<typename T>
	void ToggleDebuggerMenuItem(const char* Label, T& Debugger)
	{
		if (ImGui::MenuItem(Label))
		{
			if (!Debugger.bShow)
			{
				Debugger.bShow = true;
				Debugger.Initialize();
			}
			else
			{
				Debugger.bShow = false;
				Debugger.Uninitialize();
			}
		}
	}

	template<typename T>
	void DrawIfVisible(T& Debugger)
	{
		if (Debugger.bShow)
		{
			Debugger.Draw();
			if (!Debugger.bShow)
			{
				Debugger.Uninitialize();
			}
		}
	}

	/** Draws a small filled-circle marker with a labelled, padded background at a screen position. */
	void DrawMarkerWithLabel(ImDrawList* DrawList, const ImVec2& Center, ImU32 MarkerColor, const char* LabelText)
	{
		DrawList->AddCircleFilled(Center, 6.0f, MarkerColor);
		DrawList->AddCircle(Center, 6.0f, IM_COL32(0, 0, 0, 255), 0, 1.5f);

		const ImVec2 TextPos(Center.x + 10.0f, Center.y - 6.0f);
		const ImVec2 TextSize = ImGui::CalcTextSize(LabelText);
		const ImVec2 Padding(4.0f, 2.0f);
		DrawList->AddRectFilled(
			ImVec2(TextPos.x - Padding.x, TextPos.y - Padding.y),
			ImVec2(TextPos.x + TextSize.x + Padding.x, TextPos.y + TextSize.y + Padding.y),
			IM_COL32(0, 0, 0, 160),
			3.0f);
		DrawList->AddText(TextPos, IM_COL32_WHITE, LabelText);
	}

	/**
	 * Projects the local player pawn's world location to screen and draws a marker + label on the foreground draw list.
	 * The screen position is exponentially smoothed (SmoothedPos / bHasPos persist across frames) to remove per-frame
	 * projection jitter, snapping rather than gliding when the marker jumps a large distance (teleport / camera cut).
	 */
	void DrawPawnOverlay(UWorld* World, float DeltaTime, FVector2D& SmoothedPos, bool& bHasPos)
	{
		APlayerController* const PC = UGameplayStatics::GetPlayerController(World, 0);
		if (PC == nullptr)
		{
			return;
		}

		APawn* const Pawn = PC->GetPawn();
		if (Pawn == nullptr)
		{
			bHasPos = false;
			return;
		}

		const FVector WorldLocation = Pawn->GetActorLocation();
		FVector2D ScreenLocation = FVector2D::ZeroVector;
		if (!PC->ProjectWorldLocationToScreen(WorldLocation, ScreenLocation))
		{
			// Behind the camera or otherwise off-screen; drop smoothing so we snap when it reappears.
			bHasPos = false;
			return;
		}

		// Exponential smoothing: frame-rate independent, light enough to still track the pawn closely.
		constexpr double SmoothingSpeed = 18.0;
		constexpr double SnapDistanceSq = 300.0 * 300.0;
		if (!bHasPos || FVector2D::DistSquared(SmoothedPos, ScreenLocation) > SnapDistanceSq)
		{
			SmoothedPos = ScreenLocation;
		}
		else
		{
			const double Alpha = 1.0 - FMath::Exp(-SmoothingSpeed * static_cast<double>(DeltaTime));
			SmoothedPos = FMath::Lerp(SmoothedPos, ScreenLocation, Alpha);
		}
		bHasPos = true;

		ImDrawList* const DrawList = ImGui::GetForegroundDrawList();
		if (DrawList == nullptr)
		{
			return;
		}

		const ImVec2 Center(static_cast<float>(SmoothedPos.X), static_cast<float>(SmoothedPos.Y));
		const FString Label = FString::Printf(TEXT("Pawn  X=%.0f Y=%.0f Z=%.0f"), WorldLocation.X, WorldLocation.Y, WorldLocation.Z);
		DrawMarkerWithLabel(DrawList, Center, IM_COL32(255, 80, 80, 255), TCHAR_TO_UTF8(*Label));
	}

	/** Draws a fixed reference marker pinned to a constant offset from the viewport's top-left work area. */
	void DrawPinnedOverlay()
	{
		ImDrawList* const DrawList = ImGui::GetForegroundDrawList();
		if (DrawList == nullptr)
		{
			return;
		}

		const ImGuiViewport* const Viewport = ImGui::GetMainViewport();
		const ImVec2 Origin = Viewport ? Viewport->WorkPos : ImVec2(0.0f, 0.0f);
		const ImVec2 Center(Origin.x + 120.0f, Origin.y + 120.0f);
		DrawMarkerWithLabel(DrawList, Center, IM_COL32(80, 180, 255, 255), "Pinned reference (screen-space)");
	}
}

void UArcGameplayDebuggerSubsystem::Tick(float DeltaTime)
{
	TacticsDebugger.Tick();

	if (CVarArcDrawPawnOverlay.GetValueOnGameThread() || CVarArcDrawPinnedOverlay.GetValueOnGameThread())
	{
		const ImGui::FScopedContext ScopedContext;
		if (ScopedContext)
		{
			if (CVarArcDrawPawnOverlay.GetValueOnGameThread())
			{
				DrawPawnOverlay(GetWorld(), DeltaTime, PawnOverlayScreenPos, bPawnOverlayHasPos);
			}
			if (CVarArcDrawPinnedOverlay.GetValueOnGameThread())
			{
				DrawPinnedOverlay();
			}
		}
	}

	if (CVarArcDebugDraw.GetValueOnGameThread())
	{
		const ImGui::FScopedContext ScopedContext;
		if (ScopedContext)
		{
			if (ImGui::BeginMainMenuBar())
			{
				// ---- File ----
				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("Show Demo"))
					{
						bDrawDebug = true;
					}
					ImGui::EndMenu();
				}

				// ---- Items ----
				if (ImGui::BeginMenu("Items"))
				{
					ToggleDebuggerMenuItem("Loot Tables", LootTableDebugger);
					ToggleDebuggerMenuItem("Character Persistence", CharacterPersistenceDebugger);
					ImGui::Separator();
					ToggleDebuggerMenuItem("World Persistence", WorldPersistenceDebugger);
					ImGui::EndMenu();
				}

				// ---- Combat ----
				if (ImGui::BeginMenu("Combat"))
				{
					ToggleDebuggerMenuItem("Arc Gun", ArcGunDebugger);
					ImGui::EndMenu();
				}

				// ---- Gameplay (Builder, Craft, Knowledge, Area, Tags) ----
				if (ImGui::BeginMenu("Gameplay"))
				{
					ToggleDebuggerMenuItem("Builder", BuilderDebugger);
					ToggleDebuggerMenuItem("Knowledge", KnowledgeDebugger);
					ToggleDebuggerMenuItem("Area", AreaDebugger);
					ToggleDebuggerMenuItem("Needs", NeedsDebugger);
					ToggleDebuggerMenuItem("Interactions", InteractionDebugger);
					ToggleDebuggerMenuItem("Targeting", TargetingDebugger);
					ToggleDebuggerMenuItem("Economy", EconomyDebugger);
					ToggleDebuggerMenuItem("Level Browser", LevelBrowserDebugger);
					ImGui::Separator();
					ToggleDebuggerMenuItem("Gameplay Tag Tree", GameplayTagTreeWidget);
					ImGui::EndMenu();
				}

				// ---- Weather ----
				if (ImGui::BeginMenu("Weather"))
				{
					ToggleDebuggerMenuItem("Weather Minimap", WeatherMinimapDebugger);
					ImGui::EndMenu();
				}

				// ---- AI (includes Navigation) ----
				if (ImGui::BeginMenu("AI"))
				{
					ToggleDebuggerMenuItem("AI Debugger", AIDebugger);
					ToggleDebuggerMenuItem("Perception", PerceptionDebugger);
					ToggleDebuggerMenuItem("Plan Feasibility", PlanFeasibilityDebugger);
					ImGui::Separator();
					ToggleDebuggerMenuItem("Path Debugger", PathDebugger);
					ToggleDebuggerMenuItem("Target Query", TQSDebugger);
					ToggleDebuggerMenuItem("Tactics", TacticsDebugger);
					ImGui::EndMenu();
				}

				// ---- Mass ----
				if (ImGui::BeginMenu("Mass"))
				{
					ToggleDebuggerMenuItem("Entity Debugger", MassEntityDebugger);
					ToggleDebuggerMenuItem("Entity Visualization", VisEntityDebugger);
					ToggleDebuggerMenuItem("Physics Debugger", PhysicsDebugger);
					ImGui::Separator();
					ToggleDebuggerMenuItem("Spatial Hash Minimap", SpatialHashMinimapDebugger);
					ToggleDebuggerMenuItem("Visualization Minimap", VisualizationMinimapDebugger);
					ToggleDebuggerMenuItem("Mobile Visualization", MobileVisEntityDebugger);
					ImGui::Separator();
					ToggleDebuggerMenuItem("Mass Abilities", MassAbilitiesDebugger);
					ToggleDebuggerMenuItem("Mass Items", MassItemsDebugger);
					ToggleDebuggerMenuItem("Mass Processors", MassProcessorDebugger);
					ImGui::EndMenu();
				}

				// ---- Profiling ----
				if (ImGui::BeginMenu("Profiling"))
				{
					ToggleDebuggerMenuItem("Trace / Insights", TraceDebugger);
					ImGui::EndMenu();
				}

				ImGui::EndMainMenuBar();

				// ---- Draw all visible debuggers ----
				DrawIfVisible(LootTableDebugger);
				DrawIfVisible(CharacterPersistenceDebugger);
				DrawIfVisible(ArcGunDebugger);
				DrawIfVisible(BuilderDebugger);
				DrawIfVisible(GameplayTagTreeWidget);
				DrawIfVisible(MassEntityDebugger);
				DrawIfVisible(AIDebugger);
				DrawIfVisible(PerceptionDebugger);
				DrawIfVisible(PlanFeasibilityDebugger);
				DrawIfVisible(PathDebugger);
				DrawIfVisible(TQSDebugger);
				DrawIfVisible(VisEntityDebugger);
				DrawIfVisible(PhysicsDebugger);
				DrawIfVisible(KnowledgeDebugger);
				DrawIfVisible(AreaDebugger);
				DrawIfVisible(SpatialHashMinimapDebugger);
				DrawIfVisible(VisualizationMinimapDebugger);
				DrawIfVisible(MobileVisEntityDebugger);
				DrawIfVisible(NeedsDebugger);
				DrawIfVisible(WeatherMinimapDebugger);
				DrawIfVisible(InteractionDebugger);
				DrawIfVisible(WorldPersistenceDebugger);
				DrawIfVisible(EconomyDebugger);
				DrawIfVisible(MassAbilitiesDebugger);
				DrawIfVisible(MassItemsDebugger);
				DrawIfVisible(TargetingDebugger);
				DrawIfVisible(TacticsDebugger);
				DrawIfVisible(LevelBrowserDebugger);
				DrawIfVisible(TraceDebugger);
				DrawIfVisible(MassProcessorDebugger);
				if (bDrawDebug)
				{
					ImGui::ShowDemoWindow();
				}
			}
		}
	}
}

TStatId UArcGameplayDebuggerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArcGameplayDebuggerSubsystem, STATGROUP_Tickables);
}

void UArcGameplayDebuggerSubsystem::ToggleDebug()
{
	Toggle();
}
