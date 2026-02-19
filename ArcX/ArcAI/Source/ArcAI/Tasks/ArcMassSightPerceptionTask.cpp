// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassSightPerceptionTask.h"

#include "ArcMassEQSResultStore.h"
#include "MassActorSubsystem.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeExecutionContext.h"
#include "Perception/ArcMassPerception.h"
#include "Perception/ArcMassSightPerception.h"

FArcMassSightPerceptionTask::FArcMassSightPerceptionTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FArcMassSightPerceptionTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	//FInstanceDataType& InstanceData = MassContext.GetInstanceData<FInstanceDataType>(*this);
	
	UArcMassSightPerceptionSubsystem* PerceptionSubsystem = MassContext.GetWorld()->GetSubsystem<UArcMassSightPerceptionSubsystem>();
	if (!PerceptionSubsystem)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	
	FArcPerceptionEntityList::FDelegate Delegate = FArcPerceptionEntityList::FDelegate::CreateWeakLambda(PerceptionSubsystem,
			[WeakContext = Context.MakeWeakExecutionContext(), SignalSubsystem, Entity = MassContext.GetEntity()]
			(FMassEntityHandle PerceiverEntity, const TArray<FArcPerceivedEntity>& PerceivedEntities, FGameplayTag SenseTag)
			{
				FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
				FInstanceDataType* InstanceDataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
				if (!InstanceDataPtr)
				{
					return;
				}
				
				InstanceDataPtr->PerceivedEntities = PerceivedEntities;
				
				StrongContext.BroadcastDelegate(InstanceDataPtr->OnResultChanged);
				SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {Entity});
			}
		);
	
	PerceptionSubsystem->OnPerceptionUpdated.FindOrAdd(MassContext.GetEntity()).Add(Delegate);
	
	return EStateTreeRunStatus::Running;
}

void FArcGetActorFromArrayPropertyFunction::Execute(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	if (InstanceData.Input.Num() > 0)
	{
		InstanceData.Output = InstanceData.Input[0];
	}
	else
	{
		InstanceData.Output = nullptr;
	}
}

void FArcGetPerceivedEntityFromArrayPropertyFunction::Execute(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	if (InstanceData.Input.Num() > 0)
	{
		FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
		UMassEntitySubsystem* Subsystem = MassContext.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
		FMassEntityManager& EntityManager = Subsystem->GetMutableEntityManager();
		if (FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(InstanceData.Input[0].Entity))
		{
			InstanceData.Output.Actor = const_cast<AActor*>(ActorFragment->Get());
		}
		else
		{
			InstanceData.Output.Actor = nullptr;
		}
		InstanceData.Output.Entity = InstanceData.Input[0].Entity;
		InstanceData.Output.LastKnownLocation = InstanceData.Input[0].LastKnownLocation;
		InstanceData.Output.Distance = InstanceData.Input[0].Distance;
		InstanceData.Output.Strength = InstanceData.Input[0].Strength;
		InstanceData.Output.TimeSinceLastSeen = InstanceData.Input[0].TimeSinceLastPerceived;
		InstanceData.Output.LastTimeSeen = InstanceData.Input[0].LastTimeSeen;
		InstanceData.Output.bIsBeingPerceived = InstanceData.Input[0].TimeSinceLastPerceived <= 0.1;
	}
	else
	{
		InstanceData.Output = FArcSelectedEntity();
	}
}

FArcMassPerceptionSelectActorTask::FArcMassPerceptionSelectActorTask()
{
	bShouldCallTick = true;
}

EStateTreeRunStatus FArcMassPerceptionSelectActorTask::EnterState(FStateTreeExecutionContext& Context
	, const FStateTreeTransitionResult& Transition) const
{
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcMassPerceptionSelectActorTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	UArcMassGlobalDataStore* GlobalStore = Context.GetWorld()->GetSubsystem<UArcMassGlobalDataStore>();		
	
	if (InstanceData.PerceivedEntity.Entity != InstanceData.OutPerceivedEntity.Entity)
	{
		InstanceData.OutPerceivedEntity = InstanceData.PerceivedEntity;
		if (GlobalStore && InstanceData.DataStoreTag.IsValid())
		{
			if (InstanceData.OutPerceivedEntity.Entity.IsValid())
			{
				FArcMassDataStoreTypeMap& Store = GlobalStore->DataStore.FindOrAdd(MassContext.GetEntity());
				FArcMassEQSResults& Result = Store.Results.FindOrAdd(InstanceData.DataStoreTag);
				Result.ActorResult = InstanceData.OutPerceivedEntity.Actor;
			
				FMassEnvQueryEntityInfo EntityInfo;
				EntityInfo.CachedTransform = FTransform(InstanceData.OutPerceivedEntity.LastKnownLocation);
				EntityInfo.EntityHandle = InstanceData.OutPerceivedEntity.Entity;
			
				Result.EntityResult = FMassEnvQueryEntityInfoBlueprintWrapper(EntityInfo);
			
				Result.VectorResult = InstanceData.OutPerceivedEntity.LastKnownLocation;	
			}
			else
			{
				FArcMassDataStoreTypeMap& Store = GlobalStore->DataStore.FindOrAdd(MassContext.GetEntity());
				Store.Results.Remove(InstanceData.DataStoreTag);
			}
		}
		
		Context.BroadcastDelegate(InstanceData.OnSelectedActorChanged);
		SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {MassContext.GetEntity()});
	}
	
	return EStateTreeRunStatus::Running;
}
