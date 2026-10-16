// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcTacticsLifecycleProcessor.h"

#include "Decision/ArcTacticsConfig.h"
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
#include "Perception/ArcMassSightPerception.h"
#include "Teams/ArcMassTeamFragment.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsLifecycleProcessor)

UArcTacticsLifecycleProcessor::UArcTacticsLifecycleProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionOrder.ExecuteAfter.Add(UArcMassSightPerceptionProcessor::StaticClass()->GetFName());
	ExecutionOrder.ExecuteBefore.Add(UArcTacticsUtilityProcessor::StaticClass()->GetFName());
}

void UArcTacticsLifecycleProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>&)
{
	EntityQuery.AddTagRequirement<FArcTacticsTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FArcTacticsInstanceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FAgentHeightFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassDesiredMovementFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassNavMeshShortPathFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassNavMeshCachedPathFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcMassTeamFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcTacticsConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::Optional);
	EntityQuery.AddSubsystemRequirement<UArcTacticsInstanceSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UArcTacticsLifecycleProcessor::Execute(
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
			const FArcTacticsConfigFragment& ConfigFragment =
				ChunkContext.GetConstSharedFragment<FArcTacticsConfigFragment>();
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
				const bool bAllocated = Instances->Resolve(InstanceFragment.Handle) != nullptr;
				if (InstanceFragment.bEnabled && bAllocated && Instances->IsCurrentRevision(InstanceFragment.Handle))
				{
					continue;
				}
				if (InstanceFragment.bEnabled && !bAllocated)
				{
					InstanceFragment.Handle = Instances->AllocateInstance(
						EntityManager, Entity, ConfigFragment.Configs, ConfigFragment.Settings);
					InstanceFragment.bEnabled = InstanceFragment.Handle.IsValid();
					Instances->RequestDecision(InstanceFragment.Handle);
					continue;
				}
				TOptional<ArcPathFollow::FContext> PathContext;
				if (bHasPathContext)
				{
					PathContext.Emplace(ArcPathFollow::FContext{
						*World, Entity, Transforms[EntityIt], Radii[EntityIt], Heights[EntityIt],
						DesiredMovements[EntityIt], *MovementParameters, MoveTargets[EntityIt],
						ShortPaths[EntityIt], CachedPaths[EntityIt],
						Teams.IsEmpty() ? INDEX_NONE : Teams[EntityIt].TeamId});
				}
				if (!InstanceFragment.bEnabled)
				{
					Instances->CancelAndFreeInstance(
						EntityManager, Entity, InstanceFragment.Handle, PathContext.GetPtrOrNull());
					InstanceFragment.Handle.Reset();
					continue;
				}
				InstanceFragment.Handle = Instances->ReinitializeInstance(
					EntityManager, Entity, InstanceFragment.Handle, ConfigFragment.Configs,
					ConfigFragment.Settings, PathContext.GetPtrOrNull());
				InstanceFragment.bEnabled = InstanceFragment.Handle.IsValid();
				Instances->RequestDecision(InstanceFragment.Handle);
			}
		});
}
