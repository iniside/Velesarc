// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRmlUIShadersModule.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

void FArcRmlUIShadersModule::StartupModule()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ArcRmlUI"));
	if (Plugin.IsValid())
	{
		AddShaderSourceDirectoryMapping(TEXT("/Plugin/ArcRmlUI"), FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders")));
	}
}

void FArcRmlUIShadersModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FArcRmlUIShadersModule, ArcRmlUIShaders)
