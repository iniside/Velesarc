// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Simulation/ArcTownSimulationSession.h"

class AArcTownVolume;

class FArcTownSimulationController
{
public:
	void Register();
	void Unregister();

	bool Start(AArcTownVolume& Owner, int32 YearsToSimulate, FText& OutError);
	void Cancel(const AArcTownVolume& Owner);

	bool HasActiveSession() const;
	TOptional<FArcTownSimulationSessionSnapshot> GetSessionSnapshot(const AArcTownVolume& Owner) const;

private:
	bool Tick(float DeltaTime);
	void CancelAndReleaseActiveSession();

	TUniquePtr<FArcTownSimulationSession> ActiveSession;
	FTSTicker::FDelegateHandle TickerHandle;
	uint64 OperationGeneration = 0;
};
