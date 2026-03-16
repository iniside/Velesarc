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


#include "CommonLazyImage.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Blueprint/UserWidget.h"
#include "GameplayAbilitySpecHandle.h"
#include "Components/Image.h"
#include "Types/TargetingSystemTypes.h"

#include "ArcReticleWidget.generated.h"

class UGameplayAbility;
class UArcTargetingObject;
class UTargetingPreset;

struct FArcTargetRequest;
struct FGameplayAbilityTargetDataHandle;
struct FArcSelectedGun;
struct FArcGunShotInfo;

/**
 * 
 */
UCLASS()
class ARCFRONTEND_API UArcReticleWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	FDelegateHandle WeaponSpreadDelegate;
	FDelegateHandle WeaponEquippedDelegate;

	FDelegateHandle WeaponSwaydDelegate;
	
	FDelegateHandle GunStateComponentBeginPlayDelegate;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UTargetingPreset> TargetingPreset;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Appearance, meta = (ClampMin = 0.0))
	float BaseRadius = 48.0f;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	void HandleOnTargetingReady(FTargetingRequestHandle Handle);

	UFUNCTION(BlueprintImplementableEvent)
	void OnTargetHitReady(const FHitResult& OutHit);
	
	void ComputeSpreadAngle(float CurrentSpread, float MaxSpread);

	void ComputeSwayAngle(float HorizontalSway , float MaxHorizontalSway,  float VerticalSway, float MaxVerticalSway);

	void ComputeSwayAngleImpl(float Sway , float MaxSway);
	
	/** Returns the current weapon's maximum spread radius in screenspace units (pixels) */
	UFUNCTION(BlueprintImplementableEvent)
	void OnSpreadChanged(float CurrentAngle);

	UFUNCTION(BlueprintImplementableEvent)
	void OnSwayChanged(float CurrentSway);
	
	//void HandleOnWeaponFireHit(UGameplayAbility* InAbility
	//						   , const FArcSelectedGun& SelectedGun
	//						   , const FGameplayAbilityTargetDataHandle& InHits
	//						   , UArcTargetingObject* InTrace
	//						   , const FGameplayAbilitySpecHandle& RequestHandle);

	void HandleOnWeaponFireHit(const FArcGunShotInfo& ShotInfo);
	
	UFUNCTION(BlueprintImplementableEvent)
	void OnWeaponFireHit(const FVector& WorldLocation);

	void HandleGunStateComponentBeginPlay(UActorComponent* InComponent);
};

UCLASS()
class ARCFRONTEND_API UArcImage : public UCommonLazyImage
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Appearance")
	virtual void ArcSetBrushFromSoftTexture(TSoftObjectPtr<UTexture2D> SoftTexture);

	UFUNCTION(BlueprintCallable, Category="Appearance")
	virtual void SetBrushFromSoftObject(TSoftObjectPtr<UObject> SoftObject);
};
