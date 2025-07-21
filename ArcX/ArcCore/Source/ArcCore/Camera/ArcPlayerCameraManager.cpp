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

#include "ArcCore/Camera/ArcPlayerCameraManager.h"

#include "EnhancedInputComponent.h"
#include "ArcCore/Camera/ArcUICameraManagerComponent.h"
#include "Build/CameraObjectBuildContext.h"
#include "Components/SkeletalMeshComponent.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "Nodes/Input/InputAxisBinding2DCameraNode.h"

static FName UICameraComponentName(TEXT("UICamera"));

AArcPlayerCameraManager::AArcPlayerCameraManager(const FObjectInitializer& ObjectInitializer)
	: Super{ObjectInitializer}
{
	DefaultFOV = 90.f;
	ViewPitchMin = -70.f;
	ViewPitchMax = 60.f;

	UICamera = CreateDefaultSubobject<UArcUICameraManagerComponent>(UICameraComponentName);
}

UArcUICameraManagerComponent* AArcPlayerCameraManager::GetUICameraComponent() const
{
	return UICamera;
}

void AArcPlayerCameraManager::UpdateViewTarget(FTViewTarget& OutVT
											   , float DeltaTime)
{
	// If the UI Camera is looking at something, let it have priority.
	if (UICamera->NeedsToUpdateViewTarget())
	{
		Super::UpdateViewTarget(OutVT, DeltaTime);
		UICamera->UpdateViewTarget(OutVT, DeltaTime);
		
		return;
	}
		
	Super::UpdateViewTarget(OutVT, DeltaTime);
}

void AArcPlayerCameraManager::DisplayDebug(UCanvas* Canvas
										   , const FDebugDisplayInfo& DebugDisplay
										   , float& YL
										   , float& YPos)
{
	check(Canvas);

	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;

	DisplayDebugManager.SetFont(GEngine->GetSmallFont());
	DisplayDebugManager.SetDrawColor(FColor::Yellow);
	DisplayDebugManager.DrawString(FString::Printf(TEXT("ArcPlayerCameraManager: %s")
		, *GetNameSafe(this)));

	Super::DisplayDebug(Canvas
		, DebugDisplay
		, YL
		, YPos);
}