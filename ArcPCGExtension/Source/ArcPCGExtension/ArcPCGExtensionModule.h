// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/** Diagnostics for the ArcPCGExtension nodes (forest sim, terrain cache). */
ARCPCGEXTENSION_API DECLARE_LOG_CATEGORY_EXTERN(LogArcPCGForest, Log, All);

class FArcPCGExtensionModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
