#include "ArcMassActorMoveToTask.h"

#include "AIController.h"
#include "DrawDebugHelpers.h"
#include "MassActorSubsystem.h"
#include "MassCommonTypes.h"
#include "MassEntitySubsystem.h"
#include "MassLODFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationTypes.h"
#include "MassNavMeshNavigationUtils.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "MoverMassTranslators.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "Steering/MassSteeringFragments.h"
#include "Tasks/AITask_MoveTo.h"
#include "Translators/MassCapsuleComponentTranslators.h"
#include "Translators/MassCharacterMovementTranslators.h"
#include "Translators/MassSceneComponentVelocityTranslator.h"
#include "VisualLogger/VisualLogger.h"

FArcMassActorMoveToTask::FArcMassActorMoveToTask()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = true;
	bShouldCopyBoundPropertiesOnExitState = false;
}

bool FArcMassActorMoveToTask::Link(FStateTreeLinker& Linker)
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = true;

	Linker.LinkExternalData(MassActorFragment);
	Linker.LinkExternalData(MoveTargetHandle);
	
	return true;
}

void FArcMassActorMoveToTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite<FMassActorFragment>();
	Builder.AddReadWrite<FMassMoveTargetFragment>();
}

EStateTreeRunStatus FArcMassActorMoveToTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.AIController)
	{
		UE_VLOG(Context.GetOwner(), LogStateTree, Error, TEXT("FArcMassActorMoveToTask failed since AIController is missing."));
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

	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	UMassEntitySubsystem* MassSubsystem = MassCtx.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

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
		Frag->TargetActor = InstanceData.TargetActor;
		
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
	EntityManager.Defer().AddTag<FMassNavMoverActorOrientationCopyToMassTag>(MassCtx.GetEntity());
	//EntityManager.Defer().AddTag<FMassSceneComponentVelocityCopyToMassTag>(MassCtx.GetEntity());
	//EntityManager.Defer().SwapTags<FMassCharacterMovementCopyToActorTag, FMassCharacterMovementCopyToMassTag>(MassCtx.GetEntity());
    //EntityManager.Defer().SwapTags<FMassCharacterOrientationCopyToActorTag, FMassCharacterOrientationCopyToMassTag>(MassCtx.GetEntity());
    //EntityManager.Defer().SwapTags<FMassCapsuleTransformCopyToActorTag, FMassCapsuleTransformCopyToMassTag>(MassCtx.GetEntity());

	DrawDebugSphere(Context.GetWorld(), InstanceData.Destination, InstanceData.AcceptableRadius, 12, FColor::Green, false, 10.f);
	return NewStatus;
}

EStateTreeRunStatus FArcMassActorMoveToTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.MoveToTask)
	{
		const FVector CurrentDestination = InstanceData.MoveToTask->GetMoveRequestRef().GetDestination();
		const float Distance = FVector::Dist(CurrentDestination, InstanceData.Destination);
		const float Tolerance = (InstanceData.DestinationMoveTolerance * InstanceData.DestinationMoveTolerance) + (InstanceData.CalculatedAcceptanceRadius);
		
		if (InstanceData.bTrackMovingGoal && !InstanceData.TargetActor)
		{
			UE_VLOG(Context.GetOwner(), LogStateTree, Log, TEXT("FArcMassActorMoveToTask Distance %f Tolerance %F"), Distance, Tolerance);
			
			if (Distance > Tolerance)
			{
				UE_VLOG(Context.GetOwner(), LogStateTree, Log, TEXT("FArcMassActorMoveToTask destination has moved enough. Restarting task."));
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

	return EStateTreeRunStatus::Failed;
}

void FArcMassActorMoveToTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.MoveToTask && InstanceData.MoveToTask->GetState() != EGameplayTaskState::Finished)
	{
		UE_VLOG(Context.GetOwner(), LogStateTree, Log, TEXT("FArcMassActorMoveToTask aborting move to because state finished."));
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
	EntityManager.Defer().RemoveTag<FMassNavMoverActorOrientationCopyToMassTag>(Handle);
	//EntityManager.Defer().RemoveTag<FMassSceneComponentVelocityCopyToMassTag>(Handle);
	//EntityManager.Defer().SwapTags<FMassCharacterMovementCopyToMassTag, FMassCharacterMovementCopyToActorTag>(Handle);
	//EntityManager.Defer().SwapTags<FMassCharacterOrientationCopyToMassTag, FMassCharacterOrientationCopyToActorTag>(Handle);
	//EntityManager.Defer().SwapTags<FMassCapsuleTransformCopyToMassTag, FMassCapsuleTransformCopyToActorTag>(Handle);
}

UAITask_MoveTo* FArcMassActorMoveToTask::PrepareMoveToTask(FStateTreeExecutionContext& Context, AAIController& Controller, UAITask_MoveTo* ExistingTask, FAIMoveRequest& MoveRequest) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UAITask_MoveTo* MoveTask = ExistingTask ? ExistingTask : UAITask::NewAITask<UAITask_MoveTo>(Controller, *InstanceData.TaskOwner, EAITaskPriority::High);
	if (MoveTask)
	{
		MoveTask->SetUp(&Controller, MoveRequest);
	}

	return MoveTask;
}

