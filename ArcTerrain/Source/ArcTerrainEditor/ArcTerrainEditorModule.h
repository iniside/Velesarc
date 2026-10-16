// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class FArcTerrainEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void OnPostEngineInit();

	/** Component-visualizer class names registered by OnPostEngineInit, unregistered on shutdown
	 *  (pattern: MeshPartitionEditorModule.cpp:88-116). */
	TArray<FName> VisualizersToUnregisterOnShutdown;
};
