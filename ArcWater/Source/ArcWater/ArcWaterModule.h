// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

ARCWATER_API DECLARE_LOG_CATEGORY_EXTERN(LogArcWater, Log, All);

class FArcWaterModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
