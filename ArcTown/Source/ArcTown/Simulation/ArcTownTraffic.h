// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ArcTownTraffic.generated.h"

struct FArcTownWorldSnapshot;

USTRUCT()
struct ARCTOWN_API FArcTownTrafficSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Traffic", meta = (ClampMin = "0.0"))
	double TripsPerBuildingPerYear = 2.0;

	UPROPERTY(EditAnywhere, Category = "Traffic", meta = (ClampMin = "0", ClampMax = "1000000"))
	int32 MinTripsPerYear = 8;

	UPROPERTY(EditAnywhere, Category = "Traffic", meta = (ClampMin = "1.0", Units = "cm"))
	double HalfAttractionDistanceCm = 20000.0;

	/** Trips per year an edge must carry before it is built at all; hierarchy classes are earned above it. */
	UPROPERTY(EditAnywhere, Category = "Traffic", meta = (ClampMin = "0.0"))
	double PromotionThresholdPerYear = 6.0;

	UPROPERTY(EditAnywhere, Category = "Traffic", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	double HierarchyPromotionTopPercent = 15.0;

	UPROPERTY(EditAnywhere, Category = "Traffic", meta = (ClampMin = "1.0", Units = "cm"))
	double RoadLaneWidthCm = 320.0;

	/** Lanes carried by hierarchy class 0 to MaxRoadHierarchy; a class is half that many lanes wide either side of its axis. */
	UPROPERTY(EditAnywhere, Category = "Traffic")
	TArray<double> HierarchyLaneCounts = {1.0, 1.5, 2.0, 3.0};

	/** Trips per year an edge must carry to be promoted into hierarchy class 0 to MaxRoadHierarchy, indexed by class like HierarchyLaneCounts; entry 0 is never read, and a class with no finite entry can never be reached. */
	UPROPERTY(EditAnywhere, Category = "Traffic", meta = (ClampMin = "0.0"))
	TArray<double> HierarchyPromotionTripsPerYear = {0.0, 12.0, 30.0, 75.0};

	UPROPERTY(EditAnywhere, Category = "Traffic", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double TrafficDecayPerYear = 0.4;
};

struct FArcTownTrafficYearReport
{
	int32 TripsRequested = 0;

	int32 TripsRouted = 0;

	int32 TripsWithoutReachableTarget = 0;

	int32 RoadsPromoted = 0;

	int32 RoadsWidened = 0;
};

namespace ArcTownTraffic
{
	inline constexpr uint8 MaxRoadHierarchy = 3;

	ARCTOWN_API double HalfWidthCmForHierarchy(const FArcTownTrafficSettings& TrafficSettings, uint8 Hierarchy);

	/** The half-width promotion can still widen this edge to, which placement sets back from. */
	ARCTOWN_API double MaxHalfWidthCm(const FArcTownTrafficSettings& TrafficSettings, double CurrentHalfWidthCm);

	ARCTOWN_API void AccumulateTraffic(
		const FArcTownTrafficSettings& TrafficSettings,
		int32 Year,
		FArcTownWorldSnapshot& Snapshot,
		FArcTownTrafficYearReport& OutReport);

	ARCTOWN_API void PromoteRoads(
		const FArcTownTrafficSettings& TrafficSettings,
		int32 Year,
		FArcTownWorldSnapshot& Snapshot,
		FArcTownTrafficYearReport& OutReport);

	ARCTOWN_API void DecayTraffic(const FArcTownTrafficSettings& TrafficSettings, FArcTownWorldSnapshot& Snapshot);
}
