#include "ArcMassWaitTask.h"

#include "MassCommands.h"
#include "MassCommonTypes.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "MassNavigationTypes.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeExecutionContext.h"

FArcMassWaitTask::FArcMassWaitTask()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = true;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FArcMassWaitTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassEntityManager& EntityManager = MassCtx.GetEntityManager();
	const FMassEntityHandle Entity = MassCtx.GetEntity();

	const float WaitDuration = InstanceData.Duration;
	InstanceData.Time = 0.f;

	EntityManager.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Add>>(
		[Entity, WaitDuration](FMassEntityManager& Mgr)
		{
			FArcMassWaitTaskFragment& WaitFragment = Mgr.AddSparseElementToEntity<FArcMassWaitTaskFragment>(Entity);
			WaitFragment.Duration = WaitDuration;
			WaitFragment.Time = 0.f;
			Mgr.AddSparseElementToEntity<FArcMassWaitTaskTag>(Entity);
		});

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcMassWaitTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassEntityManager& EntityManager = MassCtx.GetEntityManager();
	FStructView WaitView = EntityManager.GetMutableSparseElementDataForEntity(FArcMassWaitTaskFragment::StaticStruct(), MassCtx.GetEntity());
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (FArcMassWaitTaskFragment* MassWait = WaitView.GetPtr<FArcMassWaitTaskFragment>())
	{
		if (MassWait->Time >= InstanceData.Duration)
		{
			return EStateTreeRunStatus::Succeeded;
		}
	}

	return EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FArcMassWaitTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	if (InstanceDataView.IsValid())
	{
		const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
		if (InstanceData)
		{
			return FText::Format(NSLOCTEXT("ArcAI", "WaitTaskDesc", "Wait {0}s"), FText::AsNumber(InstanceData->Duration));
		}
	}
	return NSLOCTEXT("ArcAI", "WaitTaskDescDefault", "Wait");
}
#endif

UArcMassWaitTaskProcessor::UArcMassWaitTaskProcessor()
	: EntityQuery_Conditional(*this)
{
	//bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Avoidance);
	
	QueryBasedPruning = EMassQueryBasedPruning::Never;
}

void UArcMassWaitTaskProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery_Conditional.AddSparseRequirement<FArcMassWaitTaskFragment>(EMassFragmentPresence::All);
	EntityQuery_Conditional.AddSparseRequirement<FArcMassWaitTaskTag>(EMassFragmentPresence::All);
	EntityQuery_Conditional.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);

	//EntityQuery_Conditional.RegisterWithProcessor(*this);
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
		const float WorldDeltaTime = MyContext.GetDeltaTimeSeconds();

		for (FMassExecutionContext::FEntityIterator EntityIt = MyContext.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcMassWaitTaskFragment* WaitTime = MyContext.GetMutableSparseElement<FArcMassWaitTaskFragment>(EntityIt);
			if (!WaitTime)
			{
				continue;
			}

			WaitTime->Time += WorldDeltaTime;

			if (WaitTime->Time >= WaitTime->Duration)
			{
				FMassEntityHandle Entity = MyContext.GetEntity(EntityIt);
				EntitiesToSignalPathDone.Add(MyContext.GetEntity(EntityIt));
				MyContext.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Remove>>([Entity](FMassEntityManager& Mgr)
				{
					Mgr.RemoveSparseElementFromEntity<FArcMassWaitTaskFragment>(Entity);
					Mgr.RemoveSparseElementFromEntity<FArcMassWaitTaskTag>(Entity);
				});
			}
		}
	});

	if (EntitiesToSignalPathDone.Num())
	{
		SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, EntitiesToSignalPathDone);
	}
}
