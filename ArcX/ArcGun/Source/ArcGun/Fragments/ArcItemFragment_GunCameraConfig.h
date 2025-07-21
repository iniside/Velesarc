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


#include "Items/Fragments/ArcItemFragment.h"

#include "ArcItemFragment_GunCameraConfig.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Gun Camera Config"))
struct ARCGUN_API FArcItemFragment_GunCameraConfig : public FArcItemFragment
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Constant Attributes|Camera")
	float SpringArmModifier;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Constant Attributes|Camera")
	float MaxSpringArmModifier;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Constant Attributes|Camera")
	float SpringArmInterpolationSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Constant Attributes|Camera")
	float FOVModifier;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Constant Attributes|Camera")
	float MaxFOVModifier;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Constant Attributes|Camera")
	float CameraInterpolationSpeed;

	FArcItemFragment_GunCameraConfig()
		: SpringArmModifier(0)
		, MaxSpringArmModifier(0)
		, SpringArmInterpolationSpeed(0)
		, FOVModifier(0)
		, MaxFOVModifier(0)
		, CameraInterpolationSpeed(0)
	{}

	virtual ~FArcItemFragment_GunCameraConfig() override = default;
};