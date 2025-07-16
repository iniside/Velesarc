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

#include "ArcCore/Camera/ArcCameraMode_ThirdPerson.h"
#include "Engine/World.h"
#include "Animation/AnimInstance.h"
#include "ArcCameraComponent.h"
#include "ArcCore/Camera/ArcCameraAssistInterface.h"
#include "ArcStaticsBFL.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveVector.h"
#include "Engine/Canvas.h"
#include "GameFramework/CameraBlockingVolume.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "KismetAnimationLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace ArcxCameraMode_ThirdPerson_Statics
{
	static const FName NAME_IgnoreCameraCollision = TEXT("IgnoreCameraCollision");
}

UArcCameraMode_ThirdPerson::UArcCameraMode_ThirdPerson()
{
	TargetOffsetCurve = nullptr;

	PenetrationAvoidanceFeelers.Add(FArcPenetrationAvoidanceFeeler(FRotator(+00.0f
			, +00.0f
			, 0.0f)
		, 1.00f
		, 1.00f
		, 14.f
		, 0));
	PenetrationAvoidanceFeelers.Add(FArcPenetrationAvoidanceFeeler(FRotator(+00.0f
			, +16.0f
			, 0.0f)
		, 0.75f
		, 0.75f
		, 00.f
		, 3));
	PenetrationAvoidanceFeelers.Add(FArcPenetrationAvoidanceFeeler(FRotator(+00.0f
			, -16.0f
			, 0.0f)
		, 0.75f
		, 0.75f
		, 00.f
		, 3));
	PenetrationAvoidanceFeelers.Add(FArcPenetrationAvoidanceFeeler(FRotator(+00.0f
			, +32.0f
			, 0.0f)
		, 0.50f
		, 0.50f
		, 00.f
		, 5));
	PenetrationAvoidanceFeelers.Add(FArcPenetrationAvoidanceFeeler(FRotator(+00.0f
			, -32.0f
			, 0.0f)
		, 0.50f
		, 0.50f
		, 00.f
		, 5));
	PenetrationAvoidanceFeelers.Add(FArcPenetrationAvoidanceFeeler(FRotator(+20.0f
			, +00.0f
			, 0.0f)
		, 1.00f
		, 1.00f
		, 00.f
		, 4));
	PenetrationAvoidanceFeelers.Add(FArcPenetrationAvoidanceFeeler(FRotator(-20.0f
			, +00.0f
			, 0.0f)
		, 0.50f
		, 0.50f
		, 00.f
		, 4));
}

void UArcCameraMode_ThirdPerson::OnActivation()
{
	FVector PivotLocation = GetPivotLocation();
	UArcCameraComponent* Camera = GetArcCameraComponent();
	if (bInterpolateZPivot)
	{
		Camera->SetCurrentPivotZ(PivotLocation.Z);
	}
	View.Location = GetPivotLocation();
	AnimCameraCurveStartHandle = GetArcCameraComponent()->AnimCameraOffsetStartDelegate.AddUObject(this
		, &UArcCameraMode_ThirdPerson::HandleAnimCameraCurveStart);
	AnimCameraCurveEndHandle = GetArcCameraComponent()->AnimCameraOffsetEndDelegate.AddUObject(this
		, &UArcCameraMode_ThirdPerson::HandleAnimCameraCurveEnd);
	Super::OnActivation();
}

void UArcCameraMode_ThirdPerson::OnDeactivation()
{
	GetArcCameraComponent()->AnimCameraOffsetStartDelegate.Remove(AnimCameraCurveStartHandle);
	GetArcCameraComponent()->AnimCameraOffsetEndDelegate.Remove(AnimCameraCurveEndHandle);
	Super::OnDeactivation();
}

void UArcCameraMode_ThirdPerson::HandleAnimCameraCurveStart(UCurveVector* InCurve)
{
	AnimCameraAdditive = InCurve;
}

void UArcCameraMode_ThirdPerson::HandleAnimCameraCurveEnd(UCurveVector* InCurve)
{
	AnimCameraAdditive = nullptr;
}

