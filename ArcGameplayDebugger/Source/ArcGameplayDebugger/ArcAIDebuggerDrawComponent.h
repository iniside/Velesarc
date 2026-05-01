// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcDebugDrawComponent.h"
#include "ArcAIDebuggerDrawComponent.generated.h"

struct FArcAIDebugPlanStep
{
	FVector Location = FVector::ZeroVector;
	FString Label;
	FColor Color = FColor::White;
};

struct FArcAIDebugPlanDrawData
{
	FVector EntityLocation = FVector::ZeroVector;
	FVector PlayerLocation = FVector::ZeroVector;
	FColor EntityColor = FColor::Cyan;
	float ZOffset = 0.f;
	float StepSphereRadius = 30.f;
	bool bHasEntity = false;
	TArray<FArcAIDebugPlanStep> Steps;
};

UCLASS(ClassGroup = Debug)
class UArcAIDebuggerDrawComponent : public UArcDebugDrawComponent
{
	GENERATED_BODY()
public:
	void UpdatePlanData(const FArcAIDebugPlanDrawData& InData);
protected:
	virtual void CollectShapes() override;
private:
	FArcAIDebugPlanDrawData Data;
};
