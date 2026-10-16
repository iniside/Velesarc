// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Simulation/ArcForestSimulationSession.h"

class AArcEcotopeVolume;

/** Game-thread owner of the active ArcForest editor simulation session. */
class ARCECOTOPEEDITOR_API FArcForestSimulationController
{
public:
	void Register();
	void Unregister();

	bool Start(
		AArcEcotopeVolume& Owner,
		const FArcForestStartRequest& Request,
		FText& OutError);
	void Cancel(const AArcEcotopeVolume& Owner);
	void UnregisterOwner(const AArcEcotopeVolume& Owner);

	/** The controller serializes all editor simulations, regardless of their owning volume. */
	bool HasActiveSession() const;
	bool HasSessionFor(const AArcEcotopeVolume& Owner) const;
	TOptional<FArcForestSimulationSessionSnapshot> GetSessionSnapshot(
		const AArcEcotopeVolume& Owner) const;

private:
	bool Tick(float DeltaTime);
	void CancelAndReleaseActiveSession();

	TUniquePtr<FArcForestSimulationSession> ActiveSession;
	FTSTicker::FDelegateHandle TickerHandle;
	uint64 OperationGeneration = 0;
};
