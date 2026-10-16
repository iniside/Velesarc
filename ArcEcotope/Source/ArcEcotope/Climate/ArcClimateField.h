// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AArcClimateVolume;
class UWorld;

namespace UE::ArcEcotope::Climate
{
	struct FArcClimateAnalyticSample
	{
		double Temperature = 0.5;
		double Humidity = 0.5;
		double ReferenceElevationM = 0.0;
	};

	ARCECOTOPE_API FBox ComputeContributionBounds(const AArcClimateVolume& Volume);
	ARCECOTOPE_API void GatherSortedVolumes(
		const UWorld& World,
		const FBox& QueryWorldBounds,
		TArray<AArcClimateVolume*>& OutSorted);
	ARCECOTOPE_API double EvaluateMask(const AArcClimateVolume& Volume, const FVector& WorldPosition);
	ARCECOTOPE_API FArcClimateAnalyticSample EvaluateAt(
		const TArray<AArcClimateVolume*>& SortedVolumes,
		const AArcClimateVolume* BakingVolume,
		double SeedReferenceElevationM,
		const FVector& WorldPosition);
}
