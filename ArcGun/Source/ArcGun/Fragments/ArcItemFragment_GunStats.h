/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */



#pragma once


#include "ArcScalableFloat.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemScalableFloat.h"
#include "Items/Fragments/ArcItemFragment.h"

#include "ArcItemFragment_GunStats.generated.h"

class UCurveFloat;
class UArcRecoilPattern;

USTRUCT(BlueprintType)
struct FArcGunKickParams
{
	GENERATED_BODY()

	/* Base upward kick amount per shot (in degrees) */
	UPROPERTY(EditAnywhere, Category = "Kick")
	FArcScalableFloat VerticalBase = 0.3f;

	/* Base left/right kick amount per shot (in degrees) */
	UPROPERTY(EditAnywhere, Category = "Kick")
	FArcScalableFloat HorizontalBase = 0.15f;

	/* Random variation applied to kick (0.0 - 1.0) */
	UPROPERTY(EditAnywhere, Category = "Kick")
	FArcScalableFloat Variation = 0.5f;

	/* Multiplier for the very first shot */
	UPROPERTY(EditAnywhere, Category = "Kick")
	FArcScalableFloat FirstShotMultiplier = 1.0f;

	/* How strongly the weapon favors one horizontal direction (-1.0 to 1.0) */
	UPROPERTY(EditAnywhere, Category = "Kick")
	FArcScalableFloat BiasStrength = 0.0f;
};

USTRUCT(BlueprintType)
struct FArcGunStabilityParams
{
	GENERATED_BODY()

	/* Maximum screen-space offset from center */
	UPROPERTY(EditAnywhere, Category = "Stability")
	FArcScalableFloat MaxOffset = 4.0f;

	/* Base kick applied to screen offset per shot */
	UPROPERTY(EditAnywhere, Category = "Stability")
	FArcScalableFloat BaseKick = 0.4f;

	/* How fast the offset returns to center */
	UPROPERTY(EditAnywhere, Category = "Stability")
	FArcScalableFloat RecoverySpeed = 5.0f;

	/* Horizontal/Vertical bias for the stability pattern */
	UPROPERTY(EditAnywhere, Category = "Stability")
	FVector2D Bias = FVector2D(-0.5f, 1.0f);
};

USTRUCT(BlueprintType)
struct FArcGunSwayParams
{
	GENERATED_BODY()

	/* Base radius of the idle sway */
	UPROPERTY(EditAnywhere, Category = "Sway")
	FArcScalableFloat Radius = 1.0f;

	/* Speed of the idle sway */
	UPROPERTY(EditAnywhere, Category = "Sway")
	FArcScalableFloat Speed = 1.0f;

	/* Multiplier for sway when aiming */
	UPROPERTY(EditAnywhere, Category = "Sway")
	FArcScalableFloat AimMultiplier = 0.5f;
};

/**
 * 
 */
USTRUCT(BlueprintType, meta=(DisplayName="Gun Stats", AllowCategories))
struct ARCGUN_API FArcItemFragment_GunStats : public FArcScalableFloatItemFragment
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(EditAnywhere, Category = "Base", meta = (EnableCategories, NoPreview))
	FArcScalableFloat BaseDamage;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, BaseDamage);

	UPROPERTY(EditAnywhere, Category = "Base", meta = (EnableCategories))
	FArcScalableFloat HeadshotDamageMultiplier = 1;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, HeadshotDamageMultiplier);

	UPROPERTY(EditAnywhere, Category = "Base", meta = (EnableCategories))
	FArcScalableFloat CriticalHitChance;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, CriticalHitChance);

	UPROPERTY(EditAnywhere, Category = "Base", meta = (EnableCategories))
	FArcScalableFloat CriticalHitDamageMultiplier;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, CriticalHitDamageMultiplier);

	UPROPERTY(EditAnywhere, Category = "Base", meta = (EnableCategories))
	FArcScalableFloat BaseMagazine;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, BaseMagazine);

	UPROPERTY(EditAnywhere, Category = "Base", meta=(ForceUnits="cm", EnableCategories))
	FArcScalableFloat BaseRange;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, BaseRange);

	/* Single shot shoots (how many bullets) */
	UPROPERTY(EditAnywhere, Category = "Base", meta = (EnableCategories))
	FArcScalableFloat Shots;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, Shots);

	/* Single shot ammo cost. */
	UPROPERTY(EditAnywhere, Category = "Base", meta = (EnableCategories))
	FArcScalableFloat AmmoCost;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, AmmoCost);

	/*
	 * Cooldown between repeated trigger pulls.
	 */
	UPROPERTY(EditAnywhere, Category = "Base", meta = (EnableCategories))
	FArcScalableFloat TriggerCooldown;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, TriggerCooldown);

	UPROPERTY(EditAnywhere, Category = "Base", meta = (EnableCategories))
	FArcScalableFloat RoundsPerMinute;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, RoundsPerMinute);
	
