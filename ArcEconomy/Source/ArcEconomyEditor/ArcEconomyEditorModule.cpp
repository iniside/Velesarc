#include "ArcEconomyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Editor.h"
#include "Training/SArcStrategyTrainingTab.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "EditorVisualization/ArcEditorEntityDrawSubsystem.h"
#include "Contributors/ArcSettlementRadiusDrawContributor.h"
#include "Contributors/ArcBuildingProductionDrawContributor.h"
#include "Contributors/ArcSmartObjectSlotDrawContributor.h"
#include "Contributors/ArcDependencyGraphDrawContributor.h"

IMPLEMENT_MODULE(FArcEconomyEditorModule, ArcEconomyEditor)

const FName FArcEconomyEditorModule::StrategyTrainingTabId(TEXT("ArcStrategyTraining"));

void FArcEconomyEditorModule::StartupModule()
{
	if (IsRunningGame())
	{
		return;
	}

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(StrategyTrainingTabId,
		FOnSpawnTab::CreateRaw(this, &FArcEconomyEditorModule::SpawnStrategyTrainingTab))
		.SetDisplayName(NSLOCTEXT("ArcEconomyEditor", "StrategyTrainingTitle", "Strategy Training"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());

	PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddLambda([this]()
	{
		if (!GEditor)
		{
			return;
		}

		UArcEditorEntityDrawSubsystem* Subsystem = GEditor->GetEditorSubsystem<UArcEditorEntityDrawSubsystem>();
		if (!Subsystem)
		{
			return;
		}

		TSharedPtr<FArcSettlementRadiusDrawContributor> SettlementRadius = MakeShared<FArcSettlementRadiusDrawContributor>();
		Subsystem->RegisterContributor(SettlementRadius);
		RegisteredContributors.Add(SettlementRadius);

		TSharedPtr<FArcBuildingProductionDrawContributor> ProductionInfo = MakeShared<FArcBuildingProductionDrawContributor>();
		Subsystem->RegisterContributor(ProductionInfo);
		RegisteredContributors.Add(ProductionInfo);

		TSharedPtr<FArcSmartObjectSlotDrawContributor> SlotVis = MakeShared<FArcSmartObjectSlotDrawContributor>();
		Subsystem->RegisterContributor(SlotVis);
		RegisteredContributors.Add(SlotVis);

		TSharedPtr<FArcDependencyGraphDrawContributor> DepGraph = MakeShared<FArcDependencyGraphDrawContributor>();
		Subsystem->RegisterContributor(DepGraph);
		RegisteredContributors.Add(DepGraph);
	});
}

TSharedRef<SDockTab> FArcEconomyEditorModule::SpawnStrategyTrainingTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(NomadTab)
		[
			SNew(SArcStrategyTrainingTab)
		];
}

void FArcEconomyEditorModule::ShutdownModule()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(StrategyTrainingTabId);

	FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);

	if (GEditor)
	{
		if (UArcEditorEntityDrawSubsystem* Subsystem = GEditor->GetEditorSubsystem<UArcEditorEntityDrawSubsystem>())
		{
			for (const TSharedPtr<IArcEditorEntityDrawContributor>& Contributor : RegisteredContributors)
			{
				Subsystem->UnregisterContributor(Contributor);
			}
		}
	}
	RegisteredContributors.Empty();
}
