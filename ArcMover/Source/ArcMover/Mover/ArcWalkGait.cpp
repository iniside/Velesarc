// Copyright Lukasz Baran. All Rights Reserved.

#include "Mover/ArcWalkGait.h"

#include "ChaosMover/ChaosCompositeMovementMode.h"
#include "Mover/ArcMoverTypes.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"

namespace ArcWalkGaitPrivate
{
	constexpr double FacingOffsetLimit = 179.0;
	constexpr float FacingOffsetSlowStart = 90.0f;
	constexpr float FacingOffsetSlowEnd = 135.0f;
	constexpr float FacingOffsetSlowTime = 0.2f;
	constexpr double UnderRotationOffset = 135.0;
	constexpr double UnderRotationRateLimit = 1000.0;

	const EArcMoverGaitType Gaits[] = { EArcMoverGaitType::Walk, EArcMoverGaitType::Run, EArcMoverGaitType::Sprint };
}

FArcWalkLimits ArcWalkGait::ResolveWalkLimits(const FArcWalkGaitSettings& Settings, EArcMoverGaitType Gait, bool bCrouching, bool bHasMoveInput
	, float BaseMaxSpeed, float DecelerationSmoothingTime, float CurrentSpeedXY)
{
	FArcWalkLimits Limits;

	float GaitSpeed = Settings.WalkSpeed;
	float GaitAcceleration = Settings.WalkAcceleration;
	switch (Gait)
	{
		case EArcMoverGaitType::Walk:
			GaitSpeed = Settings.WalkSpeed;
			GaitAcceleration = Settings.WalkAcceleration;
			break;
		case EArcMoverGaitType::Run:
			GaitSpeed = Settings.RunSpeed;
			GaitAcceleration = Settings.RunAcceleration;
			break;
		case EArcMoverGaitType::Sprint:
			GaitSpeed = Settings.SprintSpeed;
			GaitAcceleration = CurrentSpeedXY > Settings.RunSpeed ? Settings.SprintAcceleration : Settings.RunAcceleration;
			break;
	}

	Limits.MaxSpeed = FMath::Clamp(bCrouching ? Settings.CrouchSpeed : GaitSpeed, 0.0f, BaseMaxSpeed);
	Limits.Acceleration = GaitAcceleration;
	Limits.Deceleration = bHasMoveInput ? Settings.GaitChangeDeceleration : Settings.StoppingDeceleration;
	Limits.DecelerationSmoothingTime = DecelerationSmoothingTime;

	return Limits;
}

EArcMoverGaitType ArcWalkGait::GaitForSpeed(const FArcWalkGaitSettings& Settings, float Speed, bool bCrouching, float BaseMaxSpeed)
{
	for (const EArcMoverGaitType Gait : ArcWalkGaitPrivate::Gaits)
	{
		if (ResolveWalkLimits(Settings, Gait, bCrouching, true, BaseMaxSpeed, 0.0f).MaxSpeed >= Speed)
		{
			return Gait;
		}
	}

	EArcMoverGaitType FastestGait = EArcMoverGaitType::Walk;
	float FastestSpeed = -1.0f;
	for (const EArcMoverGaitType Gait : ArcWalkGaitPrivate::Gaits)
	{
		const float GaitSpeed = ResolveWalkLimits(Settings, Gait, bCrouching, true, BaseMaxSpeed, 0.0f).MaxSpeed;
		if (GaitSpeed > FastestSpeed)
		{
			FastestSpeed = GaitSpeed;
			FastestGait = Gait;
		}
	}
	return FastestGait;
}

float ArcWalkGait::MaxAvailableSpeed(const FArcWalkGaitSettings& Settings, bool bCrouching, float BaseMaxSpeed)
{
	float MaxSpeed = 0.0f;
	for (const EArcMoverGaitType Gait : ArcWalkGaitPrivate::Gaits)
	{
		MaxSpeed = FMath::Max(MaxSpeed, ResolveWalkLimits(Settings, Gait, bCrouching, true, BaseMaxSpeed, 0.0f).MaxSpeed);
	}
	return MaxSpeed;
}

float ArcWalkGait::GetBrakingLag(float DecelerationSmoothingTime, float DecelerationSmoothingCompensation)
{
	return DecelerationSmoothingTime * (1.0f - DecelerationSmoothingCompensation);
}

