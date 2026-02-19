#include "ArcMassActorPlayAnimMontage.h"

#include "MassEntitySubsystem.h"
#include "MassNavMeshNavigationUtils.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "MoverComponent.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/LayeredMoves/AnimRootMotionLayeredMove.h"

class UMassSignalSubsystem;
class UMassEntitySubsystem;
struct FMassStateTreeExecutionContext;

FArcMassActorPlayAnimMontageTask::FArcMassActorPlayAnimMontageTask()
{
	bUseMover = false;
	bShouldCallTick = true;
}


bool FArcMassActorPlayAnimMontageTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MoveTargetHandle);
	Linker.LinkExternalData(MassActorFragment);
	Linker.LinkExternalData(MassSignalSubsystemHandle);
	
	return true;
}

void FArcMassActorPlayAnimMontageTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite<FMassActorFragment>();
	Builder.AddReadWrite<FMassMoveTargetFragment>();
}

EStateTreeRunStatus FArcMassActorPlayAnimMontageTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.AnimMontage)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassActorFragment* MassActor = MassCtx.GetExternalDataPtr(MassActorFragment);

	if (!MassActor || !MassActor->Get())
	{
		return EStateTreeRunStatus::Failed;
	}
	
	USkeletalMeshComponent* MeshComponent = MassActor->Get()->FindComponentByClass<USkeletalMeshComponent>();

	if (!MeshComponent)
	{
		return EStateTreeRunStatus::Failed;
	}

	UMoverComponent* MoverComponent = MassActor->Get()->FindComponentByClass<UMoverComponent>();
	
	UAnimInstance* AnimInst = MeshComponent->GetAnimInstance();
	if (!AnimInst)
	{
		return EStateTreeRunStatus::Failed;
	}

	float Duration = AnimInst->Montage_Play(InstanceData.AnimMontage, 1.f);
	if (Duration <= 0)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	const UWorld* World = Context.GetWorld();
	const FVector AgentNavLocation = MassActor->Get()->GetActorLocation();
	
	FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);
	MoveTarget.Center = AgentNavLocation;
	MoveTarget.CreateNewAction(EMassMovementAction::Animate, *World);
	
	const bool bSuccess = UE::MassNavigation::ActivateActionAnimate(Context.GetOwner(), MassCtx.GetEntity(), MoveTarget);

	if (!bSuccess)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	if (InstanceData.bLooping)
	{
		if (InstanceData.Duration > 0.f)
		{
			UMassSignalSubsystem& MassSignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
			MassSignalSubsystem.DelaySignalEntityDeferred(MassCtx.GetMassEntityExecutionContext(),
				UE::Mass::Signals::NewStateTreeTaskRequired, MassCtx.GetEntity(), InstanceData.Duration);
		}
	}
	else
	{
		if (Duration > 0.f)
		{
			InstanceData.Duration = Duration;
			UMassSignalSubsystem& MassSignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
			MassSignalSubsystem.DelaySignalEntityDeferred(MassCtx.GetMassEntityExecutionContext(),
				UE::Mass::Signals::NewStateTreeTaskRequired, MassCtx.GetEntity(), Duration);
		}	
	}
	
	
	if (MoverComponent)
	{
		if (FAnimMontageInstance* MontageInstance = AnimInst->GetActiveInstanceForMontage(InstanceData.AnimMontage))
		{
			// Listen for possible ways the montage could end
			//OnCompleted.AddUniqueDynamic(this, &UPlayMoverMontageCallbackProxy::OnMoverMontageEnded);
			//OnInterrupted.AddUniqueDynamic(this, &UPlayMoverMontageCallbackProxy::OnMoverMontageEnded);

			// Disable the actual animation-driven root motion, in favor of our own layered move
			MontageInstance->PushDisableRootMotion();

			const float StartingMontagePosition = MontageInstance->GetPosition();	// position in seconds, disregarding PlayRate

			// Queue a layered move to perform the same anim root motion over the same time span
			TSharedPtr<FLayeredMove_AnimRootMotion> AnimRootMotionMove = MakeShared<FLayeredMove_AnimRootMotion>();
			AnimRootMotionMove->MontageState.Montage = InstanceData.AnimMontage;
			AnimRootMotionMove->MontageState.PlayRate = 1.f;
			AnimRootMotionMove->MontageState.StartingMontagePosition = StartingMontagePosition;
			AnimRootMotionMove->MontageState.CurrentPosition = StartingMontagePosition;
				
			float RemainingUnscaledMontageSeconds(0.f);

			//if (PlayRate > 0.f)
			{
				// playing forwards, so working towards the end of the montage
				RemainingUnscaledMontageSeconds = InstanceData.AnimMontage->GetPlayLength() - StartingMontagePosition;
			}
			//else
			//{
			//	// playing backwards, so working towards the start of the montage
			//	RemainingUnscaledMontageSeconds = StartingMontagePosition;	
			//}

			AnimRootMotionMove->DurationMs = (RemainingUnscaledMontageSeconds / FMath::Abs(1)) * 1000.f;

			MoverComponent->QueueLayeredMove(AnimRootMotionMove);
		}
	}
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcMassActorPlayAnimMontageTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	InstanceData.Time += DeltaTime;
	
	return InstanceData.Duration <= 0.f ? EStateTreeRunStatus::Running :
		(InstanceData.Time < InstanceData.Duration ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded);
}

void FArcMassActorPlayAnimMontageTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.AnimMontage)
	{
		return;
	}
	
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassActorFragment* MassActor = MassCtx.GetExternalDataPtr(MassActorFragment);

	if (!MassActor || !MassActor->Get())
	{
		return;
	}
	
	USkeletalMeshComponent* MeshComponent = MassActor->Get()->FindComponentByClass<USkeletalMeshComponent>();

	if (!MeshComponent)
	{
		return;
	}
	
	UAnimInstance* AnimInst = MeshComponent->GetAnimInstance();
	if (!AnimInst)
	{
		return;
	}
	
	AnimInst->Montage_Stop(0.1f, InstanceData.AnimMontage);
}
