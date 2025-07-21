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
	UPROPERTY(EditAnywhere, Category = "Equipment", meta = (EnableCategories))
	FArcScalableFloat AimCameraSpeed = 1;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, AimCameraSpeed);

	UPROPERTY(EditAnywhere, Category = "Equipment", meta = (EnableCategories))
	FArcScalableFloat HolsterDuration;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, HolsterDuration);

	UPROPERTY(EditAnywhere, Category = "Equipment", meta = (EnableCategories))
	FArcScalableFloat UnholsterDuration;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, UnholsterDuration);


public:
	// A curve that maps the current heat to the amount a single shot will further 'heat up'
	// This is typically a flat curve with a single data point indicating how much heat a shot adds,
	// but can be other shapes to do things like punish overheating by adding progressively more heat.
	UPROPERTY(EditAnywhere, Category = "Heat", meta = (DisplayThumbnail = false, EnableCategories))
	TObjectPtr<UCurveFloat> HeatToHeatPerShotCurve;
	ARC_CURVE_GETTER(FArcItemFragment_GunStats, HeatToHeatPerShotCurve);
	/* Base minimal spread weapon can have. */
	// A curve that maps the heat to the spread angle
	// The X range of this curve typically sets the min/max heat range of the weapon
	// The Y range of this curve is used to define the min and maximum spread angle
	
public:
	UPROPERTY(EditAnywhere, Category = "Spread", meta = (DisplayThumbnail = false, EnableCategories))
	TObjectPtr<UCurveFloat> HeatToSpreadCurve;
	ARC_CURVE_GETTER(FArcItemFragment_GunStats, HeatToSpreadCurve);
	// A curve that maps the current heat to the heat cooldown rate per second
	// This is typically a flat curve with a single data point indicating how fast the heat
	// wears off, but can be other shapes to do things like punish overheating by slowing down
	// recovery at high heat.
public:
	UPROPERTY(EditAnywhere, Category = "Spread", meta = (DisplayThumbnail = false, EnableCategories))
	TObjectPtr<UCurveFloat> HeatToSpreadRecoveryPerSecondCurve;
	ARC_CURVE_GETTER(FArcItemFragment_GunStats, HeatToSpreadRecoveryPerSecondCurve);
	
protected:
	UPROPERTY(EditAnywhere, Category = "Spread", meta = (EnableCategories))
	FArcScalableFloat SpreadHeatRecoveryDelay;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, SpreadHeatRecoveryDelay);
	
public:
	//Maybe add separate Aim Curve ?
	UPROPERTY(EditAnywhere, Category = "Spread", meta = (DisplayThumbnail = false, EnableCategories))
	TObjectPtr<UCurveFloat> SpreadMovementAdditiveCurve;
	ARC_CURVE_GETTER(FArcItemFragment_GunStats, SpreadMovementAdditiveCurve);

	UPROPERTY(EditAnywhere, Category = "Spread", meta = (DisplayThumbnail = false, EnableCategories))
	TObjectPtr<UCurveFloat> AimSpreadMovementAdditiveCurve;
	ARC_CURVE_GETTER(FArcItemFragment_GunStats, AimSpreadMovementAdditiveCurve);
	
	/*
	 * How fast velocity level is interpolated from old value to new.
	 */
	UPROPERTY(EditAnywhere, Category = "Spread", meta = (EnableCategories))
	FArcScalableFloat SpreadVelocityGainInterpolation = 40.f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, SpreadVelocityGainInterpolation);

	/*
	 * How fast velocity level is interpolated from old value to new, while SpreadHeatRecoveryDelay passed.
	 */
	UPROPERTY(EditAnywhere, Category = "Spread", meta = (EnableCategories))
	FArcScalableFloat SpreadVelocityRecoveryInterpolation = 120.f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, SpreadVelocityRecoveryInterpolation);
	
	UPROPERTY(EditAnywhere, Category = "Spread", meta = (DisplayThumbnail = false, EnableCategories))
	TObjectPtr<UCurveFloat> SpreadRotationAdditiveCurve;
	ARC_CURVE_GETTER(FArcItemFragment_GunStats, SpreadRotationAdditiveCurve);

	/*
	 * How fast rotation level is interpolated from old level to new level.
	 * Rotation level is created from how much player rotated camera since last update.
	 */
	UPROPERTY(EditAnywhere, Category = "Spread", meta = (EnableCategories))
	FArcScalableFloat SpreadRotationGainInterpolation = 40.f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, SpreadRotationGainInterpolation);

	/*
	 * Interpolation speed to use when weapon is recovering. In most cases level will be reaching zero, while recovering.
	 */
	UPROPERTY(EditAnywhere, Category = "Spread", meta = (EnableCategories))
	FArcScalableFloat SpreadRotationRecoveryInterpolation = 120.f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, SpreadRotationRecoveryInterpolation);
	