FArcWalkTuning ArcWalkGait::ComputeWalkTuning(const FArcWalkGaitSettings& Settings, const FCharacterDefaultInputs& DefaultInputs, const FArcMoverCustomInputs& CustomInputs
	, const FQuat& DesiredFacing, const FQuat& CurrentFacing, float SpeedXY, bool bCrouching, bool bJustLanded, float BaseMaxSpeed, float DecelerationSmoothingTime)
{
	FArcWalkTuning Tuning;

	FRotator CurrentOffsetRotator = CurrentFacing.Rotator() - DesiredFacing.Rotator();
	CurrentOffsetRotator.Normalize();
	Tuning.CurrentOffset = CurrentOffsetRotator.Yaw;

	if (CustomInputs.bHaveFocusTarget)
	{
		Tuning.DesiredFacing = DefaultInputs.ControlRotation.Quaternion();
	}
	else
	{
		const double ClampedOffset = FMath::Clamp(CustomInputs.RotationOffset
			, Tuning.CurrentOffset - ArcWalkGaitPrivate::FacingOffsetLimit
			, Tuning.CurrentOffset + ArcWalkGaitPrivate::FacingOffsetLimit);
		Tuning.DesiredFacing = FRotator(0.0, ClampedOffset, 0.0).Quaternion() * DesiredFacing.Rotator().Quaternion();
	}

	const bool bHasMoveInput = !DefaultInputs.GetMoveInput().IsZero();

	const FArcWalkLimits Limits = ResolveWalkLimits(Settings, CustomInputs.Gait, bCrouching, bHasMoveInput, BaseMaxSpeed, DecelerationSmoothingTime, SpeedXY);
	Tuning.MaxSpeed = Limits.MaxSpeed;
	Tuning.Acceleration = Limits.Acceleration;
	Tuning.Deceleration = (!bHasMoveInput && bJustLanded) ? Settings.JustLandedDeceleration : Limits.Deceleration;

	Tuning.TurningStrength = FMath::GetMappedRangeValueClamped(FVector2f(Settings.RunSpeed, Settings.SprintSpeed)
		, FVector2f(Settings.WalkRunTurnStrength, Settings.SprintTurnStrength), SpeedXY);

	if (bHasMoveInput)
	{
		const float MovingFacingTime = FMath::GetMappedRangeValueClamped(FVector2f(Settings.RunSpeed, Settings.SprintSpeed)
			, FVector2f(Settings.WalkRunFacingTime, Settings.SprintFacingTime), SpeedXY);
		Tuning.FacingSmoothingTime = FMath::GetMappedRangeValueClamped(
			FVector2f(ArcWalkGaitPrivate::FacingOffsetSlowStart, ArcWalkGaitPrivate::FacingOffsetSlowEnd)
			, FVector2f(MovingFacingTime, ArcWalkGaitPrivate::FacingOffsetSlowTime)
			, static_cast<float>(FMath::Abs(Tuning.CurrentOffset)));
	}
	else
	{
		Tuning.FacingSmoothingTime = Settings.IdleFacingTime;
	}

	return Tuning;
}

FVector ArcWalkGait::ClampWalkVelocity(const FVector& Velocity, float BaseMaxSpeed)
{
	return Velocity.GetClampedToMaxSize(BaseMaxSpeed);
}

void ArcWalkGait::ClampUnderRotation(double CurrentOffset, double ControlRotationRate, FVector& InOutAngularVelocityDegrees)
{
	if (CurrentOffset >= ArcWalkGaitPrivate::UnderRotationOffset)
	{
		InOutAngularVelocityDegrees.Z = FMath::Clamp(InOutAngularVelocityDegrees.Z, -ArcWalkGaitPrivate::UnderRotationRateLimit, ControlRotationRate);
	}
	else if (CurrentOffset <= -ArcWalkGaitPrivate::UnderRotationOffset)
	{
		InOutAngularVelocityDegrees.Z = FMath::Clamp(InOutAngularVelocityDegrees.Z, ControlRotationRate, ArcWalkGaitPrivate::UnderRotationRateLimit);
	}
}

const IArcWalkGaitProvider* ArcMover::FindWalkGaitProvider(const UMoverComponent& Mover)
{
	for (const TPair<FName, TObjectPtr<UBaseMovementMode>>& NameModePair : Mover.MovementModes)
	{
		if (const IArcWalkGaitProvider* Provider = Cast<const IArcWalkGaitProvider>(NameModePair.Value.Get()))
		{
			return Provider;
		}
	}

	for (const TPair<FName, TObjectPtr<UBaseMovementMode>>& NameModePair : Mover.MovementModes)
	{
		const UBaseMovementMode* Mode = NameModePair.Value.Get();
		if (const IArcWalkGaitProvider* Provider = UE::ChaosMover::GetModeInterface<IArcWalkGaitProvider>(Mode))
		{
			return Provider;
		}
	}

	return nullptr;
}