void UArcCameraMode_ThirdPerson::UpdateView(float DeltaTime)
{
	FVector PivotLocation = GetPivotLocation();
	UArcCameraComponent* Camera = GetArcCameraComponent();
	if (bInterpolateZPivot)
	{
		double CurrentPivot = Camera->GetCurrentPivotZ();
		FMath::ExponentialSmoothingApprox(CurrentPivot
			, PivotLocation.Z
			, DeltaTime
			, ZPivotSmoothingTime);
		Camera->SetCurrentPivotZ(CurrentPivot);

		// double InOutRate = 1.;
		//
		// FMath::SpringDamper(CurrentPivotZ
		//	, InOutRate
		//	, PivotLocation.Z
		//	,  1.
		//	, DeltaTime
		//	, ZPivotSmoothingTime
		//	, 0.5);

		// CurrentPivotZ = FMath::FInterpTo(CurrentPivotZ, PivotLocation.Z, DeltaTime,
		// ZPivotSmoothingTime);

		PivotLocation.Z = CurrentPivot;
	}
	CurrentTPPPivot = PivotLocation;
	
	FRotator PivotRotation = FRotator::ZeroRotator;
	FRotator ControlPivotRotation = FRotator::ZeroRotator;

	PivotRotation = GetPivotRotation();
	PivotRotation.Pitch = FMath::ClampAngle(PivotRotation.Pitch
	, ViewPitchMin
	, ViewPitchMax);

	ControlPivotRotation = PivotRotation;
	
	if (bRotationLag)
	{
		PivotRotation = FRotator(FMath::QInterpTo(FQuat(PreviousDesiredRot), FQuat(PivotRotation), DeltaTime, CameraRotationLagSpeed));
	}
	PreviousDesiredRot = PivotRotation;
	View.Rotation = PivotRotation;
	View.ControlRotation = ControlPivotRotation;
	
	View.Location = PivotLocation;
	View.FieldOfView = FieldOfView;

	// Apply third person offset using pitch.

	const AActor* TargetActor = GetTargetActor();
	check(TargetActor);
	// Height adjustments for characters to account for crouching.
	const ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor);
	
	if (TargetOffsetCurve)
	{
		const FVector AdditiveOffset = AnimCameraAdditive != nullptr
									   ? AnimCameraAdditive->GetVectorValue(PivotRotation.Pitch)
									   : FVector::ZeroVector;
		TargetOffset = TargetOffsetCurve->GetVectorValue(PivotRotation.Pitch) + AdditiveOffset;
		
		if (ForwardVelocityOffset.Offset != nullptr && ForwardVelocityOffset.PitchAlpha != nullptr)
		{
			if (TargetCharacter && ForwardVelocityOffset.Offset)
			{
				MovingDirection = UKismetAnimationLibrary::CalculateDirection(TargetCharacter->GetVelocity()
					, TargetCharacter->GetActorRotation());
				CardinalDirection = UArcStaticsBFL::GetCardinalDirectionFromAngle(CardinalDirection
					, MovingDirection
					, 60.0f);

				float Alpha = ForwardVelocityOffset.PitchAlpha != nullptr ? ForwardVelocityOffset.PitchAlpha->GetFloatValue(PivotRotation.Pitch) : 1.f;

				float NewVelocity = TargetCharacter->GetVelocity().Size();

				CharacterVelocity = FMath::FInterpTo(CharacterVelocity, NewVelocity, DeltaTime, ForwardVelocityOffset.InterpolationTime);
				
				float NormalizedVelocity = FMath::GetMappedRangeValueClamped(
					TRange<float>(ForwardVelocityOffset.MinVelocity, ForwardVelocityOffset.MaxVelocity)
					, TRange<float>(0, 1)
					, CharacterVelocity);
				
				if (CardinalDirection == EAnimCardinalDirection::North)
				{
					FVector VelocityOffset = ForwardVelocityOffset.Offset->GetVectorValue(NormalizedVelocity);
					FVector FinalOffset = FMath::Lerp(FVector::ZeroVector
						, VelocityOffset
						, Alpha);

					FVector Final = TargetOffset + FinalOffset;
					TargetOffset += FinalOffset;// FMath::FInterpTo(TargetOffset.X, Final.X, DeltaTime, ForwardVelocityOffset.InterpolationTime);
				}
				else if (CardinalDirection == EAnimCardinalDirection::East)
				{
					FVector VelocityOffset = ForwardVelocityOffset.Offset->GetVectorValue(NormalizedVelocity);
					FVector FinalOffset = FMath::Lerp(FVector::ZeroVector
						, VelocityOffset
						, Alpha);
					
					VelocityOffset = TargetOffset + FinalOffset;
					TargetOffset = FMath::VInterpTo(TargetOffset, VelocityOffset, DeltaTime, ForwardVelocityOffset.InterpolationTime);
				}
				else if (CardinalDirection == EAnimCardinalDirection::West)
				{
					FVector VelocityOffset = -1 * ForwardVelocityOffset.Offset->GetVectorValue(NormalizedVelocity);
					FVector FinalOffset = FMath::Lerp(FVector::ZeroVector
						, VelocityOffset
						, Alpha);
					
					VelocityOffset = TargetOffset + FinalOffset;
					TargetOffset = FMath::VInterpTo(TargetOffset, VelocityOffset, DeltaTime, ForwardVelocityOffset.InterpolationTime);
				}
			}
		}

		FVector NewLocation = PivotLocation + View.Rotation.RotateVector(TargetOffset);
		View.Location = CurrentTPPPivot + View.Rotation.RotateVector(TargetOffset);
	}

	// Adjust final desired camera location to prevent any penetration
	UpdatePreventPenetration(DeltaTime);
}

