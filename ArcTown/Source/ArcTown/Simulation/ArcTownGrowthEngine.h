// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Simulation/ArcTownSimulationInput.h"

namespace ArcTownGrowthEngine
{
	ARCTOWN_API uint64 ComputeSettingsHash(
		const FArcTownGrowthSettings& Settings,
		const FBox& VolumeBoundsTownLocal,
		TConstArrayView<FArcTownFrozenBuildingEntry> Catalog,
		TConstArrayView<FArcTownFrozenAttractor> SortedAttractors);

	ARCTOWN_API void SortAttractors(TArray<FArcTownFrozenAttractor>& Attractors);

	ARCTOWN_API bool Run(
		const FArcTownSimulationInput& Input,
		TFunctionRef<bool(int32 CompletedYears)> OnYearBegin,
		FArcTownWorldSnapshot& OutSnapshot,
		FArcTownTrafficYearReport& OutFinalYearTraffic,
		FText& OutError);
}
