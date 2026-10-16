// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"

#include "ArcWaterRiverModifierTypes.generated.h"

USTRUCT(BlueprintType)
struct ARCWATER_API FArcWaterRiverModifierSettings
{
	GENERATED_BODY()

	FArcWaterRiverModifierSettings();

	UPROPERTY(EditAnywhere, Category = "ArcWater River", meta = (ClampMin = "0.0", ClampMax = "1.0",
		ToolTip = "Brightness threshold for detecting rivers in the Rivers data map (0 = black, 1 = white). Higher values exclude faint marks; lower values include more branches and noise. Run Rebuild Rivers after changing settings."))
	float RiversThreshold = 0.01f;

	UPROPERTY(EditAnywhere, Category = "ArcWater River", meta = (ClampMin = "1.0", Units = "cm",
		ToolTip = "Base distance from the river centerline to either shore, in cm. Full width is twice this value, multiplied by both width curves. The width of the white mark in Rivers does not set the channel width."))
	float HalfWidthCm = 500.0f;

	UPROPERTY(EditAnywhere, Category = "ArcWater River", meta = (
		ToolTip = "Width multiplier by terrain slope along the river. X = slope in degrees; Y = positive width multiplier (1 = unchanged, 0.5 = half width). Multiplies Half Width and the elevation curve."))
	FRuntimeFloatCurve WidthScaleBySlope;

	UPROPERTY(EditAnywhere, Category = "ArcWater River", meta = (
		ToolTip = "Width multiplier by terrain elevation. X = world elevation in meters; Y = positive width multiplier. Multiplies Half Width and the slope curve; use 1 for unchanged width."))
	FRuntimeFloatCurve WidthScaleByElevation;

	UPROPERTY(EditAnywhere, Category = "ArcWater River", meta = (ClampMin = "0.0", Units = "cm",
		ToolTip = "Channel depth below the water surface at the centerline, in cm. Depth smoothly decreases to zero at either shore and tapers at original river endpoints."))
	float DepthCm = 200.0f;

	UPROPERTY(EditAnywhere, Category = "ArcWater River", meta = (ClampMin = "1.0", Units = "cm",
		ToolTip = "Bank transition distance outside each shore, in cm. The inner half rises from water level to Bank Freeboard; the outer half blends back into the original terrain."))
	float FalloffCm = 500.0f;

	UPROPERTY(EditAnywhere, Category = "ArcWater River", meta = (ClampMin = "0.0", Units = "cm",
		ToolTip = "Height of the bank above the water, in cm. Lowers the water surface by this amount below the sampled terrain height; Depth is measured below that surface."))
	float BankFreeboardCm = 60.0f;

	UPROPERTY(EditAnywhere, Category = "ArcWater River", meta = (ClampMin = "1.0", Units = "cm",
		ToolTip = "Distance along the river between the two terrain samples used for slope and average surface height. Larger values smooth short height changes; smaller values follow local detail. Short reaches use a shorter window."))
	float SlopeSampleDistanceCm = 1000.0f;

	UPROPERTY(EditAnywhere, Category = "ArcWater River", meta = (ClampMin = "0.0", Units = "cm",
		ToolTip = "Allowed horizontal deviation when simplifying the extracted river axis, in cm. Larger values remove more points and small bends; zero preserves the extracted detail. Junction anchors are retained."))
	float SplineToleranceCm = 25.0f;

	UPROPERTY(EditAnywhere, Category = "ArcWater River", meta = (ClampMin = "1.0", Units = "cm",
		ToolTip = "Maximum spacing between generated channel sections, in cm. Smaller values add geometry and height samples. Narrow channels and endpoint tapers can require closer spacing."))
	float MaxStepCm = 200.0f;

	UPROPERTY(EditAnywhere, Category = "ArcWater River|Surface", meta = (Units = "cm/s",
		ToolTip = "Flow speed magnitude passed to the water material, in cm/s. Direction is derived from the local terrain slope; the sign entered here does not reverse it. Does not change channel geometry."))
	float FlowSpeedCmS = 150.0f;

	UPROPERTY(EditAnywhere, Category = "ArcWater River|Surface", meta = (ClampMin = "1.0", Units = "cm",
		ToolTip = "Distance along the river for one water texture repeat, in cm. Larger values stretch the texture along the river. This is UV tiling, not terrain tile size."))
	float TileCm = 500.0f;

	UPROPERTY(EditAnywhere, Category = "ArcWater River|Surface", meta = (ClampMin = "1",
		ToolTip = "Number of water mesh strips across the full river width. Higher values add triangles across the surface; channel carving resolution is independent of this value."))
	int32 WidthSegments = 4;
};

USTRUCT()
struct ARCWATER_API FArcWaterRiverChannelSection
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Position = FVector::ZeroVector;

	UPROPERTY()
	FVector2D Lateral = FVector2D(0.0, 1.0);

	UPROPERTY()
	float DistanceAlongCm = 0.0f;

	UPROPERTY()
	float HalfWidthCm = 0.0f;

	UPROPERTY()
	float DepthCm = 0.0f;

	UPROPERTY()
	float FalloffCm = 0.0f;

	UPROPERTY()
	float FreeboardCm = 0.0f;

	UPROPERTY()
	float FlowSpeedCmS = 0.0f;

	UPROPERTY()
	int32 ReachIndex = INDEX_NONE;

	UPROPERTY()
	int32 SectionIndex = INDEX_NONE;

	UPROPERTY()
	int32 WidthSegments = 4;

	UPROPERTY()
	bool bClosesReach = false;
};

USTRUCT()
struct ARCWATER_API FArcWaterRiverGeneratedReach
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FVector> SplinePoints;

	UPROPERTY()
	TArray<FArcWaterRiverChannelSection> Sections;

	UPROPERTY()
	float LengthCm = 0.0f;

	UPROPERTY()
	float TileCm = 500.0f;

	UPROPERTY()
	bool bClosed = false;

	UPROPERTY()
	bool bStartIsOriginalEndpoint = false;

	UPROPERTY()
	bool bEndIsOriginalEndpoint = false;
};

USTRUCT()
struct ARCWATER_API FArcWaterRiverGeneratedData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArcWaterRiverGeneratedReach> Reaches;

	UPROPERTY()
	TArray<FVector2D> JunctionPositions;

	UPROPERTY()
	FVector2D CoverageCenterXY = FVector2D::ZeroVector;

	UPROPERTY()
	FVector2D CoverageXY = FVector2D::ZeroVector;

	UPROPERTY()
	float MinimumUntaperedHalfWidthCm = 0.0f;

	UPROPERTY()
	float MinimumUntaperedFalloffCm = 0.0f;
};
