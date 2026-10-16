#include "ArcTerrainEditorModule.h"
#include "Modules/ModuleManager.h"

#include "ArcTerrainStampVisualizer.h"
#include "Stamps/ArcTerrainStampComponent.h"
#include "UnrealEdGlobals.h" // GUnrealEd
#include "Editor/UnrealEdEngine.h" // UUnrealEdEngine

void FArcTerrainEditorModule::StartupModule()
{
	FCoreDelegates::GetOnPostEngineInit().AddRaw(this, &FArcTerrainEditorModule::OnPostEngineInit);
}

void FArcTerrainEditorModule::ShutdownModule()
{
	FCoreDelegates::GetOnPostEngineInit().RemoveAll(this);

	if (GUnrealEd)
	{
		for (const FName& Name : VisualizersToUnregisterOnShutdown)
		{
			GUnrealEd->UnregisterComponentVisualizer(Name);
		}
	}
}

void FArcTerrainEditorModule::OnPostEngineInit()
{
	if (GUnrealEd)
	{
		auto RegisterVisualizer = [this](FName ComponentClassName, TSharedPtr<FComponentVisualizer> Visualizer)
		{
			GUnrealEd->RegisterComponentVisualizer(ComponentClassName, Visualizer);
			Visualizer->OnRegister();
			VisualizersToUnregisterOnShutdown.Add(ComponentClassName);
		};

		RegisterVisualizer(UArcTerrainStampComponent::StaticClass()->GetFName(), MakeShared<FArcTerrainStampVisualizer>());
	}
}

IMPLEMENT_MODULE(FArcTerrainEditorModule, ArcTerrainEditor)
