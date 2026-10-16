// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcTacticsMovementProcessor.h"

#include "Actions/ArcTacticsAction.h"
#include "Engine/World.h"
#include "Mass/ArcTacticsFragments.h"
#include "Mass/ArcTacticsInstanceSubsystem.h"
#include "Mass/ArcTacticsUtilityProcessor.h"
#include "Mass/EntityFragments.h"
#include "Mass/ExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassNavMeshNavigationFragments.h"
#include "Navigation/ArcPathFollow.h"
#include "Teams/ArcMassTeamFragment.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsMovementProcessor)

UArcTacticsMovementProcessor::UArcTacticsMovementProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionOrder.ExecuteAfter.Add(UArcTacticsUtilityProcessor::StaticClass()->GetFName());
}

void UArcTacticsMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>&)
{
	EntityQuery.AddTagRequirement<FArcTacticsTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FArcTacticsInstanceFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FAgentHeightFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassDesiredMovementFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassNavMeshShortPathFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassNavMeshCachedPathFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcMassTeamFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::Optional);
	EntityQuery.AddSubsystemRequirement<UArcTacticsInstanceSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UArcTacticsMovementProcessor::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	UWorld* World = Context.GetWorld();
	UArcTacticsInstanceSubsystem* Instances =
		World ? World->GetSubsystem<UArcTacticsInstanceSubsystem>() : nullptr;
	if (!World || !Instances)
	{
		return;
	}
	EntityQuery.ForEachEntityChunk(Context,
		[World, Instances, &EntityManager](FMassExecutionContext& ChunkContext)
		{
			const FMassMovementParameters* MovementParameters =
				ChunkContext.GetConstSharedFragmentPtr<FMassMovementParameters>();
			const TConstArrayView<FArcTacticsInstanceFragment> InstanceFragments =
				ChunkContext.GetFragmentView<FArcTacticsInstanceFragment>();
			TArrayView<FTransformFragment> Transforms =
				ChunkContext.GetMutableFragmentView<FTransformFragment>();
			const TConstArrayView<FAgentRadiusFragment> Radii =
				ChunkContext.GetFragmentView<FAgentRadiusFragment>();
			const TConstArrayView<FAgentHeightFragment> Heights =
				ChunkContext.GetFragmentView<FAgentHeightFragment>();
			const TConstArrayView<FMassDesiredMovementFragment> DesiredMovements =
				ChunkContext.GetFragmentView<FMassDesiredMovementFragment>();
			TArrayView<FMassMoveTargetFragment> MoveTargets =
				ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
			TArrayView<FMassNavMeshShortPathFragment> ShortPaths =
				ChunkContext.GetMutableFragmentView<FMassNavMeshShortPathFragment>();
			TArrayView<FMassNavMeshCachedPathFragment> CachedPaths =
				ChunkContext.GetMutableFragmentView<FMassNavMeshCachedPathFragment>();
			const TConstArrayView<FArcMassTeamFragment> Teams =
				ChunkContext.GetFragmentView<FArcMassTeamFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FMassEntityHandle Entity = ChunkContext.GetEntity(EntityIt);
				const FArcTacticsInstanceFragment& InstanceFragment = InstanceFragments[EntityIt];
				if (!InstanceFragment.bEnabled || !InstanceFragment.Handle.IsValid())
				{
					continue;
				}
				const bool bHasPathContext = MovementParameters
					&& !Transforms.IsEmpty() && !Radii.IsEmpty() && !Heights.IsEmpty()
					&& !DesiredMovements.IsEmpty() && !MoveTargets.IsEmpty()
					&& !ShortPaths.IsEmpty() && !CachedPaths.IsEmpty();
				if (bHasPathContext)
				{
					const ArcPathFollow::FContext PathContext{
						*World, Entity, Transforms[EntityIt], Radii[EntityIt], Heights[EntityIt],
						DesiredMovements[EntityIt], *MovementParameters, MoveTargets[EntityIt],
						ShortPaths[EntityIt], CachedPaths[EntityIt],
						Teams.IsEmpty() ? INDEX_NONE : Teams[EntityIt].TeamId};
					UArcTacticsUtilityProcessor::TickAction(
						*Instances, EntityManager, InstanceFragment.Handle,
						EArcTacticsActionTickLane::Movement, true, &PathContext);
				}
				else
				{
					UArcTacticsUtilityProcessor::TickAction(
						*Instances, EntityManager, InstanceFragment.Handle,
						EArcTacticsActionTickLane::Movement, true, nullptr);
				}
			}
		});
}
