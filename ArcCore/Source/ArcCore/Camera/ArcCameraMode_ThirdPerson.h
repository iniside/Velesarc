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

#include "ArcCore/Camera/ArcCameraMode.h"
#include "ArcCore/Camera/ArcPenetrationAvoidanceFeeler.h"
#include "ArcStaticsBFL.h"
#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "DrawDebugHelpers.h"
#include "ArcCameraMode_ThirdPerson.generated.h"

class UCurveVector;

/**
 * UArcCameraMode_ThirdPerson
 *
 *	A basic third person camera mode.
 */
UCLASS(Abstract
	, Blueprintable)
class ARCCORE_API UArcCameraMode_ThirdPerson : public UArcCameraMode
{
	GENERATED_BODY()

public:
	UArcCameraMode_ThirdPerson();

	virtual void OnActivation() override;

	// Called when this camera mode is deactivated on the camera mode stack.
	virtual void OnDeactivation() override;

protected:
	void HandleAnimCameraCurveStart(UCurveVector* InCurve);

	void HandleAnimCameraCurveEnd(UCurveVector* InCurve);

	virtual void UpdateView(float DeltaTime) override;

	void UpdatePreventPenetration(float DeltaTime);

	void PreventCameraPenetration(const class AActor& ViewTarget
								  , const FVector& SafeLoc
								  , FVector& CameraLoc
								  , const float& DeltaTime
								  , float& DistBlockedPct
								  , bool bSingleRayOnly);

	virtual void DrawDebug(UCanvas* Canvas) const override;

protected:
	float CharacterVelocity = 0;
	FVector TargetOffset = FVector::ZeroVector;
	FDelegateHandle AnimCameraCurveStartHandle;
	FDelegateHandle AnimCameraCurveEndHandle;
	FRotator CurrentRotation;
	FVector CurrentTPPPivot;
	
	UPROPERTY(Transient)
	TObjectPtr<const UCurveVector> AnimCameraAdditive;

	// Curve that defines local-space offsets from the target using the view pitch to
	// evaluate the curve.
	UPROPERTY(EditDefaultsOnly, Category = "Third Person")
	TObjectPtr<const UCurveVector> TargetOffsetCurve;

	UPROPERTY(EditDefaultsOnly, Category = "Third Person")
	FArcCameraVelocityOffset ForwardVelocityOffset;

	UPROPERTY(EditDefaultsOnly, Category = "Third Person")
	bool bInterpolateZPivot = false;

	UPROPERTY(EditDefaultsOnly, Category = "Third Person")
	float ZPivotSmoothingTime = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "Third Person")
	bool bRotationLag = false;

	UPROPERTY(EditDefaultsOnly, Category = "Third Person")
	float CameraRotationLagSpeed = 10;
	
	FRotator PreviousDesiredRot = FRotator::ZeroRotator;
	
	double CurrentPivotZ = 0;
	EAnimCardinalDirection CardinalDirection;
	float MovingDirection = 0;
	// Penetration prevention

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	float PenetrationBlendInTime = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	float PenetrationBlendOutTime = 0.15f;

	/** If true, does collision checks to keep the camera out of the world. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	bool bPreventPenetration = true;

	/** If true, try to detect nearby walls and move the camera in anticipation.  Helps
	 * prevent popping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	bool bDoPredictiveAvoidance = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	float CollisionPushOutDistance = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	float CollisionPushHorizontalDistance = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	float CollisionPushVerticalDistance = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	float HorizontalDistance = 50.f;
	
	/** When the camera's distance is pushed into this percentage of its full distance due
	 * to penetration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	float ReportPenetrationPercent = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Collision")
	TArray<FArcPenetrationAvoidanceFeeler> PenetrationAvoidanceFeelers;

	UPROPERTY(Transient)
	float AimLineToDesiredPosBlockedPct;

	UPROPERTY(Transient)
	TArray<TObjectPtr<const AActor>> DebugActorsHitDuringCameraPenetration;

#if ENABLE_DRAW_DEBUG
	mutable float LastDrawDebugTime = -MAX_FLT;
#endif
};