public:
	UPROPERTY(EditAnywhere, Category = "Base", meta = (DisplayThumbnail = false))
	TObjectPtr<UCurveFloat> RoundsPerMinuteCurveScale;
	
protected:
	UPROPERTY(EditAnywhere, Category = "Base", meta = (EnableCategories))
	FArcScalableFloat ReloadTime;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, ReloadTime);

	UPROPERTY(EditAnywhere, Config, Category = "Base", meta = (EnableCategories))
	FArcScalableFloat RotationSpeed = 2.0f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, RotationSpeed);
	
	/*
	 * If weapon is in relaxed/idle state how long before it can shot for first time ?
	 * Time it takes to go from relaxed to aim.
	 */
	UPROPERTY(EditAnywhere, Category = "Base", meta = (EnableCategories))
	FArcScalableFloat DelayToFirstShot = 0.0f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, DelayToFirstShot);
	
	/*
	 * How fast is camera zooming in when aiming.
	 **/
	UPROPERTY(EditAnywhere, Category = "Mobility", meta = (EnableCategories))
	FArcScalableFloat AimCameraSpeed = 1;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, AimCameraSpeed);

	UPROPERTY(EditAnywhere, Category = "Handling", meta = (EnableCategories))
	FArcScalableFloat HolsterDuration;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, HolsterDuration);

	UPROPERTY(EditAnywhere, Category = "Handling", meta = (EnableCategories))
	FArcScalableFloat UnholsterDuration;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, UnholsterDuration);


public:
	/**
	 * GUN INTENSITY (HEAT) MODEL
	 * A single 0.0 - 1.0 value that drives spread and recoil scaling.
	 */

	UPROPERTY(EditAnywhere, Category = "Intensity")
	FArcScalableFloat IntensityPerShot = 0.1f;

	UPROPERTY(EditAnywhere, Category = "Intensity")
	FArcScalableFloat IntensityDecayRate = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Intensity")
	FArcScalableFloat IntensityDecayDelay = 0.2f;

	/* Maps Intensity (0-1) to final Spread Angle */
	UPROPERTY(EditAnywhere, Category = "Accuracy")
	TObjectPtr<UCurveFloat> IntensityToSpreadCurve;

	/* Maps Intensity (0-1) to Recoil Strength Multiplier */
	UPROPERTY(EditAnywhere, Category = "Recoil")
	TObjectPtr<UCurveFloat> IntensityToRecoilMultiplierCurve;

	UPROPERTY(EditAnywhere, Category = "Recoil")
	FArcGunKickParams KickParams;

	UPROPERTY(EditAnywhere, Category = "Stability")
	FArcGunStabilityParams StabilityParams;

	UPROPERTY(EditAnywhere, Category = "Sway")
	FArcGunSwayParams SwayParams;

	/* Multiplier for spread when aiming */
	UPROPERTY(EditAnywhere, Category = "Accuracy")
	FArcScalableFloat AimSpreadMultiplier = 0.5f;

public:
	FArcItemFragment_GunStats()
		: RoundsPerMinuteCurveScale(nullptr)
		, IntensityToSpreadCurve(nullptr)
		, IntensityToRecoilMultiplierCurve(nullptr)
	{
	};
};

typedef FArcItemFragment_GunStats ArcGunStats;