protected:
	UPROPERTY(EditAnywhere, Category = "Spread", meta = (EnableCategories))
	FArcScalableFloat SpreadAimMultiplier;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, SpreadAimMultiplier);

	UPROPERTY(EditAnywhere, Category = "Spread", meta = (EnableCategories))
	FArcScalableFloat SpreadRecoveryAimMultiplier = 0.8;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, SpreadRecoveryAimMultiplier);
	
	UPROPERTY(EditAnywhere, Category = "Spread", meta = (EnableCategories))
	FArcScalableFloat DelayBeforeRotationSpread = 1.0f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, DelayBeforeRotationSpread);

public:
	UPROPERTY(EditAnywhere, Config, Category = "Recoil", meta = (EnableCategories))
	float RecoilUpdateRate = 0.01667f; // 60 FPS
	
	/*
	 * How much recoil is gained per shot. Does not specify direction of recoil
	 * It can usually be left as linear growth curve. Ie. for each shot gain 1 heat.
	 */
	UPROPERTY(EditAnywhere, Category = "Recoil", meta = (DisplayThumbnail = false, EnableCategories))
	TObjectPtr<UCurveFloat> HeatToRecoilCurve;

	// A curve that maps the current heat to the heat cooldown rate per second
	// This is typically a flat curve with a single data point indicating how fast the heat
	// wears off, but can be other shapes to do things like punish overheating by slowing down
	// recovery at high heat.
	UPROPERTY(EditAnywhere, Category = "Recoil", meta = (DisplayThumbnail = false, EnableCategories))
	TObjectPtr<UCurveFloat> HeatToRecoilRecoveryPerSecond;

protected:
	UPROPERTY(EditAnywhere, Config, Category = "Recoil", meta = (EnableCategories))
	FArcScalableFloat RecoilVerticalMin = 0.5f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, RecoilVerticalMin);

	UPROPERTY(EditAnywhere, Config, Category = "Recoil", meta = (EnableCategories))
	FArcScalableFloat RecoilVerticalMax = 1.5f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, RecoilVerticalMax);

	UPROPERTY(EditAnywhere, Config, Category = "Recoil", meta = (EnableCategories))
	FArcScalableFloat RecoilHorizontalMin = -1.0f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, RecoilHorizontalMin);

	UPROPERTY(EditAnywhere, Config, Category = "Recoil", meta = (EnableCategories))
	FArcScalableFloat RecoilHorizontalMax = 1.9f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, RecoilHorizontalMax);
	
	UPROPERTY(EditDefaultsOnly, Category="Recoil", meta=(ClampMin="0.1", EnableCategories))
	FArcScalableFloat BaseMultiplier = 1.0f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, BaseMultiplier);

public:
	// Horizontal bias
	UPROPERTY(EditDefaultsOnly, Category="Recoil", meta=(ClampMin="1", EnableCategories))
	FArcScalableFloat HeatToFullBias = 10;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, HeatToFullBias);
	
	UPROPERTY(EditDefaultsOnly, Category="Recoil",meta=(ClampMin="-1", ClampMax="1", EnableCategories))
	FArcScalableFloat InitialBiasDirection = 0;  // -1 / 0 / +1  (0 = random)
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, InitialBiasDirection);
	
	// Flip logic
	UPROPERTY(EditDefaultsOnly, Category="Recoil", meta=(ClampMin="0", ClampMax="1", EnableCategories))
	FArcScalableFloat FlipChancePerShot = 0.06f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, FlipChancePerShot);
	
	UPROPERTY(EditDefaultsOnly, Category="Recoil", meta=(ClampMin="0", ClampMax="1", EnableCategories))
	FArcScalableFloat FlipDampenFactor = 0.35f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, FlipDampenFactor);
	
