// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassModifyTargetHealth.h"

#include "MassStateTreeExecutionContext.h"
#include "ArcMassDamageSystem/ArcMassHealthStatsFragment.h"
#include "ArcMassDamageSystem/ArcMassDamageTypes.h"
#include "Modifiers/ArcModifierFunctions.h"

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

	switch (InstanceData.Operation)
	{
	case EArcHealthOperation::Add:
		ArcModifiers::QueueInstant(ME, InstanceData.TargetEntity,
			FArcMassHealthStatsFragment::GetHealthAttribute(), EArcModifierOp::Add, InstanceData.ModifyValue);
		break;
	case EArcHealthOperation::Subtract:
		ArcModifiers::QueueExecute(ME, InstanceData.TargetEntity,
			FArcMassDamageStatsFragment::GetHealthDamageAttribute(), EArcModifierOp::Add, InstanceData.ModifyValue);
		break;
	case EArcHealthOperation::Override:
		ArcModifiers::QueueInstant(ME, InstanceData.TargetEntity,
			FArcMassHealthStatsFragment::GetHealthAttribute(), EArcModifierOp::Override, InstanceData.ModifyValue);
		break;
	default:
		break;
	}

	return EStateTreeRunStatus::Succeeded;
}

FText FArcMassModifyTargetHealthTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView
	, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FMassStateTreeTaskBase::GetDescription(ID, InstanceDataView, BindingLookup, Formatting);
}
