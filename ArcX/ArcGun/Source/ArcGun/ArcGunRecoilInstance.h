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

#include "ArcGunInstanceBase.h"
#include "Engine/DataAsset.h"
#include "StructUtils/InstancedStruct.h"
#include "Items/Fragments/ArcItemFragment.h"

#include "ArcGunRecoilInstance.generated.h"

class UArcGunStateComponent;

USTRUCT()
struct ARCGUN_API FArcGunRecoilInstance
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() {}
	
	virtual void StartRecoil(UArcGunStateComponent* InGunState) {};
	virtual void StopRecoil(UArcGunStateComponent* InGunState) {};
	virtual void OnShootFired(UArcGunStateComponent* InGunState) {};

	virtual void HandlePreProcessRotation(float Value) {};

	virtual void UpdateRecoil(float DeltaTime, UArcGunStateComponent* InGunState) {};
	

	//These functions are for accessing recoil parameters.
	//but I'm not entirely sure if this should be done this way
	//The recoil calculations are good, but maybe I should try to make data accessible without overriding some ambiguios functions ?

	virtual void GetSway(float& OutVerticalSwayMultiplier
		, float& OutVerticalSwayInterpolationTime
		, float& OutHorizontalSwayMultiplier
		, float& OutHorizontalSwayInterpolationTime) {};

	// The can return the same value.
	// Or crosshair can have different spread to indicate ie, range of sway
	// while bullet spread if different.
	// Spread of "Crosshair"
	virtual float GetCurrentSpread() const { return 0; }

	// Spread of bullets.
	virtual float GetBulletSpread() const { return 0; }

	virtual FVector GetScreenORecoilffset(const FVector& Direction) const { return Direction; };
	
	virtual ~FArcGunRecoilInstance() = default;
};

struct FArcItemFragment_GunStats;

class AArcCorePlayerController;
class UArcCoreAbilitySystemComponent;
class ACharacter;
class AArcPlayerCameraManager;
class UCameraComponent;
class UCurveFloat;
class UInputAction;

struct FEnhancedInputActionValueBinding;

USTRUCT()
struct ARCGUN_API FArcGunRecoilInstance_Base : public FArcGunRecoilInstance
{
	GENERATED_BODY()

public:
	virtual ~FArcGunRecoilInstance_Base() override = default;

protected:
	
	UPROPERTY(EditAnywhere)
	FGameplayTag AimTag;

	UPROPERTY()
	TObjectPtr<AArcCorePlayerController> ArcPC = nullptr;

	UPROPERTY()
	TObjectPtr<UArcCoreAbilitySystemComponent> ArcASC = nullptr;

	UPROPERTY()
	TObjectPtr<ACharacter> CharacterOwner = nullptr;

	FVector StartCameraLocation = FVector::ZeroVector;
	FVector EndCameraLocation = FVector::ZeroVector;
	
public:
	FVector GetScreenORecoilffset(const FVector& Direction) const override;
	
	bool bBreakRecovery = false;
	
	float SpreadHeat = 0;
	float RecoilHeat = 0;

	float RecoveredPitch = 0;
	double LastFireTime = 0;
	
	double GetLastFireTime() const
	{
		return LastFireTime;
	}
	
private:
	mutable bool bIsFiring;
	
	bool bIsAiming = false;

public:
	int32 GetShootCount() const
	{
		return ShotsFired;
	}
	
	TArray<FEnhancedInputActionValueBinding*> AxisValueBindings;
	
	int32 ShotsFired          = 0;
	float  BiasValue          = 0.f;   // -1 … +1
	int32  TargetBiasDir      = 0;     // -1 / +1
	//float  BiasAccelPerShot   = 0.f;
	
	float CurrentVelocityLevel = 0;
	float CurrentRotationLevel = 0;
	float AccumulatedPhase = 0.0f;
	
	float HighestAxisRotation = 0.0f;
	FVector2D NewStability;

	FVector2D GenerateStabilitySpreadPattern(
							float BaseKick,
							float MaxSpread,           // Base spread amount (0.5-2.0 typical)
							float HorizontalBias,       // -1.0 (left) to 1.0 (right)
							float VerticalBias,         // -1.0 (down) to 1.0 (up)
							float PatternTightness,     // 0.0 (chaotic) to 1.0 (tight pattern)
							float OscillationRate,      // 0.0 to 2.0 - how often pattern changes)
							FVector2D& CurrentOffset

	);
    // Reduced from 0.05f for less randomness
	/**
	 * @brief The speed at which the rotation spread is gained or recovered.
	 *
	 * This variable determines the speed at which the rotation spread is increased or decreased during certain actions. It is used in the calculation of the current rotation level and in
	 * the calculation of recoil modifiers.
	 *
	 * The rotation spread gain speed is set to 0 by default. It should be set to a positive value to increase the rotation spread and to a negative value to decrease it. The appropriate
	 * value should be determined based on the desired effect and gameplay requirements.
	 *
	 * This variable is used in the following functions:
	 * - FArcGunRecoilInstance_Base::CalculateCurrentRotationLevel()
	 * - FArcGunRecoilInstance_Base::CalculateRecoilModifires()
	 *
	 * @see FArcGunRecoilInstance_Base::CalculateCurrentRotationLevel()
	 * @see FArcGunRecoilInstance_Base::CalculateRecoilModifires()
	 *
	 * @note This variable is a protected member of the FArcGunRecoilInstance_Base class.
	 */

