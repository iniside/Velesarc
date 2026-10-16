// Copyright Lukasz Baran. All Rights Reserved.

#include "Actions/ArcTacticsSequenceAction.h"

#include "Debugger/ArcTacticsTrace.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsSequenceAction)

namespace ArcTacticsSequenceActionPrivate
{
	struct FCurrentStep
	{
		const FArcTacticsStepNode* Node = nullptr;
		FStructView Data;
	};

	FCurrentStep GetStep(const FArcTacticsStepAccess& Steps, const int32 StepIndex)
	{
		FCurrentStep Step;
		Step.Node = Steps.GetNode(StepIndex);
		Step.Data = Steps.GetData(StepIndex);
		if (!Step.Node || !Step.Data.IsValid())
		{
			return FCurrentStep();
		}
		return Step;
	}

	FCurrentStep GetCurrentStep(const FArcTacticsActionView& Action)
	{
		const FArcTacticsSequenceActionInstanceData* Data =
			Action.ActionData.GetPtr<FArcTacticsSequenceActionInstanceData>();
		return Data ? GetStep(Action.Steps, Data->StepIndex) : FCurrentStep();
	}

	bool EnterStep(
		FArcTacticsActionContext& Context,
		FArcTacticsSequenceActionInstanceData& Data,
		const FCurrentStep& Step)
	{
		if (!Step.Node)
		{
			return false;
		}
		TRACE_ARCTACTICS_ACTION_STEP_CHANGED(
			Context.World, Context.EntityManager, Context.Entity, Context.RequestToken.ActionSerial,
			Data.StepIndex, Context.Steps.Num(), Context.Steps.GetName(Data.StepIndex), Step.Node);
		if (!Context.Steps.ApplyStepInputs(Data.StepIndex))
		{
			return false;
		}
		FArcTacticsStepContext StepContext(Context, Data.RequestGeneration, Data.RequestDecisionGeneration);
		return Step.Node->Enter(StepContext, Step.Data);
	}

	void ExitActiveSteps(FArcTacticsActionContext& Context, const int32 Count)
	{
		for (int32 ActiveStepIndex = Count - 1; ActiveStepIndex >= 0; --ActiveStepIndex)
		{
			const FArcTacticsActiveStepNode* Node = Context.ActiveSteps.GetNode(ActiveStepIndex);
			const FStructView NodeData = Context.ActiveSteps.GetData(ActiveStepIndex);
			if (Node && NodeData.IsValid())
			{
				Node->Exit(Context, NodeData);
			}
		}
	}

	bool EnterActiveSteps(FArcTacticsActionContext& Context, FArcTacticsSequenceActionInstanceData& Data)
	{
		for (int32 ActiveStepIndex = 0; ActiveStepIndex < Context.ActiveSteps.Num(); ++ActiveStepIndex)
		{
			const FArcTacticsActiveStepNode* Node = Context.ActiveSteps.GetNode(ActiveStepIndex);
			const FStructView NodeData = Context.ActiveSteps.GetData(ActiveStepIndex);
			if (!Node || !NodeData.IsValid()
				|| !Context.ActiveSteps.ApplyInputs(ActiveStepIndex)
				|| !Node->Enter(Context, NodeData))
			{
				ExitActiveSteps(Context, ActiveStepIndex);
				return false;
			}
		}
		Data.bActiveStepsEntered = true;
		return true;
	}

	void ExitEnteredActiveSteps(FArcTacticsActionContext& Context, FArcTacticsSequenceActionInstanceData& Data)
	{
		if (!Data.bActiveStepsEntered)
		{
			return;
		}
		Data.bActiveStepsEntered = false;
		ExitActiveSteps(Context, Context.ActiveSteps.Num());
	}
}

const UScriptStruct* FArcTacticsSequenceAction::GetInstanceDataType() const
{
	return FArcTacticsSequenceActionInstanceData::StaticStruct();
}

