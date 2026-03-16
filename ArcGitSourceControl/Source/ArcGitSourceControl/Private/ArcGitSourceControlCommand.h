// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISourceControlProvider.h"
#include "ISourceControlChangelist.h"
#include "Misc/IQueuedWork.h"

class FArcGitSourceControlCommand : public IQueuedWork
{
public:
	FArcGitSourceControlCommand(const TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const TSharedRef<class IArcGitSourceControlWorker, ESPMode::ThreadSafe>& InWorker, const FSourceControlOperationComplete& InOperationCompleteDelegate = FSourceControlOperationComplete());

	bool DoWork();
	virtual void Abandon() override;
	virtual void DoThreadedWork() override;
	ECommandResult::Type ReturnResults();

public:
	FString PathToGitBinary;
	FString PathToRepositoryRoot;

	TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> Operation;
	TSharedRef<class IArcGitSourceControlWorker, ESPMode::ThreadSafe> Worker;
	FSourceControlOperationComplete OperationCompleteDelegate;

	/** Changelist context for CL-aware operations */
	FSourceControlChangelistPtr Changelist;

	volatile int32 bExecuteProcessed;
	bool bCommandSuccessful;
	bool bAutoDelete;
	EConcurrency::Type Concurrency;

	TArray<FString> Files;
	TArray<FString> InfoMessages;
	TArray<FString> ErrorMessages;

	/** Process handle for cancel support */
	FProcHandle ProcessHandle;
	TAtomic<bool> bCancelRequested{false};
};
