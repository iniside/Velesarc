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


#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "Engine/DataAsset.h"
#include "Items/Fragments/ArcItemFragment.h"

#include "ArcGunInstanceBase.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogArcGunInstanceBase, Log, Log);

struct FArcItemData;

/*
 * Maybe it should be exposed in blueprint ?
 * I'd assume most of those value would be calculated, but if someone does not care, those can be set in blueprint directly.
 */
struct FArcGunShootModifier
{
	/*
	 * How fast will recoil will interpolate input gain. Making it higher makes weapon much harder to controll
	 * as input gained from Sampling VerticalRecoilCurve/HorizontalRecoilCurve will be applied faster.
	 */
	float RecoilGainInterpolationSpeed = 20.0f;

	/*
	 * How fast will recoil recover from current position to start position.
	 */
	float RecoilRecoverInterpolationSpeed = 20.0f;
	/*
	 * Multiplies recoil gained. After recoil heat, but before calculating final value for added player input.
	 */
	float HeatPerShotMultiplier = 1.0f;

	/*
	 * Multiplies recoil gained. After recoil heat, but before calculating final value for added player input.
	 */
	float RecoilHeatGainMultiplier = 1.0f;

	/*
	 * Multiplies recoil pitch gained from curve.
	 */
	float RecoilPitchMultiplier = 1.0f;

	/*
	 * Multiplies recoil Yaw gained from curve.
	 */
	float RecoilYawMultiplier = 1.0f;

	/*
	 * Multiplies recoil recovered value.
	 */
	float RecoilRecoveryMultiplier = 1.0f;
	
	/*
	 * How long before we start recovering recoil heat (which will try to pull crosshair to starting point).
	 */
	float RecoilHeatRecoveryDelay = 0.2f;

	/*
	 * How longer before spread heat start to recover.
	 */
	float SpreadHeatRecoveryDelay = 0.5f;

	/*
	 * How fast we will gain spread heat. Effevtively increasing the speed at which spread increases.
	 */
	float SpreadHeatGainMultiplier = 1.0f;
	
	/*
	 * Increase (or decrease) and final angle gained at current spread heat.
	 */
	float SpreadAngleMultiplier = 1.0f;
	/*
	 * When recovering Spread Heat this value will multiply the current recovery value.
	 */
	float SpreadRecoveryMultiplier = 1.0f;
	
	float SpreadMovementMultiplierCurve = 0;
	/*
	 * Value at which CurrentVelocityLevel will be interpolated to new value.
	 */
	float SpreadVelocityGainInterpolation = 25.f;
	float CurrentVelocityLevel = 0;

	float SpreadRotationMultiplier = 0;
	/*
	 * Value at which CurrentRotationLevel will be interpolated to new value.
	 */
	float RotationSpreadGainSpeed = 15.f;
	float CurrentRotationLevel = 0;

	float RecoilHeat = 0;
	float RotationSpreadAdditive = 0;
	float VelocitySpreadAdditive = 0;
};


class UArcGunStateComponent;

/**
 * 
 */
USTRUCT()
struct ARCGUN_API FArcGunInstanceBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Data")
	FGameplayTag AimTag;

	TWeakObjectPtr<APawn> Character;
	
public:
	virtual void Initialize(UArcGunStateComponent* InComponent);
	
	bool CommitAmmo(const FArcItemData* InItem, UArcGunStateComponent* InComponent);

	virtual bool CanReload(const FArcItemData* InItem, UArcGunStateComponent* InComponent);
	virtual void Reload(const FArcItemData* InItem, UArcGunStateComponent* InComponent);

	/*
	 * Get ammo currently in magaizne
	 */
	virtual int32 GetMagazineAmmo(const FArcItemData* InItem, UArcGunStateComponent* InComponent);

	/*
	 * Get all available ammo for this weapon.
	 */
	virtual int32 GetWeaponAmmo(const FArcItemData* InItem, UArcGunStateComponent* InComponent);
	virtual float GetFireRate(const FArcItemData* InItem, UArcGunStateComponent* InComponent) const;
	virtual float GetTriggerCooldown(const FArcItemData* InItem, UArcGunStateComponent* InComponent) const;
	virtual float GetReloadTime(const FArcItemData* InItem, UArcGunStateComponent* InComponent) const;
	virtual float GetSwapTime(const FArcItemData* InItem, UArcGunStateComponent* InComponent) const { return 0.0f; }
	virtual float GetFireRange(const FArcItemData* InItem, UArcGunStateComponent* InComponent) const { return 0.0f; }

	virtual int32 GetMaxMagazine(const FArcItemData* InItem, UArcGunStateComponent* InComponent) const;
	
	virtual ~FArcGunInstanceBase() = default;
};

UCLASS()
class UArcGunInstancePreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/ArcGun.ArcGunInstanceBase"))
	FInstancedStruct GunInstance;
	
};


USTRUCT()
struct FArcItemFragment_GunInstancePreset : public FArcItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Data", meta=(DisplayThumbnail="false"))
	TObjectPtr<UArcGunInstancePreset> InstanceAsset;
};