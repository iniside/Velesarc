// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPCGArcIWInterop, Log, All);

class FPCGArcIWInteropModule final : public IModuleInterface
{
public:
	virtual bool SupportsDynamicReloading() override { return true; }
};
