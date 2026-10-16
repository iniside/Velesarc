// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FArcTownSimulationController;

class FArcTownEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static TWeakPtr<FArcTownSimulationController> FindSimulationController();

	TWeakPtr<FArcTownSimulationController> GetSimulationController() const;

private:
	TSharedPtr<FArcTownSimulationController> SimulationController;
	bool bDetailsCustomizationRegistered = false;
};
