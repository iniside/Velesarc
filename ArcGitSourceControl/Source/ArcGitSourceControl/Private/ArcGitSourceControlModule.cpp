// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcGitSourceControlModule.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "ArcGitSourceControlOperations.h"
#include "Features/IModularFeatures.h"

#define LOCTEXT_NAMESPACE "ArcGitSourceControl"

template<typename Type>
static TSharedRef<IArcGitSourceControlWorker, ESPMode::ThreadSafe> CreateWorker()
{
	return MakeShareable( new Type() );
}

void FArcGitSourceControlModule::StartupModule()
{
	ArcGitSourceControlProvider.RegisterWorker("Connect", FGetArcGitSourceControlWorker::CreateStatic(&CreateWorker<FArcGitConnectWorker>));
	ArcGitSourceControlProvider.RegisterWorker("UpdateStatus", FGetArcGitSourceControlWorker::CreateStatic(&CreateWorker<FArcGitUpdateStatusWorker>));
	ArcGitSourceControlProvider.RegisterWorker("MarkForAdd", FGetArcGitSourceControlWorker::CreateStatic(&CreateWorker<FArcGitMarkForAddWorker>));
	ArcGitSourceControlProvider.RegisterWorker("Delete", FGetArcGitSourceControlWorker::CreateStatic(&CreateWorker<FArcGitDeleteWorker>));
	ArcGitSourceControlProvider.RegisterWorker("Revert", FGetArcGitSourceControlWorker::CreateStatic(&CreateWorker<FArcGitRevertWorker>));
	ArcGitSourceControlProvider.RegisterWorker("Sync", FGetArcGitSourceControlWorker::CreateStatic(&CreateWorker<FArcGitSyncWorker>));
	ArcGitSourceControlProvider.RegisterWorker("CheckIn", FGetArcGitSourceControlWorker::CreateStatic(&CreateWorker<FArcGitCheckInWorker>));
	ArcGitSourceControlProvider.RegisterWorker("Copy", FGetArcGitSourceControlWorker::CreateStatic(&CreateWorker<FArcGitCopyWorker>));
	ArcGitSourceControlProvider.RegisterWorker("Resolve", FGetArcGitSourceControlWorker::CreateStatic(&CreateWorker<FArcGitResolveWorker>));
	// New: Locking
	ArcGitSourceControlProvider.RegisterWorker("CheckOut", FGetArcGitSourceControlWorker::CreateStatic(&CreateWorker<FArcGitCheckOutWorker>));
	// New: Changelists
	ArcGitSourceControlProvider.RegisterWorker("NewChangelist", FGetArcGitSourceControlWorker::CreateStatic(&CreateWorker<FArcGitNewChangelistWorker>));
	ArcGitSourceControlProvider.RegisterWorker("DeleteChangelist", FGetArcGitSourceControlWorker::CreateStatic(&CreateWorker<FArcGitDeleteChangelistWorker>));
	ArcGitSourceControlProvider.RegisterWorker("EditChangelist", FGetArcGitSourceControlWorker::CreateStatic(&CreateWorker<FArcGitEditChangelistWorker>));
	ArcGitSourceControlProvider.RegisterWorker("MoveToChangelist", FGetArcGitSourceControlWorker::CreateStatic(&CreateWorker<FArcGitMoveToChangelistWorker>));
	ArcGitSourceControlProvider.RegisterWorker("UpdateChangelistsStatus", FGetArcGitSourceControlWorker::CreateStatic(&CreateWorker<FArcGitUpdateChangelistsStatusWorker>));
	// New: Shelving
	ArcGitSourceControlProvider.RegisterWorker("Shelve", FGetArcGitSourceControlWorker::CreateStatic(&CreateWorker<FArcGitShelveWorker>));
	ArcGitSourceControlProvider.RegisterWorker("Unshelve", FGetArcGitSourceControlWorker::CreateStatic(&CreateWorker<FArcGitUnshelveWorker>));
	ArcGitSourceControlProvider.RegisterWorker("DeleteShelved", FGetArcGitSourceControlWorker::CreateStatic(&CreateWorker<FArcGitDeleteShelvedWorker>));
	// New: History/Utilities
	ArcGitSourceControlProvider.RegisterWorker("GetSourceControlRevisionInfo", FGetArcGitSourceControlWorker::CreateStatic(&CreateWorker<FArcGitGetRevisionInfoWorker>));
	ArcGitSourceControlProvider.RegisterWorker("DownloadFile", FGetArcGitSourceControlWorker::CreateStatic(&CreateWorker<FArcGitDownloadFileWorker>));

	ArcGitSourceControlSettings.LoadSettings();
	IModularFeatures::Get().RegisterModularFeature("SourceControl", &ArcGitSourceControlProvider);
}

void FArcGitSourceControlModule::ShutdownModule()
{
	// shut down the provider, as this module is going away
	ArcGitSourceControlProvider.Close();

	// unbind provider from editor
	IModularFeatures::Get().UnregisterModularFeature("SourceControl", &ArcGitSourceControlProvider);
}

void FArcGitSourceControlModule::SaveSettings()
{
	if (FApp::IsUnattended() || IsRunningCommandlet())
	{
		return;
	}

	ArcGitSourceControlSettings.SaveSettings();
}

IMPLEMENT_MODULE(FArcGitSourceControlModule, ArcGitSourceControl);

#undef LOCTEXT_NAMESPACE
