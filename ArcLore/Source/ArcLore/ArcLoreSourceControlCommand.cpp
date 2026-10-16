// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcLoreSourceControlCommand.h"
#include "Modules/ModuleManager.h"
#include "ArcLoreSourceControlModule.h"
#include "ISourceControlModule.h" // LogSourceControl
#include "HAL/PlatformTime.h"

FArcLoreSourceControlCommand::FArcLoreSourceControlCommand(const TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const TSharedRef<class IArcLoreSourceControlWorker, ESPMode::ThreadSafe>& InWorker, const FSourceControlOperationComplete& InOperationCompleteDelegate)
	: Operation(InOperation)
	, Worker(InWorker)
	, OperationCompleteDelegate(InOperationCompleteDelegate)
	, bExecuteProcessed(0)
	, bCommandSuccessful(false)
	, bAutoDelete(true)
	, Concurrency(EConcurrency::Synchronous)
{
	check(IsInGameThread());
	FArcLoreSourceControlModule& LoreSourceControl = FModuleManager::LoadModuleChecked<FArcLoreSourceControlModule>("ArcLore");
	PathToGitBinary = LoreSourceControl.AccessSettings().GetBinaryPath();
	PathToRepositoryRoot = LoreSourceControl.GetProvider().GetPathToRepositoryRoot();
	Identity = LoreSourceControl.GetProvider().GetUserName();
}

bool FArcLoreSourceControlCommand::DoWork()
{
	// [diag] Total wall-clock for the whole command (all lore ops + bookkeeping),
	// to sit alongside the per-op RunLoreOp timings and attribute the menu/add latency.
	const FString OpName = Operation->GetName().ToString();
	const double StartSeconds = FPlatformTime::Seconds();
	UE_LOGF(LogSourceControl, Verbose,"ArcLore: command '%ls' START — %d file(s), %ls",
		*OpName, Files.Num(),
		Concurrency == EConcurrency::Synchronous ? TEXT("Sync(game thread)") : TEXT("Async(pool thread)"));

	bCommandSuccessful = Worker->Execute(*this);
	FPlatformAtomics::InterlockedExchange(&bExecuteProcessed, 1);

	const double Elapsed = FPlatformTime::Seconds() - StartSeconds;
	UE_LOGF(LogSourceControl, Verbose,"ArcLore: command '%ls' END — %.2fs total (success=%d)",
		*OpName, Elapsed, bCommandSuccessful ? 1 : 0);
	return bCommandSuccessful;
}

void FArcLoreSourceControlCommand::Abandon()
{
	FPlatformAtomics::InterlockedExchange(&bExecuteProcessed, 1);
}

void FArcLoreSourceControlCommand::DoThreadedWork()
{
	Concurrency = EConcurrency::Asynchronous;
	DoWork();
}

ECommandResult::Type FArcLoreSourceControlCommand::ReturnResults()
{
	for (FString& String : InfoMessages)
	{
		Operation->AddInfoMessge(FText::FromString(String));
	}
	for (FString& String : ErrorMessages)
	{
		Operation->AddErrorMessge(FText::FromString(String));
	}

	ECommandResult::Type Result = bCommandSuccessful ? ECommandResult::Succeeded : ECommandResult::Failed;
	OperationCompleteDelegate.ExecuteIfBound(Operation, Result);
	return Result;
}
