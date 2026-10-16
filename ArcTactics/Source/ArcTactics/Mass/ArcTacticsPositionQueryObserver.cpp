// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcTacticsPositionQueryObserver.h"

#include "Engine/World.h"
#include "Mass/ArcTacticsFragments.h"
#include "Mass/ArcTacticsInstanceSubsystem.h"
#include "Mass/EntityFragments.h"
#include "Mass/ExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassNavMeshNavigationFragments.h"
#include "Navigation/ArcPathFollow.h"
#include "Teams/ArcMassTeamFragment.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsPositionQueryObserver)

UArcTacticsPositionQueryObserver::UArcTacticsPositionQueryObserver()
{
	ObservedTypes.Add(FArcTacticsTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcTacticsPositionQueryObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>&)
{
	ObserverQuery.AddTagRequirement<FArcTacticsTag>(EMassFragmentPresence::All);
	ObserverQuery.AddRequirement<FArcTacticsInstanceFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	ObserverQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	ObserverQuery.AddRequirement<FAgentHeightFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	ObserverQuery.AddRequirement<FMassDesiredMovementFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	ObserverQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	ObserverQuery.AddRequirement<FMassNavMeshShortPathFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	ObserverQuery.AddRequirement<FMassNavMeshCachedPathFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	ObserverQuery.AddRequirement<FArcMassTeamFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	ObserverQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::Optional);
	ObserverQuery.AddSubsystemRequirement<UArcTacticsInstanceSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UArcTacticsPositionQueryObserver::Execute(
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
	ObserverQuery.ForEachEntityChunk(Context,
		[World, Instances, &EntityManager](FMassExecutionContext& ChunkContext)
		{
			const FMassMovementParameters* MovementParameters =
				ChunkContext.GetConstSharedFragmentPtr<FMassMovementParameters>();
			TArrayView<FArcTacticsInstanceFragment> InstanceFragments =
				ChunkContext.GetMutableFragmentView<FArcTacticsInstanceFragment>();
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
				FArcTacticsInstanceFragment& InstanceFragment = InstanceFragments[EntityIt];
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
					Instances->CancelAndFreeInstance(
						EntityManager, Entity, InstanceFragment.Handle, &PathContext);
				}
				else
				{
					Instances->CancelAndFreeInstance(
						EntityManager, Entity, InstanceFragment.Handle, nullptr);
				}
				InstanceFragment.Handle.Reset();
			}
		});
}
