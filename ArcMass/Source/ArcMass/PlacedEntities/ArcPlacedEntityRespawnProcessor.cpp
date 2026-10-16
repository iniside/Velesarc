// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/PlacedEntities/ArcPlacedEntityRespawnProcessor.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityRespawnTrackerTypes.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "Mass/CommandBuffer.h"
#include "MassCommonTypes.h"

UArcPlacedEntityRespawnProcessor::UArcPlacedEntityRespawnProcessor()
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true; // Sweep work calls AActor methods that mutate UPROPERTY state.
}

void UArcPlacedEntityRespawnProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	TrackerQuery.AddRequirement<FArcPlacedEntityRespawnTrackerFragment>(EMassFragmentAccess::ReadOnly);
	TrackerQuery.AddTagRequirement<FArcPlacedEntityRespawnTrackerTag>(EMassFragmentPresence::All);
	// No RegisterWithProcessor — TrackerQuery was bound via {*this} in the header.
}

void UArcPlacedEntityRespawnProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	AccumulatedTime += Context.GetDeltaTimeSeconds();
	if (AccumulatedTime < SweepInterval)
	{
		return;
	}
	AccumulatedTime = 0.0f;

	const int64 NowUtc = FDateTime::UtcNow().ToUnixTimestamp();

	TrackerQuery.ForEachEntityChunk(Context, [NowUtc](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FArcPlacedEntityRespawnTrackerFragment> Trackers =
			Ctx.GetFragmentView<FArcPlacedEntityRespawnTrackerFragment>();

		const int32 NumEntities = Ctx.GetNumEntities();
		for (int32 i = 0; i < NumEntities; ++i)
		{
			TWeakObjectPtr<AArcPlacedEntityPartitionActor> WeakActor = Trackers[i].OwnerPartitionActor;
			if (!WeakActor.IsValid())
			{
				UE_LOGF(LogMass, Warning,"ArcPlacedEntityRespawnProcessor: tracker entity has stale OwnerPartitionActor");
				continue;
			}

			Ctx.Defer().PushCommand<FMassDeferredSetCommand>(
				[WeakActor, NowUtc](FMassEntityManager& /*EM*/)
				{
					if (AArcPlacedEntityPartitionActor* Actor = WeakActor.Get())
					{
						Actor->RestoreExpiredRemovals(NowUtc);
					}
				});
		}
	});
}
