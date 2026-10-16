// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace ArcRPGCoreDerivation
{
	/** floor((V - 10) / 2) */
	ARCRPGMECHANICS_API float CalculateModifier(float Value);

	/**
	 * DaySequence-aware real seconds per game hour.
	 * Reads UDaySequenceSubsystem -> ADaySequenceActor. Returns 120.f if any step fails.
	 */
	ARCRPGMECHANICS_API float ResolveRealSecondsPerGameHour(UWorld* World);
}
