// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCPModule.h"
#include "ArcMCPToolset.h"
#include "StateTree/ArcStateTreeNodeRegistry.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

#define LOCTEXT_NAMESPACE "FArcMCPModule"

void FArcMCPModule::StartupModule()
{
	FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddLambda([]()
	{
		FArcStateTreeNodeRegistry::Get().Initialize();
		UToolsetRegistry::RegisterToolsetClass(UArcMCPToolset::StaticClass());
	});
}

void FArcMCPModule::ShutdownModule()
{
	if (UObjectInitialized())
	{
		UToolsetRegistry::UnregisterToolsetClass(UArcMCPToolset::StaticClass());
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcMCPModule, ArcMCP)
