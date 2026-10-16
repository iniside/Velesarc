// Copyright Lukasz Baran. All Rights Reserved.

#include "Steps/ArcTacticsFocusActiveStep.h"

#include "Actions/ArcTacticsAction.h"
#include "ArcMass/ArcMassFragments.h"
#include "Mass/CommandBuffer.h"
#include "Mass/Commands.h"
#include "Mass/EntityManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsFocusActiveStep)

namespace ArcTacticsFocusActiveStepPrivate
{
	FArcMassFocusFragment* FindFocus(FMassEntityManager& EntityManager, const FMassEntityHandle Entity)
	{
		return EntityManager.GetMutableSparseElementDataForEntity(FArcMassFocusFragment::StaticStruct(), Entity)
			.GetPtr<FArcMassFocusFragment>();
	}
}

const UScriptStruct* FArcTacticsFocusActiveStep::GetInstanceDataType() const
{
	return FArcTacticsFocusActiveStepInstanceData::StaticStruct();
}

FText FArcTacticsFocusActiveStep::GetDisplayName() const
{
	return NSLOCTEXT("ArcTactics", "FocusActiveStep", "Focus");
}

bool FArcTacticsFocusActiveStep::Enter(FArcTacticsActionContext& Context, FStructView InstanceData) const
{
	const FArcTacticsFocusActiveStepInstanceData* Data = InstanceData.GetPtr<FArcTacticsFocusActiveStepInstanceData>();
	if (!Data || !Context.EntityManager || !Context.EntityManager->IsEntityActive(Context.Entity))
	{
		return false;
	}
	FArcMassFocusFragment NewFocus;
	if (Data->bUseTargetLocation)
	{
		if (Data->TargetLocation.ContainsNaN())
		{
			return false;
		}
		NewFocus.SetLocation(Data->TargetLocation);
	}
	else
	{
		if (!Data->Target.IsSet() || !Context.EntityManager->IsEntityActive(Data->Target))
		{
			return false;
		}
		NewFocus.SetEntity(Data->Target);
	}
	NewFocus.OwnerSerial = Context.RequestToken.ActionSerial;
	const FMassEntityHandle Entity = Context.Entity;
	Context.EntityManager->Defer().PushCommand<FMassDeferredSetCommand>(
		[Entity, NewFocus](FMassEntityManager& EntityManager)
		{
			if (!EntityManager.IsEntityActive(Entity))
			{
				return;
			}
			FArcMassFocusFragment* Focus = ArcTacticsFocusActiveStepPrivate::FindFocus(EntityManager, Entity);
			if (!Focus)
			{
				Focus = &EntityManager.AddSparseElementToEntity<FArcMassFocusFragment>(Entity);
			}
			*Focus = NewFocus;
		});
	return true;
}

void FArcTacticsFocusActiveStep::Exit(FArcTacticsActionContext& Context, FStructView) const
{
	if (!Context.EntityManager)
	{
		return;
	}
	const FMassEntityHandle Entity = Context.Entity;
	const uint32 OwnerSerial = Context.RequestToken.ActionSerial;
	Context.EntityManager->Defer().PushCommand<FMassDeferredSetCommand>(
		[Entity, OwnerSerial](FMassEntityManager& EntityManager)
		{
			if (!EntityManager.IsEntityActive(Entity))
			{
				return;
			}
			const FArcMassFocusFragment* Focus = ArcTacticsFocusActiveStepPrivate::FindFocus(EntityManager, Entity);
			if (Focus && Focus->OwnerSerial == OwnerSerial)
			{
				EntityManager.RemoveSparseElementFromEntity<FArcMassFocusFragment>(Entity);
			}
		});
}

EArcTacticsExecutionRequirement FArcTacticsFocusActiveStep::GetRequirements(FConstStructView) const
{
	return EArcTacticsExecutionRequirement::Focus;
}
