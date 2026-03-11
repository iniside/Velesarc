// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaQueryTasks.h"
#include "MassStateTreeExecutionContext.h"
#include "ArcAreaSubsystem.h"
#include "ArcArea/Mass/ArcAreaFragments.h"
#include "MassEntityView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAreaQueryTasks)

namespace UE::ArcArea::Private
{
	/** Shared helper: extract AreaData for the executing entity. Returns nullptr on failure. */
	static const FArcAreaData* GetAreaDataForEntity(
		const FStateTreeExecutionContext& Context,
		const FMassStateTreeExecutionContext*& OutMassCtx)
	{
		OutMassCtx = &static_cast<const FMassStateTreeExecutionContext&>(Context);
		const FMassEntityView EntityView(OutMassCtx->GetEntityManager(), OutMassCtx->GetEntity());

		const FArcAreaFragment* AreaFragment = EntityView.GetFragmentDataPtr<FArcAreaFragment>();
		if (!AreaFragment || !AreaFragment->AreaHandle.IsValid())
		{
			return nullptr;
		}

		const UWorld* World = OutMassCtx->GetWorld();
		const UArcAreaSubsystem* Subsystem = World ? World->GetSubsystem<UArcAreaSubsystem>() : nullptr;
		if (!Subsystem)
		{
			return nullptr;
		}

		return Subsystem->GetAreaData(AreaFragment->AreaHandle);
	}
}

// ------------------------------------------------------------------
// Get Area Info
// ------------------------------------------------------------------

EStateTreeRunStatus FArcAreaGetAreaInfoTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData = {};

	const FMassStateTreeExecutionContext* MassCtx = nullptr;
	const FArcAreaData* AreaData = UE::ArcArea::Private::GetAreaDataForEntity(Context, MassCtx);
	if (!AreaData)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.AreaHandle = AreaData->Handle;
	InstanceData.AreaTags = AreaData->AreaTags;
	InstanceData.Location = AreaData->Location;
	InstanceData.SmartObjectHandle = AreaData->SmartObjectHandle;

	return EStateTreeRunStatus::Succeeded;
}

// ------------------------------------------------------------------
// Get Slot Counts
// ------------------------------------------------------------------

EStateTreeRunStatus FArcAreaGetSlotCountsTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData = {};

	const FMassStateTreeExecutionContext* MassCtx = nullptr;
	const FArcAreaData* AreaData = UE::ArcArea::Private::GetAreaDataForEntity(Context, MassCtx);
	if (!AreaData)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.TotalSlots = AreaData->Slots.Num();
	for (const FArcAreaSlotRuntime& Slot : AreaData->Slots)
	{
		switch (Slot.State)
		{
		case EArcAreaSlotState::Vacant:   ++InstanceData.VacantCount; break;
		case EArcAreaSlotState::Assigned: ++InstanceData.AssignedCount; break;
		case EArcAreaSlotState::Active:   ++InstanceData.ActiveCount; break;
		case EArcAreaSlotState::Disabled: ++InstanceData.DisabledCount; break;
		}
	}

	return EStateTreeRunStatus::Succeeded;
}

// ------------------------------------------------------------------
// Get Slot State
// ------------------------------------------------------------------

EStateTreeRunStatus FArcAreaGetSlotStateTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.SlotState = EArcAreaSlotState::Vacant;
	InstanceData.AssignedEntity = FMassEntityHandle();

	if (!InstanceData.SlotHandle.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	const FMassStateTreeExecutionContext& MassCtx = static_cast<const FMassStateTreeExecutionContext&>(Context);
	const UWorld* World = MassCtx.GetWorld();
	const UArcAreaSubsystem* Subsystem = World ? World->GetSubsystem<UArcAreaSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FArcAreaData* AreaData = Subsystem->GetAreaData(InstanceData.SlotHandle.AreaHandle);
	if (!AreaData || !AreaData->Slots.IsValidIndex(InstanceData.SlotHandle.SlotIndex))
	{
		return EStateTreeRunStatus::Failed;
	}

	const FArcAreaSlotRuntime& Slot = AreaData->Slots[InstanceData.SlotHandle.SlotIndex];
	InstanceData.SlotState = Slot.State;
	InstanceData.AssignedEntity = Slot.AssignedEntity;

	return EStateTreeRunStatus::Succeeded;
}

// ------------------------------------------------------------------
// Get Slot Definition
// ------------------------------------------------------------------

EStateTreeRunStatus FArcAreaGetSlotDefinitionTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bAutoPostVacancy = false;
	InstanceData.VacancyRelevance = 0.0f;

	if (!InstanceData.SlotHandle.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	const FMassStateTreeExecutionContext& MassCtx = static_cast<const FMassStateTreeExecutionContext&>(Context);
	const UWorld* World = MassCtx.GetWorld();
	const UArcAreaSubsystem* Subsystem = World ? World->GetSubsystem<UArcAreaSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FArcAreaData* AreaData = Subsystem->GetAreaData(InstanceData.SlotHandle.AreaHandle);
	if (!AreaData || !AreaData->SlotDefinitions.IsValidIndex(InstanceData.SlotHandle.SlotIndex))
	{
		return EStateTreeRunStatus::Failed;
	}

	const FArcAreaSlotDefinition& SlotDef = AreaData->SlotDefinitions[InstanceData.SlotHandle.SlotIndex];
	InstanceData.bAutoPostVacancy = SlotDef.VacancyConfig.bAutoPostVacancy;
	InstanceData.VacancyRelevance = SlotDef.VacancyConfig.VacancyRelevance;

	return EStateTreeRunStatus::Succeeded;
}

// ------------------------------------------------------------------
// Find Vacant Slot
// ------------------------------------------------------------------

EStateTreeRunStatus FArcAreaFindVacantSlotTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.SlotHandle = FArcAreaSlotHandle();
	InstanceData.bFound = false;

	const FMassStateTreeExecutionContext* MassCtx = nullptr;
	const FArcAreaData* AreaData = UE::ArcArea::Private::GetAreaDataForEntity(Context, MassCtx);
	if (!AreaData)
	{
		return EStateTreeRunStatus::Failed;
	}

	for (int32 i = 0; i < AreaData->Slots.Num(); ++i)
	{
		if (AreaData->Slots[i].State == EArcAreaSlotState::Vacant)
		{
			InstanceData.SlotHandle = FArcAreaSlotHandle(AreaData->Handle, i);
			InstanceData.bFound = true;
			return EStateTreeRunStatus::Succeeded;
		}
	}

	return EStateTreeRunStatus::Failed;
}
