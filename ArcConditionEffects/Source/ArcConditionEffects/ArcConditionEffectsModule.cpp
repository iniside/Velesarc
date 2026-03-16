// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcConditionEffectsModule.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebugger.h"
#include "GameplayDebuggerCategory_ArcConditions.h"
#endif

#define LOCTEXT_NAMESPACE "FArcConditionEffectsModule"

void FArcConditionEffectsModule::StartupModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
	GameplayDebuggerModule.RegisterCategory("Arc Conditions",
		IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_ArcConditions::MakeInstance),
		EGameplayDebuggerCategoryState::EnabledInGameAndSimulate);
	GameplayDebuggerModule.NotifyCategoriesChanged();
#endif
}

void FArcConditionEffectsModule::ShutdownModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	if (IGameplayDebugger::IsAvailable())
	{
		IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
		GameplayDebuggerModule.UnregisterCategory("Arc Conditions");
		GameplayDebuggerModule.NotifyCategoriesChanged();
	}
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcConditionEffectsModule, ArcConditionEffects)
