// Copyright Lukasz Baran. All Rights Reserved.

#include "River/ArcWaterRiverImmersion.h"

#include "Materials/MaterialParameterCollectionInstance.h"
#include "SceneView.h"

#include "River/ArcWaterRiverComponent.h"
#include "River/ArcWaterRiverMesh.h"

namespace ArcWaterRiverImmersionLocal_7c12
{
	const FName RiverSurfacePoint(TEXT("RiverSurfacePoint"));
	const FName RiverSurfaceNormal(TEXT("RiverSurfaceNormal"));
	const FName RiverBankNormal(TEXT("RiverBankNormal"));
	const FName RiverFlowNormal(TEXT("RiverFlowNormal"));
	const FName RiverBankHalfCm(TEXT("RiverBankHalfCm"));
	const FName RiverReachCm(TEXT("RiverReachCm"));
	const FName RiverExtinction(TEXT("RiverExtinction"));
	const FName RiverScatterAlbedo(TEXT("RiverScatterAlbedo"));
	const FName NearPlaneCm(TEXT("NearPlaneCm"));
	const FName MaskBandHalfCm(TEXT("MaskBandHalfCm"));

	constexpr float Ln10 = 2.302585093f;
	constexpr float MinClarityMeters = 0.25f;
	constexpr float CentimetresPerMetre = 100.0f;
	constexpr float OpticalDepthCutoff = 3.0f;

	FLinearColor PackVector(const FVector& Value)
	{
		return FLinearColor(
			static_cast<float>(Value.X),
			static_cast<float>(Value.Y),
			static_cast<float>(Value.Z),
			0.0f);
	}
}

namespace ArcWaterRiverImmersion
{
	FClarity DeriveClarity(const FLinearColor& AbsorptionTint, const float ClarityDistanceMeters, const float ScatterAlbedo)
	{
		const float ClarityCm =
			FMath::Max(ClarityDistanceMeters, ArcWaterRiverImmersionLocal_7c12::MinClarityMeters)
			* ArcWaterRiverImmersionLocal_7c12::CentimetresPerMetre;
		const float ExtinctionScale = ArcWaterRiverImmersionLocal_7c12::Ln10 / ClarityCm;

		FClarity Clarity;
		Clarity.ExtinctionPerCm = FVector3f(
			AbsorptionTint.R * ExtinctionScale,
			AbsorptionTint.G * ExtinctionScale,
			AbsorptionTint.B * ExtinctionScale);
		Clarity.ScatterAlbedo = FMath::Clamp(ScatterAlbedo, 0.0f, 1.0f);
		return Clarity;
	}

	float DeriveMaskReachCm(const FClarity& Clarity, const float MinCurvatureRadiusCm, const float HalfWidthCm,
		const float SplineLengthCm)
	{
		const float LongestReachCm = FMath::Max(SplineLengthCm, 0.0f);
		const float MaxExtinctionPerCm = Clarity.ExtinctionPerCm.GetMax();

		float ReachCm = LongestReachCm;
		if (MaxExtinctionPerCm > UE_SMALL_NUMBER)
		{
			ReachCm = FMath::Min(ArcWaterRiverImmersionLocal_7c12::OpticalDepthCutoff / MaxExtinctionPerCm, LongestReachCm);
		}

		if (HalfWidthCm > UE_SMALL_NUMBER)
		{
			const float BindingRadiusCm = (ReachCm * ReachCm) / (2.0f * HalfWidthCm);
			if (MinCurvatureRadiusCm > 0.0f && MinCurvatureRadiusCm < BindingRadiusCm)
			{
				ReachCm = FMath::Sqrt(2.0f * MinCurvatureRadiusCm * HalfWidthCm);
			}
		}

		return ReachCm;
	}

	FViewGeometry SolveViewGeometry(const FSceneView& View)
	{
		FViewGeometry ViewGeometry;
		ViewGeometry.TanHalfFov = View.ViewMatrices.GetTanHalfFov();
		ViewGeometry.NearPlaneCm = static_cast<float>(View.NearClippingDistance);
		ViewGeometry.NearPlaneRadiusCm = ViewGeometry.NearPlaneCm * FMath::Sqrt(1.0f
			+ ViewGeometry.TanHalfFov.X * ViewGeometry.TanHalfFov.X
			+ ViewGeometry.TanHalfFov.Y * ViewGeometry.TanHalfFov.Y);
		return ViewGeometry;
	}

	FViewMargin DeriveViewMargin(const FViewGeometry& ViewGeometry, const FVector& SurfaceNormal)
	{
		const float HorizontalLength = static_cast<float>(FMath::Sqrt(
			SurfaceNormal.X * SurfaceNormal.X + SurfaceNormal.Y * SurfaceNormal.Y));
		const float VerticalLength = static_cast<float>(FMath::Max(SurfaceNormal.Z, 0.0));
		const float SlopeMagnitude = HorizontalLength / FMath::Max(VerticalLength, UE_SMALL_NUMBER);

		FViewMargin Margin;
		Margin.MaskBandHalfCm = ViewGeometry.NearPlaneCm * ViewGeometry.TanHalfFov.Size() * SlopeMagnitude;
		Margin.MarginCm = ViewGeometry.NearPlaneRadiusCm * FMath::Sqrt(1.0f + SlopeMagnitude * SlopeMagnitude)
			+ Margin.MaskBandHalfCm;
		return Margin;
	}

