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

#include "Camera/PlayerCameraManager.h"

#include "ArcPlayerCameraManager.generated.h"

constexpr float OAK_CAMERA_DEFAULT_FOV(90.0f);
constexpr float OAK_CAMERA_DEFAULT_PITCH_MIN(-70.0f);
constexpr float OAK_CAMERA_DEFAULT_PITCH_MAX(60.0f);

class UArcUICameraManagerComponent;

/**
 * AArcPlayerCameraManager
 *
 *	The base player camera manager class used by this project.
 */
UCLASS(notplaceable)
class ARCCORE_API AArcPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	AArcPlayerCameraManager(const FObjectInitializer& ObjectInitializer);

	UArcUICameraManagerComponent* GetUICameraComponent() const;

protected:
	virtual void UpdateViewTarget(FTViewTarget& OutVT
								  , float DeltaTime) override;

	virtual void DisplayDebug(UCanvas* Canvas
							  , const FDebugDisplayInfo& DebugDisplay
							  , float& YL
							  , float& YPos) override;

private:
	/** The UI Camera Component, controls the camera when UI is doing something important
	 * that gameplay doesn't get priority over. */
	UPROPERTY(Transient)
	TObjectPtr<UArcUICameraManagerComponent> UICamera;
};