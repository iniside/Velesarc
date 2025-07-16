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

#include "ArcCore/Camera/ArcCameraMode.h"
#include "Animation/AnimInstance.h"
#include "ArcCore/Camera/ArcCameraComponent.h"
#include "ArcCore/Camera/ArcPlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/Canvas.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"

//////////////////////////////////////////////////////////////////////////
// FArcCameraModeView
//////////////////////////////////////////////////////////////////////////
FArcCameraModeView::FArcCameraModeView()
	: Location(ForceInit)
	, Rotation(ForceInit)
	, ControlRotation(ForceInit)
	, FieldOfView(OAK_CAMERA_DEFAULT_FOV)
{
}

void FArcCameraModeView::Blend(const FArcCameraModeView& Other
							   , float OtherWeight)
{
	if (OtherWeight <= 0.0f)
	{
		return;
	}
	else if (OtherWeight >= 1.0f)
	{
		*this = Other;
		return;
	}

	Location = FMath::Lerp(Location
		, Other.Location
		, OtherWeight);

	const FRotator DeltaRotation = (Other.Rotation - Rotation).GetNormalized();
	Rotation = Rotation + (OtherWeight * DeltaRotation);

	const FRotator DeltaControlRotation = (Other.ControlRotation - ControlRotation).GetNormalized();
	ControlRotation = ControlRotation + (OtherWeight * DeltaControlRotation);

	FieldOfView = FMath::Lerp(FieldOfView
		, Other.FieldOfView
		, OtherWeight);
}

//////////////////////////////////////////////////////////////////////////
// UArcCameraMode
//////////////////////////////////////////////////////////////////////////
UArcCameraMode::UArcCameraMode()
{
	FieldOfView = OAK_CAMERA_DEFAULT_FOV;
	ViewPitchMin = OAK_CAMERA_DEFAULT_PITCH_MIN;
	ViewPitchMax = OAK_CAMERA_DEFAULT_PITCH_MAX;

	BlendTime = 0.5f;
	BlendFunction = EArcCameraModeBlendFunction::EaseOut;
	BlendExponent = 4.0f;
	BlendAlpha = 1.0f;
	BlendWeight = 1.0f;
}

UArcCameraComponent* UArcCameraMode::GetArcCameraComponent() const
{
	return CastChecked<UArcCameraComponent>(GetOuter());
}

UWorld* UArcCameraMode::GetWorld() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? nullptr : GetOuter()->GetWorld();
}

AActor* UArcCameraMode::GetTargetActor() const
{
	const UArcCameraComponent* ArcCameraComponent = GetArcCameraComponent();

	return ArcCameraComponent->GetTargetActor();
}

