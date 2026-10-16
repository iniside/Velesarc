// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcSmartObjectBehaviorDefinitions.h"

#include "Mass/CommandBuffer.h"
#include "Mass/Commands.h"
#include "Mass/EntityManager.h"
#include "Sim/ArcBodyFragments.h"
#include "Sim/ArcSmartObjectBehaviorFragments.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcSmartObjectBehaviorDefinitions)

namespace ArcSmartObjectBehaviorDefinitionsPrivate
{
template<typename TBehaviorFragment>
void ActivateBehavior(
	FMassCommandBuffer& CommandBuffer,
	const FMassBehaviorEntityContext& EntityContext)
{
	const FArcBodyFragment* Body = EntityContext.EntityView.GetFragmentDataPtr<FArcBodyFragment>();
	if (!Body || !Body->Command.IsActive())
	{
		return;
	}

	const FMassEntityHandle Entity = EntityContext.EntityView.GetEntity();
	const uint32 CommandSerial = Body->Command.Serial;
	CommandBuffer.PushCommand<FMassDeferredCommand<EMassCommandOperationType::Set>>(
		[Entity, CommandSerial](FMassEntityManager& EntityManager)
		{
			if (!EntityManager.IsEntityValid(Entity))
			{
				return;
			}

			const FArcBodyFragment* CurrentBody = EntityManager.GetFragmentDataPtr<FArcBodyFragment>(Entity);
			if (!CurrentBody || CurrentBody->State != EArcBodyState::Interaction
				|| CurrentBody->Command.Serial != CommandSerial)
			{
				return;
			}

			FStructView View = EntityManager.GetMutableSparseElementDataForEntity(
				TBehaviorFragment::StaticStruct(), Entity);
			TBehaviorFragment* Behavior = View.template GetPtr<TBehaviorFragment>();
			if (!Behavior)
			{
				Behavior = &EntityManager.AddSparseElementToEntity<TBehaviorFragment>(Entity);
			}
			Behavior->CommandSerial = CommandSerial;
		});
}

template<typename TBehaviorFragment>
void DeactivateBehavior(
	FMassCommandBuffer& CommandBuffer,
	const FMassBehaviorEntityContext& EntityContext)
{
	const TBehaviorFragment* Behavior =
		EntityContext.EntityView.GetSparseFragmentDataPtr<TBehaviorFragment>();
	if (!Behavior)
	{
		return;
	}

	const FMassEntityHandle Entity = EntityContext.EntityView.GetEntity();
	const uint32 CommandSerial = Behavior->CommandSerial;
	CommandBuffer.PushCommand<FMassDeferredCommand<EMassCommandOperationType::Remove>>(
		[Entity, CommandSerial](FMassEntityManager& EntityManager)
		{
			if (!EntityManager.IsEntityValid(Entity))
			{
				return;
			}

			FStructView LifecycleView = EntityManager.GetMutableSparseElementDataForEntity(
				FArcSmartObjectBehaviorFragment::StaticStruct(), Entity);
			const FArcSmartObjectBehaviorFragment* Lifecycle =
				LifecycleView.GetPtr<FArcSmartObjectBehaviorFragment>();
			FStructView BehaviorView = EntityManager.GetMutableSparseElementDataForEntity(
				TBehaviorFragment::StaticStruct(), Entity);
			const TBehaviorFragment* CurrentBehavior = BehaviorView.template GetPtr<TBehaviorFragment>();
			if (Lifecycle && Lifecycle->CommandSerial == CommandSerial
				&& CurrentBehavior && CurrentBehavior->CommandSerial == CommandSerial)
			{
				EntityManager.RemoveSparseElementFromEntity<TBehaviorFragment>(Entity);
			}
		});
}
}

void UArcSmartObjectWorkBehaviorDefinition::Activate(
	FMassCommandBuffer& CommandBuffer,
	const FMassBehaviorEntityContext& EntityContext) const
{
	UArcSmartObjectMassBehaviorDefinition::Activate(CommandBuffer, EntityContext);

	const FArcBodyFragment* Body = EntityContext.EntityView.GetFragmentDataPtr<FArcBodyFragment>();
	if (!Body || !Body->Command.IsActive())
	{
		return;
	}

	const FMassEntityHandle Entity = EntityContext.EntityView.GetEntity();
	const uint32 CommandSerial = Body->Command.Serial;
	const float ShiftDuration = WorkShiftDuration;
	const float LoopDuration = WorkLoopDuration;
	CommandBuffer.PushCommand<FMassDeferredCommand<EMassCommandOperationType::Set>>(
		[Entity, CommandSerial, ShiftDuration, LoopDuration](FMassEntityManager& EntityManager)
		{
			if (!EntityManager.IsEntityValid(Entity))
			{
				return;
			}

			const FArcBodyFragment* CurrentBody = EntityManager.GetFragmentDataPtr<FArcBodyFragment>(Entity);
			if (!CurrentBody || CurrentBody->State != EArcBodyState::Interaction
				|| CurrentBody->Command.Serial != CommandSerial)
			{
				return;
			}

			FStructView View = EntityManager.GetMutableSparseElementDataForEntity(
				FArcSmartObjectWorkBehaviorFragment::StaticStruct(), Entity);
			FArcSmartObjectWorkBehaviorFragment* Work = View.GetPtr<FArcSmartObjectWorkBehaviorFragment>();
			if (!Work)
			{
				Work = &EntityManager.AddSparseElementToEntity<FArcSmartObjectWorkBehaviorFragment>(Entity);
			}

			Work->CommandSerial = CommandSerial;
			Work->WorkShiftElapsed = 0.0f;
			Work->WorkShiftDuration = ShiftDuration;
			Work->WorkLoopElapsed = 0.0f;
			Work->WorkLoopDuration = LoopDuration;
		});
}

