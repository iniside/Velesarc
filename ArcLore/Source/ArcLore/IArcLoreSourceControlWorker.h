// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class IArcLoreSourceControlWorker
{
public:
	virtual FName GetName() const = 0;
	virtual bool Execute( class FArcLoreSourceControlCommand& InCommand ) = 0;
	virtual bool UpdateStates() const = 0;
};

typedef TSharedRef<IArcLoreSourceControlWorker, ESPMode::ThreadSafe> FArcLoreSourceControlWorkerRef;
