// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRecoilImpulse.h"

#include "Fragments/ArcRecoilFragments.h"

namespace ArcRecoilImpulsePrivate
{
	void BuildAimBasis(const FVector& Forward, FVector& OutRight, FVector& OutUp)
	{
		const FVector Reference = FMath::Abs(Forward.Z) < 0.99 ? FVector::UpVector : FVector::ForwardVector;
		OutRight = FVector::CrossProduct(Reference, Forward).GetSafeNormal();
		if (OutRight.IsNearlyZero())
		{
			OutRight = FVector::YAxisVector;
		}
		OutUp = FVector::CrossProduct(Forward, OutRight);
	}
}

FVector2f ArcRecoil::ComputeShotImpulse(const FArcRecoilBakedLayerKick& Layer,
	const int32 SuppressionShots,
	const float ShotIndex,
	const float WalkShots,
	FRandomStream& Stream)
{
	// FirstShotMul scales pitch only; yaw keeps its bias and noise unscaled.
	const float Multiplier = ShotIndex < static_cast<float>(SuppressionShots) ? Layer.FirstShotMul : 1.0f;

	float WalkPitchMul = 1.0f;
	float YawBias = Layer.YawBiasSigned;
	if (Layer.WalkRadPerShot > 0.0f)
	{
		const float Walk = Layer.WalkRadPerShot * WalkShots;
		WalkPitchMul += Layer.WalkAmplitude.X * FMath::Cos(Walk);
		YawBias += Layer.WalkAmplitude.Y * FMath::Cos(Walk + Layer.WalkPhaseOffsetRad);
	}

	const float Pitch = Layer.KickPerShotDeg.X * Multiplier * WalkPitchMul;
	const float Noise = static_cast<float>(Stream.FRandRange(-1.0, 1.0));
	const float Yaw = Layer.KickPerShotDeg.Y * (YawBias + Noise * Layer.YawNoiseFrac);

	return FVector2f(Pitch, Yaw);
}

FVector ArcRecoil::PerturbAimDirection(const FVector& AimDirection,
	const FVector2f& AngularOffsetDeg,
	const float SpreadHalfAngleDeg,
	const float SpreadExponent,
	FRandomStream& Stream)
{
	const FVector Forward = AimDirection.GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		return AimDirection;
	}

	FVector RightAxis = FVector::ZeroVector;
	FVector UpAxis = FVector::ZeroVector;
	ArcRecoilImpulsePrivate::BuildAimBasis(Forward, RightAxis, UpAxis);

	const FQuat PitchRotation(FVector::CrossProduct(Forward, UpAxis), FMath::DegreesToRadians(AngularOffsetDeg.X));
	const FQuat YawRotation(UpAxis, FMath::DegreesToRadians(AngularOffsetDeg.Y));
	const FVector OffsetDirection = YawRotation.RotateVector(PitchRotation.RotateVector(Forward));

	FVector ConeRight = FVector::ZeroVector;
	FVector ConeUp = FVector::ZeroVector;
	ArcRecoilImpulsePrivate::BuildAimBasis(OffsetDirection, ConeRight, ConeUp);

	const float FromCenter = FMath::Pow(Stream.FRand(), SpreadExponent);
	const float AngleFromCenterDeg = FromCenter * SpreadHalfAngleDeg;
	const float AngleAroundDeg = Stream.FRand() * 360.0f;

	const FQuat TiltRotation(ConeRight, FMath::DegreesToRadians(AngleFromCenterDeg));
	const FQuat SpinRotation(OffsetDirection, FMath::DegreesToRadians(AngleAroundDeg));

	FVector Result = SpinRotation.RotateVector(TiltRotation.RotateVector(OffsetDirection));
	Result.Normalize();

	return Result;
}
