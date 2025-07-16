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

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcCameraMode.generated.h"

class UArcCameraComponent;
class UCanvas;

/**
 * EArcCameraModeBlendFunction
 *
 *	Blend function used for transitioning between camera modes.
 */
UENUM(BlueprintType)
enum class EArcCameraModeBlendFunction : uint8
{
	// Does a simple linear interpolation.
	Linear
	,

	// Immediately accelerates, but smoothly decelerates into the target.  Ease amount
	// controlled by the exponent.
	EaseIn
	,

	// Smoothly accelerates, but does not decelerate into the target.  Ease amount
	// controlled by the exponent.
	EaseOut
	,

	// Smoothly accelerates and decelerates.  Ease amount controlled by the exponent.
	EaseInOut
	, SinIn
	, SinOut
	, SinInOut
	, ExpoIn
	, ExpoOut
	, ExpoInOut
	, CircularIn
	, CircularOur
	, CircularInOut
	, COUNT UMETA(Hidden)};

UENUM(BlueprintType)
enum class EArcBlendTimeMode : uint8
{
	Direct
	, Custom
};

USTRUCT()
struct ARCCORE_API FArcCameraModeBlendTime
{
	GENERATED_BODY()

public:
	virtual float GetBlendTime(const class UArcCameraMode* InCameraMode) const
	{
		return 1.0f;
	}

	virtual ~FArcCameraModeBlendTime()
	{
	};
};

/**
 * FArcCameraModeView
 *
 *	View data produced by the camera mode that is used to blend camera modes.
 */
struct FArcCameraModeView
{
public:
	FArcCameraModeView();

	void Blend(const FArcCameraModeView& Other
			   , float OtherWeight);

public:
	FVector Location;
	FRotator Rotation;
	FRotator ControlRotation;
	float FieldOfView;
};

USTRUCT(BlueprintType)
struct FArcCameraVelocityOffset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	float MinVelocity = 0;

	UPROPERTY(EditAnywhere)
	float MaxVelocity = 250;

	UPROPERTY(EditAnywhere)
	float InterpolationTime = 1.f;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<class UCurveVector> Offset;

	UPROPERTY(EditAnywhere)
	TObjectPtr<class UCurveFloat> PitchAlpha;

	FArcCameraVelocityOffset()
		: Offset(nullptr)
		, PitchAlpha(nullptr)
	{
	}
};

/**
 * UArcCameraMode
 *
 *	Base class for all camera modes.
 */
UCLASS(Abstract
	, NotBlueprintable)
class ARCCORE_API UArcCameraMode : public UObject
{
	GENERATED_BODY()

public:
	UArcCameraMode();

	UArcCameraComponent* GetArcCameraComponent() const;

	virtual UWorld* GetWorld() const override;

	AActor* GetTargetActor() const;

	const FArcCameraModeView& GetCameraModeView() const
	{
		return View;
	}

	// Called when this camera mode is activated on the camera mode stack.
	virtual void OnActivation()
	{
	};

	// Called when this camera mode is deactivated on the camera mode stack.
	virtual void OnDeactivation()
	{
	};

	void UpdateCameraMode(float DeltaTime);

	float GetBlendTime() const;

	float GetBlendWeight() const
	{
		return BlendWeight;
	}

	void SetBlendWeight(float Weight);

	FGameplayTag GetCameraTypeTag() const
	{
		return CameraTypeTag;
	}

	virtual void DrawDebug(UCanvas* Canvas) const;

	FName GetSocketPivot() const
	{
		return SocketPivot;
	}

protected:
	virtual FVector GetPivotLocation() const;

	virtual FRotator GetPivotRotation() const;

	virtual void UpdateView(float DeltaTime);

	virtual void UpdateBlending(float DeltaTime);

protected:
	UPROPERTY(EditDefaultsOnly
		, Category = "Pivot")
	TObjectPtr<const class UCurveFloat> VelocityPivotOffset;

