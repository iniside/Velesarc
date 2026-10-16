// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ArcWalkGait.generated.h"

enum class EArcMoverGaitType : uint8;
struct FArcMoverCustomInputs;
struct FCharacterDefaultInputs;
class IArcWalkGaitProvider;
class UMoverComponent;

USTRUCT(BlueprintType)
struct ARCMOVER_API FArcWalkGaitSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float WalkSpeed = 165.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float WalkAcceleration = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float RunSpeed = 375.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float RunAcceleration = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SprintSpeed = 585.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float SprintAcceleration = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float CrouchSpeed = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0"))
	float WalkRunTurnStrength = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0"))
	float SprintTurnStrength = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float GaitChangeDeceleration = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float StoppingDeceleration = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "s"))
	float WalkRunFacingTime = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "s"))
	float SprintFacingTime = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "s"))
	float IdleFacingTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float JustLandedDeceleration = 20000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc|Walking", meta = (ClampMin = "0", ForceUnits = "ms"))
	double JustLandedDurationMs = 200.0;
};

struct ARCMOVER_API FArcWalkLimits
{
	float MaxSpeed = 0.0f;
	float Acceleration = 0.0f;
	float Deceleration = 0.0f;
	float DecelerationSmoothingTime = 0.0f;
};

struct ARCMOVER_API FArcWalkTuning
{
	FQuat DesiredFacing = FQuat::Identity;
	double CurrentOffset = 0.0;
	float MaxSpeed = 0.0f;
	float Acceleration = 0.0f;
	float Deceleration = 0.0f;
	float TurningStrength = 0.0f;
	float FacingSmoothingTime = 0.0f;
};

namespace ArcWalkGait
{
	ARCMOVER_API FArcWalkLimits ResolveWalkLimits(const FArcWalkGaitSettings& Settings, EArcMoverGaitType Gait, bool bCrouching, bool bHasMoveInput
		, float BaseMaxSpeed, float DecelerationSmoothingTime, float CurrentSpeedXY = 0.0f);

	ARCMOVER_API EArcMoverGaitType GaitForSpeed(const FArcWalkGaitSettings& Settings, float Speed, bool bCrouching, float BaseMaxSpeed);

	ARCMOVER_API float MaxAvailableSpeed(const FArcWalkGaitSettings& Settings, bool bCrouching, float BaseMaxSpeed);

	ARCMOVER_API float GetBrakingLag(float DecelerationSmoothingTime, float DecelerationSmoothingCompensation);

	ARCMOVER_API FArcWalkTuning ComputeWalkTuning(const FArcWalkGaitSettings& Settings, const FCharacterDefaultInputs& DefaultInputs, const FArcMoverCustomInputs& CustomInputs
		, const FQuat& DesiredFacing, const FQuat& CurrentFacing, float SpeedXY, bool bCrouching, bool bJustLanded, float BaseMaxSpeed, float DecelerationSmoothingTime);

	ARCMOVER_API FVector ClampWalkVelocity(const FVector& Velocity, float BaseMaxSpeed);

	ARCMOVER_API void ClampUnderRotation(double CurrentOffset, double ControlRotationRate, FVector& InOutAngularVelocityDegrees);
}

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class ARCMOVER_API UArcWalkGaitProvider : public UInterface
{
	GENERATED_BODY()
};

class ARCMOVER_API IArcWalkGaitProvider
{
	GENERATED_BODY()

public:
	virtual FArcWalkGaitSettings GetGaitSettings() const = 0;
	virtual float GetBaseMaxSpeed() const = 0;
	virtual float GetDecelerationSmoothingTime() const = 0;
	virtual float GetBrakingLag() const = 0;
};

namespace ArcMover
{
	ARCMOVER_API const IArcWalkGaitProvider* FindWalkGaitProvider(const UMoverComponent& Mover);
}
