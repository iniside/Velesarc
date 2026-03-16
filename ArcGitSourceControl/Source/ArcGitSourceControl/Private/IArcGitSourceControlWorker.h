// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class IArcGitSourceControlWorker
{
public:
	virtual FName GetName() const = 0;
	virtual bool Execute( class FArcGitSourceControlCommand& InCommand ) = 0;
	virtual bool UpdateStates() const = 0;
};

typedef TSharedRef<IArcGitSourceControlWorker, ESPMode::ThreadSafe> FArcGitSourceControlWorkerRef;
