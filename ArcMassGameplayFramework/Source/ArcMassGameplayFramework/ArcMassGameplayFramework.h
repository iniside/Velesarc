// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"

class FArcMassGameFrameworkModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
};

DECLARE_LOG_CATEGORY_EXTERN(LogArcMassGameFramework, Log, All);
