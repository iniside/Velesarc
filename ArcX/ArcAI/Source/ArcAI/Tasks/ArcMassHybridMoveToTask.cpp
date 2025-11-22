#include "ArcMassHybridMoveToTask.h"

#include "AIController.h"
#include "ArcMassActorMoveToTask.h"
#include "MassAIBehaviorTypes.h"
#include "MassDebugger.h"
#include "MassEntitySubsystem.h"
#include "MassMovementFragments.h"
#include "MassNavMeshNavigationUtils.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "MoverMassTranslators.h"
#include "NavCorridor.h"
#include "NavigationSystem.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "Steering/MassSteeringFragments.h"
#include "Tasks/AITask_MoveTo.h"
#include "Translators/MassCapsuleComponentTranslators.h"
#include "Translators/MassCharacterMovementTranslators.h"
#include "Translators/MassSceneComponentVelocityTranslator.h"

FArcMassHybridMoveToTask::FArcMassHybridMoveToTask()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = true;
	bShouldCopyBoundPropertiesOnExitState = false;
}

bool FArcMassHybridMoveToTask::Link(FStateTreeLinker& Linker)
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = true;

	Linker.LinkExternalData(MassActorFragment);
	
	Linker.LinkExternalData(TransformHandle);
	Linker.LinkExternalData(MoveTargetHandle);
	Linker.LinkExternalData(AgentRadiusHandle);
	Linker.LinkExternalData(DesiredMovementHandle);
	Linker.LinkExternalData(MovementParamsHandle);
	Linker.LinkExternalData(CachedPathHandle);
	Linker.LinkExternalData(ShortPathHandle);
	
	return true;
}

void FArcMassHybridMoveToTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite<FMassActorFragment>();
	Builder.AddReadWrite<FMassMoveTargetFragment>();
}

