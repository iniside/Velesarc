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
class APawn;
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
	TObjectPtr<APawn> CharacterOwner = nullptr;

	// Refactored state
	float CurrentIntensity = 0.0f;
	float IntensityDecayTimer = 0.0f;
	
	FVector2D CrosshairOffset = FVector2D::ZeroVector;
	FVector2D SwayOffset = FVector2D::ZeroVector;
	FVector2D StabilityOffset = FVector2D::ZeroVector;

	int32 ShotsFired = 0;
	float LastFireTime = 0.0f;
	
	// Pre-calculated modifiers (cached from attributes)
	float RecoilMultiplier = 1.0f;
	float SpreadMultiplier = 1.0f;
	float HandlingMultiplier = 1.0f;

	bool bIsAiming = false;
	bool bIsFiring = false;

public:
	virtual void Deinitialize() override;
	virtual void StartRecoil(UArcGunStateComponent* InGunState) override;
	virtual void StopRecoil(UArcGunStateComponent* InGunState) override;
	virtual void OnShootFired(UArcGunStateComponent* InGunState) override;
	virtual void UpdateRecoil(float DeltaTime, UArcGunStateComponent* InGunState) override;
	virtual void HandlePreProcessRotation(float Value) override;

	virtual FVector GetScreenORecoilffset(const FVector& Direction) const override;
	virtual float GetCurrentSpread() const override;

protected:
	void UpdateAttributes(const FArcItemData* InItem);
	void UpdateIntensity(float DeltaTime, const FArcItemFragment_GunStats* Stats);
	void UpdateCrosshairDynamics(float DeltaTime, const FArcItemFragment_GunStats* Stats);
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
