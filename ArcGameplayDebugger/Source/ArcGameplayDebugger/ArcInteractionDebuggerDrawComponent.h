// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcDebugDrawComponent.h"
#include "ArcInteractionDebuggerDrawComponent.generated.h"

UCLASS(ClassGroup = Debug)
class UArcInteractionDebuggerDrawComponent : public UArcDebugDrawComponent
{
	GENERATED_BODY()

public:
	void UpdateInteractionTarget(const FVector& InOrigin, float InRadius);

protected:
	virtual void CollectShapes() override;

private:
	FVector TargetOrigin = FVector::ZeroVector;
	float TargetRadius = 0.f;
	bool bHasTarget = false;
};