EStateTreeRunStatus FArcMassHybridMoveToTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassActorFragment& OwningActor = Context.GetExternalData(MassActorFragment);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	UMassEntitySubsystem* MassSubsystem = MassCtx.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();
	
	if (OwningActor.Get() && !InstanceData.bPreferMassNavMesh && InstanceData.AIController)
	{
		if (!InstanceData.AIController)
		{
			UE_VLOG(Context.GetOwner(), LogStateTree, Error, TEXT("FArcMassHybridMoveToTask failed since AIController is missing."));
			return EStateTreeRunStatus::Failed;
		}

		InstanceData.TaskOwner = TScriptInterface<IGameplayTaskOwnerInterface>(InstanceData.AIController->FindComponentByInterface(UGameplayTaskOwnerInterface::StaticClass()));
		if (!InstanceData.TaskOwner)
		{
			InstanceData.TaskOwner = InstanceData.AIController;
		}

		if (!InstanceData.AIController->GetPawn())
		{
			return EStateTreeRunStatus::Failed;
		}
	
		EStateTreeRunStatus NewStatus = PerformMoveTask(Context, *InstanceData.AIController);
		if (NewStatus != EStateTreeRunStatus::Running)
		{
			return NewStatus;
		}

		

		FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);
	
		const UWorld* World = Context.GetWorld();
		checkf(World != nullptr, TEXT("A valid world is expected from the execution context."));
	
		MoveTarget.Center = InstanceData.AIController->GetPawn()->GetActorLocation();
		MoveTarget.CreateNewAction(EMassMovementAction::Animate, *World);
	
		const bool bSuccess = UE::MassNavigation::ActivateActionAnimate(Context.GetOwner(), MassCtx.GetEntity(), MoveTarget);

		FMassActorFragment* MassActor = MassCtx.GetExternalDataPtr(MassActorFragment);
	
		if (FArcMassActorMoveToFragment* Frag = EntityManager.GetFragmentDataPtr<FArcMassActorMoveToFragment>(MassCtx.GetEntity()))
		{
			Frag->Destination = InstanceData.Destination;
			Frag->AcceptanceRadius = InstanceData.AcceptableRadius;
			if (const AActor* Actor = MassActor->Get())
			{
				if (InstanceData.bReachTestIncludesAgentRadius)
				{
					float Radius, HalfHeight;
					Actor->GetComponentsBoundingCylinder(Radius, HalfHeight);
					Frag->AcceptanceRadius += Radius;
				}
				if (InstanceData.bReachTestIncludesGoalRadius)
				{
					if (InstanceData.TargetActor)
					{
						float Radius, HalfHeight;
						InstanceData.TargetActor->GetComponentsBoundingCylinder(Radius, HalfHeight);
						Frag->AcceptanceRadius += Radius;	
					}
					else
					{
						Frag->AcceptanceRadius += InstanceData.TargetAcceptanceRadius;
					}
				}
			}

			InstanceData.CalculatedAcceptanceRadius = Frag->AcceptanceRadius;
		}
		//EntityManager.Defer().PushCommand<FMassDeferredSetCommand>([Entity = MassCtx.GetEntity()](const FMassEntityManager& Manager)
		//		{
		//			if (Manager.IsEntityValid(Entity) )
		//			{
		//				FMassVelocityFragment* VelocityFragment = Manager.GetFragmentDataPtr<FMassVelocityFragment>(Entity);
		//				if (VelocityFragment)
		//				{
		//					VelocityFragment->Value = FVector::ZeroVector;
		//				}
		//
		//				FMassDesiredMovementFragment* DesiredMovement = Manager.GetFragmentDataPtr<FMassDesiredMovementFragment>(Entity);
		//				if (DesiredMovement)
		//				{
		//					DesiredMovement->DesiredFacing = FQuat::Identity;
		//					DesiredMovement->DesiredVelocity = FVector::ZeroVector;
		//					//DesiredMovement->DesiredMaxSpeedOverride = 0;
		//				}
		//				FMassForceFragment* Force = Manager.GetFragmentDataPtr<FMassForceFragment>(Entity);
		//				if (Force)
		//				{
		//					Force->Value = FVector::ZeroVector;
		//				}
		//				FMassStandingSteeringFragment* Steering = Manager.GetFragmentDataPtr<FMassStandingSteeringFragment>(Entity);
		//				if (Steering)
		//				{
		//					Steering->TargetLocation = FVector::ZeroVector;
		//					//Steering->TrackedTargetSpeed = 0;
		//					//Steering->TargetSelectionCooldown = 0;
		//					//Steering->bIsUpdatingTarget = false;
		//					//Steering->bEnteredFromMoveAction = false;
		//				}
		//				FMassSteeringFragment* Steer = Manager.GetFragmentDataPtr<FMassSteeringFragment>(Entity);
		//				if (Steer)
		//				{
		//					Steer->DesiredVelocity = FVector::ZeroVector;
		//				}
		//			}
		//		});
		//
		//EntityManager.Defer().RemoveTag<FMassCodeDrivenMovementTag>(MassCtx.GetEntity());
		EntityManager.Defer().AddTag<FArcMassActorMoveToTag>(MassCtx.GetEntity());
		EntityManager.Defer().RemoveTag<FMassCopyToNavMoverTag>(MassCtx.GetEntity());
		EntityManager.Defer().AddTag<FMassNavMoverCopyToMassTag>(MassCtx.GetEntity());
		
		//EntityManager.Defer().RemoveTag<FMassOrientationCopyToNavMoverActorOrientationTag>(MassCtx.GetEntity());
		//EntityManager.Defer().AddTag<FMassNavMoverActorOrientationCopyToMassTag>(MassCtx.GetEntity());
		//EntityManager.Defer().RemoveTag<FMassCopyToNavMoverTag>(MassCtx.GetEntity());
		
		//EntityManager.Defer().AddTag<FMassSceneComponentVelocityCopyToMassTag>(MassCtx.GetEntity());
		//EntityManager.Defer().SwapTags<FMassCharacterMovementCopyToActorTag, FMassCharacterMovementCopyToMassTag>(MassCtx.GetEntity());
		//EntityManager.Defer().SwapTags<FMassCharacterOrientationCopyToActorTag, FMassCharacterOrientationCopyToMassTag>(MassCtx.GetEntity());
		//EntityManager.Defer().SwapTags<FMassCapsuleTransformCopyToActorTag, FMassCapsuleTransformCopyToMassTag>(MassCtx.GetEntity());

		DrawDebugSphere(Context.GetWorld(), InstanceData.Destination, InstanceData.AcceptableRadius, 12, FColor::Green, false, 10.f);
		return NewStatus;
	}
	else
	{
		FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
		FMassEntityHandle Handle = MassContext.GetEntity();
		EntityManager.Defer().AddTag<FMassCopyToNavMoverTag>(MassCtx.GetEntity());
		EntityManager.Defer().RemoveTag<FMassNavMoverCopyToMassTag>(MassCtx.GetEntity());
		
		//EntityManager.Defer().AddTag<FMassCopyToNavMoverTag>(MassCtx.GetEntity());
		//EntityManager.Defer().AddTag<FMassCodeDrivenMovementTag>(Handle);
		//EntityManager.Defer().RemoveTag<FArcMassActorMoveToTag>(Handle);
		//EntityManager.Defer().RemoveTag<FMassSceneComponentVelocityCopyToMassTag>(Handle);
		//EntityManager.Defer().SwapTags<FMassCharacterMovementCopyToMassTag, FMassCharacterMovementCopyToActorTag>(Handle);
		//EntityManager.Defer().SwapTags<FMassCharacterOrientationCopyToMassTag, FMassCharacterOrientationCopyToActorTag>(Handle);
		//EntityManager.Defer().SwapTags<FMassCapsuleTransformCopyToMassTag, FMassCapsuleTransformCopyToActorTag>(Handle);
		
		bool bDisplayDebug = false;
#if WITH_MASSGAMEPLAY_DEBUG
		bDisplayDebug = UE::Mass::Debug::IsDebuggingEntity(MassContext.GetEntity());
#endif // WITH_MASSGAMEPLAY_DEBUG
		if (bDisplayDebug)
		{
			MASSBEHAVIOR_LOG(Verbose, TEXT("enterstate."));
		}

		if (!InstanceData.TargetLocation.EndOfPathPosition.IsSet())
		{
			MASSBEHAVIOR_LOG(Error, TEXT("Target is not defined."));
			return EStateTreeRunStatus::Failed;
		}
		
		if (!RequestPath(Context, InstanceData.TargetLocation))
		{
			MASSBEHAVIOR_LOG(Warning, TEXT("Failed to request path."));
			return EStateTreeRunStatus::Failed;
		}

		return EStateTreeRunStatus::Running;
	}
}

