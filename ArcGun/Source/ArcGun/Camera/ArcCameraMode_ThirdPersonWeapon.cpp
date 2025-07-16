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

#include "Camera/ArcCameraMode_ThirdPersonWeapon.h"

#include "ArcGunStateComponent.h"
#include "Curves/CurveVector.h"
#include "Engine/Canvas.h"
#include "GameFramework/CameraBlockingVolume.h"
#include "GameFramework/Character.h"
#include "ArcCore/Camera/ArcCameraAssistInterface.h"
#include "ArcCore/Camera/ArcCameraComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"

void UArcCameraMode_ThirdPersonWeapon::UpdateView(float DeltaTime)
{
	FVector PivotLocation = GetPivotLocation();
	FRotator PivotRotation = GetPivotRotation();

	PivotRotation.Pitch = FMath::ClampAngle(PivotRotation.Pitch, ViewPitchMin, ViewPitchMax);

	View.Location = PivotLocation;
	View.Rotation = PivotRotation;
	View.ControlRotation = View.Rotation;
	View.FieldOfView = FieldOfView;

	// Apply third person offset using pitch.
	
	if (TargetOffsetCurve)
	{
		const FVector AdditiveOffset = AnimCameraAdditive != nullptr ? AnimCameraAdditive->GetVectorValue(PivotRotation.Pitch) : FVector::ZeroVector;
		TargetOffset = TargetOffsetCurve->GetVectorValue(PivotRotation.Pitch) + AdditiveOffset;

		if (GunComponent.IsValid())
		{
			float Value = ShootCountToDistance->GetFloatValue(GunComponent->GetShootCount());
			CurrentDistance = FMath::FInterpConstantTo(CurrentDistance, Value, DeltaTime, ShootCountToDistanceInterpolation);
			TargetOffset.X += CurrentDistance;

			FVector RotationVector = ShootCountToOffset->GetVectorValue(GunComponent->GetShootCount());
			CurrentRotation = FMath::VInterpConstantTo(CurrentRotation, RotationVector, DeltaTime, ShootCountToOffsetInterpolation);
			View.Rotation.Roll += CurrentRotation.Z;
			TargetOffset.X += CurrentRotation.X;
		}

		View.Location = PivotLocation + PivotRotation.RotateVector(TargetOffset);
	}
	
	// Adjust final desired camera location to prevent any penetration
	UpdatePreventPenetration(DeltaTime);
}

void UArcCameraMode_ThirdPersonWeapon::OnActivation()
{
	Super::OnActivation();
	ACharacter* ArcCharacter = Cast<ACharacter>(GetArcCameraComponent()->GetOwner());
	APlayerState* ArcPS = ArcCharacter->GetPlayerState();

	GunComponent = UArcGunStateComponent::FindGunStateComponent(ArcPS);
}
