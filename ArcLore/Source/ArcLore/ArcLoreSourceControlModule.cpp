// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcLoreSourceControlModule.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "ArcLoreSourceControlOperations.h"
#include "ArcLoreApi.h"
#include "Features/IModularFeatures.h"
#include "Logging/LogMacros.h"
#include "ISourceControlModule.h"

#include "Panel/SArcLoreSourceControlPanel.h"
#include "Panel/SArcLoreBranchHistoryPanel.h"
#include "Panel/SArcLoreBranchGraphPanel.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "ToolMenus.h"
#include "HAL/IConsoleManager.h"
#include "RevisionControlStyle/RevisionControlStyle.h"
#include "ArcLoreAssetContextMenu.h"

#define LOCTEXT_NAMESPACE "ArcLoreSourceControl"

const FName FArcLoreSourceControlModule::ArcLoreSourceControlTabName = FName(TEXT("ArcLoreSourceControl"));
const FName FArcLoreSourceControlModule::ArcLoreBranchHistoryTabName = FName(TEXT("ArcLoreBranchHistory"));
const FName FArcLoreSourceControlModule::ArcLoreChangesTabName = FName(TEXT("ArcLoreChanges"));
const FName FArcLoreSourceControlModule::ArcLoreBranchGraphTabName = FName(TEXT("ArcLoreBranchGraph"));

template<typename Type>
static TSharedRef<IArcLoreSourceControlWorker, ESPMode::ThreadSafe> CreateWorker()
{
	return MakeShareable( new Type() );
}

