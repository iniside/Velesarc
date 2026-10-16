#pragma once

#include "Modules/ModuleInterface.h"

class FArcTextureGraphTerrainModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
