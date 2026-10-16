// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcSmartObjectEatBehaviorDefinition.h"

#include "Mass/CommandBuffer.h"
#include "Mass/Commands.h"
#include "Mass/EntityManager.h"
#include "Sim/ArcBodyFragments.h"
#include "Sim/ArcSmartObjectBehaviorFragments.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcSmartObjectEatBehaviorDefinition)

void UArcSmartObjectEatBehaviorDefinition::Activate(
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
				FArcSmartObjectEatBehaviorFragment::StaticStruct(), Entity);
			FArcSmartObjectEatBehaviorFragment* Eat = View.GetPtr<FArcSmartObjectEatBehaviorFragment>();
			if (!Eat)
			{
				Eat = &EntityManager.AddSparseElementToEntity<FArcSmartObjectEatBehaviorFragment>(Entity);
			}
			Eat->CommandSerial = CommandSerial;
		});
}

void UArcSmartObjectEatBehaviorDefinition::Deactivate(
	FMassCommandBuffer& CommandBuffer,
	const FMassBehaviorEntityContext& EntityContext) const
{
	const FArcSmartObjectEatBehaviorFragment* Eat =
		EntityContext.EntityView.GetSparseFragmentDataPtr<FArcSmartObjectEatBehaviorFragment>();
	if (Eat)
	{
		const FMassEntityHandle Entity = EntityContext.EntityView.GetEntity();
		const uint32 CommandSerial = Eat->CommandSerial;
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
				FStructView EatView = EntityManager.GetMutableSparseElementDataForEntity(
					FArcSmartObjectEatBehaviorFragment::StaticStruct(), Entity);
				const FArcSmartObjectEatBehaviorFragment* CurrentEat =
					EatView.GetPtr<FArcSmartObjectEatBehaviorFragment>();
				if (Lifecycle && Lifecycle->CommandSerial == CommandSerial
					&& CurrentEat && CurrentEat->CommandSerial == CommandSerial)
				{
					EntityManager.RemoveSparseElementFromEntity<FArcSmartObjectEatBehaviorFragment>(Entity);
				}
			});
	}

	UArcSmartObjectMassBehaviorDefinition::Deactivate(CommandBuffer, EntityContext);
}
