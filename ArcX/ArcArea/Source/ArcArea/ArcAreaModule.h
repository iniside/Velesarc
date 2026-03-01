// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FArcAreaModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
