// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcDebugDrawComponent.h"
#include "ArcGunDebuggerDrawComponent.generated.h"

struct FArcGunDebugDrawData
{
	bool bDrawTargeting = false;
	FVector TraceStart = FVector::ZeroVector;
	FVector TraceEnd = FVector::ZeroVector;
	FColor TargetingColor = FColor::White;
	float TargetingSize = 1.f;

	bool bDrawCameraDirection = false;
	FVector EyeLocation = FVector::ZeroVector;
	FVector EyeDirection = FVector::ZeroVector;
	FColor CameraDirectionColor = FColor::White;
	float CameraDirectionDrawDistance = 0.f;
	float CameraDirectionDrawSize = 1.f;

	bool bDrawCameraPoint = false;
	FVector CameraPointLocation = FVector::ZeroVector;
	float CameraPointDrawSize = 1.f;
	FColor CameraPointColor = FColor::White;
};

UCLASS(ClassGroup = Debug)
class UArcGunDebuggerDrawComponent : public UArcDebugDrawComponent
{
	GENERATED_BODY()

public:
	void UpdateGunDebug(const FArcGunDebugDrawData& InData);

protected:
	virtual void CollectShapes() override;

private:
	FArcGunDebugDrawData Data;
};
