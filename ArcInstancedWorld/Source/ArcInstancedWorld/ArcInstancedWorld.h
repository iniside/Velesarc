// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"

ARCINSTANCEDWORLD_API DECLARE_LOG_CATEGORY_EXTERN(LogArcIW, Log, All);

class FArcInstancedWorldModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