public:
	// Optional spice
	UPROPERTY(EditDefaultsOnly, Category="Recoil", meta=(ClampMin="0", ClampMax="0.5", EnableCategories))
	FArcScalableFloat RandomStrengthJitter = 0.05f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, RandomStrengthJitter);
	
	UPROPERTY(EditDefaultsOnly, Category="Recoil", meta = (EnableCategories))
	FArcScalableFloat FirstShotBonus = 0.0f;   // extra ° on shot #0 (use negative for reduced kick)
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, FirstShotBonus);

	UPROPERTY(EditDefaultsOnly, Category="Recoil", meta=(EnableCategories))
	FArcScalableFloat  PeakStrength   = 1.6f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, PeakStrength);

	UPROPERTY(EditDefaultsOnly, Category="Recoil", meta=(EnableCategories))
	FArcScalableFloat  GrowthRate    = 0.08f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, GrowthRate);
	
	UPROPERTY(EditDefaultsOnly, Category="Recoil", meta=(EnableCategories))
	FArcScalableFloat  RampUpShots    = 8;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, RampUpShots);
	
	UPROPERTY(EditDefaultsOnly, Category="Recoil", meta=(EnableCategories))
	FArcScalableFloat  OscillationDownwardChance      = 0.f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, OscillationDownwardChance);
	
	UPROPERTY(EditDefaultsOnly, Category="Recoil", meta=(EnableCategories))
	FArcScalableFloat  OscAmplitude   = 0.15f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, OscAmplitude);
	
	UPROPERTY(EditDefaultsOnly, Category="Recoil", meta=(EnableCategories))
	FArcScalableFloat  OscPeriod      = 4;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, OscPeriod);
	
	UPROPERTY(EditDefaultsOnly, Category="Recoil", meta=(EnableCategories))
	FArcScalableFloat  NoiseJitter    = 0.05;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, NoiseJitter);

	UPROPERTY(EditAnywhere, Config, Category = "Recoil", meta = (EnableCategories))
	FArcScalableFloat RecoilHeatRecoveryDelay = 0.2f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, RecoilHeatRecoveryDelay);
	
	UPROPERTY(EditAnywhere, Config, Category = "Recoil", meta = (EnableCategories))
	FArcScalableFloat RecoilAimMultiplier;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, RecoilAimMultiplier);

	UPROPERTY(EditAnywhere, Config, Category = "Recoil", meta = (EnableCategories))
	FArcScalableFloat RecoilRecoverySpeed = 0.5f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, RecoilRecoverySpeed);

	///// Local Space Recoil /////
	UPROPERTY(EditAnywhere, Config, Category = "Recoil", meta = (EnableCategories))
	FArcScalableFloat LocalSpaceRecoilBase = 1.0f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, LocalSpaceRecoilBase);
	
	// Range: -1.0 (left) to 1.0 (right)
	UPROPERTY(EditAnywhere, Config, Category = "Recoil", meta = (EnableCategories))
	FArcScalableFloat LocalSpaceRecoilHorizontalBias = 0.0f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, LocalSpaceRecoilHorizontalBias);
	
	// Range: -1.0 (down) to 1.0 (up)
	UPROPERTY(EditAnywhere, Config, Category = "Recoil", meta = (EnableCategories))
	FArcScalableFloat LocalSpaceRecoilVerticalBias = 0.7f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, LocalSpaceRecoilVerticalBias);
	
	// How strongly to follow the main bias (0-1)
	UPROPERTY(EditAnywhere, Config, Category = "Recoil", meta = (EnableCategories))
	FArcScalableFloat LocalSpaceRecoilHeatScale = 0.05f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, LocalSpaceRecoilHeatScale);
	
	// Chance to temporarily move in opposite direction (0-1)
	UPROPERTY(EditAnywhere, Config, Category = "Recoil", meta = (EnableCategories))
	FArcScalableFloat LocalSpaceRecoilOscillationPeriod = 0.5f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, LocalSpaceRecoilOscillationPeriod);
	
	// How strong the oscillation movement is (0-1)
	UPROPERTY(EditAnywhere, Config, Category = "Recoil", meta = (EnableCategories))
	FArcScalableFloat LocalSpaceRecoilOscillationStrength = 0.3f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, LocalSpaceRecoilOscillationStrength);
	
	// Controls horizontal spread tightness (0-1)
	UPROPERTY(EditAnywhere, Config, Category = "Recoil", meta = (EnableCategories))
	FArcScalableFloat LocalSpaceRecoilHorizontalTightness = 0.7f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, LocalSpaceRecoilHorizontalTightness);
	
	UPROPERTY(EditAnywhere, Config, Category = "Recoil", meta = (EnableCategories))
	FArcScalableFloat LocalSpaceRecoilVerticalMinValue = -3.f;  
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, LocalSpaceRecoilVerticalMinValue);

	UPROPERTY(EditAnywhere, Config, Category = "Recoil", meta = (EnableCategories))
	FArcScalableFloat LocalSpaceRecoilVerticalMaxValue = 3.0f;  
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, LocalSpaceRecoilVerticalMaxValue);

	UPROPERTY(EditAnywhere, Config, Category = "Recoil", meta = (EnableCategories))
	FArcScalableFloat LocalSpaceRecoilHorizontalMinValue = -2.0f;  
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, LocalSpaceRecoilHorizontalMinValue);

	UPROPERTY(EditAnywhere, Config, Category = "Recoil", meta = (EnableCategories))
	FArcScalableFloat LocalSpaceRecoilHorizontalMaxValue = 2.0f;  
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, LocalSpaceRecoilHorizontalMaxValue);
	
	UPROPERTY(EditAnywhere, Config, Category = "Recoil", meta = (EnableCategories))
	FArcScalableFloat ScreenRecoilGainInterpolationSpeed = 5.f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, ScreenRecoilGainInterpolationSpeed);

	UPROPERTY(EditAnywhere, Config, Category = "Recoil", meta = (EnableCategories))
	FArcScalableFloat ScreenRecoilRecoveryInterpolationSpeed = 5.f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, ScreenRecoilRecoveryInterpolationSpeed);

