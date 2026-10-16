// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcSmartObjectBehaviorDefinitions.h"

#include "Engine/World.h"
#include "Fragments/ArcNeedBands.h"
#include "Fragments/ArcNeedFragment.h"
#include "Mass/CommandBuffer.h"
#include "Mass/Commands.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Sim/ArcBodyFragments.h"
#include "Sim/ArcSmartObjectBehaviorFragments.h"

UArcSmartObjectSleepBehaviorDefinition::UArcSmartObjectSleepBehaviorDefinition()
{
	UseTime = 0.0f;
}

void UArcSmartObjectSleepBehaviorDefinition::Activate(
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
	const float SleepComfort = Comfort;
	CommandBuffer.PushCommand<FMassDeferredCommand<EMassCommandOperationType::Set>>(
		[Entity, CommandSerial, SleepComfort](FMassEntityManager& EntityManager)
		{
			if (!EntityManager.IsEntityValid(Entity))
			{
				return;
			}

			const FArcBodyFragment* CurrentBody = EntityManager.GetFragmentDataPtr<FArcBodyFragment>(Entity);
			FArcFatigueNeedFragment* Fatigue = EntityManager.GetFragmentDataPtr<FArcFatigueNeedFragment>(Entity);
			UWorld* World = EntityManager.GetWorld();
			if (!CurrentBody || CurrentBody->State != EArcBodyState::Interaction
				|| CurrentBody->Command.Serial != CommandSerial || !Fatigue || !World)
			{
				return;
			}

			const FMassEntityView EntityView(EntityManager, Entity);
			const FArcNeedBandTableFragment* BandTable =
				EntityView.GetConstSharedFragmentDataPtr<FArcNeedBandTableFragment>();
			const float SecondsPerGameHour = BandTable
				? BandTable->SecondsPerGameHour
				: ArcNeeds::DefaultSecondsPerGameHour;
			const double Now = World->GetTimeSeconds();
			ArcNeeds::AdvanceNeed(*Fatigue, Now, SecondsPerGameHour, true);
			Fatigue->LastUpdateTime = Now;

			FStructView View = EntityManager.GetMutableSparseElementDataForEntity(
				FArcSmartObjectSleepBehaviorFragment::StaticStruct(), Entity);
			FArcSmartObjectSleepBehaviorFragment* Sleep =
				View.GetPtr<FArcSmartObjectSleepBehaviorFragment>();
			if (!Sleep)
			{
				Sleep = &EntityManager.AddSparseElementToEntity<FArcSmartObjectSleepBehaviorFragment>(Entity);
			}

			Sleep->CommandSerial = CommandSerial;
			Sleep->LastRecoveryTimeSeconds = Now;
			Sleep->Comfort = SleepComfort;
		});
}

void UArcSmartObjectSleepBehaviorDefinition::Deactivate(
	FMassCommandBuffer& CommandBuffer,
	const FMassBehaviorEntityContext& EntityContext) const
{
	const FArcSmartObjectSleepBehaviorFragment* Sleep =
		EntityContext.EntityView.GetSparseFragmentDataPtr<FArcSmartObjectSleepBehaviorFragment>();
	if (Sleep)
	{
		const FMassEntityHandle Entity = EntityContext.EntityView.GetEntity();
		const uint32 CommandSerial = Sleep->CommandSerial;
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
				FStructView SleepView = EntityManager.GetMutableSparseElementDataForEntity(
					FArcSmartObjectSleepBehaviorFragment::StaticStruct(), Entity);
				const FArcSmartObjectSleepBehaviorFragment* CurrentSleep =
					SleepView.GetPtr<FArcSmartObjectSleepBehaviorFragment>();
				if (Lifecycle && Lifecycle->CommandSerial == CommandSerial
					&& CurrentSleep && CurrentSleep->CommandSerial == CommandSerial)
				{
					EntityManager.RemoveSparseElementFromEntity<FArcSmartObjectSleepBehaviorFragment>(Entity);
				}
			});
	}

	UArcSmartObjectMassBehaviorDefinition::Deactivate(CommandBuffer, EntityContext);
}