EStateTreeRunStatus FArcMassHybridMoveToTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FMassActorFragment& OwningActor = Context.GetExternalData(MassActorFragment);

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	if (OwningActor.Get() && !InstanceData.bPreferMassNavMesh && InstanceData.AIController)
	{
		if (InstanceData.MoveToTask)
		{
			const FVector CurrentDestination = InstanceData.MoveToTask->GetMoveRequestRef().GetDestination();
			const float Distance = FVector::Dist(CurrentDestination, InstanceData.Destination);
			const float Tolerance = (InstanceData.DestinationMoveTolerance * InstanceData.DestinationMoveTolerance) + (InstanceData.CalculatedAcceptanceRadius);
		
			if (InstanceData.bTrackMovingGoal && !InstanceData.TargetActor)
			{
				UE_VLOG(Context.GetOwner(), LogStateTree, Log, TEXT("FArcMassHybridMoveToTask Distance %f Tolerance %F"), Distance, Tolerance);
			
				if (Distance > Tolerance)
				{
					UE_VLOG(Context.GetOwner(), LogStateTree, Log, TEXT("FArcMassHybridMoveToTask destination has moved enough. Restarting task."));
					return PerformMoveTask(Context, *InstanceData.AIController);
				}
				else
				{
					return EStateTreeRunStatus::Succeeded;
				}
			}
			else
			{
				if (Distance <= (InstanceData.CalculatedAcceptanceRadius))
				{
					return EStateTreeRunStatus::Succeeded;
				}
			}
			return EStateTreeRunStatus::Running;
		}
	}
	else
	{
		FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
		FMassNavMeshShortPathFragment& ShortPathFragment = Context.GetExternalData(ShortPathHandle);
	
		bool bDisplayDebug = false;
#if WITH_MASSGAMEPLAY_DEBUG
		bDisplayDebug = UE::Mass::Debug::IsDebuggingEntity(MassContext.GetEntity());
#endif // WITH_MASSGAMEPLAY_DEBUG
		if (bDisplayDebug)
		{
			MASSBEHAVIOR_LOG(Verbose, TEXT("tick"));
		}
	
		// Current path follow is done, but it was partial (i.e. many points on a curve), try again until we get there.
		if (ShortPathFragment.IsDone() && ShortPathFragment.bPartialResult)
		{
			if (!InstanceData.TargetLocation.EndOfPathPosition.IsSet())
			{
				MASSBEHAVIOR_LOG(Error, TEXT("Target is not defined."));
				return EStateTreeRunStatus::Failed;
			}

			if (!UpdateShortPath(Context))
			{
				MASSBEHAVIOR_LOG(Error, TEXT("Failed to update short path."));
				return EStateTreeRunStatus::Failed;
			}
		}

		if (ShortPathFragment.IsDone())
		{
			FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);

			MoveTarget.CreateNewAction(EMassMovementAction::Animate, *Context.GetWorld());
			
			return EStateTreeRunStatus::Succeeded;
		}

		return EStateTreeRunStatus::Running;
	}
	
	return EStateTreeRunStatus::Failed;
}

void FArcMassHybridMoveToTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassActorFragment& OwningActor = Context.GetExternalData(MassActorFragment);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	if (OwningActor.Get()&& !InstanceData.bPreferMassNavMesh && InstanceData.AIController)
	{	
		if (InstanceData.MoveToTask && InstanceData.MoveToTask->GetState() != EGameplayTaskState::Finished)
		{
			UE_VLOG(Context.GetOwner(), LogStateTree, Log, TEXT("FArcMassHybridMoveToTask aborting move to because state finished."));
			InstanceData.MoveToTask->ExternalCancel();
		}

		// todo: remove this once we fixed the instance data retention issue for re-entering state
		InstanceData.MoveToTask = nullptr;

		FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
		UMassEntitySubsystem* MassSubsystem = MassCtx.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
		FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

		FMassEntityHandle Handle = MassCtx.GetEntity();

		//EntityManager.Defer().PushCommand<FMassDeferredSetCommand>([Entity = Handle](const FMassEntityManager& Manager)
		//{
		//	if (Manager.IsEntityValid(Entity) )
		//	{
		//		FMassVelocityFragment* VelocityFragment = Manager.GetFragmentDataPtr<FMassVelocityFragment>(Entity);
		//		if (VelocityFragment)
		//		{
		//			VelocityFragment->Value = FVector::ZeroVector;
		//		}
		//		FMassDesiredMovementFragment* DesiredMovement = Manager.GetFragmentDataPtr<FMassDesiredMovementFragment>(Entity);
		//		if (DesiredMovement)
		//		{
		//			DesiredMovement->DesiredFacing = FQuat::Identity;
		//			DesiredMovement->DesiredVelocity = FVector::ZeroVector;
		//			//DesiredMovement->DesiredMaxSpeedOverride = 0;
		//		}
		//		FMassForceFragment* Force = Manager.GetFragmentDataPtr<FMassForceFragment>(Entity);
		//				if (Force)
		//				{
		//					Force->Value = FVector::ZeroVector;
		//				}
		//		FMassStandingSteeringFragment* Steering = Manager.GetFragmentDataPtr<FMassStandingSteeringFragment>(Entity);
		//				if (Steering)
		//				{
		//					Steering->TargetLocation = FVector::ZeroVector;
		//					//Steering->TrackedTargetSpeed = 0;
		//					//Steering->TargetSelectionCooldown = 0;
		//					//Steering->bIsUpdatingTarget = false;
		//					//Steering->bEnteredFromMoveAction = false;
		//				}
		//		FMassSteeringFragment* Steer = Manager.GetFragmentDataPtr<FMassSteeringFragment>(Entity);
		//				if (Steer)
		//				{
		//					Steer->DesiredVelocity = FVector::ZeroVector;
		//				}
		//	}
		//});
		//
		//EntityManager.Defer().AddTag<FMassCodeDrivenMovementTag>(Handle);
		EntityManager.Defer().RemoveTag<FArcMassActorMoveToTag>(Handle);
		EntityManager.Defer().RemoveTag<FMassCopyToNavMoverTag>(Handle);
		EntityManager.Defer().RemoveTag<FMassNavMoverCopyToMassTag>(MassCtx.GetEntity());
		
		//EntityManager.Defer().RemoveTag<FMassSceneComponentVelocityCopyToMassTag>(Handle);
		//EntityManager.Defer().SwapTags<FMassCharacterMovementCopyToMassTag, FMassCharacterMovementCopyToActorTag>(Handle);
		//EntityManager.Defer().SwapTags<FMassCharacterOrientationCopyToMassTag, FMassCharacterOrientationCopyToActorTag>(Handle);
		//EntityManager.Defer().SwapTags<FMassCapsuleTransformCopyToMassTag, FMassCapsuleTransformCopyToActorTag>(Handle);
	}
}

