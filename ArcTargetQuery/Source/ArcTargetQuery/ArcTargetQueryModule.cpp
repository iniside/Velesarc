#include "ArcTargetQueryModule.h"
#include "Debugger/GameplayDebuggerCategory_ArcTQS.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebugger.h"
#endif

#define LOCTEXT_NAMESPACE "FArcTargetQueryModule"

void FArcTargetQueryModule::StartupModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();

	GameplayDebuggerModule.RegisterCategory("Arc TQS"
		, IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_ArcTQS::MakeInstance)
		, EGameplayDebuggerCategoryState::EnabledInGameAndSimulate);

	GameplayDebuggerModule.NotifyCategoriesChanged();
#endif
}

void FArcTargetQueryModule::ShutdownModule()
{
#if WITH_GAMEPLAY_DEBUGGER
	if (IGameplayDebugger::IsAvailable())
	{
		IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
		GameplayDebuggerModule.UnregisterCategory("Arc TQS");
		GameplayDebuggerModule.NotifyCategoriesChanged();
	}
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcTargetQueryModule, ArcTargetQuery)
