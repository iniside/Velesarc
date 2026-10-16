// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"

#include "ArcRecoilFragments.generated.h"

USTRUCT(BlueprintType)
struct ARCRECOIL_API FArcRecoilStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "100"))
	float Stability = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "100"))
	float Control = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "100"))
	float Accuracy = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "100"))
	float Handling = 50.0f;
};

USTRUCT()
struct ARCRECOIL_API FArcRecoilStateFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	FVector2f AngularOffsetDeg = FVector2f::ZeroVector;

	UPROPERTY()
	FVector2f OffsetVelDegSec = FVector2f::ZeroVector;

	UPROPERTY()
	float Heat = 0.0f;

	UPROPERTY()
	float ShotIndex = 0.0f;

	UPROPERTY()
	float WalkShots = 0.0f;

	UPROPERTY()
	float TimeSinceShot = 0.0f;

	UPROPERTY()
	FVector2f PendingCameraKickDeg = FVector2f::ZeroVector;

	UPROPERTY()
	FRandomStream Stream;
};

USTRUCT()
struct ARCRECOIL_API FArcRecoilBakedLayerKick
{
	GENERATED_BODY()

	UPROPERTY()
	FVector2f KickPerShotDeg = FVector2f::ZeroVector;

	UPROPERTY()
	float YawBiasSigned = 0.0f;

	UPROPERTY()
	float YawNoiseFrac = 0.0f;

	UPROPERTY()
	float FirstShotMul = 1.0f;

	UPROPERTY()
	float WalkRadPerShot = 0.0f;

	UPROPERTY()
	FVector2f WalkAmplitude = FVector2f::ZeroVector;

	UPROPERTY()
	float WalkPhaseOffsetRad = 0.0f;
};

USTRUCT()
struct ARCRECOIL_API FArcRecoilProfileFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	FArcRecoilBakedLayerKick CameraKick;

	UPROPERTY()
	FArcRecoilBakedLayerKick AimOffsetKick;

	UPROPERTY()
	int32 SuppressionShots = 0;

	UPROPERTY()
	float ImpulseToVelGain = 0.0f;

	UPROPERTY()
	float HeatPerShot = 0.0f;

	UPROPERTY()
	float CoolRate = 0.0f;

	UPROPERTY()
	float CooldownDelay = 0.0f;

	UPROPERTY()
	float ShotIndexDecay = 0.0f;

	UPROPERTY()
	float WalkHoldSeconds = 0.0f;

	UPROPERTY()
	float WalkShotsDecay = 0.0f;

	UPROPERTY()
	float WalkPeriodShots = 0.0f;

	UPROPERTY()
	float BaseSpreadFullDeg = 0.0f;

	UPROPERTY()
	float MaxSpreadFullDeg = 0.0f;

	UPROPERTY()
	float SpreadCurveExp = 1.0f;

	UPROPERTY()
	float SpreadExponent = 1.0f;

	UPROPERTY()
	float Stiffness = 0.0f;

	UPROPERTY()
	float DampingCoeff = 0.0f;

	UPROPERTY()
	FVector2f MaxOffsetDeg = FVector2f::ZeroVector;

	UPROPERTY()
	float RecoveryDelay = 0.0f;

	UPROPERTY()
	float RecoveryRate = 0.0f;
};
