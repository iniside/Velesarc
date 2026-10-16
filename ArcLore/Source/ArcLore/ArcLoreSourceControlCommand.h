// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISourceControlProvider.h"
#include "ISourceControlChangelist.h"
#include "Misc/IQueuedWork.h"

class FArcLoreSourceControlCommand : public IQueuedWork
{
public:
	FArcLoreSourceControlCommand(const TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const TSharedRef<class IArcLoreSourceControlWorker, ESPMode::ThreadSafe>& InWorker, const FSourceControlOperationComplete& InOperationCompleteDelegate = FSourceControlOperationComplete());

	bool DoWork();
	virtual void Abandon() override;
	virtual void DoThreadedWork() override;
	ECommandResult::Type ReturnResults();

public:
	FString PathToGitBinary;
	FString PathToRepositoryRoot;

	/** Lore identity (display name) snapshotted from the provider on the game thread */
	FString Identity;

	TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> Operation;
	TSharedRef<class IArcLoreSourceControlWorker, ESPMode::ThreadSafe> Worker;
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
