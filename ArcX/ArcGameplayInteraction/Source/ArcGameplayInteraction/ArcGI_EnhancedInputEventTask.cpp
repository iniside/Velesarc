#include "ArcGI_EnhancedInputEventTask.h"

#include "EnhancedInputComponent.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FArcGI_EnhancedInputEventTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// This will be always running.
	EStateTreeRunStatus Status = EStateTreeRunStatus::Running;
	
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.Actor)
	{
		return Status;
	}
	
	APlayerController* PC = Cast<APlayerController>(InstanceData.Actor);
	if (!PC)
	{
		APawn* Pawn = Cast<APawn>(InstanceData.Actor);
		if (Pawn)
		{
			PC = Pawn->GetController<APlayerController>();
		}
	}
	
	if (!PC)
	{
		return Status;
	}
	if (InstanceData.Bindings.IsEmpty())
	{
		if (UEnhancedInputComponent* InputComp = Cast<UEnhancedInputComponent>(PC->InputComponent))
		{
			for (UInputAction* AxisAction : InputActions)
			{
				FEnhancedInputActionValueBinding* AxisValueBinding = &InputComp->BindActionValue(AxisAction);
				InstanceData.Bindings.Add(AxisValueBinding);
			}
		}
	}
	return Status;
}

EStateTreeRunStatus FArcGI_EnhancedInputEventTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	EStateTreeRunStatus Status = EStateTreeRunStatus::Running;
	
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	for (FEnhancedInputActionValueBinding* AxisValueBinding : InstanceData.Bindings)
	{
		if (!AxisValueBinding)
		{
			continue;
		}

		FInputActionValue Value = AxisValueBinding->GetValue();
		if (Value.GetMagnitude() > 0 && InstanceData.OnInputStarted.IsValid())
		{
			Context.BroadcastDelegate(InstanceData.OnInputStarted);
		}
		
	}
	
	return Status;
}

void FArcGI_EnhancedInputEventTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FGameplayInteractionStateTreeTask::ExitState(Context, Transition);
}
