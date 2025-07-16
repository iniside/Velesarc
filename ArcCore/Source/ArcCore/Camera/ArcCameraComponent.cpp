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

#include "ArcCore/Camera/ArcCameraComponent.h"

#include "ArcCore/Camera/ArcCameraMode.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void UArcPushAdditiveCameraOffset::NotifyBegin(USkeletalMeshComponent* MeshComp
											   , UAnimSequenceBase* Animation
											   , float TotalDuration
											   , const FAnimNotifyEventReference& EventReference)
{
	UArcCameraComponent* ArcCamera = UArcCameraComponent::FindCameraComponent(MeshComp->GetOwner());
	if (ArcCamera)
	{
		ArcCamera->BroadcastAnimCameraOffsetStart(AdditiveCameraOffset);
	}
}

void UArcPushAdditiveCameraOffset::NotifyTick(USkeletalMeshComponent* MeshComp
											  , UAnimSequenceBase* Animation
											  , float FrameDeltaTime
											  , const FAnimNotifyEventReference& EventReference)
{
	if (MeshComp->GetNetMode() == ENetMode::NM_DedicatedServer)
	{
		return;
	}
	UArcCameraComponent* ArcCamera = UArcCameraComponent::FindCameraComponent(MeshComp->GetOwner());
	if (ArcCamera && bBroadcasted == false)
	{
		ArcCamera->BroadcastAnimCameraOffsetStart(AdditiveCameraOffset);
		bBroadcasted = true;
	}
}

void UArcPushAdditiveCameraOffset::NotifyEnd(USkeletalMeshComponent* MeshComp
											 , UAnimSequenceBase* Animation
											 , const FAnimNotifyEventReference& EventReference)
{
	UArcCameraComponent* ArcCamera = UArcCameraComponent::FindCameraComponent(MeshComp->GetOwner());
	if (ArcCamera)
	{
		ArcCamera->BroadcastAnimCameraOffsetEnd(AdditiveCameraOffset);
	}
}

UArcCameraComponent::UArcCameraComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CameraModeStack = nullptr;
	FieldOfViewOffset = 0.0f;
}

void UArcCameraComponent::SetPivotSocket(const FName InSocket)
{
	PivotSocket = InSocket;
	if (const APawn* TargetPawn = Cast<APawn>(GetTargetActor()))
	{
		// Height adjustments for characters to account for crouching.
		if (const ACharacter* TargetCharacter = Cast<ACharacter>(TargetPawn))
		{
			USkeletalMeshComponent* SMC = TargetCharacter->GetMesh();
			if (PivotSocket != NAME_None)
			{
				FTransform CompSpaceTM = SMC->GetSocketTransform(PivotSocket
					, ERelativeTransformSpace::RTS_Component);

				FTransform WorldSpaceTM = SMC->GetSocketTransform(PivotSocket
					, ERelativeTransformSpace::RTS_World);
				
				FVector ActorLoc = TargetCharacter->GetActorLocation();
				ActorLoc.Y += CompSpaceTM.GetLocation().Y;
				ActorLoc.Z = WorldSpaceTM.GetLocation().Z;

				PivotSocketTransform.SetLocation(ActorLoc);
				
			}
		}
	}
}

void UArcCameraComponent::OnRegister()
{
	Super::OnRegister();

	if (!CameraModeStack)
	{
		CameraModeStack = NewObject<UArcCameraModeStack>(this);
		check(CameraModeStack);
	}
}

