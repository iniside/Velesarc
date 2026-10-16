// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/** Diagnostics for ArcForest simulation, commit and instance generation. */
ARCECOTOPE_API DECLARE_LOG_CATEGORY_EXTERN(LogArcEcotope, Log, All);

/** Per-year growth diagnostics for the ArcForest simulation, separable from timing/report output on LogArcEcotope. */
ARCECOTOPE_API DECLARE_LOG_CATEGORY_EXTERN(LogArcForestGrowth, Log, All);

class FArcEcotopeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
