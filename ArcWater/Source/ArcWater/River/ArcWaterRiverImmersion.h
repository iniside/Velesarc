// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FSceneView;
class UArcWaterRiverComponent;
class UMaterialParameterCollectionInstance;

namespace ArcWaterRiverImmersion
{
	struct FClarity
	{
		FVector3f ExtinctionPerCm = FVector3f::ZeroVector;
		float ScatterAlbedo = 0.0f;
	};

	struct FCorridorSample
	{
		FVector Center = FVector::ZeroVector;
		FVector Tangent = FVector::ForwardVector;
		FVector Right = FVector::RightVector;
		FVector SurfaceNormal = FVector::UpVector;
		float HalfWidthCm = 0.0f;
		float DistanceAlongCm = 0.0f;
		float ReachCm = 0.0f;
	};

	struct FViewGeometry
	{
		FVector2f TanHalfFov = FVector2f::ZeroVector;
		float NearPlaneCm = 0.0f;
		float NearPlaneRadiusCm = 0.0f;
	};

	struct FViewMargin
	{
		float MaskBandHalfCm = 0.0f;
		float MarginCm = 0.0f;
	};

	FClarity DeriveClarity(const FLinearColor& AbsorptionTint, float ClarityDistanceMeters, float ScatterAlbedo);

	float DeriveMaskReachCm(const FClarity& Clarity, float MinCurvatureRadiusCm, float HalfWidthCm, float SplineLengthCm);

	FViewGeometry SolveViewGeometry(const FSceneView& View);

	FViewMargin DeriveViewMargin(const FViewGeometry& ViewGeometry, const FVector& SurfaceNormal);

	bool IsNearCorridorXY(const UArcWaterRiverComponent& River, const FVector& ViewLocation, float InflateCm);

	void SolveCorridor(const UArcWaterRiverComponent& River, const FVector& ViewLocation, FCorridorSample& OutSample);

	bool IsViewImmersed(const UArcWaterRiverComponent& River, const FVector& ViewLocation,
		const FCorridorSample& Sample, float MarginCm);

	void PushClarity(UMaterialParameterCollectionInstance& Collection, const FClarity& Clarity);

	void PushCorridor(UMaterialParameterCollectionInstance& Collection, const FCorridorSample& Sample,
		const FViewGeometry& ViewGeometry, const FViewMargin& Margin);
}