FVector UArcCameraMode::GetPivotLocation() const
{
	const AActor* TargetActor = GetTargetActor();
	check(TargetActor);

	if (const APawn* TargetPawn = Cast<APawn>(TargetActor))
	{
		// Height adjustments for characters to account for crouching.
		if (const ACharacter* TargetCharacter = Cast<ACharacter>(TargetPawn))
		{
			USkeletalMeshComponent* SMC = TargetCharacter->GetMesh();
			float CameraPivotZAdditive = 0.0f;
			if (SMC->GetAnimInstance() != nullptr)
			{
				SMC->GetAnimInstance()->GetCurveValue("CameraPivotHeightAdditive");
			}

			UArcCameraComponent* Camera = GetArcCameraComponent();
			FName CurrentPivotSocket = Camera->GetPivotSocket();
			if (SocketPivot.IsNone() == false)
			{
				if (CurrentPivotSocket != SocketPivot)
				{
					Camera->SetPivotSocket(SocketPivot);
					FTransform SocketTransform = Camera->GetPivotSocketTransform();
					FVector Loc = SocketTransform.GetLocation();
					Loc.Z += TargetPawn->BaseEyeHeight;
					
					return Loc;
				}
				else
				{
					FTransform SocketTransform = Camera->GetPivotSocketTransform();
					FVector Loc = SocketTransform.GetLocation();
					Loc.Z += TargetPawn->BaseEyeHeight;
					Camera->SetPivotSocketTransform(SocketTransform);
				
					return Loc;// TargetCharacter->GetActorLocation() + Loc;
				}
			}
			const ACharacter* TargetCharacterCDO = TargetCharacter->GetClass()->GetDefaultObject<ACharacter>();
			check(TargetCharacterCDO);

			const UCapsuleComponent* CapsuleComp = TargetCharacter->GetCapsuleComponent();
			check(CapsuleComp);

			const UCapsuleComponent* CapsuleCompCDO = TargetCharacterCDO->GetCapsuleComponent();
			check(CapsuleCompCDO);

			const float DefaultHalfHeight = CapsuleCompCDO->GetUnscaledCapsuleHalfHeight();
			float ActualHalfHeight = CapsuleComp->GetUnscaledCapsuleHalfHeight();
			float HeightAdjustment = (DefaultHalfHeight - ActualHalfHeight) + TargetCharacterCDO->BaseEyeHeight +
									 CameraPivotZAdditive;
			
			if (VelocityPivotOffset)
			{
				FVector V = TargetCharacter->GetVelocity();
				float VM = V.Size();

				float Offset = VelocityPivotOffset->GetFloatValue(VM);

				HeightAdjustment += Offset;
			}
			return TargetCharacter->GetActorLocation() + (FVector::UpVector * HeightAdjustment);
		}

		return TargetPawn->GetPawnViewLocation();
	}

	return TargetActor->GetActorLocation();
}

FRotator UArcCameraMode::GetPivotRotation() const
{
	const AActor* TargetActor = GetTargetActor();
	check(TargetActor);

	if (const APawn* TargetPawn = Cast<APawn>(TargetActor))
	{
		return TargetPawn->GetViewRotation();
	}

	return TargetActor->GetActorRotation();
}

void UArcCameraMode::UpdateCameraMode(float DeltaTime)
{
	UpdateView(DeltaTime);
	UpdateBlending(DeltaTime);
}

float UArcCameraMode::GetBlendTime() const
{
	switch (BlendTimeType)
	{
		case EArcBlendTimeMode::Direct:
			return BlendTime;
		case EArcBlendTimeMode::Custom:
			return BlendTimeCustom.GetPtr<FArcCameraModeBlendTime>()->GetBlendTime(this);
		default: ;
	}
	return BlendTime;
}

void UArcCameraMode::UpdateView(float DeltaTime)
{
	FVector PivotLocation = GetPivotLocation();
	FRotator PivotRotation = GetPivotRotation();

	PivotRotation.Pitch = FMath::ClampAngle(PivotRotation.Pitch
		, ViewPitchMin
		, ViewPitchMax);

	View.Location = PivotLocation;
	View.Rotation = PivotRotation;
	View.ControlRotation = View.Rotation;
	View.FieldOfView = FieldOfView;
}

