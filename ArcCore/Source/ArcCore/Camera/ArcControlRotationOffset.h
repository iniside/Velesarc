#pragma once
#include "Nodes/Input/Input2DCameraNode.h"

#include "ArcControlRotationOffset.generated.h"

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