UAITask_MoveTo* FArcMassHybridMoveToTask::PrepareMoveToTask(FStateTreeExecutionContext& Context, AAIController& Controller, UAITask_MoveTo* ExistingTask, FAIMoveRequest& MoveRequest) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UAITask_MoveTo* MoveTask = ExistingTask ? ExistingTask : UAITask::NewAITask<UAITask_MoveTo>(Controller, *InstanceData.TaskOwner, EAITaskPriority::High);
	if (MoveTask)
	{
		MoveTask->SetUp(&Controller, MoveRequest);
	}

	return MoveTask;
}

EStateTreeRunStatus FArcMassHybridMoveToTask::PerformMoveTask(FStateTreeExecutionContext& Context, AAIController& Controller) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	FAIMoveRequest MoveReq;
	MoveReq.SetNavigationFilter(InstanceData.FilterClass ? InstanceData.FilterClass : Controller.GetDefaultNavigationFilterClass())
		.SetAllowPartialPath(InstanceData.bAllowPartialPath)
		.SetAcceptanceRadius(InstanceData.AcceptableRadius)
		.SetCanStrafe(InstanceData.bAllowStrafe)
		.SetReachTestIncludesAgentRadius(InstanceData.bReachTestIncludesAgentRadius)
		.SetReachTestIncludesGoalRadius(InstanceData.bReachTestIncludesGoalRadius)
		.SetRequireNavigableEndLocation(InstanceData.bRequireNavigableEndLocation)
		.SetProjectGoalLocation(InstanceData.bProjectGoalLocation)
		.SetUsePathfinding(true);

	if (InstanceData.TargetActor)
	{
		if (InstanceData.bTrackMovingGoal)
		{
			MoveReq.SetGoalActor(InstanceData.TargetActor);
		}
		else
		{
			MoveReq.SetGoalLocation(InstanceData.TargetActor->GetActorLocation());
		}
	}
	else
	{
		MoveReq.SetGoalLocation(InstanceData.Destination);
	}

	if (MoveReq.IsValid())
	{	
		InstanceData.MoveToTask = PrepareMoveToTask(Context, Controller, InstanceData.MoveToTask, MoveReq);
		if (InstanceData.MoveToTask)
		{
			const bool bIsGameplayTaskAlreadyActive = InstanceData.MoveToTask->IsActive();
			if (bIsGameplayTaskAlreadyActive)
			{
				InstanceData.MoveToTask->ConditionalPerformMove();
			}
			else
			{
				InstanceData.MoveToTask->ReadyForActivation();
			}

			// if it is already active, don't re-register the callback and wait for the callback to finish task
			if (!bIsGameplayTaskAlreadyActive)
			{
				// @todo: we want to check the state first time in case the task is a temporary task and the gameplay task is finished instantly, that WeakContext
				// won't be able to find the active frame/state. Remove this once we support that.
				if (InstanceData.MoveToTask->GetState() == EGameplayTaskState::Finished)
				{
					return InstanceData.MoveToTask->WasMoveSuccessful() ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
				}

				UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
				FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
				
				InstanceData.MoveToTask->OnMoveTaskFinished.AddLambda(
					[WeakContext = Context.MakeWeakExecutionContext(), SignalSubsystem, Entity = MassCtx.GetEntity()]
					(TEnumAsByte<EPathFollowingResult::Type> InResult, AAIController* InController)
					{
						WeakContext.FinishTask(InResult == EPathFollowingResult::Success ? EStateTreeFinishTaskType::Succeeded : EStateTreeFinishTaskType::Failed);
						SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
					});
			}

			return EStateTreeRunStatus::Running;
		}
	}

	UE_VLOG(Context.GetOwner(), LogStateTree, Error, TEXT("FArcMassHybridMoveToTask failed because it doesn't have a destination."));
	return EStateTreeRunStatus::Failed;
}


