// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcPCGExtensionModule.h"

#if WITH_EDITOR
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"
#endif

DEFINE_LOG_CATEGORY(LogArcPCGForest);

void FArcPCGExtensionModule::StartupModule()
{
#if WITH_EDITOR
	// Map the plugin's shader directory so native kernel sources (ArcTerrainDataInterface.ush,
	// ArcTerrainScatter.usf) can be loaded at graph compile time (kernel source loading is
	// editor-only; mirrors FPCGMegaMeshInteropModule::StartupModule).
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ArcPCGExtension"));
	if (Plugin.IsValid())
	{
		const FString PluginShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(TEXT("/Plugin/ArcPCGExtension"), PluginShaderDir);
	}
#endif
}

void FArcPCGExtensionModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FArcPCGExtensionModule, ArcPCGExtension)
