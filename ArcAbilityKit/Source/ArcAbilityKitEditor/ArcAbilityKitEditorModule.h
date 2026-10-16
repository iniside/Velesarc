// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FObjectPreSaveContext;
class UObject;

class FArcAbilityKitEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void HandleObjectPreSave(UObject* Object, FObjectPreSaveContext Context);

	FDelegateHandle ObjectPreSaveHandle;
	bool bHandlingObjectPreSave = false;
};