	float StartYInputValue = 0;
	float EndYInputValue = 0;
	float RecoveredInputValue = 0;
	float RecoveryDirection = 0;

	float LocalSpaceRecoilAccumulatedPhase = 0.0f;

	float TimeAccumulator = 0;
	
	float RotationSpreadGainSpeed = 0;
	float SpreadVelocityGainInterpolation = 0;
	float HeatPerShotMultiplier = 1;
	float SpreadHeatGainMultiplier = 0;
	float SpreadHeatRecoveryDelay = 0;
	float VelocitySpreadAdditive = 0;
	float RotationSpreadAdditive = 0;
	float SpreadAngleMultiplier = 0;
	float SpreadRecoveryMultiplier = 0;
	float CurrentSpreadAngle = 0;

	float RecoilHeatRecoveryDelay = 0;
	// TODO:: Needs to be implemented.

	float RecoilRecoveryMultiplier = 1;
	float RecoilHeatGainMultiplier = 0;
	
	float RecoilPitchMultiplier = 0;
	float RecoilYawMultiplier = 0;

	// Screen space based recoil.
	FVector2D ScreenRecoil = FVector2D::ZeroVector;
	FVector2D TargetScreenRecoil = FVector2D::ZeroVector;
	
	float ScreenRecoilInterpolationSpeed = 1.f;
	float ScreenRecoilRecoveryInterpolationSpeed = 1.f;
	
	
	// Sway
	FVector2D TargetSway;
	FVector2D CurrentSway;

	float SwayGainInterpolationSpeed = 1.f;
	float SwayRecoveryInterpolationSpeed = 1.f;
	float SwayRadius = 1;
	float CurrentSwayPoint = 0;
	
	bool bDirection = true;
	void CalculateCurrentVelocityLevel(
	const FVector2D& InRange
	, const FVector2D& OutRange
	, float Velocity
	, float DeltaTime
	);

	void CalculateCurrentRotationLevel(
		const FVector2D& InRange
		, const FVector2D& OutRange
		, float RotationRate
		, float DeltaTime
	);
	
	void CalculateRecoilModifires(const FArcItemData* InItem, UArcGunStateComponent* InGunState);

	virtual void Deinitialize() override;
	
	virtual void StartRecoil(UArcGunStateComponent* InGunState) override;

	virtual void StopRecoil(UArcGunStateComponent* InGunState) override;

	virtual void OnShootFired(UArcGunStateComponent* InGunState) override;

	virtual void UpdateRecoil(float DeltaTime, UArcGunStateComponent* InGunState) override;
	
	void ComputeSpreadHeatRange(const FArcItemFragment_GunStats* WeaponStats
	                      , float& MinHeat
	                      , float& MaxHeat) const;
	
	float ClampSpreadHeat(const FArcItemFragment_GunStats* WeaponStats
	                , float NewHeat) const;

	void ComputeRecoilHeatRange(const FArcItemFragment_GunStats* WeaponStats
		, float& MinHeat
		, float& MaxHeat) const;
	
	float ClampRecoilHeat(const FArcItemFragment_GunStats* WeaponStats
		, float NewHeat) const;

	virtual void HandlePreProcessRotation(float Value) override;
	
protected:
	//These functions are for accessing recoil parameters.
	//but I'm not entirely sure if this should be done this way
	//The recoil calculations are good, but maybe I should try to make data accessible without overriding some ambiguios functions ?

	virtual void GetSway(float& OutVerticalSwayMultiplier
		, float& OutVerticalSwayInterpolationTime
		, float& OutHorizontalSwayMultiplier
		, float& OutHorizontalSwayInterpolationTime) override {};

public:
	virtual float GetCurrentSpread() const override
	{
		return CurrentSpreadAngle;
	}
};


UCLASS()
class UArcGunRecoilInstancePreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Arc Gun", meta = (BaseStruct = "/Script/ArcGun.ArcGunRecoilInstance"))
	FInstancedStruct RecoilInstance;
};

USTRUCT()
struct FArcItemFragment_GunRecoilPreset : public FArcItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, meta = (DisplayThumbnail = "false"))
	TObjectPtr<UArcGunRecoilInstancePreset> InstancePreset;
};
