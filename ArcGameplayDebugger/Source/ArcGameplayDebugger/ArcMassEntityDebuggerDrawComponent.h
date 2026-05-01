// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcDebugDrawComponent.h"
#include "ArcMassEntityDebuggerDrawComponent.generated.h"

UCLASS(ClassGroup = Debug)
class UArcMassEntityDebuggerDrawComponent : public UArcDebugDrawComponent
{
	GENERATED_BODY()

public:
	void UpdateSelectedEntity(const FVector& InEntityLocation, float InRadius, const FVector* InPawnLocation);
	void ClearSelectedEntity();

protected:
	virtual void CollectShapes() override;

private:
	FVector EntityLocation = FVector::ZeroVector;
	float SphereRadius = 0.f;
	FVector PawnLocation = FVector::ZeroVector;
	bool bHasEntity = false;
	bool bHasPawnLine = false;
};
