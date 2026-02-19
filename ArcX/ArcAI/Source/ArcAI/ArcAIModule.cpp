/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "ArcAIModule.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/CoreDelegates.h"
#include "SmartObjectPlanner/ArcSmartObjectPlannerSubsystem.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebugger.h"
#endif

#define LOCTEXT_NAMESPACE "FArcAIModule"

void FArcAIModule::StartupModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
	//GameplayDebuggerModule.RegisterCategory("Arc AI", IGameplayDebugger::FOnGetCategory::CreateStatic(&FArcGameplayDebuggerCategory_ArcAI::MakeInstance), EGameplayDebuggerCategoryState::EnabledInGameAndSimulate);
	//GameplayDebuggerModule.NotifyCategoriesChanged();
	
	GameplayDebuggerModule.RegisterCategory("Arc Planner"
		, IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_SmartObjectPlanner::MakeInstance)
		, EGameplayDebuggerCategoryState::EnabledInGameAndSimulate);
	GameplayDebuggerModule.NotifyCategoriesChanged();
#endif
	
	//AIDebuggerWindow 
	
#if WITH_EDITOR
	FWorldDelegates::OnPIEMapReady.AddLambda([this](UGameInstance* GameInstace)
		{
			AIDebuggerWindow.DebuggerWidget.World = GameInstace->GetWorld();
		});
	
	FWorldDelegates::OnPIEEnded.AddLambda([this](UGameInstance* GameInstace)
		{
			AIDebuggerWindow.DebuggerWidget.OnPieEnd();
		});
#endif
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FArcAIModule::ShutdownModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	if (IGameplayDebugger::IsAvailable())
	{
		IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
		GameplayDebuggerModule.UnregisterCategory("Arc Planner");
		GameplayDebuggerModule.NotifyCategoriesChanged();
	}
#endif
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FArcAIModule, ArcAI)