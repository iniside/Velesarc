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


#include "StructUtils/InstancedStruct.h"
#include "Engine/DataAsset.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "ArcGunFireMode.generated.h"

class UArcGunStateComponent;
struct FGameplayAbilitySpecHandle;
USTRUCT()
struct ARCGUN_API FArcGunFireMode
{
	GENERATED_BODY()
	
public:
	// Return true if weapon can shot
	virtual bool FireStart(const FGameplayAbilitySpecHandle& TargetRequest, UArcGunStateComponent* InGunComponent) { return false; };

	virtual void FireStop(const FGameplayAbilitySpecHandle& TargetRequest, UArcGunStateComponent* InGunComponent) {};
	
	// Return true if weapon can shot
	virtual bool FireUpdate(const FGameplayAbilitySpecHandle& TargetRequest, UArcGunStateComponent* InGunComponent, float DeltaTime) { return false; };

	virtual ~FArcGunFireMode() = default;
	float ElapsedTime = 0;
};

USTRUCT()
struct ARCGUN_API FArcGunFireMode_FullAuto : public FArcGunFireMode
{
	GENERATED_BODY()
	
public:
	virtual bool FireStart(const FGameplayAbilitySpecHandle& TargetRequest, UArcGunStateComponent* InGunComponent) override;
	virtual void FireStop(const FGameplayAbilitySpecHandle& TargetRequest, UArcGunStateComponent* InGunComponent) override;
	virtual bool FireUpdate(const FGameplayAbilitySpecHandle& TargetRequest, UArcGunStateComponent* InGunComponent, float DeltaTime) override;

	virtual ~FArcGunFireMode_FullAuto() override = default;
	
	int32 ShootCount;
	float ROF = 0;
	UCurveFloat* ShootScaleCurve = nullptr;
	float CurrentFirstShootDelayTime = 0;
	float ShootDelay = 0;
	float CurrentShootTime = 0;
	double StartTime = 0;
};

USTRUCT()
struct ARCGUN_API FArcGunFireMode_Single : public FArcGunFireMode
{
	GENERATED_BODY()
	
public:
	virtual bool FireStart(const FGameplayAbilitySpecHandle& TargetRequest, UArcGunStateComponent* InGunComponent) override;
	virtual void FireStop(const FGameplayAbilitySpecHandle& TargetRequest, UArcGunStateComponent* InGunComponent) override;
	virtual bool FireUpdate(const FGameplayAbilitySpecHandle& TargetRequest, UArcGunStateComponent* InGunComponent, float DeltaTime) override;

	virtual ~FArcGunFireMode_Single() override = default;
	
	int32 ShootCount;
	float ROF = 0;
	UCurveFloat* ShootScaleCurve = nullptr;
	float CurrentFirstShootDelayTime = 0;
	float ShootDelay = 0;
	float CurrentShootTime = 0;
	double StartTime = 0;
};


UCLASS()
class UArcGunFireModePreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, meta = (BaseStruct = "/Script/ArcGun.ArcGunFireMode"))
	FInstancedStruct FireMode;
};

USTRUCT()
struct FArcItemFragment_GunFireMode : public FArcItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Data", meta=(DisplayThumbnail="false"))
	TObjectPtr<UArcGunFireModePreset> FireModeAsset;
};