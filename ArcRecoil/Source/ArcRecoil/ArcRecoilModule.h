// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "HAL/IConsoleManager.h"
#include "Logging/LogMacros.h"

ARCRECOIL_API DECLARE_LOG_CATEGORY_EXTERN(LogArcRecoil, Log, All);

// Entity index to log ArcRecoil per-frame state for; -1 disables.
extern ARCRECOIL_API TAutoConsoleVariable<int32> CVarArcRecoilLogEntity;

extern ARCRECOIL_API TAutoConsoleVariable<int32> CVarArcRecoilDebug;

class FArcRecoilModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
