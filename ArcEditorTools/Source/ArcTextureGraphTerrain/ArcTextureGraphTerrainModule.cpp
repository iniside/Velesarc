#include "ArcTextureGraphTerrainModule.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"
#include "Misc/Paths.h"

void FArcTextureGraphTerrainModule::StartupModule()
{
	const FString VirtualShaderDir = TEXT("/Plugin/ArcEditorTools");
	if (!AllShaderSourceDirectoryMappings().Contains(VirtualShaderDir))
	{
		const FString ShaderDir = FPaths::Combine(
			IPluginManager::Get().FindPlugin(TEXT("ArcEditorTools"))->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(VirtualShaderDir, ShaderDir);
	}
}

void FArcTextureGraphTerrainModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FArcTextureGraphTerrainModule, ArcTextureGraphTerrain)
