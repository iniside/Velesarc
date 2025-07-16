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

#include "Camera/PlayerCameraManager.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "ArcUICameraManagerComponent.generated.h"

class APlayerController;
class AArcPlayerCameraManager;
class AHUD;
class UCanvas;

UCLASS(Transient
	, Within = ArcPlayerCameraManager)
class ARCCORE_API UArcUICameraManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	static UArcUICameraManagerComponent* GetComponent(APlayerController* PC);

public:
	UArcUICameraManagerComponent();

	virtual void InitializeComponent() override;

	bool IsSettingViewTarget() const
	{
		return bUpdatingViewTarget;
	}

	AActor* GetViewTarget() const
	{
		return ViewTarget;
	}

	void SetViewTarget(AActor* InViewTarget
					   , FViewTargetTransitionParams TransitionParams = FViewTargetTransitionParams());

	bool NeedsToUpdateViewTarget() const;

	void UpdateViewTarget(struct FTViewTarget& OutVT
						  , float DeltaTime);

	void OnShowDebugInfo(AHUD* HUD
						 , UCanvas* Canvas
						 , const FDebugDisplayInfo& DisplayInfo
						 , float& YL
						 , float& YPos);

private:
	UPROPERTY(Transient)
	TObjectPtr<AActor> ViewTarget;

	UPROPERTY(Transient)
	bool bUpdatingViewTarget;
};
