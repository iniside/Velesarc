// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass.h"

#include "ArcMassReplication.h"
#include "Iris/ReplicationSystem/NetObjectFactoryRegistry.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebugger.h"
#include "GameplayDebuggerCategory_ArcMass.h"
#endif // WITH_GAMEPLAY_DEBUGGER

#define LOCTEXT_NAMESPACE "FArcMassModule"

void FArcMassModule::StartupModule()
{
	// Register our custom Iris factory for replication proxies.
	// Must be done before any ReplicationSystem is created.
	UE::Net::FNetObjectFactoryRegistry::RegisterFactory(
		UArcMassReplicationProxyFactory::StaticClass(),
		UArcMassReplicationProxyFactory::GetFactoryName());

#if WITH_GAMEPLAY_DEBUGGER
	IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
	GameplayDebuggerModule.RegisterCategory("ArcMass", IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_ArcMass::MakeInstance), EGameplayDebuggerCategoryState::EnabledInGameAndSimulate);
	GameplayDebuggerModule.NotifyCategoriesChanged();
#endif
}

void FArcMassModule::ShutdownModule()
{
	UE::Net::FNetObjectFactoryRegistry::UnregisterFactory(UArcMassReplicationProxyFactory::GetFactoryName());

#if WITH_GAMEPLAY_DEBUGGER
	if (IGameplayDebugger::IsAvailable())
	{
		IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
		GameplayDebuggerModule.UnregisterCategory("ArcMass");
		GameplayDebuggerModule.NotifyCategoriesChanged();
	}
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcMassModule, ArcMass)
