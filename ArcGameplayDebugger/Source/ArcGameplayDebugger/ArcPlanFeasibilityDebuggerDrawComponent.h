// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcDebugDrawComponent.h"
#include "ArcPlanFeasibilityDebuggerDrawComponent.generated.h"

struct FArcPlanFeasibilityDebugStep
{
	FVector Location = FVector::ZeroVector;
	FString Label;
	FColor Color = FColor::White;
};

struct FArcPlanFeasibilityDebugDrawData
{
	FVector SearchOrigin = FVector::ZeroVector;
	float SearchRadius = 0.f;
	bool bHasSelectedPlan = false;
	TArray<FArcPlanFeasibilityDebugStep> Steps;
};

UCLASS(ClassGroup = Debug)
class UArcPlanFeasibilityDebuggerDrawComponent : public UArcDebugDrawComponent
{
	GENERATED_BODY()
public:
	void UpdatePlanData(const FArcPlanFeasibilityDebugDrawData& InData);
protected:
	virtual void CollectShapes() override;
private:
	FArcPlanFeasibilityDebugDrawData Data;
};