	UPROPERTY(EditDefaultsOnly
		, Category = "Pivot")
	FName SocketPivot = NAME_None;
	
	// A tag that can be queried by gameplay code that cares when a kind of camera mode is
	// active without having to ask about a specific mode (e.g., when aiming downsights to
	// get more accuracy)
	UPROPERTY(EditDefaultsOnly
		, Category = "Blending")
	FGameplayTag CameraTypeTag;

	// View output produced by the camera mode.
	FArcCameraModeView View;

	// The horizontal field of view (in degrees).
	UPROPERTY(EditDefaultsOnly
		, Category = "View"
		, Meta = (UIMin = "5.0", UIMax = "170", ClampMin = "5.0", ClampMax = "170.0"))
	float FieldOfView;

	// Minimum view pitch (in degrees).
	UPROPERTY(EditDefaultsOnly
		, Category = "View"
		, Meta = (UIMin = "-89.9", UIMax = "89.9", ClampMin = "-89.9", ClampMax = "89.9"))
	float ViewPitchMin;

	// Maximum view pitch (in degrees).
	UPROPERTY(EditDefaultsOnly
		, Category = "View"
		, Meta = (UIMin = "-89.9", UIMax = "89.9", ClampMin = "-89.9", ClampMax = "89.9"))
	float ViewPitchMax;

	UPROPERTY(EditDefaultsOnly
		, Category = "Blending")
	EArcBlendTimeMode BlendTimeType;

	// How long it takes to blend in this mode.
	UPROPERTY(EditDefaultsOnly
		, Category = "Blending"
		, meta = (EditCondition = "BlendTimeType==EArcBlendTimeMode::Direct", EditConditionHides))
	float BlendTime;

	UPROPERTY(EditDefaultsOnly
		, Category = "Blending"
		, meta = (BaseStruct = "/Script/ArcCore.ArcCameraModeBlendTime", EditCondition =
			"BlendTimeType==EArcBlendTimeMode::Custom", EditConditionHides))
	FInstancedStruct BlendTimeCustom;

	// Function used for blending.
	UPROPERTY(EditDefaultsOnly
		, Category = "Blending")
	EArcCameraModeBlendFunction BlendFunction;

	// Exponent used by blend functions to control the shape of the curve.
	UPROPERTY(EditDefaultsOnly
		, Category = "Blending")
	float BlendExponent;

	// Linear blend alpha used to determine the blend weight.
	float BlendAlpha;

	// Blend weight calculated using the blend alpha and function.
	float BlendWeight;

protected:
	/** If true, skips all interpolation and puts camera in ideal location.  Automatically
	 * set to false next frame. */
	UPROPERTY(transient)
	uint32 bResetInterpolation : 1;
};

/**
 * UArcCameraModeStack
 *
 *	Stack used for blending camera modes.
 */
UCLASS()
class ARCCORE_API UArcCameraModeStack : public UObject
{
	GENERATED_BODY()

public:
	UArcCameraModeStack();

	void ActivateStack();

	void DeactivateStack();

	bool IsStackActivate() const
	{
		return bIsActive;
	}

	void PushCameraMode(UClass* CameraModeClass);

	bool EvaluateStack(float DeltaTime
					   , FArcCameraModeView& OutCameraModeView);

	void DrawDebug(UCanvas* Canvas) const;

	// Gets the tag associated with the top layer and the blend weight of it
	void GetBlendInfo(float& OutWeightOfTopLayer
					  , FGameplayTag& OutTagOfTopLayer) const;

protected:
	UArcCameraMode* GetCameraModeInstance(UClass* CameraModeClass);

	void UpdateStack(float DeltaTime);

	void BlendStack(FArcCameraModeView& OutCameraModeView) const;

protected:
	bool bIsActive;

	UPROPERTY()
	TArray<TObjectPtr<UArcCameraMode>> CameraModeInstances;

	UPROPERTY()
	TArray<TObjectPtr<UArcCameraMode>> CameraModeStack;
};