void UArcCameraComponent::GetCameraView(float DeltaTime
										, FMinimalViewInfo& DesiredView)
{
	check(CameraModeStack);

	UpdateCameraModes();

	if (const APawn* TargetPawn = Cast<APawn>(GetTargetActor()))
	{
		// Height adjustments for characters to account for crouching.
		if (const ACharacter* TargetCharacter = Cast<ACharacter>(TargetPawn))
		{
			USkeletalMeshComponent* SMC = TargetCharacter->GetMesh();
			if (PivotSocket != NAME_None)
			{
				FVector BoneLoc = SMC->GetBoneLocation(PivotSocket, EBoneSpaces::ComponentSpace);
				
				FTransform CompSpaceTM = SMC->GetSocketTransform(PivotSocket
									, ERelativeTransformSpace::RTS_Actor);

				FTransform WorldSpaceTM = SMC->GetSocketTransform(PivotSocket
					, ERelativeTransformSpace::RTS_World);
				
				FVector ActorLoc = TargetCharacter->GetActorLocation();
				//ActorLoc.Y += BoneLoc.Y;
				ActorLoc.Z = WorldSpaceTM.GetLocation().Z;

				PivotSocketTransform.SetLocation(ActorLoc);
			}
		}
	}

	FArcCameraModeView CameraModeView;
	CameraModeStack->EvaluateStack(DeltaTime
		, CameraModeView);

	// Keep player controller in sync with the latest view.
	//if (APawn* TargetPawn = Cast<APawn>(GetTargetActor()))
	//{
	//	if (APlayerController* PC = TargetPawn->GetController<APlayerController>())
	//	{
	//		PC->SetControlRotation(CameraModeView.ControlRotation);
	//	}
	//}

	// Apply any offset that was added to the field of view.
	CameraModeView.FieldOfView += FieldOfViewOffset;
	FieldOfViewOffset = 0.0f;

	// Keep camera component in sync with the latest view.
	SetWorldLocationAndRotation(CameraModeView.Location
		, CameraModeView.Rotation);
	FieldOfView = CameraModeView.FieldOfView;

	// Apply Camera Offset with Blend
	BlendOffset(DeltaTime);

	// Fill in desired view.
	DesiredView.Location = CameraModeView.Location + CurrentCameraOffset;
	DesiredView.Rotation = CameraModeView.Rotation;
	DesiredView.FOV = CameraModeView.FieldOfView;
	DesiredView.OrthoWidth = OrthoWidth;
	DesiredView.OrthoNearClipPlane = OrthoNearClipPlane;
	DesiredView.OrthoFarClipPlane = OrthoFarClipPlane;
	DesiredView.AspectRatio = AspectRatio;
	DesiredView.bConstrainAspectRatio = bConstrainAspectRatio;
	DesiredView.bUseFieldOfViewForLOD = bUseFieldOfViewForLOD;
	DesiredView.ProjectionMode = ProjectionMode;

	// See if the CameraActor wants to override the PostProcess settings used.
	DesiredView.PostProcessBlendWeight = PostProcessBlendWeight;
	if (PostProcessBlendWeight > 0.0f)
	{
		DesiredView.PostProcessSettings = PostProcessSettings;
	}
}

void UArcCameraComponent::UpdateCameraModes()
{
	check(CameraModeStack);

	if (CameraModeStack->IsStackActivate())
	{
		if (DetermineCameraModeDelegate.IsBound())
		{
			if (UClass* CameraMode = DetermineCameraModeDelegate.Execute())
			{
				CameraModeStack->PushCameraMode(CameraMode);
			}
		}
	}
}

void UArcCameraComponent::DrawDebug(UCanvas* Canvas) const
{
	check(Canvas);

	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;

	DisplayDebugManager.SetFont(GEngine->GetSmallFont());
	DisplayDebugManager.SetDrawColor(FColor::Yellow);
	DisplayDebugManager.DrawString(FString::Printf(TEXT("ArcCameraComponent: %s")
		, *GetNameSafe(GetTargetActor())));

	DisplayDebugManager.SetDrawColor(FColor::White);
	DisplayDebugManager.DrawString(FString::Printf(TEXT("   Location: %s")
		, *GetComponentLocation().ToCompactString()));
	DisplayDebugManager.DrawString(FString::Printf(TEXT("   Rotation: %s")
		, *GetComponentRotation().ToCompactString()));
	DisplayDebugManager.DrawString(FString::Printf(TEXT("   FOV: %f")
		, FieldOfView));

	check(CameraModeStack);
	CameraModeStack->DrawDebug(Canvas);
}

void UArcCameraComponent::GetBlendInfo(float& OutWeightOfTopLayer
									   , FGameplayTag& OutTagOfTopLayer) const
{
	check(CameraModeStack);
	CameraModeStack->GetBlendInfo(/*out*/ OutWeightOfTopLayer
		, /*out*/ OutTagOfTopLayer);
}

void UArcCameraComponent::ApplyCameraOffset(FVector Offset)
{
	BlendOffsetPct = 0.0f;
	InitialCameraOffset = CurrentCameraOffset;
	TargetCameraOffset = Offset;
}

void UArcCameraComponent::BroadcastAnimCameraOffsetStart(UCurveVector* InCurve) const
{
	AnimCameraOffsetStartDelegate.Broadcast(InCurve);
}

void UArcCameraComponent::BroadcastAnimCameraOffsetEnd(UCurveVector* InCurve) const
{
	AnimCameraOffsetEndDelegate.Broadcast(InCurve);
}

void UArcCameraComponent::BlendOffset(float DeltaTime)
{
	float BlendSpeed = 5.0f;

	if (BlendOffsetPct < 1.0f)
	{
		BlendOffsetPct = FMath::Min(BlendOffsetPct + DeltaTime * BlendSpeed
			, 1.0f);
		CurrentCameraOffset = FMath::InterpEaseInOut(InitialCameraOffset
			, TargetCameraOffset
			, BlendOffsetPct
			, 1.0);
	}
	else
	{
		CurrentCameraOffset = TargetCameraOffset;
		BlendOffsetPct = 1.0f;
	}
}
