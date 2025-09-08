#include "ArcMassWaitTask.h"

#include "MassCommonTypes.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "MassNavigationTypes.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"

FArcMassWaitTask::FArcMassWaitTask()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = true;
	bShouldCopyBoundPropertiesOnExitState = false;
}

bool FArcMassWaitTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(MassWaitFragment);

	return true;
}

void FArcMassWaitTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite<FArcMassWaitTaskFragment>();
}

EStateTreeRunStatus FArcMassWaitTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	UMassEntitySubsystem* MassSubsystem = MassCtx.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	UMassSignalSubsystem* MassSignalSubsystem = MassCtx.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	FArcMassWaitTaskFragment* MassWait = MassCtx.GetExternalDataPtr(MassWaitFragment);
	MassWait->Duration = InstanceData.Duration;
	MassWait->Time = 0;
	InstanceData.Time = 0.f;
	
	EntityManager.Defer().AddTag<FArcMassWaitTaskTag>(MassCtx.GetEntity());

	
	
	//if (InstanceData.Duration > 0.f)
	//{
	//	MassSignalSubsystem->DelaySignalEntityDeferred(MassCtx.GetMassEntityExecutionContext(),
	//		UE::Mass::Signals::NewStateTreeTaskRequired, MassCtx.GetEntity(), InstanceData.Duration);
	//}
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcMassWaitTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FArcMassWaitTaskFragment* MassWait = MassCtx.GetExternalDataPtr(MassWaitFragment);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	if (MassWait->Time >= InstanceData.Duration)
	{
		
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

UArcMassWaitTaskProcessor::UArcMassWaitTaskProcessor()
	: EntityQuery_Conditional(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Avoidance);
}

void UArcMassWaitTaskProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::ConfigureQueries(EntityManager);

	EntityQuery_Conditional.AddRequirement<FArcMassWaitTaskFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery_Conditional.AddTagRequirement<FArcMassWaitTaskTag>(EMassFragmentPresence::All);
	EntityQuery_Conditional.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);

	EntityQuery_Conditional.RegisterWithProcessor(*this);
}

void UArcMassWaitTaskProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& MassEntityManager)
{
	Super::InitializeInternal(Owner, MassEntityManager);
	SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());


}

void UArcMassWaitTaskProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (!SignalSubsystem)
	{
		return;
	}
	
	TArray<FMassEntityHandle> EntitiesToSignalPathDone;

	EntityQuery_Conditional.ForEachEntityChunk(Context, [this, &EntitiesToSignalPathDone](FMassExecutionContext& MyContext)
	{
		TArrayView<FArcMassWaitTaskFragment> TransformList = MyContext.GetMutableFragmentView<FArcMassWaitTaskFragment>();
			
		const float WorldDeltaTime = MyContext.GetDeltaTimeSeconds();

		for (FMassExecutionContext::FEntityIterator EntityIt = MyContext.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcMassWaitTaskFragment& WaitTime = TransformList[EntityIt];
			WaitTime.Time += WorldDeltaTime;
			
			if (WaitTime.Time >= WaitTime.Duration)
			{
				FMassEntityHandle Handle = MyContext.GetEntity(EntityIt);
				EntitiesToSignalPathDone.Add(Handle);

				MyContext.Defer().RemoveTag<FArcMassWaitTaskTag>(Handle);
			}
		}
	});

	if (EntitiesToSignalPathDone.Num())
	{
		SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, EntitiesToSignalPathDone);
	}
}
