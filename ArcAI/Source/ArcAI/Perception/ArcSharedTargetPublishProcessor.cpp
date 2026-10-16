// Copyright Lukasz Baran. All Rights Reserved.

#include "Perception/ArcSharedTargetPublishProcessor.h"

#include "ArcKnowledgeSubsystem.h"
#include "Mass/ExecutionContext.h"
#include "Perception/ArcMassSightPerception.h"
#include "Perception/ArcSharedTargetKnowledge.h"
#include "Teams/ArcMassTeamFragment.h"
#include "Teams/ArcTeamAttitude.h"

UArcSharedTargetPublishProcessor::UArcSharedTargetPublishProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionOrder.ExecuteAfter.Add(TEXT("ArcMassSightPerceptionProcessor"));
	ExecutionOrder.ExecuteBefore.Add(TEXT("ArcTacticsUtilityProcessor"));
}

void UArcSharedTargetPublishProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProcessorRequirements.AddSubsystemRequirement<UArcKnowledgeSubsystem>(EMassFragmentAccess::ReadWrite);

	EntityQuery.AddRequirement<FArcMassSightPerceptionResult>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcMassTeamFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcPerceptionSightSenseConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddSubsystemRequirement<UArcKnowledgeSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UArcSharedTargetPublishProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcSharedTargetPublish);

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager](FMassExecutionContext& ChunkContext)
		{
			const TConstArrayView<FArcMassTeamFragment> Teams = ChunkContext.GetFragmentView<FArcMassTeamFragment>();
			if (Teams.IsEmpty())
			{
				return;
			}

			UArcKnowledgeSubsystem* Knowledge = ChunkContext.GetMutableSubsystem<UArcKnowledgeSubsystem>();
			if (!Knowledge)
			{
				return;
			}

			const FArcPerceptionSightSenseConfigFragment& Config =
				ChunkContext.GetConstSharedFragment<FArcPerceptionSightSenseConfigFragment>();
			const TConstArrayView<FArcMassSightPerceptionResult> SightResults =
				ChunkContext.GetFragmentView<FArcMassSightPerceptionResult>();

			const double RefreshInterval = static_cast<double>(Config.UpdateInterval);

			for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				// Publishing rides perception's cadence: the sight processor zeroes TimeSinceLastUpdate
				// on the tick it samples, so anything else is a re-walk of results already published.
				if (SightResults[EntityIt].TimeSinceLastUpdate != 0.0f)
				{
					continue;
				}

				const int32 TeamId = Teams[EntityIt].TeamId;
				if (TeamId == INDEX_NONE)
				{
					continue;
				}

				for (const FArcPerceivedEntity& Perceived : SightResults[EntityIt].PerceivedEntities)
				{
					// PerceivedEntities is memory, not sight: a record survives until ForgetTime, so the
					// handle may already be dead and must be validated before the random-access team read.
					if (!EntityManager.IsEntityActive(Perceived.Entity))
					{
						continue;
					}

					// Deliberately the opposite rule to the teamless perceiver above, which publishes
					// nothing because it has no side to publish for: a teamless target resolves Neutral
					// and a Neutral is a legitimate thing for a side to record. Only a Friendly is not.
					const FArcMassTeamFragment* TargetTeam =
						EntityManager.GetFragmentDataPtr<FArcMassTeamFragment>(Perceived.Entity);
					const int32 TargetTeamId = TargetTeam ? TargetTeam->TeamId : INDEX_NONE;
					if (ArcTeamAttitude::Resolve(TeamId, TargetTeamId) == EArcTeamAttitude::Friendly)
					{
						continue;
					}

					ArcSharedTargetKnowledge::PublishSighting(
						*Knowledge,
						Perceived.Entity,
						TeamId,
						Perceived.LastKnownLocation,
						Perceived.LastTimeSeen,
						RefreshInterval);
				}
			}
		});
}
