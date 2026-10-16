// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

ARCRIVER_API DECLARE_LOG_CATEGORY_EXTERN(LogArcRiver, Log, All);

class FArcRiverModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