FText FArcTacticsSequenceAction::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "SequenceAction", "Tactical Action Sequence");
}

EArcTacticsActionStatus FArcTacticsSequenceAction::Enter(
	FArcTacticsActionContext& Context,
	FStructView InstanceData) const
{
	FArcTacticsSequenceActionInstanceData* Data = InstanceData.GetPtr<FArcTacticsSequenceActionInstanceData>();
	if (!Data || !Context.Entity.IsSet() || Context.Steps.Num() == 0)
	{
		return EArcTacticsActionStatus::Failed;
	}
	Data->StepIndex = INDEX_NONE;
	Data->RequestGeneration = 0;
	Data->RequestDecisionGeneration = 0;
	if (!ArcTacticsSequenceActionPrivate::EnterActiveSteps(Context, *Data))
	{
		return EArcTacticsActionStatus::Failed;
	}
	Data->StepIndex = 0;
	const ArcTacticsSequenceActionPrivate::FCurrentStep Step =
		ArcTacticsSequenceActionPrivate::GetStep(Context.Steps, 0);
	return ArcTacticsSequenceActionPrivate::EnterStep(Context, *Data, Step)
		? EArcTacticsActionStatus::Running
		: EArcTacticsActionStatus::Failed;
}

EArcTacticsActionStatus FArcTacticsSequenceAction::Tick(
	FArcTacticsActionContext& Context,
	FStructView InstanceData) const
{
	FArcTacticsSequenceActionInstanceData* Data = InstanceData.GetPtr<FArcTacticsSequenceActionInstanceData>();
	if (!Data)
	{
		return EArcTacticsActionStatus::Failed;
	}
	while (true)
	{
		const ArcTacticsSequenceActionPrivate::FCurrentStep Step =
			ArcTacticsSequenceActionPrivate::GetStep(Context.Steps, Data->StepIndex);
		if (!Step.Node || !Context.Steps.ApplyStepInputs(Data->StepIndex))
		{
			return EArcTacticsActionStatus::Failed;
		}
		FArcTacticsStepContext StepContext(Context, Data->RequestGeneration, Data->RequestDecisionGeneration);
		const EArcTacticsActionStatus Status = Step.Node->Tick(StepContext, Step.Data);
		if (Status == EArcTacticsActionStatus::Failed || !Context.Steps.PublishStepOutputs(Data->StepIndex))
		{
			return EArcTacticsActionStatus::Failed;
		}
		if (Status == EArcTacticsActionStatus::Running)
		{
			return EArcTacticsActionStatus::Running;
		}
		++Data->StepIndex;
		if (Data->StepIndex >= Context.Steps.Num())
		{
			ArcTacticsSequenceActionPrivate::ExitEnteredActiveSteps(Context, *Data);
			return EArcTacticsActionStatus::Succeeded;
		}
		const ArcTacticsSequenceActionPrivate::FCurrentStep NextStep =
			ArcTacticsSequenceActionPrivate::GetStep(Context.Steps, Data->StepIndex);
		if (!ArcTacticsSequenceActionPrivate::EnterStep(Context, *Data, NextStep))
		{
			return EArcTacticsActionStatus::Failed;
		}
		const EArcTacticsActionTickLane NextLane = NextStep.Node->GetTickLane(NextStep.Data);
		if ((NextLane == EArcTacticsActionTickLane::Movement && !Context.PathContext)
			|| (NextLane == EArcTacticsActionTickLane::Utility && Context.PathContext))
		{
			Context.RequestExecution.ExecuteIfBound();
			return EArcTacticsActionStatus::Running;
		}
	}
}

