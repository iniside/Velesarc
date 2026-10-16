// Copyright Lukasz Baran. All Rights Reserved.

#include "Steps/ArcTacticsMemorySteps.h"

#include "Actions/ArcTacticsMovement.h"
#include "Engine/World.h"
#include "Memory/ArcMassMemory.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsMemorySteps)

namespace ArcTacticsMemoryStepsPrivate
{
	const FName MemoryFragmentMissing(TEXT("MemoryFragmentMissing"));

	template<typename TData, typename TValue>
	EArcTacticsActionStatus TickWrite(
		FArcTacticsStepContext& Context,
		FStructView InstanceData,
		TValue TData::* ValueMember)
	{
		const TData* Data = InstanceData.GetPtr<TData>();
		FArcTacticsActionContext& Action = Context.Action;
		if (!Data || !Action.EntityManager)
		{
			return EArcTacticsActionStatus::Failed;
		}
		if (!ArcMassMemory::Write(*Action.EntityManager, Action.Entity, Data->Key, Data->*ValueMember))
		{
			Action.FailureReason = MemoryFragmentMissing;
			return EArcTacticsActionStatus::Failed;
		}
		return EArcTacticsActionStatus::Succeeded;
	}
}

bool FArcTacticsMemoryStep::Enter(FArcTacticsStepContext& Context, FStructView InstanceData) const
{
	FArcTacticsMemoryStepInstanceData* Data = InstanceData.GetPtr<FArcTacticsMemoryStepInstanceData>();
	if (!Data || !Context.Action.World)
	{
		return false;
	}
	Data->WakeTime = Context.Action.World->GetTimeSeconds();
	return true;
}

EArcTacticsActionTickLane FArcTacticsMemoryStep::GetTickLane(FConstStructView) const
{
	return EArcTacticsActionTickLane::Utility;
}

double FArcTacticsMemoryStep::GetNextWakeTime(const FConstStructView InstanceData) const
{
	const FArcTacticsMemoryStepInstanceData* Data = InstanceData.GetPtr<const FArcTacticsMemoryStepInstanceData>();
	return Data ? Data->WakeTime : 0.0;
}

const UScriptStruct* FArcTacticsWriteMemoryLocationStep::GetInstanceDataType() const
{
	return FArcTacticsWriteMemoryLocationStepInstanceData::StaticStruct();
}

FText FArcTacticsWriteMemoryLocationStep::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "WriteMemoryLocationStep", "Write Memory Location");
}

EArcTacticsActionStatus FArcTacticsWriteMemoryLocationStep::Tick(FArcTacticsStepContext& Context, FStructView InstanceData) const
{
	return ArcTacticsMemoryStepsPrivate::TickWrite(Context, InstanceData, &FArcTacticsWriteMemoryLocationStepInstanceData::Value);
}

const UScriptStruct* FArcTacticsWriteMemoryEntityStep::GetInstanceDataType() const
{
	return FArcTacticsWriteMemoryEntityStepInstanceData::StaticStruct();
}

FText FArcTacticsWriteMemoryEntityStep::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "WriteMemoryEntityStep", "Write Memory Entity");
}

EArcTacticsActionStatus FArcTacticsWriteMemoryEntityStep::Tick(FArcTacticsStepContext& Context, FStructView InstanceData) const
{
	return ArcTacticsMemoryStepsPrivate::TickWrite(Context, InstanceData, &FArcTacticsWriteMemoryEntityStepInstanceData::Value);
}

const UScriptStruct* FArcTacticsWriteMemoryFloatStep::GetInstanceDataType() const
{
	return FArcTacticsWriteMemoryFloatStepInstanceData::StaticStruct();
}

FText FArcTacticsWriteMemoryFloatStep::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "WriteMemoryFloatStep", "Write Memory Float");
}

EArcTacticsActionStatus FArcTacticsWriteMemoryFloatStep::Tick(FArcTacticsStepContext& Context, FStructView InstanceData) const
{
	return ArcTacticsMemoryStepsPrivate::TickWrite(Context, InstanceData, &FArcTacticsWriteMemoryFloatStepInstanceData::Value);
}

const UScriptStruct* FArcTacticsWriteMemoryBoolStep::GetInstanceDataType() const
{
	return FArcTacticsWriteMemoryBoolStepInstanceData::StaticStruct();
}

FText FArcTacticsWriteMemoryBoolStep::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "WriteMemoryBoolStep", "Write Memory Bool");
}

EArcTacticsActionStatus FArcTacticsWriteMemoryBoolStep::Tick(FArcTacticsStepContext& Context, FStructView InstanceData) const
{
	return ArcTacticsMemoryStepsPrivate::TickWrite(Context, InstanceData, &FArcTacticsWriteMemoryBoolStepInstanceData::bValue);
}

const UScriptStruct* FArcTacticsWriteMemoryTagStep::GetInstanceDataType() const
{
	return FArcTacticsWriteMemoryTagStepInstanceData::StaticStruct();
}

FText FArcTacticsWriteMemoryTagStep::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "WriteMemoryTagStep", "Write Memory Tag");
}

EArcTacticsActionStatus FArcTacticsWriteMemoryTagStep::Tick(FArcTacticsStepContext& Context, FStructView InstanceData) const
{
	return ArcTacticsMemoryStepsPrivate::TickWrite(Context, InstanceData, &FArcTacticsWriteMemoryTagStepInstanceData::Value);
}

const UScriptStruct* FArcTacticsForgetMemoryStep::GetInstanceDataType() const
{
	return FArcTacticsForgetMemoryStepInstanceData::StaticStruct();
}

FText FArcTacticsForgetMemoryStep::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "ForgetMemoryStep", "Forget Memory");
}

EArcTacticsActionStatus FArcTacticsForgetMemoryStep::Tick(FArcTacticsStepContext& Context, FStructView InstanceData) const
{
	const FArcTacticsForgetMemoryStepInstanceData* Data = InstanceData.GetPtr<FArcTacticsForgetMemoryStepInstanceData>();
	FArcTacticsActionContext& Action = Context.Action;
	if (!Data || !Action.EntityManager)
	{
		return EArcTacticsActionStatus::Failed;
	}
	if (!ArcMassMemory::Forget(*Action.EntityManager, Action.Entity, Data->Key))
	{
		Action.FailureReason = ArcTacticsMemoryStepsPrivate::MemoryFragmentMissing;
		return EArcTacticsActionStatus::Failed;
	}
	return EArcTacticsActionStatus::Succeeded;
}
