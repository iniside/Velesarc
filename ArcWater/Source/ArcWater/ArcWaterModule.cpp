#include "ArcWaterModule.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"
#include "Misc/Paths.h"

#include "Ocean/ArcOceanFoamAccumulator.h"

DEFINE_LOG_CATEGORY(LogArcWater);

void FArcWaterModule::StartupModule()
{
	const FString VirtualShaderDir = TEXT("/Plugin/ArcWater");
	if (!AllShaderSourceDirectoryMappings().Contains(VirtualShaderDir))
	{
		const FString ShaderDir = FPaths::Combine(
			IPluginManager::Get().FindPlugin(TEXT("ArcWater"))->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(VirtualShaderDir, ShaderDir);
	}

	ArcOcean::RegisterFoamViewExtension();
}

void FArcWaterModule::ShutdownModule()
{
	ArcOcean::UnregisterFoamViewExtension();
}

IMPLEMENT_MODULE(FArcWaterModule, ArcWater)
