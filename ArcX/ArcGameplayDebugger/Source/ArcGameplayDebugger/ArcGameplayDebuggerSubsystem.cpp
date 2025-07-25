// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGameplayDebuggerSubsystem.h"

#include "ArcItemInstanceDataDebugger.h"
#include "imgui.h"
#include "ImGuiConfig.h"
#include "Items/Fragments/ArcItemFragment_AbilityEffectsToApply.h"
#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
#include "Kismet/GameplayStatics.h"

static TAutoConsoleVariable<bool> CVarArcDebugDraw(
	TEXT("Arc.ToggleDebug"),
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
	return CVarArcDebugDraw.GetValueOnGameThread();
}

void UArcGameplayDebuggerSubsystem::Tick(float DeltaTime)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (CVarArcDebugDraw.GetValueOnGameThread())
	{
		if (PC)
		{
			PC->bShowMouseCursor = true;
			//PC->bEnableMouseOverEvents = false;
		}
		
		const ImGui::FScopedContext ScopedContext;
		if (ScopedContext)
		{
			if (ImGui::BeginMainMenuBar())
			{
				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("Show Demo", "CTRL+Z"))
					{
						bDrawDebug = true;
						
					}
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Items"))
				{
					if (ImGui::MenuItem("Items Store"))
					{
						if (DebuggerItems.bShow == false)
						{
							DebuggerItems.bShow = true;
							DebuggerItems.Initialize();
						}
						else
						{
							DebuggerItems.bShow = false;
							DebuggerItems.Uninitialize();
						}
					}

					if (ImGui::MenuItem("Equipment"))
					{
						if (EquipmentDebugger.bShow == false)
						{
							EquipmentDebugger.bShow = true;
							EquipmentDebugger.Initialize();
						}
						else
						{
							EquipmentDebugger.bShow = false;
							EquipmentDebugger.Uninitialize();
						}
					}

					if (ImGui::MenuItem("Quick Bar"))
					{
						if (QuickBarDebugger.bShow == false)
						{
							QuickBarDebugger.bShow = true;
							QuickBarDebugger.Initialize();
						}
						else
						{
							QuickBarDebugger.bShow = false;
							QuickBarDebugger.Uninitialize();
						}
					}

					if (ImGui::MenuItem("Item Attachment"))
					{
						if (ItemAttachmentDebugger.bShow == false)
						{
							ItemAttachmentDebugger.bShow = true;
							ItemAttachmentDebugger.Initialize();
						}
						else
						{
							ItemAttachmentDebugger.bShow = false;
							ItemAttachmentDebugger.Uninitialize();
						}
					}

					if (ImGui::MenuItem("Make Item"))
					{
					}
					
					ImGui::EndMenu();
				}
				
				if (ImGui::BeginMenu("Local Ability System"))
				{
					if (ImGui::MenuItem("Gameplay Abilities"))
					{
						if (GameplayAbilitiesDebugger.bShow == false)
						{
							GameplayAbilitiesDebugger.bShow = true;
							GameplayAbilitiesDebugger.Initialize();
						}
						else
						{
							GameplayAbilitiesDebugger.bShow = false;
							GameplayAbilitiesDebugger.Uninitialize();
						}
					}

					if (ImGui::MenuItem("Gameplay Effects"))
					{
						if (GameplayEffectsDebugger.bShow == false)
						{
							GameplayEffectsDebugger.bShow = true;
							GameplayEffectsDebugger.Initialize();
						}
						else
						{
							GameplayEffectsDebugger.bShow = false;
							GameplayEffectsDebugger.Uninitialize();
						}
					}

					if (ImGui::MenuItem("Attributes"))
					{
					}

					if (ImGui::MenuItem("Targeting"))
					{
						// Current Global targeting results.
					}
					
					ImGui::EndMenu();
				}

				// Sim proxies ability systems. They have less info available, so I will separate them into different windows.
				if (ImGui::BeginMenu("Proxy Ability System"))
				{
					if (ImGui::MenuItem("Gameplay Effects"))
					{
					}

					if (ImGui::MenuItem("Attributes"))
					{
					}
	
					ImGui::EndMenu();
				}
				
				ImGui::EndMainMenuBar();

				if (DebuggerItems.bShow)
				{
					DebuggerItems.Draw();
				}
				if (EquipmentDebugger.bShow)
				{
					EquipmentDebugger.Draw();
				}
				if (QuickBarDebugger.bShow)
				{
					QuickBarDebugger.Draw();
				}
				if (ItemAttachmentDebugger.bShow)
				{
					ItemAttachmentDebugger.Draw();
				}
				if (GameplayAbilitiesDebugger.bShow)
				{
					GameplayAbilitiesDebugger.Draw();
				}
				if (GameplayEffectsDebugger.bShow)
				{
					GameplayEffectsDebugger.Draw();
				}
				if (bDrawDebug)
				{
					ImGui::ShowDemoWindow();
				}
			}
			// Your ImGui code goes here!
			//ImGui::ShowDemoWindow();
		}
	}
	else
	{
		if (PC)
		{
			PC->bShowMouseCursor = false;
			//PC->bEnableMouseOverEvents = true;
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
