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

#include "ArcCore/Camera/ArcUICameraManagerComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"

#include "ArcCore/Camera/ArcPlayerCameraManager.h"

UArcUICameraManagerComponent* UArcUICameraManagerComponent::GetComponent(APlayerController* PC)
{
	if (PC != nullptr)
	{
		if (AArcPlayerCameraManager* PCCamera = Cast<AArcPlayerCameraManager>(PC->PlayerCameraManager))
		{
			return PCCamera->GetUICameraComponent();
		}
	}

	return nullptr;
}

UArcUICameraManagerComponent::UArcUICameraManagerComponent()
{
	bWantsInitializeComponent = true;

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		// Register "showdebug" hook.
		if (!IsRunningDedicatedServer())
		{
			AHUD::OnShowDebugInfo.AddUObject(this
				, &ThisClass::OnShowDebugInfo);
		}
	}
}

void UArcUICameraManagerComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void UArcUICameraManagerComponent::SetViewTarget(AActor* InViewTarget
												 , FViewTargetTransitionParams TransitionParams)
{
	TGuardValue<bool> UpdatingViewTargetGuard(bUpdatingViewTarget
		, true);

	ViewTarget = InViewTarget;
	CastChecked<AArcPlayerCameraManager>(GetOwner())->SetViewTarget(ViewTarget
		, TransitionParams);
}

bool UArcUICameraManagerComponent::NeedsToUpdateViewTarget() const
{
	return false;
}

void UArcUICameraManagerComponent::UpdateViewTarget(struct FTViewTarget& OutVT
													, float DeltaTime)
{
}

void UArcUICameraManagerComponent::OnShowDebugInfo(AHUD* HUD
												   , UCanvas* Canvas
												   , const FDebugDisplayInfo& DisplayInfo
												   , float& YL
												   , float& YPos)
{
}
