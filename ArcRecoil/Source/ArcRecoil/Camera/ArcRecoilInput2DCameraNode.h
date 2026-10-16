// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Nodes/Input/Input2DCameraNode.h"

#include "ArcRecoilInput2DCameraNode.generated.h"

UCLASS(MinimalAPI, meta = (
	CameraNodeCategories = "Input",
	ObjectTreeGraphSelfPinDirection = "Output",
	ObjectTreeGraphDefaultPropertyPinDirection = "Input"))
class UArcRecoilInput2DCameraNode : public UInput2DCameraNode
{
	GENERATED_BODY()

public:
	UArcRecoilInput2DCameraNode(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(meta = (ObjectTreeGraphPinDirection = Input))
	TObjectPtr<UInput2DCameraNode> InputSlot;

	// Recoil is authored in camera degrees; the wrapped slot lives in its own Speed/Normalize/Clamp scale.
	UPROPERTY(EditAnywhere, Category = "Arc Recoil", meta = (ClampMin = "0.0001"))
	float DegreesToSlotUnits = 1.0f;

protected:
	virtual FCameraNodeChildrenView OnGetChildren() override;
	virtual FCameraNodeEvaluatorPtr OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const override;
};
