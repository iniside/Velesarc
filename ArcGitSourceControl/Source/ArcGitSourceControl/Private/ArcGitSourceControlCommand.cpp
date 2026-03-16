// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArcGitSourceControlCommand.h"
#include "Modules/ModuleManager.h"
#include "ArcGitSourceControlModule.h"

FArcGitSourceControlCommand::FArcGitSourceControlCommand(const TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const TSharedRef<class IArcGitSourceControlWorker, ESPMode::ThreadSafe>& InWorker, const FSourceControlOperationComplete& InOperationCompleteDelegate)
	: Operation(InOperation)
	, Worker(InWorker)
	, OperationCompleteDelegate(InOperationCompleteDelegate)
	, bExecuteProcessed(0)
	, bCommandSuccessful(false)
	, bAutoDelete(true)
	, Concurrency(EConcurrency::Synchronous)
{
	check(IsInGameThread());
	FArcGitSourceControlModule& GitSourceControl = FModuleManager::LoadModuleChecked<FArcGitSourceControlModule>("ArcGitSourceControl");
	PathToGitBinary = GitSourceControl.AccessSettings().GetBinaryPath();
	PathToRepositoryRoot = GitSourceControl.GetProvider().GetPathToRepositoryRoot();
}

bool FArcGitSourceControlCommand::DoWork()
{
	bCommandSuccessful = Worker->Execute(*this);
	FPlatformAtomics::InterlockedExchange(&bExecuteProcessed, 1);
	return bCommandSuccessful;
}

void FArcGitSourceControlCommand::Abandon()
{
	FPlatformAtomics::InterlockedExchange(&bExecuteProcessed, 1);
}

void FArcGitSourceControlCommand::DoThreadedWork()
{
	Concurrency = EConcurrency::Asynchronous;
	DoWork();
}

ECommandResult::Type FArcGitSourceControlCommand::ReturnResults()
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