void UArcCameraMode_ThirdPerson::DrawDebug(UCanvas* Canvas) const
{
	Super::DrawDebug(Canvas);

#if ENABLE_DRAW_DEBUG
	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
	for (int i = 0; i < DebugActorsHitDuringCameraPenetration.Num(); i++)
	{
		DisplayDebugManager.DrawString(FString::Printf(TEXT("HitActorDuringPenetration[%d]: %s")
			, i
			, *DebugActorsHitDuringCameraPenetration[i]->GetName()));
	}

	LastDrawDebugTime = GetWorld()->GetTimeSeconds();
#endif
}

void UArcCameraMode_ThirdPerson::UpdatePreventPenetration(float DeltaTime)
{
	if (!bPreventPenetration)
	{
		return;
	}

	AActor* TargetActor = GetTargetActor();

	APawn* TargetPawn = Cast<APawn>(TargetActor);
	AController* TargetController = TargetPawn ? TargetPawn->GetController() : nullptr;
	IArcCameraAssistInterface* TargetControllerAssist = Cast<IArcCameraAssistInterface>(TargetController);

	IArcCameraAssistInterface* TargetActorAssist = Cast<IArcCameraAssistInterface>(TargetActor);

	TOptional<AActor*> OptionalPPTarget = TargetActorAssist
										  ? TargetActorAssist->GetCameraPreventPenetrationTarget()
										  : TOptional<AActor*>();
	AActor* PPActor = OptionalPPTarget.IsSet() ? OptionalPPTarget.GetValue() : TargetActor;
	IArcCameraAssistInterface* PPActorAssist = OptionalPPTarget.IsSet()
											   ? Cast<IArcCameraAssistInterface>(PPActor)
											   : nullptr;

	const UPrimitiveComponent* PPActorRootComponent = Cast<UPrimitiveComponent>(PPActor->GetRootComponent());
	if (PPActorRootComponent)
	{
		// Attempt at picking SafeLocation automatically, so we reduce camera translation
		// when aiming. Our camera is our reticle, so we want to preserve our aim and keep
		// that as steady and smooth as possible. Pick closest point on capsule to our aim
		// line.
		FVector ClosestPointOnLineToCapsuleCenter;
		FVector SafeLocation = PPActor->GetActorLocation();
		FMath::PointDistToLine(SafeLocation
			, View.Rotation.Vector()
			, View.Location
			, ClosestPointOnLineToCapsuleCenter);

		// Adjust Safe distance height to be same as aim line, but within capsule.
		const float PushInDistance = PenetrationAvoidanceFeelers[0].Extent + CollisionPushOutDistance;
		const float MaxHalfHeight = PPActor->GetSimpleCollisionHalfHeight() - PushInDistance;
		SafeLocation.Z = FMath::Clamp(ClosestPointOnLineToCapsuleCenter.Z
			, SafeLocation.Z - MaxHalfHeight
			, SafeLocation.Z + MaxHalfHeight);

		float DistanceSqr;
		PPActorRootComponent->GetSquaredDistanceToCollision(ClosestPointOnLineToCapsuleCenter
			, DistanceSqr
			, SafeLocation);
		// Push back inside capsule to avoid initial penetration when doing line checks.
		if (PenetrationAvoidanceFeelers.Num() > 0)
		{
			SafeLocation += (SafeLocation - ClosestPointOnLineToCapsuleCenter).GetSafeNormal() * PushInDistance;
			SafeLocation += PPActor->GetActorUpVector() * CollisionPushVerticalDistance;
			SafeLocation += PPActor->GetActorRightVector() * CollisionPushHorizontalDistance;
		}

		// Then aim line to desired camera position
		const bool bSingleRayPenetrationCheck = !bDoPredictiveAvoidance;
		PreventCameraPenetration(*PPActor
			, SafeLocation
			, View.Location
			, DeltaTime
			, AimLineToDesiredPosBlockedPct
			, bSingleRayPenetrationCheck);

		IArcCameraAssistInterface* AssistArray[] = {TargetControllerAssist, TargetActorAssist, PPActorAssist};

		if (AimLineToDesiredPosBlockedPct < ReportPenetrationPercent)
		{
			for (IArcCameraAssistInterface* Assist : AssistArray)
			{
				if (Assist)
				{
					// camera is too close, tell the assists
					Assist->OnCameraPenetratingTarget();
				}
			}
		}
	}
}

