// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcDebugDrawComponent.h"
#include "ArcGlobalAbilityTargetingDebuggerDrawComponent.generated.h"

struct FArcTargetingDebugSphere
{
	FVector Location = FVector::ZeroVector;
	float Radius = 0.f;
	FColor Color = FColor::White;
};

UCLASS(ClassGroup = Debug)
class UArcGlobalAbilityTargetingDebuggerDrawComponent : public UArcDebugDrawComponent
{
	GENERATED_BODY()

public:
	void UpdateTargetingSpheres(TArrayView<const FArcTargetingDebugSphere> InSpheres);

protected:
	virtual void CollectShapes() override;

private:
	TArray<FArcTargetingDebugSphere> TargetingSpheres;
};