void UArcSmartObjectWorkBehaviorDefinition::Deactivate(
	FMassCommandBuffer& CommandBuffer,
	const FMassBehaviorEntityContext& EntityContext) const
{
	ArcSmartObjectBehaviorDefinitionsPrivate::DeactivateBehavior<FArcSmartObjectWorkBehaviorFragment>(
		CommandBuffer, EntityContext);
	UArcSmartObjectMassBehaviorDefinition::Deactivate(CommandBuffer, EntityContext);
}

void UArcSmartObjectPickupBehaviorDefinition::Activate(
	FMassCommandBuffer& CommandBuffer,
	const FMassBehaviorEntityContext& EntityContext) const
{
	UArcSmartObjectMassBehaviorDefinition::Activate(CommandBuffer, EntityContext);
	ArcSmartObjectBehaviorDefinitionsPrivate::ActivateBehavior<FArcSmartObjectPickupBehaviorFragment>(
		CommandBuffer, EntityContext);
}

void UArcSmartObjectPickupBehaviorDefinition::Deactivate(
	FMassCommandBuffer& CommandBuffer,
	const FMassBehaviorEntityContext& EntityContext) const
{
	ArcSmartObjectBehaviorDefinitionsPrivate::DeactivateBehavior<FArcSmartObjectPickupBehaviorFragment>(
		CommandBuffer, EntityContext);
	UArcSmartObjectMassBehaviorDefinition::Deactivate(CommandBuffer, EntityContext);
}

void UArcSmartObjectDropBehaviorDefinition::Activate(
	FMassCommandBuffer& CommandBuffer,
	const FMassBehaviorEntityContext& EntityContext) const
{
	UArcSmartObjectMassBehaviorDefinition::Activate(CommandBuffer, EntityContext);
	ArcSmartObjectBehaviorDefinitionsPrivate::ActivateBehavior<FArcSmartObjectDropBehaviorFragment>(
		CommandBuffer, EntityContext);
}

void UArcSmartObjectDropBehaviorDefinition::Deactivate(
	FMassCommandBuffer& CommandBuffer,
	const FMassBehaviorEntityContext& EntityContext) const
{
	ArcSmartObjectBehaviorDefinitionsPrivate::DeactivateBehavior<FArcSmartObjectDropBehaviorFragment>(
		CommandBuffer, EntityContext);
	UArcSmartObjectMassBehaviorDefinition::Deactivate(CommandBuffer, EntityContext);
}

void UArcSmartObjectGatherBehaviorDefinition::Activate(
	FMassCommandBuffer& CommandBuffer,
	const FMassBehaviorEntityContext& EntityContext) const
{
	UArcSmartObjectMassBehaviorDefinition::Activate(CommandBuffer, EntityContext);
	ArcSmartObjectBehaviorDefinitionsPrivate::ActivateBehavior<FArcSmartObjectGatherBehaviorFragment>(
		CommandBuffer, EntityContext);
}

void UArcSmartObjectGatherBehaviorDefinition::Deactivate(
	FMassCommandBuffer& CommandBuffer,
	const FMassBehaviorEntityContext& EntityContext) const
{
	ArcSmartObjectBehaviorDefinitionsPrivate::DeactivateBehavior<FArcSmartObjectGatherBehaviorFragment>(
		CommandBuffer, EntityContext);
	UArcSmartObjectMassBehaviorDefinition::Deactivate(CommandBuffer, EntityContext);
}

void UArcSmartObjectDrawBehaviorDefinition::Activate(
	FMassCommandBuffer& CommandBuffer,
	const FMassBehaviorEntityContext& EntityContext) const
{
	UArcSmartObjectMassBehaviorDefinition::Activate(CommandBuffer, EntityContext);
	ArcSmartObjectBehaviorDefinitionsPrivate::ActivateBehavior<FArcSmartObjectDrawBehaviorFragment>(
		CommandBuffer, EntityContext);
}

void UArcSmartObjectDrawBehaviorDefinition::Deactivate(
	FMassCommandBuffer& CommandBuffer,
	const FMassBehaviorEntityContext& EntityContext) const
{
	ArcSmartObjectBehaviorDefinitionsPrivate::DeactivateBehavior<FArcSmartObjectDrawBehaviorFragment>(
		CommandBuffer, EntityContext);
	UArcSmartObjectMassBehaviorDefinition::Deactivate(CommandBuffer, EntityContext);
}
