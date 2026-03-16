// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FArcNeedsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