	bool IsNearCorridorXY(const UArcWaterRiverComponent& River, const FVector& ViewLocation, const float InflateCm)
	{
		const FBox WorldBox = River.Bounds.GetBox();
		if (!WorldBox.IsValid)
		{
			return false;
		}

		const double Inflate = static_cast<double>(FMath::Max(InflateCm, 0.0f));
		return ViewLocation.X >= WorldBox.Min.X - Inflate
			&& ViewLocation.X <= WorldBox.Max.X + Inflate
			&& ViewLocation.Y >= WorldBox.Min.Y - Inflate
			&& ViewLocation.Y <= WorldBox.Max.Y + Inflate;
	}

	void SolveCorridor(const UArcWaterRiverComponent& River, const FVector& ViewLocation, FCorridorSample& OutSample)
	{
		const float Key = River.FindInputKeyClosestToWorldLocation(ViewLocation);
		OutSample.DistanceAlongCm = River.GetDistanceAlongSplineAtSplineInputKey(Key);

		ArcWaterRiverMesh::ResolveFrameAt(River, OutSample.DistanceAlongCm, ESplineCoordinateSpace::World,
			OutSample.Center, OutSample.Tangent, OutSample.Right);

		OutSample.SurfaceNormal = FVector::CrossProduct(OutSample.Tangent, OutSample.Right)
			.GetSafeNormal(UE_DOUBLE_SMALL_NUMBER, FVector::UpVector);
		OutSample.HalfWidthCm = FMath::Max(River.GetHalfWidthAtKey(Key), 0.0f);
	}

	bool IsViewImmersed(const UArcWaterRiverComponent& River, const FVector& ViewLocation,
		const FCorridorSample& Sample, const float MarginCm)
	{
		if (FVector::Dist2D(ViewLocation, Sample.Center) > static_cast<double>(Sample.HalfWidthCm))
		{
			return false;
		}

		const double AlongCm = FVector::DotProduct(ViewLocation - Sample.Center, Sample.Tangent);
		const double ProjectedCm = static_cast<double>(Sample.DistanceAlongCm) + AlongCm;
		if (AlongCm > 0.0 && ProjectedCm >= static_cast<double>(River.GetSplineLength()))
		{
			return false;
		}

		if (AlongCm < 0.0 && ProjectedCm <= 0.0)
		{
			return false;
		}

		return ViewLocation.Z <= Sample.Center.Z + static_cast<double>(MarginCm);
	}

	void PushClarity(UMaterialParameterCollectionInstance& Collection, const FClarity& Clarity)
	{
		Collection.SetVectorParameterValue(
			ArcWaterRiverImmersionLocal_7c12::RiverExtinction,
			FLinearColor(Clarity.ExtinctionPerCm.X, Clarity.ExtinctionPerCm.Y, Clarity.ExtinctionPerCm.Z, 0.0f));
		Collection.SetScalarParameterValue(ArcWaterRiverImmersionLocal_7c12::RiverScatterAlbedo, Clarity.ScatterAlbedo);
	}

	// Five planes, not one: the surface plane alone reads every pixel below the sheet as submerged and the bank pair alone runs unbounded along the flow, so the flow-facing pair caps the box at the reach the mask still holds over.
	void PushCorridor(UMaterialParameterCollectionInstance& Collection, const FCorridorSample& Sample,
		const FViewGeometry& ViewGeometry, const FViewMargin& Margin)
	{
		Collection.SetVectorParameterValue(ArcWaterRiverImmersionLocal_7c12::RiverSurfacePoint,
			ArcWaterRiverImmersionLocal_7c12::PackVector(Sample.Center));
		Collection.SetVectorParameterValue(ArcWaterRiverImmersionLocal_7c12::RiverSurfaceNormal,
			ArcWaterRiverImmersionLocal_7c12::PackVector(Sample.SurfaceNormal));
		Collection.SetVectorParameterValue(ArcWaterRiverImmersionLocal_7c12::RiverBankNormal,
			ArcWaterRiverImmersionLocal_7c12::PackVector(Sample.Right));
		Collection.SetVectorParameterValue(ArcWaterRiverImmersionLocal_7c12::RiverFlowNormal,
			ArcWaterRiverImmersionLocal_7c12::PackVector(Sample.Tangent));
		Collection.SetScalarParameterValue(ArcWaterRiverImmersionLocal_7c12::RiverBankHalfCm, Sample.HalfWidthCm);
		Collection.SetScalarParameterValue(ArcWaterRiverImmersionLocal_7c12::RiverReachCm, Sample.ReachCm);
		Collection.SetScalarParameterValue(ArcWaterRiverImmersionLocal_7c12::NearPlaneCm, ViewGeometry.NearPlaneCm);
		Collection.SetScalarParameterValue(ArcWaterRiverImmersionLocal_7c12::MaskBandHalfCm, Margin.MaskBandHalfCm);
	}
}
