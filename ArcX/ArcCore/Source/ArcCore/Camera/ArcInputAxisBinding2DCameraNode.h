#pragma once
#include "Nodes/Input/CameraRigInput2DSlot.h"

#include "ArcInputAxisBinding2DCameraNode.generated.h"

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