protected:
	UPROPERTY(EditAnywhere, Config, Category = "Sway", meta = (EnableCategories))
	FArcScalableFloat BaseSwayAimMultiplier = 0.65f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, BaseSwayAimMultiplier);
	
	UPROPERTY(EditAnywhere, Config, Category = "Sway", meta = (EnableCategories))
	FArcScalableFloat BaseSwayRadius = 1.f;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, BaseSwayRadius);

	UPROPERTY(EditAnywhere, Config, Category = "Sway", meta = (EnableCategories))
	FArcScalableFloat BaseSwayInterpolationRate = 1.f;;
	ARC_ITEMSCALABLEFLOAT_STRUCT_GETTER(FArcItemFragment_GunStats, BaseSwayInterpolationRate);
	
public:
	UPROPERTY(EditAnywhere, Config, Category = "Sway", meta = (EnableCategories))
	FVector2D VelocitySwayVelocityRange = FVector2D(0, 350);
	
protected:	
	UPROPERTY(EditAnywhere, Category = "Sway", meta = (DisplayThumbnail = false, EnableCategories))
	TObjectPtr<UCurveFloat> VelocitySwayMultiplierCurve;
	ARC_CURVE_GETTER(FArcItemFragment_GunStats, VelocitySwayMultiplierCurve);

	UPROPERTY(EditAnywhere, Category = "Sway", meta = (DisplayThumbnail = false, EnableCategories))
	TObjectPtr<UCurveFloat> VelocitySwayInterpolationMultiplierCurve;
	ARC_CURVE_GETTER(FArcItemFragment_GunStats, VelocitySwayInterpolationMultiplierCurve);
	
public:
	FArcItemFragment_GunStats()
		: RoundsPerMinuteCurveScale(nullptr)
		, HeatToHeatPerShotCurve(nullptr)
		, HeatToSpreadCurve(nullptr)
		, HeatToSpreadRecoveryPerSecondCurve(nullptr)
		, SpreadMovementAdditiveCurve(nullptr)
		, SpreadRotationAdditiveCurve(nullptr)
		, HeatToRecoilCurve(nullptr)
		, HeatToRecoilRecoveryPerSecond(nullptr)
	{
	};
};

typedef FArcItemFragment_GunStats ArcGunStats;