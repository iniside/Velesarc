// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEcotopeSamplerEditorModule.h"

#include "ArcEcotopeSamplerVolumeDetails.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Simulation/ArcEcotopeSamplerVolume.h"

void FArcEcotopeSamplerEditorModule::StartupModule()
{
	if (IsRunningGame())
	{
		return;
	}

	FPropertyEditorModule& PropertyEditor =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FRegisterCustomClassLayoutParams LayoutParams;
	LayoutParams.OptionalOrder = MAX_int32;
	PropertyEditor.RegisterCustomClassLayout(
		AArcEcotopeSamplerVolume::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FArcEcotopeSamplerVolumeDetails::MakeInstance),
		LayoutParams);
	PropertyEditor.NotifyCustomizationModuleChanged();
	bDetailsCustomizationRegistered = true;
}

void FArcEcotopeSamplerEditorModule::ShutdownModule()
{
	if (!bDetailsCustomizationRegistered
		|| IsEngineExitRequested()
		|| !FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		bDetailsCustomizationRegistered = false;
		return;
	}

	FPropertyEditorModule& PropertyEditor =
		FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyEditor.UnregisterCustomClassLayout(AArcEcotopeSamplerVolume::StaticClass()->GetFName());
	PropertyEditor.NotifyCustomizationModuleChanged();
	bDetailsCustomizationRegistered = false;
}

IMPLEMENT_MODULE(FArcEcotopeSamplerEditorModule, ArcEcotopeSamplerEditor)
