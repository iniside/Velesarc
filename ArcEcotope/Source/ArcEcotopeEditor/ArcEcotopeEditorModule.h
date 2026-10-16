// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class AArcEcotopeVolume;
class FArcForestSimulationController;
class ILevelEditor;
class ISCSEditorUICustomization;

class FArcEcotopeEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FArcEcotopeEditorModule& Get();
	static bool IsAvailable();

	FArcForestSimulationController& GetSimulationController() const;

private:
	void OnEditorSelectionChanged(UObject* NewSelection);
	void RegisterSubobjectEditorCustomization(TSharedPtr<ILevelEditor> InLevelEditor);

	TUniquePtr<FArcForestSimulationController> SimulationController;
	TSharedPtr<ISCSEditorUICustomization> SubobjectEditorCustomization;
	FDelegateHandle SelectionChangedHandle;
	TArray<TWeakObjectPtr<AArcEcotopeVolume>> LastReportedVolumes;
	bool bDetailsCustomizationRegistered = false;
};
