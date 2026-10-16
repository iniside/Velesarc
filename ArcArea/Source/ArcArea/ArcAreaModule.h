// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

/** Bring-up diagnostics for area registration and slot assignment. */
ARCAREA_API DECLARE_LOG_CATEGORY_EXTERN(LogArcAreaDiag, Log, All);

class FArcAreaModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
