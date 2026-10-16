// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTownEditorModule.h"

#include "Build/ArcTownTeardown.h"
#include "Details/ArcTownVolumeDetails.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Simulation/ArcTownSimulationController.h"
#include "Town/ArcTownVolume.h"

void FArcTownEditorModule::StartupModule()
{
	if (!IsRunningGame())
	{
		AArcTownVolume::OnTeardownRequested.BindStatic(&ArcTownTeardown::ClearTown);

		SimulationController = MakeShared<FArcTownSimulationController>();
		SimulationController->Register();

		FPropertyEditorModule& PropertyEditor =
			FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyEditor.RegisterCustomClassLayout(
			AArcTownVolume::StaticClass()->GetFName(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FArcTownVolumeDetails::MakeInstance));
		PropertyEditor.NotifyCustomizationModuleChanged();
		bDetailsCustomizationRegistered = true;
	}
}

void FArcTownEditorModule::ShutdownModule()
{
	AArcTownVolume::OnTeardownRequested.Unbind();

	if (SimulationController)
	{
		SimulationController->Unregister();
		SimulationController.Reset();
	}

	if (bDetailsCustomizationRegistered
		&& !IsEngineExitRequested()
		&& FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyEditor =
			FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyEditor.UnregisterCustomClassLayout(AArcTownVolume::StaticClass()->GetFName());
		PropertyEditor.NotifyCustomizationModuleChanged();
	}
	bDetailsCustomizationRegistered = false;
}

TWeakPtr<FArcTownSimulationController> FArcTownEditorModule::FindSimulationController()
{
	const FArcTownEditorModule* const Module =
		FModuleManager::GetModulePtr<FArcTownEditorModule>(TEXT("ArcTownEditor"));
	return Module != nullptr ? Module->GetSimulationController() : TWeakPtr<FArcTownSimulationController>();
}

TWeakPtr<FArcTownSimulationController> FArcTownEditorModule::GetSimulationController() const
{
	return SimulationController;
}

IMPLEMENT_MODULE(FArcTownEditorModule, ArcTownEditor)
