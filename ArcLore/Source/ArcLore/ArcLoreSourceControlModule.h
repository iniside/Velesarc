// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "ArcLoreSourceControlSettings.h"
#include "ArcLoreSourceControlProvider.h"

class SDockTab;
class FSpawnTabArgs;
struct IConsoleCommand;

class FArcLoreSourceControlModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Stable nomad tab id for the Lore source control panel. Public so the
	 *  Branch History panel's "Check in Changes" button can invoke it. */
	static const FName ArcLoreSourceControlTabName;

	/** Stable nomad tab id for the Lore branch revision-history panel. */
	static const FName ArcLoreBranchHistoryTabName;

	/** Stable nomad tab id for the Lore branch graph panel. */
	static const FName ArcLoreBranchGraphTabName;

	/** Stable nomad tab id for the 4-section Lore Changes panel (FourSection layout). */
	static const FName ArcLoreChangesTabName;

	/** Access the source control settings */
	FArcLoreSourceControlSettings& AccessSettings()
	{
		return ArcLoreSourceControlSettings;
	}

	const FArcLoreSourceControlSettings& AccessSettings() const
	{
		return ArcLoreSourceControlSettings;
	}

	/** Save the source control settings */
	void SaveSettings();

	/** Access the source control provider */
	FArcLoreSourceControlProvider& GetProvider()
	{
		return ArcLoreSourceControlProvider;
	}

	const FArcLoreSourceControlProvider& GetProvider() const
	{
		return ArcLoreSourceControlProvider;
	}

private:
	/** Tab + menu registration. Deferred via UToolMenus::RegisterStartupCallback
	 *  because ArcLore loads at DeveloperTool/Default — earlier than the editor UI. */
	void RegisterUI();

	/** Spawns the dockable nomad tab hosting SArcLoreSourceControlPanel. */
	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args);

	/** Spawns the dockable nomad tab hosting SArcLoreBranchHistoryPanel. */
	TSharedRef<SDockTab> SpawnBranchHistoryTab(const FSpawnTabArgs& Args);

	/** Spawns the dockable nomad tab hosting SArcLoreBranchGraphPanel. */
	TSharedRef<SDockTab> SpawnBranchGraphTab(const FSpawnTabArgs& Args);

	/** Spawns the dockable nomad tab hosting SArcLoreSourceControlPanel in FourSection layout. */
	TSharedRef<SDockTab> SpawnChangesTab(const FSpawnTabArgs& Args);

	/** Console command that focuses the Branch History tab; mirrors the engine's
	 *  (dead) "UnrealRevisionControl.FocusBranchHistory" entry. */
	IConsoleCommand* FocusBranchHistoryCommand = nullptr;

	/** Console command that focuses the Branch Graph tab; lights the engine's
	 *  dead "Open Branch Explorer (Beta)" status-bar entry. */
	IConsoleCommand* FocusBranchExplorerCommand = nullptr;

	/** Handle for the OnEnginePreExit binding that tears down the live-notification
	 *  subscription before modules unload (mirrors ArcMass.cpp). Bound in
	 *  StartupModule, unbound in ShutdownModule (before Provider.Close()). */
	FDelegateHandle EnginePreExitHandle;

	/** The source control provider */
	FArcLoreSourceControlProvider ArcLoreSourceControlProvider;

	/** The settings for source control */
	FArcLoreSourceControlSettings ArcLoreSourceControlSettings;
};