bool FArcMassHybridMoveToTask::RequestPath(FStateTreeExecutionContext& Context, const FMassTargetLocation& TargetLocation) const
{
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);

	bool bDisplayDebug = false;
#if WITH_MASSGAMEPLAY_DEBUG
	bDisplayDebug = UE::Mass::Debug::IsDebuggingEntity(MassContext.GetEntity());
#endif // WITH_MASSGAMEPLAY_DEBUG
	
	const FAgentRadiusFragment& AgentRadius = Context.GetExternalData(AgentRadiusHandle);
	const FVector AgentNavLocation = Context.GetExternalData(TransformHandle).GetTransform().GetLocation();
	const FNavAgentProperties& NavAgentProperties = FNavAgentProperties(AgentRadius.Radius);

	UNavigationSystemV1* NavMeshSubsystem = Cast<UNavigationSystemV1>(Context.GetWorld()->GetNavigationSystem());
	const ANavigationData* NavData = NavMeshSubsystem->GetNavDataForProps(NavAgentProperties, AgentNavLocation);

	DrawDebugSphere(Context.GetWorld(), TargetLocation.EndOfPathPosition.GetValue(), 40.f, 12, FColor::Red, false, 30.f);
	if (!NavData || !TargetLocation.EndOfPathPosition.IsSet())
	{
		MASSBEHAVIOR_LOG(Warning, TEXT("%s"), !NavData ? TEXT("Invalid NavData") : TEXT("EndOfPathPosition not set") );
		return false;
	}

	FPathFindingQuery Query(NavMeshSubsystem, *NavData, AgentNavLocation, TargetLocation.EndOfPathPosition.GetValue());
	Query.SetRequireNavigableEndLocation(InstanceData.bMassRequireNavigableEndLocation);
	Query.SetAllowPartialPaths(InstanceData.bMassAllowPartialPath);
	
	// Why fix it after if there is none??
	if (!Query.NavData.IsValid())
	{
		Query.NavData = NavMeshSubsystem->GetNavDataForProps(NavAgentProperties, Query.StartLocation);
	}

	FPathFindingResult Result(ENavigationQueryResult::Error);
	if (Query.NavData.IsValid())
	{
		if (bDisplayDebug)
		{
			MASSBEHAVIOR_LOG(Verbose, TEXT("requesting synchronous path"));
		}
		
		Result = Query.NavData->FindPath(NavAgentProperties, Query);
	}

	if (Result.IsSuccessful())
	{
		Result.Path->RemoveOverlappingPoints(FNavCorridor::OverlappingPointTolerance);
		
		// @todo: Investigate single point paths but for now, only move if we have more than one point.
		if (Result.Path.Get()->GetPathPoints().Num() > 1)
		{
			// A path was found
			if (bDisplayDebug)
			{
				MASSBEHAVIOR_LOG(Verbose, TEXT("path found"));
			}

			FMassNavMeshCachedPathFragment& CachedPathFragment = Context.GetExternalData(CachedPathHandle);
			CachedPathFragment.NavPath = Result.Path;

			CachedPathFragment.PathSource = EMassNavigationPathSource::NavMesh;

			// Build corridor
			CachedPathFragment.Corridor = MakeShared<FNavCorridor>();
			const FSharedConstNavQueryFilter NavQueryFilter = Query.QueryFilter ? Query.QueryFilter : NavData->GetDefaultQueryFilter();

			FNavCorridorParams CorridorParams;
			CorridorParams.SetFromWidth(InstanceData.CorridorWidth);
			CorridorParams.PathOffsetFromBoundaries = InstanceData.OffsetFromBoundaries;

			MASSBEHAVIOR_LOG(Verbose, TEXT("FMassNavMeshPathFollowTask::RequestPath"));
			MASSBEHAVIOR_LOG(Verbose, TEXT("Start: %s, End: %s"),
				*CachedPathFragment.NavPath->GetPathPoints()[0].Location.ToCompactString(),
				*CachedPathFragment.NavPath->GetPathPoints().Last().Location.ToCompactString());
			MASSBEHAVIOR_LOG(Verbose, TEXT("Corridor params - %s"), *CorridorParams.ToString());
			
			CachedPathFragment.Corridor->BuildFromPath(*CachedPathFragment.NavPath, NavQueryFilter, CorridorParams);

			// Update short path
			FMassNavMeshShortPathFragment& ShortPathFragment = Context.GetExternalData(ShortPathHandle);
			ShortPathFragment.RequestShortPath(CachedPathFragment.Corridor, /*NavCorridorStartIndex*/0, /*NumLeadingPoints*/0, InstanceData.EndDistanceThreshold);

			CachedPathFragment.NavPathNextStartIndex = (uint16)FMath::Max(ShortPathFragment.NumPoints - FMassNavMeshShortPathFragment::NumPointsBeyondUpdate - FMassNavMeshCachedPathFragment::NumLeadingPoints, 0);
			
			// Update MoveTarget
			FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);

			MoveTarget.IntentAtGoal = InstanceData.TargetLocation.EndOfPathIntent;
			
			const FMassMovementParameters& MovementParams = Context.GetExternalData(MovementParamsHandle);
			float DesiredSpeed = FMath::Min(
				MovementParams.GenerateDesiredSpeed(InstanceData.MovementStyle, MassContext.GetEntity().Index) * InstanceData.SpeedScale,
				MovementParams.MaxSpeed);

			// Apply DesiredMaxSpeedOverride
			const FMassDesiredMovementFragment& DesiredMovementFragment = Context.GetExternalData(DesiredMovementHandle);
			DesiredSpeed = 370.f;//FMath::Min(DesiredSpeed, DesiredMovementFragment.DesiredMaxSpeedOverride);

			MoveTarget.Center = TargetLocation.EndOfPathPosition.GetValue();
			MoveTarget.DesiredSpeed.Set(DesiredSpeed);
			
			MoveTarget.CreateNewAction(EMassMovementAction::Move, *Context.GetWorld());

			return true;
		}
		else
		{
			MASSBEHAVIOR_LOG(Warning, TEXT("Path Has one or less segment."));
		}
		return true;
	}

	MASSBEHAVIOR_LOG(Warning, TEXT("Failed to find a path, result: %s (start: %s, end: %s"),
		 *UEnum::GetValueAsString(Result.Result), *Query.StartLocation.ToCompactString(), *Query.EndLocation.ToCompactString());
	
	return false;
}

bool FArcMassHybridMoveToTask::UpdateShortPath(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	
	FMassNavMeshCachedPathFragment& CachedPathFragment = Context.GetExternalData(CachedPathHandle);
	MASSBEHAVIOR_LOG(Verbose, TEXT("updating short path, starting at index %i"), CachedPathFragment.NavPathNextStartIndex);
	
	FMassNavMeshShortPathFragment& ShortPathFragment = Context.GetExternalData(ShortPathHandle);

	ShortPathFragment.RequestShortPath(CachedPathFragment.Corridor, CachedPathFragment.NavPathNextStartIndex, CachedPathFragment.NumLeadingPoints, InstanceData.EndDistanceThreshold);

	CachedPathFragment.NavPathNextStartIndex += (uint16)FMath::Max(ShortPathFragment.NumPoints - FMassNavMeshShortPathFragment::NumPointsBeyondUpdate - FMassNavMeshCachedPathFragment::NumLeadingPoints, 0);

	return true;
}
