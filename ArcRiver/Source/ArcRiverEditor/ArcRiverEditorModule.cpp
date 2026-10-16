// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRiverEditorModule.h"

#include "ArcRiverGenerate.h"
#include "ArcRiverGeneratorComponent.h"
#include "Modules/ModuleManager.h"

void FArcRiverEditorModule::StartupModule()
{
	if (!IsRunningGame())
	{
		GArcRiverCreateWaterZone.BindStatic(&ArcRiverGenerate::CreateWaterZone);
		GArcRiverGenerate.BindStatic(&ArcRiverGenerate::GenerateRivers);
		GArcRiverClear.BindStatic(&ArcRiverGenerate::ClearRivers);
		GArcRiverClearAll.BindStatic(&ArcRiverGenerate::ClearAllRivers);
	}
}

void FArcRiverEditorModule::ShutdownModule()
{
	GArcRiverCreateWaterZone.Unbind();
	GArcRiverGenerate.Unbind();
	GArcRiverClear.Unbind();
	GArcRiverClearAll.Unbind();
}

IMPLEMENT_MODULE(FArcRiverEditorModule, ArcRiverEditor)
