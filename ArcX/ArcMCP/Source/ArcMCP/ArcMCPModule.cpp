// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCPModule.h"
#include "StateTree/ArcStateTreeNodeRegistry.h"

#define LOCTEXT_NAMESPACE "FArcMCPModule"

void FArcMCPModule::StartupModule()
{
	// Defer registry init until all modules are loaded so all node structs are available
	FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddLambda([]()
	{
		FArcStateTreeNodeRegistry::Get().Initialize();
	});
}

void FArcMCPModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcMCPModule, ArcMCP)
