// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEcotopeModule.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY(LogArcEcotope);
DEFINE_LOG_CATEGORY(LogArcForestGrowth);

void FArcEcotopeModule::StartupModule()
{
	const FString VirtualShaderDir = TEXT("/Plugin/ArcEcotope");
	if (!AllShaderSourceDirectoryMappings().Contains(VirtualShaderDir))
	{
		const FString ShaderDir = FPaths::Combine(
			IPluginManager::Get().FindPlugin(TEXT("ArcEcotope"))->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(VirtualShaderDir, ShaderDir);
	}
}

void FArcEcotopeModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FArcEcotopeModule, ArcEcotope)