void UArcCameraMode::SetBlendWeight(float Weight)
{
	BlendWeight = FMath::Clamp(Weight
		, 0.0f
		, 1.0f);

	// Since we're setting the blend weight directly, we need to calculate the blend alpha
	// to account for the blend function.
	const float InvExponent = (BlendExponent > 0.0f) ? (1.0f / BlendExponent) : 1.0f;

	switch (BlendFunction)
	{
		case EArcCameraModeBlendFunction::Linear:
			BlendAlpha = BlendWeight;
			break;

		case EArcCameraModeBlendFunction::EaseIn:
			BlendAlpha = FMath::InterpEaseIn(0.0f
				, 1.0f
				, BlendWeight
				, InvExponent);
			break;

		case EArcCameraModeBlendFunction::EaseOut:
			BlendAlpha = FMath::InterpEaseOut(0.0f
				, 1.0f
				, BlendWeight
				, InvExponent);
			break;

		case EArcCameraModeBlendFunction::EaseInOut:
			BlendAlpha = FMath::InterpEaseInOut(0.0f
				, 1.0f
				, BlendWeight
				, InvExponent);
			break;
		case EArcCameraModeBlendFunction::SinIn:
			BlendAlpha = FMath::InterpSinIn(0.0f
				, 1.0f
				, BlendWeight);
			break;
		case EArcCameraModeBlendFunction::SinOut:
			BlendAlpha = FMath::InterpSinOut(0.0f
				, 1.0f
				, BlendWeight);
			break;
		case EArcCameraModeBlendFunction::SinInOut:
			BlendAlpha = FMath::InterpSinInOut(0.0f
				, 1.0f
				, BlendWeight);
			break;
		case EArcCameraModeBlendFunction::ExpoIn:
			BlendAlpha = FMath::InterpExpoIn(0.0f
				, 1.0f
				, BlendWeight);
			break;
		case EArcCameraModeBlendFunction::ExpoOut:
			BlendAlpha = FMath::InterpExpoOut(0.0f
				, 1.0f
				, BlendWeight);
			break;
		case EArcCameraModeBlendFunction::ExpoInOut:
			BlendAlpha = FMath::InterpExpoInOut(0.0f
				, 1.0f
				, BlendWeight);
			break;
		case EArcCameraModeBlendFunction::CircularIn:
			BlendAlpha = FMath::InterpCircularIn(0.0f
				, 1.0f
				, BlendWeight);
			break;
		case EArcCameraModeBlendFunction::CircularOur:
			BlendAlpha = FMath::InterpCircularOut(0.0f
				, 1.0f
				, BlendWeight);
			break;
		case EArcCameraModeBlendFunction::CircularInOut:
			BlendAlpha = FMath::InterpCircularInOut(0.0f
				, 1.0f
				, BlendWeight);
			break;
		default: checkf(false
				, TEXT("SetBlendWeight: Invalid BlendFunction [%d]\n")
				, (uint8)BlendFunction);
			break;
	}
}

void UArcCameraMode::UpdateBlending(float DeltaTime)
{
	const float BlendTimeLocal = GetBlendTime();
	if (BlendTimeLocal > 0.0f)
	{
		BlendAlpha += (DeltaTime / BlendTimeLocal);
		BlendAlpha = FMath::Min(BlendAlpha
			, 1.0f);
	}
	else
	{
		BlendAlpha = 1.0f;
	}

	const float Exponent = (BlendExponent > 0.0f) ? BlendExponent : 1.0f;

	switch (BlendFunction)
	{
		case EArcCameraModeBlendFunction::Linear:
			BlendWeight = BlendAlpha;
			break;

		case EArcCameraModeBlendFunction::EaseIn:
			BlendWeight = FMath::InterpEaseIn(0.0f
				, 1.0f
				, BlendAlpha
				, Exponent);
			break;

		case EArcCameraModeBlendFunction::EaseOut:
			BlendWeight = FMath::InterpEaseOut(0.0f
				, 1.0f
				, BlendAlpha
				, Exponent);
			break;

		case EArcCameraModeBlendFunction::EaseInOut:
			BlendWeight = FMath::InterpEaseInOut(0.0f
				, 1.0f
				, BlendAlpha
				, Exponent);
			break;
		case EArcCameraModeBlendFunction::SinIn:
			BlendWeight = FMath::InterpSinIn(0.0f
				, 1.0f
				, BlendAlpha);
			break;
		case EArcCameraModeBlendFunction::SinOut:
			BlendWeight = FMath::InterpSinOut(0.0f
				, 1.0f
				, BlendAlpha);
			break;
		case EArcCameraModeBlendFunction::SinInOut:
			BlendWeight = FMath::InterpSinInOut(0.0f
				, 1.0f
				, BlendAlpha);
			break;
		case EArcCameraModeBlendFunction::ExpoIn:
			BlendWeight = FMath::InterpExpoIn(0.0f
				, 1.0f
				, BlendAlpha);
			break;
		case EArcCameraModeBlendFunction::ExpoOut:
			BlendWeight = FMath::InterpExpoOut(0.0f
				, 1.0f
				, BlendAlpha);
			break;
		case EArcCameraModeBlendFunction::ExpoInOut:
			BlendWeight = FMath::InterpExpoInOut(0.0f
				, 1.0f
				, BlendAlpha);
			break;
		case EArcCameraModeBlendFunction::CircularIn:
			BlendWeight = FMath::InterpCircularIn(0.0f
				, 1.0f
				, BlendAlpha);
			break;
		case EArcCameraModeBlendFunction::CircularOur:
			BlendWeight = FMath::InterpCircularOut(0.0f
				, 1.0f
				, BlendAlpha);
			break;
		case EArcCameraModeBlendFunction::CircularInOut:
			BlendWeight = FMath::InterpCircularInOut(0.0f
				, 1.0f
				, BlendAlpha);
			break;
		default: checkf(false
				, TEXT("UpdateBlending: Invalid BlendFunction [%d]\n")
				, (uint8)BlendFunction);
			break;
	}
}