EStateTreeRunStatus FArcMassActorMoveToTask::PerformMoveTask(FStateTreeExecutionContext& Context, AAIController& Controller) const
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

	UE_VLOG(Context.GetOwner(), LogStateTree, Error, TEXT("FArcMassActorMoveToTask failed because it doesn't have a destination."));
	return EStateTreeRunStatus::Failed;
}

UArcMassActorMoveToProcessor::UArcMassActorMoveToProcessor()
	: EntityQuery_Conditional(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Avoidance);
}

void UArcMassActorMoveToProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
	SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}

void UArcMassActorMoveToProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery_Conditional.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery_Conditional.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery_Conditional.AddRequirement<FArcMassActorMoveToFragment>(EMassFragmentAccess::ReadOnly);
	
	EntityQuery_Conditional.AddTagRequirement<FArcMassActorMoveToTag>(EMassFragmentPresence::All);

	EntityQuery_Conditional.RegisterWithProcessor(*this);
	// @todo: validate LOD and variable ticking
	//EntityQuery_Conditional.AddRequirement<FMassSimulationLODFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	//EntityQuery_Conditional.AddRequirement<FMassSimulationVariableTickFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
}

void UArcMassActorMoveToProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (!SignalSubsystem)
	{
		return;
	}
	
	TArray<FMassEntityHandle> EntitiesToSignalPathDone;

	EntityQuery_Conditional.ForEachEntityChunk(Context, [this, &EntitiesToSignalPathDone](FMassExecutionContext& MyContext)
	{
		const TConstArrayView<FTransformFragment> TransformList = MyContext.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FArcMassActorMoveToFragment> ActorMoveToFragments = MyContext.GetFragmentView<FArcMassActorMoveToFragment>();
			
		const float WorldDeltaTime = MyContext.GetDeltaTimeSeconds();
		
		for (FMassExecutionContext::FEntityIterator EntityIt = MyContext.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FTransformFragment& Transform = TransformList[EntityIt];
			const FArcMassActorMoveToFragment& ActorMoveto = ActorMoveToFragments[EntityIt];
			FVector CurrentDestination = FVector::ZeroVector;
			if (ActorMoveto.TargetActor.IsValid())
			{
				CurrentDestination = ActorMoveto.TargetActor->GetActorLocation();
			}
			else
			{
				CurrentDestination = ActorMoveto.Destination;
			}
			FVector::FReal Dist = FVector::Dist(Transform.GetTransform().GetLocation(), CurrentDestination);
			if (Dist <= (ActorMoveto.AcceptanceRadius))
			{
				FMassEntityHandle Handle = MyContext.GetEntity(EntityIt);
				EntitiesToSignalPathDone.Add(Handle);
			}
		}
	});

	if (EntitiesToSignalPathDone.Num())
	{
		SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, EntitiesToSignalPathDone);
	}
}