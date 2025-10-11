#include "ArcMassActorPlayAnimMontage.h"

#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"

class UMassSignalSubsystem;
class UMassEntitySubsystem;
struct FMassStateTreeExecutionContext;

FArcMassActorPlayAnimMontageTask::FArcMassActorPlayAnimMontageTask()
{
	bShouldCallTick = false;
}


bool FArcMassActorPlayAnimMontageTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassActorFragment);

	return true;
}

void FArcMassActorPlayAnimMontageTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite<FMassActorFragment>();
}

EStateTreeRunStatus FArcMassActorPlayAnimMontageTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.AnimMontage)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	UMassEntitySubsystem* MassSubsystem = MassCtx.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	UMassSignalSubsystem* MassSignalSubsystem = MassCtx.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

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

	FOnMontageEnded OnEnded =  FOnMontageEnded::CreateWeakLambda(MassActor->Get()
		, [WeakContext = Context.MakeWeakExecutionContext(), MassSignalSubsystem, Entity = MassCtx.GetEntity()]
		(UAnimMontage* Montage, bool bInterrupted)
		{
				const FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
				if (FInstanceDataType* InstanceDataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>())
				{
					if (InstanceDataPtr->AnimMontage == Montage)
					{
						StrongContext.FinishTask(EStateTreeFinishTaskType::Succeeded);
						MassSignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});		
					}
				};


				StrongContext.FinishTask(EStateTreeFinishTaskType::Failed);
				MassSignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
		});
	
	AnimInst->Montage_SetEndDelegate(OnEnded, InstanceData.AnimMontage);
	
	return EStateTreeRunStatus::Running;
}
