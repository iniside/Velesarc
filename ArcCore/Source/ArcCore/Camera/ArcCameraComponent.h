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

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Camera/CameraComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArcCameraComponent.generated.h"

class UArcCameraMode;
class UArcCameraModeStack;
class UCanvas;
struct FGameplayTag;

DECLARE_DELEGATE_RetVal(UClass*
	, FArcCameraModeDelegate);

UCLASS()
class UArcPushAdditiveCameraOffset : public UAnimNotifyState
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere
		, Category = "Config")
	TObjectPtr<class UCurveVector> AdditiveCameraOffset;
	
	bool bBroadcasted = false;

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp
							 , UAnimSequenceBase* Animation
							 , float TotalDuration
							 , const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyTick(USkeletalMeshComponent* MeshComp
							, UAnimSequenceBase* Animation
							, float FrameDeltaTime
							, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp
						   , UAnimSequenceBase* Animation
						   , const FAnimNotifyEventReference& EventReference) override;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FArcAnimCameraOffsetCurve
	, class UCurveVector*);

/**
 * UArcCameraComponent
 *
 *	The base camera component class used by this project.
 */
UCLASS()
class ARCCORE_API UArcCameraComponent : public UCameraComponent
{
	GENERATED_BODY()

public:
	UArcCameraComponent(const FObjectInitializer& ObjectInitializer);

	// Returns the camera component if one exists on the specified actor.
	UFUNCTION(BlueprintPure
		, Category = "Arc Core|Camera")
	static UArcCameraComponent* FindCameraComponent(const AActor* Actor)
	{
		return (Actor ? Actor->FindComponentByClass<UArcCameraComponent>() : nullptr);
	}

	// Returns the target actor that the camera is looking at.
	virtual AActor* GetTargetActor() const
	{
		return GetOwner();
	}

	// Delegate used to query for the best camera mode.
	FArcCameraModeDelegate DetermineCameraModeDelegate;

	FArcAnimCameraOffsetCurve AnimCameraOffsetStartDelegate;
	FArcAnimCameraOffsetCurve AnimCameraOffsetEndDelegate;

	// Add an offset to the field of view.  The offset is only for one frame, it gets
	// cleared once it is applied.
	void AddFieldOfViewOffset(float FovOffset)
	{
		FieldOfViewOffset += FovOffset;
	}

	virtual void DrawDebug(UCanvas* Canvas) const;

	// Gets the tag associated with the top layer and the blend weight of it
	void GetBlendInfo(float& OutWeightOfTopLayer
					  , FGameplayTag& OutTagOfTopLayer) const;

	void ApplyCameraOffset(FVector Offset);

	void BroadcastAnimCameraOffsetStart(class UCurveVector* InCurve) const;

	void BroadcastAnimCameraOffsetEnd(class UCurveVector* InCurve) const;

	void SetPivotSocketTransform(const FTransform& InTransform)
	{
		PivotSocketTransform = InTransform;
	}

	FTransform GetPivotSocketTransform() const
	{
		return PivotSocketTransform;
	}

	FName GetPivotSocket() const
	{
		return PivotSocket;
	}

	/*
	 * Set New socket and calculate new local transform for it.
	 */
	void SetPivotSocket(const FName InSocket);

	void SetCurrentPivotZ(double InPivotZ)
	{
		CurrentPivotZ = InPivotZ;
	}

	double GetCurrentPivotZ() const
	{
		return CurrentPivotZ;
	}

protected:
	virtual void OnRegister() override;

	virtual void GetCameraView(float DeltaTime
							   , FMinimalViewInfo& DesiredView) override;

	virtual void UpdateCameraModes();

	void BlendOffset(float DeltaTime);

protected:
	double CurrentPivotZ = 0;
	FName PivotSocket = NAME_None;

	FTransform PivotSocketTransform;

	// Stack used to blend the camera modes.
	UPROPERTY()
	TObjectPtr<UArcCameraModeStack> CameraModeStack;

	// Offset applied to the field of view.  The offset is only for one frame, it gets
	// cleared once it is applied.
	float FieldOfViewOffset;

	// Stored offset of the camera.
	FVector InitialCameraOffset;

	// Target offset applied to the camera.
	FVector TargetCameraOffset;

	// Current offset of the camera.
	FVector CurrentCameraOffset;

	// Linear blend alpha used to determine the offset blend weight.
	float BlendOffsetPct = 0;
};