void FArcTacticsSequenceAction::Cancel(
	FArcTacticsActionContext& Context,
	FStructView InstanceData,
	const EArcTacticsActionCancelMode CancelMode) const
{
	FArcTacticsSequenceActionInstanceData* Data = InstanceData.GetPtr<FArcTacticsSequenceActionInstanceData>();
	if (!Data)
	{
		return;
	}
	const ArcTacticsSequenceActionPrivate::FCurrentStep Step =
		ArcTacticsSequenceActionPrivate::GetStep(Context.Steps, Data->StepIndex);
	FArcTacticsStepContext StepContext(Context, Data->RequestGeneration, Data->RequestDecisionGeneration);
	StepContext.InvalidateRequests();
	Data->StepIndex = INDEX_NONE;
	if (Step.Node)
	{
		Step.Node->Cancel(StepContext, Step.Data, CancelMode);
	}
	ArcTacticsSequenceActionPrivate::ExitEnteredActiveSteps(Context, *Data);
}

void FArcTacticsSequenceAction::OnAsyncQueryCompleted(
	FArcTacticsActionContext& Context,
	FStructView InstanceData,
	const FArcTacticsAsyncRequestToken& Token,
	FArcTQSQueryInstance& CompletedQuery) const
{
	FArcTacticsSequenceActionInstanceData* Data = InstanceData.GetPtr<FArcTacticsSequenceActionInstanceData>();
	if (!Data)
	{
		return;
	}
	const int32 StepIndex = Data->StepIndex;
	const ArcTacticsSequenceActionPrivate::FCurrentStep Step =
		ArcTacticsSequenceActionPrivate::GetStep(Context.Steps, StepIndex);
	if (!Step.Node)
	{
		return;
	}
	FArcTacticsStepContext StepContext(Context, Data->RequestGeneration, Data->RequestDecisionGeneration);
	Step.Node->OnAsyncQueryCompleted(StepContext, Step.Data, Token, CompletedQuery);
	if (Context.QueryReport.bAccepted)
	{
		Context.QueryReport.SourceName = Context.Steps.GetName(StepIndex);
	}
}

EArcTacticsActionTickLane FArcTacticsSequenceAction::GetTickLane(const FArcTacticsActionView& Action) const
{
	const ArcTacticsSequenceActionPrivate::FCurrentStep Step =
		ArcTacticsSequenceActionPrivate::GetCurrentStep(Action);
	return Step.Node ? Step.Node->GetTickLane(Step.Data) : EArcTacticsActionTickLane::None;
}

EArcTacticsExecutionRequirement FArcTacticsSequenceAction::GetRequirements(FConstStructView) const
{
	return EArcTacticsExecutionRequirement::Transform
		| EArcTacticsExecutionRequirement::PathFollow
		| EArcTacticsExecutionRequirement::TQSSubsystem
		| EArcTacticsExecutionRequirement::AbilityCollection
		| EArcTacticsExecutionRequirement::AbilitySubsystem;
}

bool FArcTacticsSequenceAction::BlocksDecision(const FArcTacticsActionView& Action) const
{
	const ArcTacticsSequenceActionPrivate::FCurrentStep Step =
		ArcTacticsSequenceActionPrivate::GetCurrentStep(Action);
	return Step.Node && Step.Node->BlocksDecision(Step.Data);
}

double FArcTacticsSequenceAction::GetNextWakeTime(const FArcTacticsActionView& Action) const
{
	const ArcTacticsSequenceActionPrivate::FCurrentStep Step =
		ArcTacticsSequenceActionPrivate::GetCurrentStep(Action);
	return Step.Node ? Step.Node->GetNextWakeTime(Step.Data) : 0.0;
}

int32 FArcTacticsSequenceAction::GetCurrentStepIndex(const FArcTacticsActionView& Action) const
{
	const FArcTacticsSequenceActionInstanceData* Data =
		Action.ActionData.GetPtr<FArcTacticsSequenceActionInstanceData>();
	return Data ? Data->StepIndex : INDEX_NONE;
}

#if WITH_EDITOR
FName FArcTacticsSequenceAction::GetIconName() const
{
	return TEXT("AppStyle|Icons.Play");
}

FColor FArcTacticsSequenceAction::GetIconColor() const
{
	return FColor(230, 151, 66);
}
#endif
