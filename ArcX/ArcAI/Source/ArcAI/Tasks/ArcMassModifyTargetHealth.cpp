// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassModifyTargetHealth.h"

#include "MassSignalSubsystem.h"
#include "MassStateTreeExecutionContext.h"
#include "ArcMass/ArcMassFragments.h"
#include "ArcMass/ArcMassHealthSignalProcessor.h"

FArcMassModifyTargetHealthTask::FArcMassModifyTargetHealthTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FArcMassModifyTargetHealthTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	if (!InstanceData.TargetEntity.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}
	
	FMassEntityManager& ME = MassCtx.GetEntityManager();
	if (!ME.IsEntityValid(InstanceData.TargetEntity))
	{
		return EStateTreeRunStatus::Failed;
	}
	
	FArcMassHealthFragment* HealthFragment = ME.GetFragmentDataPtr<FArcMassHealthFragment>(InstanceData.TargetEntity);
	if (HealthFragment == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	switch (InstanceData.Operation)
	{
	case EArcHealthOperation::Add:
		HealthFragment->CurrentHealth += InstanceData.ModifyValue;
		HealthFragment->CurrentHealth = FMath::Clamp(HealthFragment->CurrentHealth, 0.f, HealthFragment->MaxHealth);
		break;
	case EArcHealthOperation::Subtract:
		HealthFragment->CurrentHealth -= InstanceData.ModifyValue;
		HealthFragment->CurrentHealth = FMath::Clamp(HealthFragment->CurrentHealth, 0.f, HealthFragment->MaxHealth);
		break;
	case EArcHealthOperation::Override:
		HealthFragment->CurrentHealth = InstanceData.ModifyValue;
		HealthFragment->CurrentHealth = FMath::Clamp(HealthFragment->CurrentHealth, 0.f, HealthFragment->MaxHealth);
		break;
	default:
		break;
	}
	
	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	SignalSubsystem->SignalEntity(UE::ArcMass::Signals::HealthChanged, InstanceData.TargetEntity);
	return EStateTreeRunStatus::Succeeded;
}

FText FArcMassModifyTargetHealthTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView
	, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FMassStateTreeTaskBase::GetDescription(ID, InstanceDataView, BindingLookup, Formatting);
}
