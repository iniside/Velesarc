// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Fragments/ArcRecoilFragments.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "ArcRecoilTestLibrary.generated.h"

class AActor;
class UArcRecoilPreset;

UCLASS()
class UArcRecoilTestLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Arc Recoil|Debug")
	static void ArcRecoilTestFire(AActor* Shooter, int32 ShotCount = 1);

	UFUNCTION(BlueprintCallable, Category = "Arc Recoil|Debug")
	static bool ArcRecoilGetState(AActor* Shooter, FVector2D& OutAngularOffsetDeg, float& OutSpreadHalfAngleDeg, float& OutHeat, float& OutShotIndex);

	UFUNCTION(BlueprintCallable, Category = "Arc Recoil|Debug")
	static bool ArcRecoilApplyProfile(AActor* Shooter, UArcRecoilPreset* Preset, FArcRecoilStats Stats);

	UFUNCTION(BlueprintCallable, Category = "Arc Recoil|Debug")
	static bool ArcRecoilPerturbDirection(AActor* Shooter, FVector AimDirection, FVector& OutDirection);
};