void UArcCameraMode::DrawDebug(UCanvas* Canvas) const
{
	check(Canvas);

	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;

	DisplayDebugManager.SetDrawColor(FColor::White);
	DisplayDebugManager.DrawString(FString::Printf(TEXT("      ArcCameraMode: %s (%f)")
		, *GetName()
		, BlendWeight));
}

//////////////////////////////////////////////////////////////////////////
// UArcCameraModeStack
//////////////////////////////////////////////////////////////////////////
UArcCameraModeStack::UArcCameraModeStack()
{
	bIsActive = true;
}

void UArcCameraModeStack::ActivateStack()
{
	if (!bIsActive)
	{
		bIsActive = true;

		// Notify camera modes that they are being activated.
		for (UArcCameraMode* CameraMode : CameraModeStack)
		{
			check(CameraMode);
			CameraMode->OnActivation();
		}
	}
}

void UArcCameraModeStack::DeactivateStack()
{
	if (bIsActive)
	{
		bIsActive = false;

		// Notify camera modes that they are being deactivated.
		for (UArcCameraMode* CameraMode : CameraModeStack)
		{
			check(CameraMode);
			CameraMode->OnDeactivation();
		}
	}
}

void UArcCameraModeStack::PushCameraMode(UClass* CameraModeClass)
{
	if (!CameraModeClass)
	{
		return;
	}

	UArcCameraMode* CameraMode = GetCameraModeInstance(CameraModeClass);
	check(CameraMode);

	int32 StackSize = CameraModeStack.Num();

	if ((StackSize > 0) && (CameraModeStack[0] == CameraMode))
	{
		// Already top of stack.
		return;
	}

	// See if it's already in the stack and remove it.
	// Figure out how much it was contributing to the stack.
	int32 ExistingStackIndex = INDEX_NONE;
	float ExistingStackContribution = 1.0f;

	for (int32 StackIndex = 0; StackIndex < StackSize; ++StackIndex)
	{
		if (CameraModeStack[StackIndex] == CameraMode)
		{
			ExistingStackIndex = StackIndex;
			ExistingStackContribution *= CameraMode->GetBlendWeight();
			break;
		}
		else
		{
			ExistingStackContribution *= (1.0f - CameraModeStack[StackIndex]->GetBlendWeight());
		}
	}

	if (ExistingStackIndex != INDEX_NONE)
	{
		CameraModeStack.RemoveAt(ExistingStackIndex);
		StackSize--;
	}
	else
	{
		ExistingStackContribution = 0.0f;
	}

	// Decide what initial weight to start with.
	const bool bShouldBlend = ((CameraMode->GetBlendTime() > 0.0f) && (StackSize > 0));
	const float BlendWeight = (bShouldBlend ? ExistingStackContribution : 1.0f);

	CameraMode->SetBlendWeight(BlendWeight);

	// Add new entry to top of stack.
	CameraModeStack.Insert(CameraMode
		, 0);

	// Make sure stack bottom is always weighted 100%.
	CameraModeStack.Last()->SetBlendWeight(1.0f);

	// Let the camera mode know if it's being added to the stack.
	if (ExistingStackIndex == INDEX_NONE)
	{
		CameraMode->OnActivation();
	}
}

bool UArcCameraModeStack::EvaluateStack(float DeltaTime
										, FArcCameraModeView& OutCameraModeView)
{
	if (!bIsActive)
	{
		return false;
	}

	UpdateStack(DeltaTime);
	BlendStack(OutCameraModeView);

	return true;
}

