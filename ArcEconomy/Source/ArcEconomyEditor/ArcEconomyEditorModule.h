#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Framework/Docking/TabManager.h"

class FArcEconomyEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedRef<SDockTab> SpawnStrategyTrainingTab(const FSpawnTabArgs& Args);

	FDelegateHandle PostEngineInitHandle;
	TArray<TSharedPtr<class IArcEditorEntityDrawContributor>> RegisteredContributors;

	static const FName StrategyTrainingTabId;
};
