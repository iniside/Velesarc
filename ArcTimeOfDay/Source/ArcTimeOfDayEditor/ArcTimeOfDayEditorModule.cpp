#include "ArcTimeOfDayEditorModule.h"

#include "ArcModifiableScalarCustomization.h"
#include "SArcTimeOfDayPanel.h"
#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FArcTimeOfDayEditorModule"

const FName FArcTimeOfDayEditorModule::ControlPanelTabId(TEXT("ArcTimeOfDay"));

void FArcTimeOfDayEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyEditorModule.RegisterCustomPropertyTypeLayout(TEXT("ArcModifiableFloat"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FArcModifiableScalarCustomization::MakeInstance));
	PropertyEditorModule.RegisterCustomPropertyTypeLayout(TEXT("ArcModifiableDouble"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FArcModifiableScalarCustomization::MakeInstance));
	PropertyEditorModule.RegisterCustomPropertyTypeLayout(TEXT("ArcModifiableInt"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FArcModifiableScalarCustomization::MakeInstance));
	PropertyEditorModule.NotifyCustomizationModuleChanged();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		ControlPanelTabId,
		FOnSpawnTab::CreateRaw(this, &FArcTimeOfDayEditorModule::SpawnControlPanelTab))
		.SetDisplayName(LOCTEXT("ControlPanelTitle", "ArcTimeOfDay"))
		.SetTooltipText(LOCTEXT("ControlPanelTooltip", "Control the selected ArcTimeOfDay actor."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());
}

void FArcTimeOfDayEditorModule::ShutdownModule()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ControlPanelTabId);
	if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyEditorModule.UnregisterCustomPropertyTypeLayout(TEXT("ArcModifiableFloat"));
		PropertyEditorModule.UnregisterCustomPropertyTypeLayout(TEXT("ArcModifiableDouble"));
		PropertyEditorModule.UnregisterCustomPropertyTypeLayout(TEXT("ArcModifiableInt"));
		PropertyEditorModule.NotifyCustomizationModuleChanged();
	}
}

TSharedRef<SDockTab> FArcTimeOfDayEditorModule::SpawnControlPanelTab(const FSpawnTabArgs& SpawnTabArgs)
{
	(void)SpawnTabArgs;
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SArcTimeOfDayPanel)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcTimeOfDayEditorModule, ArcTimeOfDayEditor)
