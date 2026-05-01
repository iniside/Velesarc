#pragma once

#include "Modules/ModuleManager.h"

class FArcTargetQueryModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
