// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"

ARCCUES_API DECLARE_LOG_CATEGORY_EXTERN(LogArcCues, Log, All);

class FArcCuesModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