void FArcLoreSourceControlModule::StartupModule()
{
	// 1. Load lore.dll FIRST (delay-load target + worker-thread limit). Every op
	//    gates on ArcLoreApi::IsReady(); capture the result to degrade the
	//    provider to UNAVAILABLE if the lib failed to load.
	const bool bLoreLibraryAvailable = ArcLoreApi::Init();
	if (!bLoreLibraryAvailable)
	{
		UE_LOGF(LogSourceControl, Error,"ArcLore: lore.dll failed to load (ArcLoreApi::Init() returned false). The Lore source control provider will register but report UNAVAILABLE; no Lore operations will be dispatched.");
	}
	else
	{
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: lore.dll loaded successfully (ArcLoreApi::Init() == true).");
	}

	// 2. Register the MVP workers and load settings.
	ArcLoreSourceControlProvider.RegisterWorker("Connect", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreConnectWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("UpdateStatus", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreUpdateStatusWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("MarkForAdd", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreMarkForAddWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("Unstage", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreUnstageWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("Copy", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreCopyWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("Delete", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreDeleteWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("Revert", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreRevertWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("Sync", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreSyncWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("CheckIn", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreCheckInWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("Resolve", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreResolveWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("CheckOut", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreCheckOutWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("LoreReleaseLock", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreReleaseLockWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("Push", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLorePushWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("LoreRevisionLog", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreRevisionLogWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("LoreRepositoryView", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreRepositoryViewWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("LoreListRemoteRepos", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreListRemoteReposWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("LoreListUnpushed", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreListUnpushedWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("LoreListLocks", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreListLocksWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("LoreRevisionFiles", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreRevisionFilesWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("LoreUndoLastCommit", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreUndoLastCommitWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("LoreAmendLastCommit", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreAmendLastCommitWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("LoreRemoveFromOutgoing", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreRemoveFromOutgoingWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("LoreLoginWithToken", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreLoginWithTokenWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("LoreLoginInteractive", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreLoginInteractiveWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("LoreLogout", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreLogoutWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("BranchSwitch", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreBranchSwitchWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("BranchCreate", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreBranchCreateWorker>));
	ArcLoreSourceControlProvider.RegisterWorker("LoreBranchGraph", FGetArcLoreSourceControlWorker::CreateStatic(&CreateWorker<FArcLoreBranchGraphWorker>));

	// Push the lib-availability into the provider so IsAvailable()/IsEnabled()
	// report UNAVAILABLE when the dll is missing (belt-and-suspenders: workers
	// already gate on ArcLoreApi::IsReady(), this also fixes provider-list UX).
	ArcLoreSourceControlProvider.SetLoreLibraryAvailable(bLoreLibraryAvailable);

	ArcLoreSourceControlSettings.LoadSettings();

	// 3. Register the modular feature REGARDLESS of lib availability, so the
	//    provider appears in the editor's source-control provider list.
	IModularFeatures::Get().RegisterModularFeature("SourceControl", &ArcLoreSourceControlProvider);

	// Tear down the live-notification subscription before modules unload. OnEnginePreExit
	// fires during FEngineLoop::Exit BEFORE ShutdownModule/Provider.Close(), so this is the
	// path that normally runs the sync unsubscribe (while lore is still up); Close()'s later
	// call is then a bSubscriptionIntended==false no-op — the actual unsubscribe fires once.
	EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddLambda([this]()
	{
		ArcLoreSourceControlProvider.PrepareForNotificationShutdown();
	});

	// 4. Register the editor UI (dockable tab + menu entry) DEFERRED. ArcLore is
	//    Type:DeveloperTool / LoadingPhase:Default, which can load before the
	//    editor's tab manager / workspace menu / ToolMenus are ready. Using
	//    UToolMenus::RegisterStartupCallback guarantees ToolMenus is initialized
	//    before we touch it, and by then FGlobalTabmanager + WorkspaceMenu are up
	//    too. Headless/cook runs (no Slate) skip the registration entirely.
	if (GIsEditor && FSlateApplication::IsInitialized())
	{
		UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FArcLoreSourceControlModule::RegisterUI));
	}

	// 5. Register the console command that focuses the Branch History tab. The
	//    engine's status-bar Revision Control menu already ships an "Open Branch
	//    History" entry wired to "UnrealRevisionControl.FocusBranchHistory"; that
	//    command is dead until something registers it. We only register if no real
	//    plugin claimed the name first (null-check), so a future engine plugin
	//    loaded before us keeps priority. The console manager is ready here — no
	//    deferral needed. The lambda captures nothing; the tab id is a static member.
	if (GIsEditor
		&& IConsoleManager::Get().FindConsoleObject(TEXT("UnrealRevisionControl.FocusBranchHistory")) == nullptr)
	{
		FocusBranchHistoryCommand = IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("UnrealRevisionControl.FocusBranchHistory"),
			TEXT("Open the Lore Branch History tab."),
			FConsoleCommandDelegate::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(FTabId(FArcLoreSourceControlModule::ArcLoreBranchHistoryTabName));
			}),
			ECVF_Default);
	}

	// 6. Register the console command that lights the engine's dead "Open Branch
	//    Explorer (Beta)" status-bar entry. Same guards as FocusBranchHistory above.
	if (GIsEditor
		&& IConsoleManager::Get().FindConsoleObject(TEXT("UnrealRevisionControl.FocusBranchExplorer")) == nullptr)
	{
		FocusBranchExplorerCommand = IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("UnrealRevisionControl.FocusBranchExplorer"),
			TEXT("Open the Lore Branch Graph tab."),
			FConsoleCommandDelegate::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(FTabId(FArcLoreSourceControlModule::ArcLoreBranchGraphTabName));
			}),
			ECVF_Default);
	}
}

void FArcLoreSourceControlModule::RegisterUI()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ArcLoreSourceControlTabName,
		FOnSpawnTab::CreateRaw(this, &FArcLoreSourceControlModule::SpawnTab))
		.SetDisplayName(LOCTEXT("ArcLoreSourceControlTabTitle", "Lore Source Control"))
		.SetTooltipText(LOCTEXT("ArcLoreSourceControlTabTooltip", "Stage, revert and check in Lore-tracked assets."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());

	// Branch History nomad tab. Hidden from the Window menu — it is reached via the
	// status-bar Revision Control menu's "Open Branch History" entry / console command.
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ArcLoreBranchHistoryTabName,
		FOnSpawnTab::CreateRaw(this, &FArcLoreSourceControlModule::SpawnBranchHistoryTab))
		.SetDisplayName(LOCTEXT("ArcLoreBranchHistoryTabTitle", "Branch History"))
		.SetTooltipText(LOCTEXT("ArcLoreBranchHistoryTabTooltip", "Browse the Lore branch revision history; expand a revision to its files; rewind to a revision."))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	// 4-section Lore Changes nomad tab (Dirty / Uncontrolled / Staged / Outgoing). Hidden
	// from the Window menu — reached via the status-bar Revision Control menu's "Lore Changes".
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ArcLoreChangesTabName,
		FOnSpawnTab::CreateRaw(this, &FArcLoreSourceControlModule::SpawnChangesTab))
		.SetDisplayName(LOCTEXT("ArcLoreChangesTabTitle", "Lore Changes"))
		.SetTooltipText(LOCTEXT("ArcLoreChangesTabTooltip", "Stage, commit and push Lore-tracked assets across four sections: Dirty, Uncontrolled, Staged and Outgoing."))
		.SetMenuType(ETabSpawnerMenuType::Hidden)
		.SetIcon(FSlateIcon(FRevisionControlStyleManager::Get().GetStyleSetName(), "RevisionControl.ChangelistsTab"));

	// Lore Branch Graph nomad tab. Hidden from the Window menu — reached via the
	// status-bar Revision Control menu's "Lore Branch Graph" entry / console command.
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ArcLoreBranchGraphTabName,
		FOnSpawnTab::CreateRaw(this, &FArcLoreSourceControlModule::SpawnBranchGraphTab))
		.SetDisplayName(LOCTEXT("BranchGraphTabTitle", "Lore Branch Explorer"))
		.SetTooltipText(LOCTEXT("BranchGraphTabTooltip", "Browse the Lore branch graph."))
		.SetMenuType(ETabSpawnerMenuType::Hidden)
		.SetIcon(FSlateIcon(FRevisionControlStyleManager::Get().GetStyleSetName(), "RevisionControl.BranchExplorerTab"));

	// Surface the Lore source-control panel from the status-bar Revision Control
	// dropdown (replacing the old Tools-menu entry). FToolMenuOwnerScoped stamps
	// `this` as the entry's owner so the ShutdownModule UnregisterOwner(this) removes it.
	// ArcLoreAssetContextMenu::Register() is also called under this scope so that the
	// Lock/Unlock Content Browser entries share the same owner and are torn down by
	// UnregisterOwner(this) in ShutdownModule.
	{
		FToolMenuOwnerScoped OwnerScoped(this);
		ArcLoreAssetContextMenu::Register();
		UToolMenu* SCMenu = UToolMenus::Get()->ExtendMenu("StatusBar.ToolBar.SourceControl");
		FToolMenuSection& Section = SCMenu->FindOrAddSection("ArcLore", LOCTEXT("ArcLoreRCSection", "ArcLore"));
		Section.AddMenuEntry(
			"ArcLoreSourceControl",
			LOCTEXT("ArcLoreSourceControlMenuEntry", "Lore Source Control"),
			LOCTEXT("ArcLoreSourceControlMenuTooltip", "Open the Lore source control panel."),
			FSlateIcon(FRevisionControlStyleManager::Get().GetStyleSetName(), "RevisionControl.SnapshotHistoryTab"),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(FTabId(ArcLoreSourceControlTabName));
			})));

		Section.AddMenuEntry(
			"ArcLoreChanges",
			LOCTEXT("ArcLoreChangesMenuEntry", "Lore Changes"),
			LOCTEXT("ArcLoreChangesMenuTooltip", "Open the 4-section Lore Changes panel (Dirty / Uncontrolled / Staged / Outgoing)."),
			FSlateIcon(FRevisionControlStyleManager::Get().GetStyleSetName(), "RevisionControl.ChangelistsTab"),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(FTabId(ArcLoreChangesTabName));
			})));

		Section.AddMenuEntry(
			"ArcLoreBranchGraph",
			LOCTEXT("ArcLoreBranchGraphMenuEntry", "Lore Branch Explorer"),
			LOCTEXT("ArcLoreBranchGraphMenuTooltip", "Open the Lore branch graph panel."),
			FSlateIcon(FRevisionControlStyleManager::Get().GetStyleSetName(), "RevisionControl.BranchExplorerTab"),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(FTabId(ArcLoreBranchGraphTabName));
			})));
	}
}

TSharedRef<SDockTab> FArcLoreSourceControlModule::SpawnTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SArcLoreSourceControlPanel)
		];
}

TSharedRef<SDockTab> FArcLoreSourceControlModule::SpawnBranchHistoryTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SArcLoreBranchHistoryPanel)
		];
}

