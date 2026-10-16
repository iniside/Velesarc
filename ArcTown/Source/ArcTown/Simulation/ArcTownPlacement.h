// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Math/Box2D.h"
#include "Morphology/ArcTownBlocks.h"
#include "Simulation/ArcTownTraffic.h"
#include "UObject/SoftObjectPath.h"

#include "ArcTownPlacement.generated.h"

struct FArcTownFieldSettings;
struct FArcTownFrozenAttractor;
struct FArcTownMorphologySettings;
struct FArcTownPlacementYearReport;
struct FArcTownSiteFields;
struct FArcTownSiteSnapshot;
struct FArcTownTrafficSettings;
struct FArcTownWorldSnapshot;

USTRUCT()
struct ARCTOWN_API FArcTownPlacementSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Placement", meta = (ClampMin = "0.0"))
	double BuildingsPerYear = 0.6;

	UPROPERTY(EditAnywhere, Category = "Placement", meta = (ClampMin = "0.0"))
	double BuildingsPerStandingBuildingPerYear = 0.05;

	UPROPERTY(EditAnywhere, Category = "Placement", meta = (ClampMin = "0.0"))
	double DistrictWeight = 1.0;

	UPROPERTY(EditAnywhere, Category = "Placement", meta = (ClampMin = "0.0"))
	double BuildingWeight = 1.0;

	UPROPERTY(EditAnywhere, Category = "Placement", meta = (ClampMin = "0.0"))
	double DenyWeight = 1.0;

	UPROPERTY(EditAnywhere, Category = "Placement", meta = (ClampMin = "0.0"))
	double SiteWeight = 1.0;
};

struct FArcTownFrozenBuildingEntry
{
	FSoftObjectPath VisualPath;

	FVector2D FootprintXY = FVector2D(400.0, 400.0);

	FVector2D PlotSizeMinXY = FVector2D(400.0, 400.0);

	FVector2D PlotSizeMaxXY = FVector2D(400.0, 400.0);

	double FloorHeightAbovePivot = 0.0;

	FVector2D MeshLocalDoorForwardXY = FVector2D(0.0, -1.0);

	FGameplayTagContainer BuildingTags;

	double InfluenceRadiusCm = 2000.0;

	FGameplayTagContainer PreferredDistricts;

	FGameplayTagContainer PreferredBuildings;

	FGameplayTagContainer DenyBuildings;

	double DenyMinDistanceCm = 0.0;

	float SelectionWeight = 1.0f;

	int32 EarliestYear = 0;

	uint8 MinHierarchy = 0;

	uint8 MaxHierarchy = ArcTownTraffic::MaxRoadHierarchy;
};

namespace ArcTownPlacement
{
	ARCTOWN_API double PeriodSetbackCm(const FArcTownMorphologySettings& MorphologySettings);

	ARCTOWN_API double MaxPlotDepthCm(
		const FArcTownMorphologySettings& MorphologySettings,
		TConstArrayView<FArcTownFrozenBuildingEntry> Catalog,
		int32 Year);

	ARCTOWN_API bool PlaceBuildings(
		const FArcTownSiteSnapshot& Site,
		const FArcTownSiteFields& Fields,
		const FArcTownFieldSettings& FieldSettings,
		const FArcTownPlacementSettings& PlacementSettings,
		const FArcTownMorphologySettings& MorphologySettings,
		const FArcTownTrafficSettings& TrafficSettings,
		TConstArrayView<FArcTownFrozenBuildingEntry> Catalog,
		TConstArrayView<FArcTownFrozenAttractor> Attractors,
		TConstArrayView<FArcTownBlock> Frontage,
		double FrontageSnapToleranceCm,
		const FBox2D& VolumeBoundsTownLocalXY,
		int32 Year,
		FArcTownWorldSnapshot& Snapshot,
		FArcTownPlacementYearReport& OutReport,
		FText& OutError);
}
