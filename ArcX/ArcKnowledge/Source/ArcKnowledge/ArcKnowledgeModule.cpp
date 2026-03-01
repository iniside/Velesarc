// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcKnowledgeModule.h"
#include "Debug/GameplayDebuggerCategory_ArcKnowledge.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebugger.h"
#endif

#define LOCTEXT_NAMESPACE "FArcKnowledgeModule"

void FArcKnowledgeModule::StartupModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();

	GameplayDebuggerModule.RegisterCategory("Arc Knowledge"
		, IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_ArcKnowledge::MakeInstance)
		, EGameplayDebuggerCategoryState::EnabledInGameAndSimulate);

	GameplayDebuggerModule.NotifyCategoriesChanged();
#endif
}

void FArcKnowledgeModule::ShutdownModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	if (IGameplayDebugger::IsAvailable())
	{
		IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
		GameplayDebuggerModule.UnregisterCategory("Arc Knowledge");
	}
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcKnowledgeModule, ArcKnowledge)