UArcCameraMode* UArcCameraModeStack::GetCameraModeInstance(UClass* CameraModeClass)
{
	check(CameraModeClass);

	// First see if we already created one.
	for (UArcCameraMode* CameraMode : CameraModeInstances)
	{
		if ((CameraMode != nullptr) && (CameraMode->GetClass() == CameraModeClass))
		{
			return CameraMode;
		}
	}

	// Not found, so we need to create it.
	UArcCameraMode* NewCameraMode = NewObject<UArcCameraMode>(GetOuter()
		, CameraModeClass
		, NAME_None
		, RF_NoFlags);
	check(NewCameraMode);

	CameraModeInstances.Add(NewCameraMode);

	return NewCameraMode;
}

void UArcCameraModeStack::UpdateStack(float DeltaTime)
{
	const int32 StackSize = CameraModeStack.Num();
	if (StackSize <= 0)
	{
		return;
	}

	int32 RemoveCount = 0;
	int32 RemoveIndex = INDEX_NONE;

	for (int32 StackIndex = 0; StackIndex < StackSize; ++StackIndex)
	{
		UArcCameraMode* CameraMode = CameraModeStack[StackIndex];
		check(CameraMode);

		CameraMode->UpdateCameraMode(DeltaTime);

		if (CameraMode->GetBlendWeight() >= 1.0f)
		{
			// Everything below this mode is now irrelevant and can be removed.
			RemoveIndex = (StackIndex + 1);
			RemoveCount = (StackSize - RemoveIndex);
			break;
		}
	}

	if (RemoveCount > 0)
	{
		// Let the camera modes know they being removed from the stack.
		for (int32 StackIndex = RemoveIndex; StackIndex < StackSize; ++StackIndex)
		{
			UArcCameraMode* CameraMode = CameraModeStack[StackIndex];
			check(CameraMode);

			CameraMode->OnDeactivation();
		}

		CameraModeStack.RemoveAt(RemoveIndex
			, RemoveCount);
	}
}

void UArcCameraModeStack::BlendStack(FArcCameraModeView& OutCameraModeView) const
{
	const int32 StackSize = CameraModeStack.Num();
	if (StackSize <= 0)
	{
		return;
	}

	// Start at the bottom and blend up the stack
	const UArcCameraMode* CameraMode = CameraModeStack[StackSize - 1];
	check(CameraMode);

	OutCameraModeView = CameraMode->GetCameraModeView();

	for (int32 StackIndex = (StackSize - 2); StackIndex >= 0; --StackIndex)
	{
		CameraMode = CameraModeStack[StackIndex];
		check(CameraMode);

		OutCameraModeView.Blend(CameraMode->GetCameraModeView()
			, CameraMode->GetBlendWeight());
	}
}

void UArcCameraModeStack::DrawDebug(UCanvas* Canvas) const
{
	check(Canvas);

	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;

	DisplayDebugManager.SetDrawColor(FColor::Green);
	DisplayDebugManager.DrawString(FString(TEXT("   --- Camera Modes (Begin) ---")));

	for (const UArcCameraMode* CameraMode : CameraModeStack)
	{
		check(CameraMode);
		CameraMode->DrawDebug(Canvas);
	}

	DisplayDebugManager.SetDrawColor(FColor::Green);
	DisplayDebugManager.DrawString(FString::Printf(TEXT("   --- Camera Modes (End) ---")));
}

void UArcCameraModeStack::GetBlendInfo(float& OutWeightOfTopLayer
									   , FGameplayTag& OutTagOfTopLayer) const
{
	if (CameraModeStack.Num() == 0)
	{
		OutWeightOfTopLayer = 1.0f;
		OutTagOfTopLayer = FGameplayTag();
		return;
	}
	else
	{
		UArcCameraMode* TopEntry = CameraModeStack.Last();
		check(TopEntry);
		OutWeightOfTopLayer = TopEntry->GetBlendWeight();
		OutTagOfTopLayer = TopEntry->GetCameraTypeTag();
	}
}
