#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FArcTimeOfDayEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedRef<class SDockTab> SpawnControlPanelTab(const class FSpawnTabArgs& SpawnTabArgs);

	static const FName ControlPanelTabId;
};
