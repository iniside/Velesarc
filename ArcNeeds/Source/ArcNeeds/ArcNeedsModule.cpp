// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcNeedsModule.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebugger.h"
#include "Debugger/GameplayDebuggerCategory_ArcNeeds.h"
#endif

#define LOCTEXT_NAMESPACE "FArcNeedsModule"

void FArcNeedsModule::StartupModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
	GameplayDebuggerModule.RegisterCategory("Arc Needs",
		IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_ArcNeeds::MakeInstance),
		EGameplayDebuggerCategoryState::EnabledInGameAndSimulate);
	GameplayDebuggerModule.NotifyCategoriesChanged();
#endif
}

void FArcNeedsModule::ShutdownModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	if (IGameplayDebugger::IsAvailable())
	{
		IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
		GameplayDebuggerModule.UnregisterCategory("Arc Needs");
		GameplayDebuggerModule.NotifyCategoriesChanged();
	}
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcNeedsModule, ArcNeeds)