TSharedRef<SDockTab> FArcLoreSourceControlModule::SpawnBranchGraphTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SArcLoreBranchGraphPanel)
		];
}

TSharedRef<SDockTab> FArcLoreSourceControlModule::SpawnChangesTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SArcLoreSourceControlPanel)
			.LayoutMode(EArcLoreChangesLayout::FourSection)
		];
}

void FArcLoreSourceControlModule::ShutdownModule()
{
	// 0. Tear down the editor UI registered in RegisterUI(). Guard on Slate so a
	//    headless shutdown (where RegisterUI never ran) is a no-op. Also drop any
	//    pending ToolMenus startup callback and our owned menu extensions.
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ArcLoreSourceControlTabName);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ArcLoreBranchHistoryTabName);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ArcLoreChangesTabName);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ArcLoreBranchGraphTabName);
	}
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	if (FocusBranchHistoryCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(FocusBranchHistoryCommand);
		FocusBranchHistoryCommand = nullptr;
	}

	if (FocusBranchExplorerCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(FocusBranchExplorerCommand);
		FocusBranchExplorerCommand = nullptr;
	}

	// 0b. Unbind the pre-exit teardown handle BEFORE closing the provider, so the
	//     handle is gone by the time Close() runs. (OnEnginePreExit normally fired
	//     already; Close()'s PrepareForNotificationShutdown is then the no-op-or-stop.)
	FCoreDelegates::OnEnginePreExit.Remove(EnginePreExitHandle);
	EnginePreExitHandle.Reset();

	// 1. Shut down the provider, as this module is going away.
	ArcLoreSourceControlProvider.Close();

	// 2. Unbind provider from editor.
	IModularFeatures::Get().UnregisterModularFeature("SourceControl", &ArcLoreSourceControlProvider);

	// 3. Shut the Lore library down LAST — after the provider is closed and
	//    unregistered, so no in-flight op can touch a lore_* import.
	ArcLoreApi::Shutdown();
}

void FArcLoreSourceControlModule::SaveSettings()
{
	if (FApp::IsUnattended() || IsRunningCommandlet())
	{
		return;
	}

	ArcLoreSourceControlSettings.SaveSettings();
}

IMPLEMENT_MODULE(FArcLoreSourceControlModule, ArcLore);

#undef LOCTEXT_NAMESPACE
