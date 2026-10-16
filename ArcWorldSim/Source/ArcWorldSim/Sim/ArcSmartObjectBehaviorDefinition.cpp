// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcSmartObjectBehaviorDefinition.h"

#include "Mass/CommandBuffer.h"
#include "Mass/Commands.h"
#include "Mass/EntityManager.h"
#include "Sim/ArcBodyFragments.h"
#include "Sim/ArcSmartObjectBehaviorFragments.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcSmartObjectBehaviorDefinition)

void UArcSmartObjectMassBehaviorDefinition::Activate(
	FMassCommandBuffer& CommandBuffer,
	const FMassBehaviorEntityContext& EntityContext) const
{
	const FArcBodyFragment* Body = EntityContext.EntityView.GetFragmentDataPtr<FArcBodyFragment>();
	if (!Body || !Body->Command.IsActive())
	{
		return;
	}

	const FMassEntityHandle Entity = EntityContext.EntityView.GetEntity();
	const uint32 CommandSerial = Body->Command.Serial;
	const float Duration = UseTime;
	CommandBuffer.PushCommand<FMassDeferredCommand<EMassCommandOperationType::Set>>(
		[Entity, CommandSerial, Duration](FMassEntityManager& EntityManager)
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
				FArcSmartObjectBehaviorFragment::StaticStruct(), Entity);
			FArcSmartObjectBehaviorFragment* Lifecycle = View.GetPtr<FArcSmartObjectBehaviorFragment>();
			if (!Lifecycle)
			{
				Lifecycle = &EntityManager.AddSparseElementToEntity<FArcSmartObjectBehaviorFragment>(Entity);
			}

			Lifecycle->CommandSerial = CommandSerial;
			Lifecycle->UseElapsed = 0.0f;
			Lifecycle->UseDuration = Duration;
			Lifecycle->bEffectApplied = false;
			Lifecycle->bCompletionQueued = false;
		});
}

void UArcSmartObjectMassBehaviorDefinition::Deactivate(
	FMassCommandBuffer& CommandBuffer,
	const FMassBehaviorEntityContext& EntityContext) const
{
	const FArcSmartObjectBehaviorFragment* Lifecycle =
		EntityContext.EntityView.GetSparseFragmentDataPtr<FArcSmartObjectBehaviorFragment>();
	if (!Lifecycle)
	{
		return;
	}

	const FMassEntityHandle Entity = EntityContext.EntityView.GetEntity();
	const uint32 CommandSerial = Lifecycle->CommandSerial;
	CommandBuffer.PushCommand<FMassDeferredCommand<EMassCommandOperationType::Remove>>(
		[Entity, CommandSerial](FMassEntityManager& EntityManager)
		{
			if (!EntityManager.IsEntityValid(Entity))
			{
				return;
			}

			FStructView View = EntityManager.GetMutableSparseElementDataForEntity(
				FArcSmartObjectBehaviorFragment::StaticStruct(), Entity);
			const FArcSmartObjectBehaviorFragment* CurrentLifecycle = View.GetPtr<FArcSmartObjectBehaviorFragment>();
			if (CurrentLifecycle && CurrentLifecycle->CommandSerial == CommandSerial)
			{
				EntityManager.RemoveSparseElementFromEntity<FArcSmartObjectBehaviorFragment>(Entity);
			}
		});
}
