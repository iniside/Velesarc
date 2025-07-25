﻿#include "ArcInputAxisBinding2DCameraNode.h"

#include "EnhancedInputComponent.h"
#include "Core/CameraEvaluationContext.h"
#include "Core/CameraSystemEvaluator.h"
#include "GameFramework/Actor.h"
#include "Player/ArcCorePlayerController.h"

namespace UE::Cameras
{

class FArcInputAxisBinding2DCameraNodeEvaluator : public FCameraRigInput2DSlotEvaluator
{
	UE_DECLARE_CAMERA_NODE_EVALUATOR_EX(ARCCORE_API, FArcInputAxisBinding2DCameraNodeEvaluator, FCameraRigInput2DSlotEvaluator)

protected:

	// FCameraNodeEvaluator interface.
	virtual void OnInitialize(const FCameraNodeEvaluatorInitializeParams& Params, FCameraNodeEvaluationResult& OutResult) override;
	virtual void OnRun(const FCameraNodeEvaluationParams& Params, FCameraNodeEvaluationResult& OutResult) override;

private:

	TObjectPtr<UEnhancedInputComponent> InputComponent;

	TCameraParameterReader<FVector2d> MultiplierReader;

	TArray<FEnhancedInputActionValueBinding*> AxisValueBindings;
};

UE_DEFINE_CAMERA_NODE_EVALUATOR(FArcInputAxisBinding2DCameraNodeEvaluator)

void FArcInputAxisBinding2DCameraNodeEvaluator::OnInitialize(const FCameraNodeEvaluatorInitializeParams& Params, FCameraNodeEvaluationResult& OutResult)
{
	UObject* ContextOwner = Params.EvaluationContext->GetOwner();
	if (ContextOwner)
	{
		if (AActor* ContextOwnerActor = Cast<AActor>(ContextOwner))
		{
			InputComponent = Cast<UEnhancedInputComponent>(ContextOwnerActor->InputComponent);
		}
		else if (AActor* OuterActor = ContextOwner->GetTypedOuter<AActor>())
		{
			InputComponent = Cast<UEnhancedInputComponent>(OuterActor->InputComponent);
		}
	}

	const UArcInputAxisBinding2DCameraNode* AxisBindingNode = GetCameraNodeAs<UArcInputAxisBinding2DCameraNode>();

	RevertAxisXReader.Initialize(AxisBindingNode->RevertAxisX);
	RevertAxisYReader.Initialize(AxisBindingNode->RevertAxisY);
	MultiplierReader.Initialize(AxisBindingNode->Multiplier);

	if (InputComponent)
	{
		for (TObjectPtr<UInputAction> AxisAction : AxisBindingNode->AxisActions)
		{
			FEnhancedInputActionValueBinding* AxisValueBinding = &InputComponent->BindActionValue(AxisAction);
			AxisValueBindings.Add(AxisValueBinding);
		}
	}
	else if (Params.Evaluator->GetRole() == ECameraSystemEvaluatorRole::Game)
	{
		//UE_LOG(LogCameraSystem, Error, TEXT("No input component found on context owner '%s' for node '%s' in '%s'."),
		//		*GetNameSafe(ContextOwner), 
		//		*GetNameSafe(AxisBindingNode),
		//		*GetNameSafe(AxisBindingNode ? AxisBindingNode->GetOutermost() : nullptr));
	}

	Super::OnInitialize(Params, OutResult);
}

void FArcInputAxisBinding2DCameraNodeEvaluator::OnRun(const FCameraNodeEvaluationParams& Params, FCameraNodeEvaluationResult& OutResult)
{
	const UArcInputAxisBinding2DCameraNode* AxisBindingNode = GetCameraNodeAs<UArcInputAxisBinding2DCameraNode>();
	
	const UCameraRigInput2DSlot* SlotNode = GetCameraNodeAs<UCameraRigInput2DSlot>();

	FVector2d HighestValue(FVector2d::ZeroVector);
	double HighestSquaredLenth = 0.f;

	for (FEnhancedInputActionValueBinding* AxisValueBinding : AxisValueBindings)
	{
		if (!AxisValueBinding)
		{
			continue;
		}

		const FVector2d Value = AxisValueBinding->GetValue().Get<FVector2D>();
		const double ValueSquaredLength = Value.SquaredLength();
		if (ValueSquaredLength > HighestSquaredLenth)
		{
			HighestValue = Value;
		}
	}

	const FVector2d Multiplier = MultiplierReader.Get(OutResult.VariableTable);
	DeltaInputValue = FVector2d(HighestValue.X * Multiplier.X, HighestValue.Y * Multiplier.Y);
	
	
	const FVector2d Speed = 
		SlotNode->bIsPreBlended ?
			OutResult.VariableTable.GetValue<FVector2d>(SlotNode->GetSpeedVariableID()) :
			SpeedReader.Get(OutResult.VariableTable);

	FVector2d FinalDelta(DeltaInputValue.X * Speed.X * Params.DeltaTime, DeltaInputValue.Y * Speed.Y * Params.DeltaTime);

	AArcCorePlayerController* PC = Cast<AArcCorePlayerController>(Params.EvaluationContext->GetPlayerController());
	if (PC)
	{
		FinalDelta.X += PC->ControlRotationOffset.Yaw;
		FinalDelta.Y += PC->ControlRotationOffset.Pitch;
		PC->ControlRotationOffset.Yaw = 0;
        PC->ControlRotationOffset.Pitch = 0;
	}
	if (RevertAxisXReader.Get(OutResult.VariableTable))
	{
		FinalDelta.X = -FinalDelta.X;
	}
	if (RevertAxisYReader.Get(OutResult.VariableTable))
	{
		FinalDelta.Y = -FinalDelta.Y;
	}

	//if (bIsAccumulated)
	{
		InputValue += FinalDelta;
	}
	//else
	{
		//	InputValue = FinalDelta;
	}


	
	InputValue.X = SlotNode->NormalizeX.NormalizeValue(InputValue.X);
	InputValue.Y = SlotNode->NormalizeY.NormalizeValue(InputValue.Y);

	InputValue.X = SlotNode->ClampX.ClampValue(InputValue.X);
	InputValue.Y = SlotNode->ClampY.ClampValue(InputValue.Y);
	if (PC)
	{
		if (PC->ClampValue.IsSet())
		{
			if (PC->ClampValue.GetValue() < 0)
			{
				if (InputValue.Y < PC->ClampValue.GetValue())
				{
					InputValue.Y = PC->ClampValue.GetValue();
				}
			}
			else if (PC->ClampValue.GetValue() > 0)
			{
				if (InputValue.Y > PC->ClampValue.GetValue())
				{
					InputValue.Y = PC->ClampValue.GetValue();
				}
			}
		}
		
		PC->InputValue = InputValue;
		
	}
	OutResult.VariableTable.SetValue<FVector2d>(SlotNode->GetVariableID(), InputValue);

	//Super::OnRun(Params, OutResult);
}
}  // namespace UE::Cameras

UArcInputAxisBinding2DCameraNode::UArcInputAxisBinding2DCameraNode(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	Multiplier = FVector2D(1, 1);
}


FCameraNodeEvaluatorPtr UArcInputAxisBinding2DCameraNode::OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const
{
	using namespace UE::Cameras;
	return Builder.BuildEvaluator<FArcInputAxisBinding2DCameraNodeEvaluator>();
}
