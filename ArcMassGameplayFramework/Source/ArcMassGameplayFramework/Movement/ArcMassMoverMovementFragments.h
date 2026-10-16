// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityElementTypes.h"

#include "ArcMassMoverMovementFragments.generated.h"

USTRUCT()
struct ARCMASSGAMEPLAYFRAMEWORK_API FArcMassMovementLimitsFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Movement", meta = (ForceUnits = "cm/s"))
	float MaxSpeed = 200.f;

	UPROPERTY(VisibleAnywhere, Category = "Movement", meta = (ForceUnits = "cm/s^2"))
	float MaxAcceleration = 250.f;

	UPROPERTY(VisibleAnywhere, Category = "Movement")
	float SpeedScale = 1.f;

	UPROPERTY(VisibleAnywhere, Category = "Movement", meta = (ForceUnits = "cm/s^2"))
	float BrakingDeceleration = 300.f;

	UPROPERTY(VisibleAnywhere, Category = "Movement", meta = (ForceUnits = "s"))
	float BrakingLag = 0.f;
};

USTRUCT()
struct ARCMASSGAMEPLAYFRAMEWORK_API FArcMassBrakingParameters : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "1", ForceUnits = "cm/s^2"))
	float BrakingDeceleration = 300.f;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0", ForceUnits = "s"))
	float BrakingLag = 0.f;
};

USTRUCT()
struct ARCMASSGAMEPLAYFRAMEWORK_API FArcMassStandBrakeFragment : public FMassFragment
{
	GENERATED_BODY()

	uint16 ActionID = 0;

	bool bBraking = false;
};

USTRUCT()
struct ARCMASSGAMEPLAYFRAMEWORK_API FArcMassForceFragment : public FMassFragment
{
	GENERATED_BODY()

	FVector Value = FVector::ZeroVector;
};
