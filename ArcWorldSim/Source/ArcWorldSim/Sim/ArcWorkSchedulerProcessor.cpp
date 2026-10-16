// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcWorkSchedulerProcessor.h"

#include "Mass/EntityManager.h"
#include "Mass/ExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "MassSmartObjectFragments.h"
#include "Sim/ArcBodyAssignmentFragments.h"
#include "Sim/ArcBodyFragments.h"
#include "Sim/ArcBodyProcessor.h"
#include "Sim/ArcBodySlotSelection.h"
#include "Sim/ArcMatchProcessor.h"
#include "Sim/ArcWorldSimTags.h"
#include "SmartObjectSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcWorkSchedulerProcessor)

UArcWorkSchedulerProcessor::UArcWorkSchedulerProcessor()
	: BodyQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(UArcMatchProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteBefore.Add(UArcBodyProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
}

void UArcWorkSchedulerProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProcessorRequirements.AddSubsystemRequirement<USmartObjectSubsystem>(EMassFragmentAccess::ReadWrite);
	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);

	BodyQuery.Initialize(EntityManager);
	BodyQuery.AddRequirement<FArcBodyFragment>(EMassFragmentAccess::ReadWrite);
	BodyQuery.AddRequirement<FArcWorkAssignmentFragment>(EMassFragmentAccess::ReadOnly);
	BodyQuery.AddRequirement<FMassSmartObjectUserFragment>(EMassFragmentAccess::ReadWrite);
	BodyQuery.RegisterWithProcessor(*this);
}

void UArcWorkSchedulerProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	USmartObjectSubsystem* SmartObjectSubsystem = Context.GetMutableSubsystem<USmartObjectSubsystem>();
	UMassSignalSubsystem* SignalSubsystem = Context.GetMutableSubsystem<UMassSignalSubsystem>();
	if (!SmartObjectSubsystem || !SignalSubsystem)
	{
		return;
	}

	BodyQuery.ForEachEntityChunk(Context,
		[&EntityManager, SmartObjectSubsystem, SignalSubsystem](FMassExecutionContext& ChunkContext)
		{
			TArrayView<FArcBodyFragment> Bodies = ChunkContext.GetMutableFragmentView<FArcBodyFragment>();
			const TConstArrayView<FArcWorkAssignmentFragment> WorkAssignments =
				ChunkContext.GetFragmentView<FArcWorkAssignmentFragment>();
			TArrayView<FMassSmartObjectUserFragment> Users =
				ChunkContext.GetMutableFragmentView<FMassSmartObjectUserFragment>();
			for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcBodyFragment& Body = Bodies[EntityIt];
				const FMassEntityHandle Workplace = WorkAssignments[EntityIt].Workplace;
				if (Body.State != EArcBodyState::Idle
					|| Body.Command.IsActive()
					|| !Workplace.IsValid()
					|| !EntityManager.IsEntityValid(Workplace))
				{
					continue;
				}

				ArcWorldSim::TryAssignSlot(
					EntityManager,
					ChunkContext,
					*SmartObjectSubsystem,
					*SignalSubsystem,
					ChunkContext.GetEntity(EntityIt),
					Body,
					Users[EntityIt],
					Workplace,
					ArcWorldSimTags::Slot_Worker);
			}
		});
}
