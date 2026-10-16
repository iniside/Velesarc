// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcWaterEditorModule.h"

#include "Details/ArcWaterRiverModifierDetails.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "River/ArcWaterRiverModifier.h"

void FArcWaterEditorModule::StartupModule()
{
	if (!IsRunningGame())
	{
		FPropertyEditorModule& PropertyEditor =
			FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyEditor.RegisterCustomClassLayout(
			UArcWaterRiverModifier::StaticClass()->GetFName(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FArcWaterRiverModifierDetails::MakeInstance));
		PropertyEditor.NotifyCustomizationModuleChanged();
		bRiverModifierDetailsRegistered = true;
	}
}

void FArcWaterEditorModule::ShutdownModule()
{
	if (bRiverModifierDetailsRegistered
		&& !IsEngineExitRequested()
		&& FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyEditor =
			FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyEditor.UnregisterCustomClassLayout(UArcWaterRiverModifier::StaticClass()->GetFName());
		PropertyEditor.NotifyCustomizationModuleChanged();
	}
	bRiverModifierDetailsRegistered = false;
}

IMPLEMENT_MODULE(FArcWaterEditorModule, ArcWaterEditor)
