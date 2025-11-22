// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGameplayDebuggerSubsystem.h"

#include "ArcItemInstanceDataDebugger.h"
#include "imgui.h"
#include "ImGuiConfig.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Items/Fragments/ArcItemFragment_AbilityEffectsToApply.h"
#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
#include "Kismet/GameplayStatics.h"

static TAutoConsoleVariable<bool> CVarArcDebugDraw(
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
	return CVarArcDebugDraw.GetValueOnGameThread();
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
						if (AttributesDebugger.bShow == false)
						{
							AttributesDebugger.bShow = true;
							AttributesDebugger.Initialize();
						}
						else
						{
							AttributesDebugger.bShow = false;
							AttributesDebugger.Uninitialize();
						}
					}

					if (ImGui::MenuItem("Targeting"))
					{
						if (GlobalTargetingDebugger.bShow == false)
						{
							GlobalTargetingDebugger.bShow = true;
							GlobalTargetingDebugger.Initialize();
						}
						else
						{
							GlobalTargetingDebugger.bShow = false;
							GlobalTargetingDebugger.Uninitialize();
						}
					}
					
					ImGui::EndMenu();
				}

				// Sim proxies ability systems. They have less info available, so I will separate them into different windows.
				if (ImGui::BeginMenu("Proxy Ability System"))
				{
					if (ImGui::MenuItem("Gameplay Effects"))
					{
						if (ProxyGameplayEffectDebugger.bShow == false)
						{
							ProxyGameplayEffectDebugger.bShow = true;
							ProxyGameplayEffectDebugger.Initialize();
						}
						else
						{
							ProxyGameplayEffectDebugger.bShow = false;
							ProxyGameplayEffectDebugger.Uninitialize();
						}
					}

					if (ImGui::MenuItem("Attributes"))
					{
						if (ProxyAttributeSetDebugger.bShow == false)
						{
							ProxyAttributeSetDebugger.bShow = true;
							ProxyAttributeSetDebugger.Initialize();
						}
						else
						{
							ProxyAttributeSetDebugger.bShow = false;
							ProxyAttributeSetDebugger.Uninitialize();
						}
					}
	
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Arc Gun"))
				{
					if (ImGui::MenuItem("Debugger"))
					{
						if (ArcGunDebugger.bShow == false)
						{
							ArcGunDebugger.bShow = true;
							ArcGunDebugger.Initialize();
						}
						else
						{
							ArcGunDebugger.bShow = false;
							ArcGunDebugger.Uninitialize();
						}
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
				if (AttributesDebugger.bShow)
				{
					AttributesDebugger.Draw();
				}
				if (GlobalTargetingDebugger.bShow)
				{
					GlobalTargetingDebugger.Draw();
				}
				if (ProxyGameplayEffectDebugger.bShow)
				{
					ProxyGameplayEffectDebugger.Draw();
				}
				if (ProxyAttributeSetDebugger.bShow)
				{
					ProxyAttributeSetDebugger.Draw();
				}
				if (ArcGunDebugger.bShow)
				{
					ArcGunDebugger.Draw();
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
}

TStatId UArcGameplayDebuggerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArcGameplayDebuggerSubsystem, STATGROUP_Tickables);
}

void UArcGameplayDebuggerSubsystem::ToggleDebug()
{
	Toggle();
}
