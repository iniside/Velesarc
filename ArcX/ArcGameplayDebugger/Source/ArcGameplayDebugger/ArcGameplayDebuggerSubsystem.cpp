// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGameplayDebuggerSubsystem.h"

#include "ArcItemInstanceDataDebugger.h"
#include "imgui.h"
#include "ImGuiConfig.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/World.h"
#include "Items/Fragments/ArcItemFragment_AbilityEffectsToApply.h"
#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
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
	
}

UWorld* UArcGameplayDebuggerSubsystem::GetTickableGameObjectWorld() const
{
	return GetWorld();
}

ETickableTickType UArcGameplayDebuggerSubsystem::GetTickableTickType() const
{
	// If this is a template or has not been initialized yet, set to never tick and it will be enabled when it is initialized
	if (IsTemplate() && GetWorld())
	{
		return ETickableTickType::Never;
	}

	// Otherwise default to conditional
	return ETickableTickType::Conditional;
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
		}
	}
}

void UArcGameplayDebuggerSubsystem::Tick(float DeltaTime)
{
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
					ToggleDebuggerMenuItem("Items Store", DebuggerItems);
					ToggleDebuggerMenuItem("Equipment", EquipmentDebugger);
					ToggleDebuggerMenuItem("Quick Bar", QuickBarDebugger);
					ToggleDebuggerMenuItem("Item Attachment", ItemAttachmentDebugger);
					ToggleDebuggerMenuItem("Loot Tables", LootTableDebugger);
					ImGui::Separator();
					ToggleDebuggerMenuItem("Character Persistence", CharacterPersistenceDebugger);
					ImGui::EndMenu();
				}

				// ---- Ability System ----
				if (ImGui::BeginMenu("Ability System"))
				{
					ToggleDebuggerMenuItem("Gameplay Abilities", GameplayAbilitiesDebugger);
					ToggleDebuggerMenuItem("Gameplay Effects", GameplayEffectsDebugger);
					ToggleDebuggerMenuItem("Attributes", AttributesDebugger);
					ToggleDebuggerMenuItem("Targeting", GlobalTargetingDebugger);
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
					ToggleDebuggerMenuItem("Craft", CraftDebugger);
					ToggleDebuggerMenuItem("Knowledge", KnowledgeDebugger);
					ToggleDebuggerMenuItem("Area", AreaDebugger);
					ImGui::Separator();
					ToggleDebuggerMenuItem("Gameplay Tag Tree", GameplayTagTreeWidget);
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
					ImGui::EndMenu();
				}

				// ---- Mass ----
				if (ImGui::BeginMenu("Mass"))
				{
					ToggleDebuggerMenuItem("Entity Debugger", MassEntityDebugger);
					ToggleDebuggerMenuItem("Entity Visualization", VisEntityDebugger);
					ImGui::EndMenu();
				}

				ImGui::EndMainMenuBar();

				// ---- Draw all visible debuggers ----
				DrawIfVisible(DebuggerItems);
				DrawIfVisible(EquipmentDebugger);
				DrawIfVisible(QuickBarDebugger);
				DrawIfVisible(ItemAttachmentDebugger);
				DrawIfVisible(LootTableDebugger);
				DrawIfVisible(CharacterPersistenceDebugger);
				DrawIfVisible(GameplayAbilitiesDebugger);
				DrawIfVisible(GameplayEffectsDebugger);
				DrawIfVisible(AttributesDebugger);
				DrawIfVisible(GlobalTargetingDebugger);
				DrawIfVisible(ArcGunDebugger);
				DrawIfVisible(BuilderDebugger);
				DrawIfVisible(CraftDebugger);
				DrawIfVisible(GameplayTagTreeWidget);
				DrawIfVisible(MassEntityDebugger);
				DrawIfVisible(AIDebugger);
				DrawIfVisible(PerceptionDebugger);
				DrawIfVisible(PlanFeasibilityDebugger);
				DrawIfVisible(PathDebugger);
				DrawIfVisible(VisEntityDebugger);
				DrawIfVisible(KnowledgeDebugger);
				DrawIfVisible(AreaDebugger);
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
