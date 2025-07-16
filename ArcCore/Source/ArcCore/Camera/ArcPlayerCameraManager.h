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

#include "Core/CameraNode.h"
#include "Core/CameraParameters.h"
#include "Core/CameraOperation.h"
#include "GameFramework/GameplayCamerasPlayerCameraManager.h"
#include "Nodes/Input/CameraRigInput2DSlot.h"
#include "Nodes/Input/Input2DCameraNode.h"

#include "ArcPlayerCameraManager.generated.h"

constexpr float OAK_CAMERA_DEFAULT_FOV(90.0f);
constexpr float OAK_CAMERA_DEFAULT_PITCH_MIN(-70.0f);
constexpr float OAK_CAMERA_DEFAULT_PITCH_MAX(60.0f);

class UArcUICameraManagerComponent;

/**
 * AArcPlayerCameraManager
 *
 *	The base player camera manager class used by this project.
 */
UCLASS(notplaceable)
class ARCCORE_API AArcPlayerCameraManager : public AGameplayCamerasPlayerCameraManager
{
	GENERATED_BODY()

public:
	AArcPlayerCameraManager(const FObjectInitializer& ObjectInitializer);

	UArcUICameraManagerComponent* GetUICameraComponent() const;

protected:
	virtual void UpdateViewTarget(FTViewTarget& OutVT
								  , float DeltaTime) override;

	virtual void DisplayDebug(UCanvas* Canvas
							  , const FDebugDisplayInfo& DebugDisplay
							  , float& YL
							  , float& YPos) override;

private:
	/** The UI Camera Component, controls the camera when UI is doing something important
	 * that gameplay doesn't get priority over. */
	UPROPERTY(Transient)
	TObjectPtr<UArcUICameraManagerComponent> UICamera;
};

class UCameraValueInterpolator;

UCLASS(meta=(CameraNodeCategories="Attachment"))
class ARCCORE_API UArcAttachToPawnSocket : public UCameraNode
{
	GENERATED_BODY()

protected:

	// UCameraNode interface.
	virtual FCameraNodeEvaluatorPtr OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const override;

public:
	UPROPERTY(EditAnywhere)
	FName SocketName;

	UPROPERTY(EditAnywhere)
	FName ComponentTag = NAME_None;
	
	UPROPERTY(EditAnywhere)
	bool bUsePawnBaseEyeHeight = false;

	UPROPERTY(EditAnywhere, Category="Collision")
	TObjectPtr<UCameraValueInterpolator> OffsetInterpolator;

	UPROPERTY(EditAnywhere, Category="Collision")
	TObjectPtr<UCameraValueInterpolator> OffsetInterpolatorZ;
};

UCLASS(meta=(CameraNodeCategories="Input",
			ObjectTreeGraphSelfPinDirection="Output",
			ObjectTreeGraphDefaultPropertyPinDirection="Input"))
class ARCCORE_API UArcControlRotationOffset : public UInput2DCameraNode
{
	GENERATED_BODY()

public:
	UPROPERTY(meta=(ObjectTreeGraphPinDirection=Input))
	TObjectPtr<UInput2DCameraNode> InputSlot;
	
	// UCameraNode interface.
	virtual void OnBuild(FCameraObjectBuildContext& BuildContext) override;
	virtual FCameraNodeEvaluatorPtr OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const override;

	FCameraVariableID GetVariableID() const { return VariableID; }
	
	UPROPERTY()
	FCameraVariableID VariableID;

};

class UInputAction;

/**
 * An input node that reads player input from an input action.
 */
UCLASS(MinimalAPI, meta=(CameraNodeCategories="Input"))
class UArcInputAxisBinding2DCameraNode : public UCameraRigInput2DSlot
{
	GENERATED_BODY()

public:

	/** The axis input action(s) to read from. */
	UPROPERTY(EditAnywhere, Category="Input")
	TArray<TObjectPtr<UInputAction>> AxisActions;

	/** A multiplier to use on the input values. */
	UPROPERTY(EditAnywhere, Category="Input Processing")
	FVector2dCameraParameter Multiplier;

public:

	UArcInputAxisBinding2DCameraNode(const FObjectInitializer& ObjInit);
	
protected:

	// UCameraNode interface.
	
	virtual FCameraNodeEvaluatorPtr OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const override;
};

