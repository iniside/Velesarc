// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcBodyProcessor.h"

#include "ArcMass/Debugger/ArcEconomyTrace.h"
#include "Engine/World.h"
#include "Mass/ExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassNavMeshNavigationFragments.h"
#include "MassSignalSubsystem.h"
#include "MassSmartObjectFragments.h"
#include "MassSmartObjectHandler.h"
#include "MassSubsystemAccess.h"
#include "Navigation/ArcPathFollow.h"
#include "NavigationData.h"
#include "Sim/ArcBodyCommandLogic.h"
#include "Sim/ArcBodyFragments.h"
#include "SmartObjectSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcBodyProcessor)

namespace ArcBodyProcessorPrivate
{
enum class EWalkResult : uint8
{
	NoPath,
	Walking,
	Arrived
};

const TCHAR* StateName(const EArcBodyState State)
{
	switch (State)
	{
	case EArcBodyState::Idle:
		return TEXT("Idle");
	case EArcBodyState::MoveTo:
		return TEXT("MoveTo");
	case EArcBodyState::Interaction:
		return TEXT("Interaction");
	}
	return TEXT("Unknown");
}

bool NeedsNewPath(
	const FMassNavMeshCachedPathFragment& CachedPath,
	const FVector& Destination,
	const float ArrivalDistance)
{
	if (!CachedPath.NavPath.IsValid())
	{
		return true;
	}

	const TArray<FNavPathPoint>& PathPoints = CachedPath.NavPath->GetPathPoints();
	return PathPoints.IsEmpty()
		|| !ArcPathFollow::IsArrived(PathPoints.Last().Location, Destination, ArrivalDistance);
}

EWalkResult Walk(
	const ArcPathFollow::FContext& PathContext,
	const ArcPathFollow::FParams& PathParams,
	const FVector& Destination)
{
	if (NeedsNewPath(PathContext.CachedPath, Destination, PathParams.ArrivalDistance)
		&& !ArcPathFollow::RequestPath(PathContext, PathParams, Destination, false))
	{
		return EWalkResult::NoPath;
	}

	if (PathContext.CachedPath.Corridor.IsValid()
		&& PathContext.ShortPath.IsDone()
		&& PathContext.ShortPath.bPartialResult)
	{
		ArcPathFollow::UpdateShortPath(PathContext, PathParams);
	}

	const FVector Location = PathContext.Transform.GetTransform().GetLocation();
	return ArcPathFollow::IsArrived(Location, Destination, PathParams.ArrivalDistance)
		? EWalkResult::Arrived
		: EWalkResult::Walking;
}
}

UArcBodyProcessor::UArcBodyProcessor()
	: EntityQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Tasks);
	bRequiresGameThreadExecution = true;
	bAutoRegisterWithProcessingPhases = true;
}

void UArcBodyProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProcessorRequirements.AddSubsystemRequirement<USmartObjectSubsystem>(EMassFragmentAccess::ReadWrite);
	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);

	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FArcBodyFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAgentHeightFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassDesiredMovementFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassNavMeshShortPathFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassNavMeshCachedPathFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassSmartObjectUserFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::All);
	EntityQuery.AddSubsystemRequirement<USmartObjectSubsystem>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	EntityQuery.RegisterWithProcessor(*this);
}

void UArcBodyProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	USmartObjectSubsystem* SmartObjectSubsystem = Context.GetMutableSubsystem<USmartObjectSubsystem>();
	UMassSignalSubsystem* SignalSubsystem = Context.GetMutableSubsystem<UMassSignalSubsystem>();
	if (!World || !SmartObjectSubsystem || !SignalSubsystem)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, World, SmartObjectSubsystem, SignalSubsystem](FMassExecutionContext& ChunkContext)
		{
			TArrayView<FArcBodyFragment> Bodies = ChunkContext.GetMutableFragmentView<FArcBodyFragment>();
			TArrayView<FTransformFragment> Transforms = ChunkContext.GetMutableFragmentView<FTransformFragment>();
			const TConstArrayView<FAgentRadiusFragment> AgentRadii = ChunkContext.GetFragmentView<FAgentRadiusFragment>();
			const TConstArrayView<FAgentHeightFragment> AgentHeights = ChunkContext.GetFragmentView<FAgentHeightFragment>();
			const TConstArrayView<FMassDesiredMovementFragment> DesiredMovements = ChunkContext.GetFragmentView<FMassDesiredMovementFragment>();
			TArrayView<FMassMoveTargetFragment> MoveTargets = ChunkContext.GetMutableFragmentView<FMassMoveTargetFragment>();
			TArrayView<FMassNavMeshShortPathFragment> ShortPaths = ChunkContext.GetMutableFragmentView<FMassNavMeshShortPathFragment>();
			TArrayView<FMassNavMeshCachedPathFragment> CachedPaths = ChunkContext.GetMutableFragmentView<FMassNavMeshCachedPathFragment>();
			TArrayView<FMassSmartObjectUserFragment> Users = ChunkContext.GetMutableFragmentView<FMassSmartObjectUserFragment>();
			const FMassMovementParameters& MovementParameters = ChunkContext.GetConstSharedFragment<FMassMovementParameters>();

			for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcBodyFragment& Body = Bodies[EntityIt];
				FMassSmartObjectUserFragment& User = Users[EntityIt];
				const FMassEntityHandle Entity = ChunkContext.GetEntity(EntityIt);
				const EArcBodyState OldState = Body.State;

				if (Body.State == EArcBodyState::Idle)
				{
					if (Body.Command.IsActive())
					{
						Body.State = EArcBodyState::MoveTo;
					}
					continue;
				}

				if (!Body.Command.IsActive())
				{
					Body.State = EArcBodyState::Idle;
					continue;
				}

				const uint32 CommandSerial = Body.Command.Serial;
				if (Body.State == EArcBodyState::Interaction && Body.Command.bStartedUsing
					&& User.InteractionStatus == EMassSmartObjectInteractionStatus::BehaviorCompleted)
				{
					ArcBodyCommand::FinalizeCommand(
						ChunkContext,
						*SmartObjectSubsystem,
						*SignalSubsystem,
						Entity,
						Body,
						User,
						CommandSerial,
						Body.Command.BehaviorResult);
				}
				else if (Body.State == EArcBodyState::Interaction && Body.Command.bStartedUsing
					&& User.InteractionStatus == EMassSmartObjectInteractionStatus::Aborted)
				{
					ArcBodyCommand::FinalizeCommand(
						ChunkContext,
						*SmartObjectSubsystem,
						*SignalSubsystem,
						Entity,
						Body,
						User,
						CommandSerial,
						EArcBodyCommandResult::Aborted);
				}
				else if (!EntityManager.IsEntityValid(Body.Command.TargetEntity)
					|| !SmartObjectSubsystem->IsClaimedSmartObjectValid(Body.Command.ClaimHandle)
					|| !SmartObjectSubsystem->GetSlotTransform(Body.Command.ClaimHandle).IsSet()
					|| (Body.Command.bStartedUsing && User.InteractionHandle != Body.Command.ClaimHandle))
				{
					ArcBodyCommand::FinalizeCommand(
						ChunkContext,
						*SmartObjectSubsystem,
						*SignalSubsystem,
						Entity,
						Body,
						User,
						CommandSerial,
						EArcBodyCommandResult::Aborted);
				}
				else if (Body.State == EArcBodyState::MoveTo)
				{
					const ArcPathFollow::FContext PathContext{
						*World,
						Entity,
						Transforms[EntityIt],
						AgentRadii[EntityIt],
						AgentHeights[EntityIt],
						DesiredMovements[EntityIt],
						MovementParameters,
						MoveTargets[EntityIt],
						ShortPaths[EntityIt],
						CachedPaths[EntityIt]
					};
					const ArcPathFollow::FParams PathParameters;
					const ArcBodyProcessorPrivate::EWalkResult WalkResult = ArcBodyProcessorPrivate::Walk(
						PathContext, PathParameters, Body.Command.TargetLocation);
					if (WalkResult == ArcBodyProcessorPrivate::EWalkResult::NoPath)
					{
						ArcBodyCommand::FinalizeCommand(
							ChunkContext,
							*SmartObjectSubsystem,
							*SignalSubsystem,
							Entity,
							Body,
							User,
							CommandSerial,
							EArcBodyCommandResult::Failed);
					}
					else if (WalkResult == ArcBodyProcessorPrivate::EWalkResult::Arrived)
					{
						MoveTargets[EntityIt].CreateNewAction(EMassMovementAction::Stand, *World);
						const FMassSmartObjectHandler Handler(ChunkContext, *SmartObjectSubsystem, *SignalSubsystem);
						if (Handler.StartUsingSmartObject(Entity, User, Body.Command.ClaimHandle))
						{
							Body.Command.bStartedUsing = true;
							Body.State = EArcBodyState::Interaction;
						}
						else
						{
							ArcBodyCommand::FinalizeCommand(
								ChunkContext,
								*SmartObjectSubsystem,
								*SignalSubsystem,
								Entity,
								Body,
								User,
								CommandSerial,
								EArcBodyCommandResult::Failed);
						}
					}
				}

				if (Body.State != OldState)
				{
					TRACE_ARC_ECONOMY_BOOKMARKF(
						TEXT("BODY-STATE %s->%s serial=%u"),
						ArcBodyProcessorPrivate::StateName(OldState),
						ArcBodyProcessorPrivate::StateName(Body.State),
						Body.Command.Serial);
				}
			}
		});
}

UArcBodyCommandCleanupObserver::UArcBodyCommandCleanupObserver()
	: EntityQuery(*this)
{
	ObservedTypes.Add(FArcBodyFragment::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcBodyCommandCleanupObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProcessorRequirements.AddSubsystemRequirement<USmartObjectSubsystem>(EMassFragmentAccess::ReadWrite);
	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);

	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FArcBodyFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassSmartObjectUserFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddSubsystemRequirement<USmartObjectSubsystem>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	EntityQuery.RegisterWithProcessor(*this);
}

void UArcBodyCommandCleanupObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	USmartObjectSubsystem* SmartObjectSubsystem = Context.GetMutableSubsystem<USmartObjectSubsystem>();
	UMassSignalSubsystem* SignalSubsystem = Context.GetMutableSubsystem<UMassSignalSubsystem>();
	if (!SmartObjectSubsystem || !SignalSubsystem)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(Context,
		[SmartObjectSubsystem, SignalSubsystem](FMassExecutionContext& ChunkContext)
		{
			TArrayView<FArcBodyFragment> Bodies = ChunkContext.GetMutableFragmentView<FArcBodyFragment>();
			TArrayView<FMassSmartObjectUserFragment> Users = ChunkContext.GetMutableFragmentView<FMassSmartObjectUserFragment>();
			for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcBodyFragment& Body = Bodies[EntityIt];
				if (!Body.Command.IsActive())
				{
					continue;
				}

				ArcBodyCommand::FinalizeCommand(
					ChunkContext,
					*SmartObjectSubsystem,
					*SignalSubsystem,
					ChunkContext.GetEntity(EntityIt),
					Body,
					Users[EntityIt],
					Body.Command.Serial,
					EArcBodyCommandResult::Aborted);
			}
		});
}
