// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/Box2D.h"
#include "Simulation/ArcTownGrowthDiagnostics.h"
#include "Simulation/ArcTownWorldSnapshot.h"

#include "ArcTownRoadExpansion.generated.h"

struct FArcTownFieldSettings;
struct FArcTownSiteFields;
struct FArcTownSiteSnapshot;
struct FArcTownTensorField;

USTRUCT()
struct ARCTOWN_API FArcTownRoadSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Roads", meta = (ClampMin = "1"))
	int32 MaxFailedExpansions = 3;

	UPROPERTY(EditAnywhere, Category = "Roads", meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
	double MaxRoadSlopeDeg = 25.0;

	/** Target spacing between parallel streets; a real city block runs 80 m to 200 m. */
	UPROPERTY(EditAnywhere, Category = "Roads", meta = (ClampMin = "100.0", Units = "cm"))
	double StreetBlockSizeCm = 12000.0;

	UPROPERTY(EditAnywhere, Category = "Roads", meta = (ClampMin = "100.0", Units = "cm"))
	double RoadStepLengthCm = 2000.0;

	/** 0 derives the radius from RoadStepLengthCm. */
	UPROPERTY(EditAnywhere, Category = "Roads", meta = (ClampMin = "0.0", Units = "cm"))
	double RoadSnapRadiusCm = 0.0;

	/** Roads that may meet at one junction; 5 leaves an ordinary crossroads room for one more street. */
	UPROPERTY(EditAnywhere, Category = "Roads", meta = (ClampMin = "3", ClampMax = "8"))
	int32 MaxNodeValence = 5;

	/** Arc radius R = L / (2 * sin(Theta / 2)): a 20 m step turning 20 deg rides a 57.6 m arc. */
	UPROPERTY(EditAnywhere, Category = "Roads", meta = (ClampMin = "1.0", ClampMax = "90.0", Units = "deg"))
	double MaxTurnPerStepDeg = 20.0;

	UPROPERTY(EditAnywhere, Category = "Roads", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
	double ExpansionAngleDeviationDeg = 8.0;

	UPROPERTY(EditAnywhere, Category = "Roads", meta = (ClampMin = "0.0", Units = "cm"))
	double BranchSpacingJitterCm = 3000.0;

	UPROPERTY(EditAnywhere, Category = "Roads", meta = (ClampMin = "1"))
	int32 MaxStepsPerRoad = 40;

	UPROPERTY(EditAnywhere, Category = "Roads", meta = (ClampMin = "1"))
	int32 RoadsPerYear = 8;

	/** Node count within one block that halves a node's draw; a full block lattice holds 2*pi*R^2 / (StreetBlockSizeCm * RoadStepLengthCm) = 38 of them. */
	UPROPERTY(EditAnywhere, Category = "Roads", meta = (ClampMin = "1.0"))
	double DensityHalfSaturationNodes = 38.0;

	UPROPERTY(EditAnywhere, Category = "Roads", meta = (ClampMin = "1.0", Units = "cm"))
	double RoadHalfWidthCm = 160.0;

	/** Longest water a road may cross in one span; a country-town road bridge runs 100 m, and 0 keeps every road on its own bank. */
	UPROPERTY(EditAnywhere, Category = "Roads", meta = (ClampMin = "0.0", Units = "cm"))
	double MaxBridgeSpanCm = 10000.0;
};

struct FArcTownRoadTraceParams
{
	const FArcTownTensorField* Field = nullptr;

	const FArcTownFieldSettings* FieldSettings = nullptr;

	const FArcTownRoadSettings* RoadSettings = nullptr;

	FBox2D VolumeBoundsTownLocalXY = FBox2D(ForceInit);

	int32 Year = 0;

	int32 StartNodeIndex = INDEX_NONE;

	double StartHeadingRad = 0.0;

	double HalfWidthCm = 160.0;

	/** INDEX_NONE takes the next road id from the snapshot; a set id continues an already started road. */
	int32 RoadId = INDEX_NONE;

	/** A road follows one eigenvector of the tensor field for its whole life; the branches it lays follow the other. */
	bool bFollowMinorField = false;

	bool bBuilt = false;
};

struct FArcTownRoadTraceResult
{
	int32 RoadId = INDEX_NONE;

	int32 EdgesAdded = 0;

	int32 EndNodeIndex = INDEX_NONE;

	EArcTownRoadTraceEnd End = EArcTownRoadTraceEnd::StartNodeInvalid;

	TArray<FArcTownRoadBranchSeed> BranchSeeds;
};

namespace ArcTownRoadExpansion
{
	extern ARCTOWN_API const TCHAR* const MaxRoadSlopeDegRequirementText;

	ARCTOWN_API bool IsMaxRoadSlopeDegUsable(const FArcTownRoadSettings& RoadSettings);

	ARCTOWN_API double ResolveSnapRadiusCm(const FArcTownRoadSettings& RoadSettings);

	ARCTOWN_API bool ResolveFollowMinorField(
		const FArcTownTensorField& Field,
		FVector2f NodeLocalXY,
		double StartHeadingRad);

	ARCTOWN_API int32 ResolveMaxPendingBranchSeeds(
		const FArcTownRoadSettings& RoadSettings,
		const FArcTownWorldSnapshot& Snapshot);

	ARCTOWN_API double ResolveStartHeadingRad(
		const FArcTownWorldSnapshot& Snapshot,
		int32 NodeIndex,
		const FVector2D& CoreTownLocalXY,
		int32 Year);

	ARCTOWN_API void TraceRoad(
		const FArcTownRoadTraceParams& Params,
		FArcTownWorldSnapshot& Snapshot,
		FArcTownExpansionYearReport& OutReport,
		FArcTownRoadTraceResult& OutResult);

	ARCTOWN_API bool ExpandRoads(
		const FArcTownTensorField& Field,
		const FArcTownFieldSettings& FieldSettings,
		const FArcTownRoadSettings& RoadSettings,
		const FBox2D& VolumeBoundsTownLocalXY,
		int32 Year,
		FArcTownWorldSnapshot& Snapshot,
		FArcTownExpansionYearReport& OutReport,
		FText& OutError);
}
