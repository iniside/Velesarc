/**
 * This file is part of ArcX.
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

#include "CoreMinimal.h"
#include "ArcCore/Camera/ArcCameraMode_ThirdPerson.h"
#include "Curves/CurveFloat.h"
#include "ArcCore/Camera/ArcPenetrationAvoidanceFeeler.h"
#include "DrawDebugHelpers.h"
#include "ArcCameraMode_ThirdPersonWeapon.generated.h"

class UCurveVector;

/**
 * UArcCameraMode_ThirdPerson
 *
 *	A basic third person camera mode.
 */
UCLASS(Abstract, Blueprintable)
class ARCGUN_API UArcCameraMode_ThirdPersonWeapon : public UArcCameraMode_ThirdPerson
{
	GENERATED_BODY()
protected:

	TWeakObjectPtr<class UArcGunStateComponent> GunComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TObjectPtr<const UCurveFloat> ShootCountToDistance;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float ShootCountToDistanceInterpolation = 20;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TObjectPtr<const UCurveVector> ShootCountToOffset;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float ShootCountToOffsetInterpolation = 20;

	float CurrentDistance = 0;
	FVector CurrentRotation = FVector::ZeroVector;

	virtual void UpdateView(float DeltaTime) override;
public:
	virtual void OnActivation() override;
};