void UArcCameraMode_ThirdPerson::PreventCameraPenetration(const class AActor& ViewTarget
														  , const FVector& SafeLoc
														  , FVector& CameraLoc
														  , const float& DeltaTime
														  , float& DistBlockedPct
														  , bool bSingleRayOnly)
{
#if ENABLE_DRAW_DEBUG
	DebugActorsHitDuringCameraPenetration.Reset();
#endif

	float HardBlockedPct = DistBlockedPct;
	float SoftBlockedPct = DistBlockedPct;

	FVector BaseRay = CameraLoc - SafeLoc;
	FRotationMatrix BaseRayMatrix(BaseRay.Rotation());
	FVector BaseRayLocalUp, BaseRayLocalFwd, BaseRayLocalRight;

	BaseRayMatrix.GetScaledAxes(BaseRayLocalFwd
		, BaseRayLocalRight
		, BaseRayLocalUp);

	float DistBlockedPctThisFrame = 1.f;

	const int32 NumRaysToShoot = bSingleRayOnly
								 ? FMath::Min(1
									 , PenetrationAvoidanceFeelers.Num())
								 : PenetrationAvoidanceFeelers.Num();
	FCollisionQueryParams SphereParams(SCENE_QUERY_STAT(CameraPen)
		, false
		, nullptr /*PlayerCamera*/);

	SphereParams.AddIgnoredActor(&ViewTarget);

	// TODO IArcCameraTarget.GetIgnoredActorsForCameraPentration();
	// if (IgnoreActorForCameraPenetration)
	//{
	//	SphereParams.AddIgnoredActor(IgnoreActorForCameraPenetration);
	// }

	FCollisionShape SphereShape = FCollisionShape::MakeSphere(0.f);
	UWorld* World = GetWorld();
	{
		const FVector RayTarget = SafeLoc + (ViewTarget.GetActorRightVector() * HorizontalDistance);
		ECollisionChannel TraceChannel = ECC_Camera;
		FHitResult Hit;
		const bool bHit = World->SweepSingleByChannel(Hit
			, ViewTarget.GetActorLocation()
			, RayTarget
			, FQuat::Identity
			, TraceChannel
			, SphereShape
			, SphereParams);
		if (bHit)
		{
			FVector NewOffset = TargetOffset;
			NewOffset.X = 0;
			NewOffset.Z = 0;

			FVector RotatedOffset = View.Rotation.RotateVector(NewOffset);
			//CameraLoc.Y *= -1;
			//TargetOffset *= -1;
			//View.Location = CurrentTPPPivot + View.Rotation.RotateVector(TargetOffset);
			return;
		}
	}
	
	for (int32 RayIdx = 0; RayIdx < NumRaysToShoot; ++RayIdx)
	{
		FArcPenetrationAvoidanceFeeler& Feeler = PenetrationAvoidanceFeelers[RayIdx];
		if (Feeler.FramesUntilNextTrace <= 0)
		{
			// calc ray target
			FVector RayTarget;
			{
				FVector RotatedRay = BaseRay.RotateAngleAxis(Feeler.AdjustmentRot.Yaw
					, BaseRayLocalUp);
				RotatedRay = RotatedRay.RotateAngleAxis(Feeler.AdjustmentRot.Pitch
					, BaseRayLocalRight);
				RayTarget = SafeLoc + RotatedRay;
			}

			// cast for world and pawn hits separately.  this is so we can safely ignore
			// the camera's target pawn
			SphereShape.Sphere.Radius = Feeler.Extent;
			ECollisionChannel TraceChannel = ECC_Camera; //(Feeler.PawnWeight > 0.f) ? ECC_Pawn : ECC_Camera;

			// do multi-line check to make sure the hits we throw out aren't
			// masking real hits behind (these are important rays).

			// MT-> passing camera as actor so that camerablockingvolumes know when it's
			// the camera doing traces
			FHitResult Hit;
			const bool bHit = World->SweepSingleByChannel(Hit
				, SafeLoc
				, RayTarget
				, FQuat::Identity
				, TraceChannel
				, SphereShape
				, SphereParams);
#if ENABLE_DRAW_DEBUG
			if (World->TimeSince(LastDrawDebugTime) < 1.f)
			{
				DrawDebugSphere(World
					, SafeLoc
					, SphereShape.Sphere.Radius
					, 8
					, FColor::Red);
				DrawDebugSphere(World
					, bHit ? Hit.Location : RayTarget
					, SphereShape.Sphere.Radius
					, 8
					, FColor::Red);
				DrawDebugLine(World
					, SafeLoc
					, bHit ? Hit.Location : RayTarget
					, FColor::Red);
			}
#endif // ENABLE_DRAW_DEBUG

			Feeler.FramesUntilNextTrace = Feeler.TraceInterval;

			const AActor* HitActor = Hit.GetActor();

			if (bHit && HitActor)
			{
				bool bIgnoreHit = false;

				if (HitActor->ActorHasTag(ArcxCameraMode_ThirdPerson_Statics::NAME_IgnoreCameraCollision))
				{
					bIgnoreHit = true;
					SphereParams.AddIgnoredActor(HitActor);
				}

				// Ignore CameraBlockingVolume hits that occur in front of the ViewTarget.
				if (!bIgnoreHit && HitActor->IsA<ACameraBlockingVolume>())
				{
					const FVector ViewTargetForwardXY = ViewTarget.GetActorForwardVector().GetSafeNormal2D();
					const FVector ViewTargetLocation = ViewTarget.GetActorLocation();
					const FVector HitOffset = Hit.Location - ViewTargetLocation;
					const FVector HitDirectionXY = HitOffset.GetSafeNormal2D();
					const float DotHitDirection = FVector::DotProduct(ViewTargetForwardXY
						, HitDirectionXY);
					if (DotHitDirection > 0.0f)
					{
						bIgnoreHit = true;
						// Ignore this CameraBlockingVolume on the remaining sweeps.
						SphereParams.AddIgnoredActor(HitActor);
					}
					else
					{
#if ENABLE_DRAW_DEBUG
						DebugActorsHitDuringCameraPenetration.AddUnique(TObjectPtr<const AActor>(HitActor));
#endif
					}
				}

				if (!bIgnoreHit)
				{
					const float Weight = Cast<APawn>(Hit.GetActor()) ? Feeler.PawnWeight : Feeler.WorldWeight;
					float NewBlockPct = Hit.Time;
					NewBlockPct += (1.f - NewBlockPct) * (1.f - Weight);

					// Recompute blocked pct taking into account pushout distance.
					NewBlockPct = ((Hit.Location - SafeLoc).Size() - CollisionPushOutDistance) / (RayTarget - SafeLoc).
								  Size();
					DistBlockedPctThisFrame = FMath::Min(NewBlockPct
						, DistBlockedPctThisFrame);

					// This feeler got a hit, so do another trace next frame
					Feeler.FramesUntilNextTrace = 0;

#if ENABLE_DRAW_DEBUG
					DebugActorsHitDuringCameraPenetration.AddUnique(TObjectPtr<const AActor>(HitActor));
#endif
				}
			}

			if (RayIdx == 0)
			{
				// don't interpolate toward this one, snap to it
				// assumes ray 0 is the center/main ray
				HardBlockedPct = DistBlockedPctThisFrame;
			}
			else
			{
				SoftBlockedPct = DistBlockedPctThisFrame;
			}
		}
		else
		{
			--Feeler.FramesUntilNextTrace;
		}
	}

	if (bResetInterpolation)
	{
		DistBlockedPct = DistBlockedPctThisFrame;
	}
	else if (DistBlockedPct < DistBlockedPctThisFrame)
	{
		// interpolate smoothly out
		if (PenetrationBlendOutTime > DeltaTime)
		{
			DistBlockedPct = DistBlockedPct + DeltaTime / PenetrationBlendOutTime * (
								 DistBlockedPctThisFrame - DistBlockedPct);
		}
		else
		{
			DistBlockedPct = DistBlockedPctThisFrame;
		}
	}
	else
	{
		if (DistBlockedPct > HardBlockedPct)
		{
			DistBlockedPct = HardBlockedPct;
		}
		else if (DistBlockedPct > SoftBlockedPct)
		{
			// interpolate smoothly in
			if (PenetrationBlendInTime > DeltaTime)
			{
				DistBlockedPct = DistBlockedPct - DeltaTime / PenetrationBlendInTime * (
									 DistBlockedPct - SoftBlockedPct);
			}
			else
			{
				DistBlockedPct = SoftBlockedPct;
			}
		}
	}

	DistBlockedPct = FMath::Clamp<float>(DistBlockedPct
		, 0.f
		, 1.f);
	if (DistBlockedPct < (1.f - ZERO_ANIMWEIGHT_THRESH))
	{
		CameraLoc = SafeLoc + (CameraLoc - SafeLoc) * DistBlockedPct;
	}
}
