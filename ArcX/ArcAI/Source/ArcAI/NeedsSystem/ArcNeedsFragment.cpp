/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "ArcNeedsFragment.h"

#include "ArcAILogs.h"
#include "MassCommonFragments.h"
#include "MassEntityConfigAsset.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "MassStateTreeExecutionContext.h"
#include "MassStateTreeFragments.h"
#include "MassStateTreeSubsystem.h"
#include "StateTreeConditionBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeExecutionContext.h"
#include "StateTreePropertyFunctionBase.h"
#include "StateTreeTaskBase.h"
#include "StateTreeTypes.h"
#include "Conditions/StateTreeAIConditionBase.h"
#include "MassSignalSubsystem.h"

#include "MassActors/Public/MassAgentComponent.h"
#include "Tasks/StateTreeAITask.h"

#include "StateTree.h"

UArcNeedsProcessor::UArcNeedsProcessor()
{
	//ProcessingPhase = EMassProcessingPhase::DuringPhysics;
}

void UArcNeedsProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	NeedsQuery.Initialize(EntityManager);
	
	NeedsQuery.AddRequirement<FArcNeedsFragment>(EMassFragmentAccess::ReadWrite);
	NeedsQuery.RegisterWithProcessor(*this);
}

void UArcNeedsProcessor::Execute(
	FMassEntityManager& EntityManager
  , FMassExecutionContext& Context)
{
	NeedsQuery.ForEachEntityChunk(EntityManager, Context, [this](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcNeedsFragment> Needs = Ctx.GetMutableFragmentView<FArcNeedsFragment>();
		const int32 NumEntities = Ctx.GetNumEntities();

		const float DeltaTime = Ctx.GetDeltaTimeSeconds();
			
		for(int32 EntityIndex = 0; EntityIndex < NumEntities; EntityIndex++)
		{
			for(int32 NeedIdx = 0; NeedIdx < Needs[EntityIndex].Needs.Num(); NeedIdx++)
			{
				Needs[EntityIndex].Needs[NeedIdx].CurrentValue =
					Needs[EntityIndex].Needs[NeedIdx].CurrentValue + (Needs[EntityIndex].Needs[NeedIdx].ChangeRate * DeltaTime);


				Needs[EntityIndex].Needs[NeedIdx].CurrentValue = FMath::Clamp(Needs[EntityIndex].Needs[NeedIdx].CurrentValue, 0.f, 100.f);
			}
		}
	});
}

UArcNeedsComponent::UArcNeedsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UArcNeedsComponent::BeginPlay()
{
	Super::BeginPlay();

	UMassEntitySubsystem* MES = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	FMassEntityManager& MEM = MES->GetMutableEntityManager();


	UMassAgentComponent* AgentComponent = GetOwner()->FindComponentByClass<UMassAgentComponent>();
	if (!AgentComponent)
	{
		return;
	}

	FMassEntityHandle Handle = AgentComponent->GetEntityHandle();
	if (!Handle.IsValid())
	{
		return;
	}
	
	FArcNeedsFragment* Data = MEM.GetFragmentDataPtr<FArcNeedsFragment>(Handle);

	Data->Needs = Needs;
}

UArcRestoreNeedComponent::UArcRestoreNeedComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UArcRestoreNeedComponent::Restore(
	AActor* For)
{

}

bool UArcRestoreNeedComponent::GetCanBeUsed() const
{
	double CurrentTime = GetWorld()->GetTimeSeconds();
	double Difference = CurrentTime - LastUseTime;
	if(Difference < LastUseTime)
	{
		return false;
	}
	return true;
}

float FArcNeedstConsideration::GetScore(FStateTreeExecutionContext& Context) const
{
	float Score = 0.f;

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.Actor)
	{
		return Score;
	}

	UMassAgentComponent* AgentComponent = InstanceData.Actor->FindComponentByClass<UMassAgentComponent>();
	if (!AgentComponent)
	{
		return Score;
	}

	UMassEntitySubsystem* MES = Context.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	FMassEntityManager& MEM = MES->GetMutableEntityManager();

	FMassEntityHandle Handle = AgentComponent->GetEntityHandle();
	if (!Handle.IsValid())
	{
		return Score;
	}
	
	FArcNeedsFragment* Data = MEM.GetFragmentDataPtr<FArcNeedsFragment>(Handle);

	int32 Idx = Data->Needs.IndexOfByKey(InstanceData.NeedName);
	if (Idx == INDEX_NONE)
	{
		return Score;
	}

	
	const float Normalized = Data->Needs[Idx].CurrentValue / 100.f;
	Score = ResponseCurve.Evaluate(Normalized);
	UE_VLOG_UELOG(Context.GetOwner(), LogArcConsiderationScore, VeryVerbose, TEXT("State %s Need %s: %.3f Normalized: %.3f"), *Context.GetActiveStateName(), *Name.ToString(), Score, Normalized);
	return Score;
}

float FArcMassNeedsConsideration::GetScore(FStateTreeExecutionContext& Context) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);

	UMassEntitySubsystem* MES = Context.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	FMassEntityManager& MEM = MES->GetMutableEntityManager();

	FArcNeedsFragment* Data = MEM.GetFragmentDataPtr<FArcNeedsFragment>(MassCtx.GetEntity());
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	float Score = 0.f;
	int32 Idx = Data->Needs.IndexOfByKey(InstanceData.NeedName);
	if (Idx == INDEX_NONE)
	{
		return Score;
	}

	
	const float Normalized = Data->Needs[Idx].CurrentValue / 100.f;
	Score = ResponseCurve.Evaluate(Normalized);
	UE_VLOG_UELOG(Context.GetOwner(), LogArcConsiderationScore, VeryVerbose, TEXT("State %s Need %s: %.3f Normalized: %.3f"), *Context.GetActiveStateName(), *Name.ToString(), Score, Normalized);
	return Score;
}

FArcProvideNextStateTreeTask::FArcProvideNextStateTreeTask()
{
	bShouldCallTick = false;
	bShouldCopyBoundPropertiesOnTick = true;
	bShouldCopyBoundPropertiesOnExitState = true;
}

EStateTreeRunStatus FArcProvideNextStateTreeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	
	if (InstanceData.TargetInput.GetEntityHandle().IsValid())
	{
		FMassEntityView TargetView(MassCtx.GetEntityManager(), InstanceData.TargetInput.GetEntityHandle());
		FArcNeedStateTreeFragment* PossibleActions = TargetView.GetFragmentDataPtr<FArcNeedStateTreeFragment>();
		if (PossibleActions)
		{
			
		}
		
	}	
	auto [StateTreePtr] = InstanceData.StateTree.GetMutablePtrTuple<UStateTree*>(Context);

	UStateTree* NextStateTree = nullptr;
	if (StateTreePtr)
	{
		NextStateTree = *StateTreePtr;
	}
	
	if (InstanceData.QueuedStateTrees.IsEmpty() && NextStateTree)
	{
		Context.BroadcastDelegate(InstanceData.OnQueueFinished);
		*StateTreePtr = nullptr;
		return EStateTreeRunStatus::Succeeded;
	}
	
	if (InstanceData.QueuedStateTrees.IsEmpty() == false)
	{
		*StateTreePtr = InstanceData.QueuedStateTrees.Pop();
		return EStateTreeRunStatus::Succeeded;
	}
	
	if (InstanceData.StateTrees.IsEmpty())
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.QueuedStateTrees = InstanceData.StateTrees;

	*StateTreePtr = InstanceData.QueuedStateTrees.Pop();
	return EStateTreeRunStatus::Succeeded;
	//Context.BroadcastDelegate(InstanceData.OnQueueFinished);
	
}

FArcMassInjectStateTreeTask::FArcMassInjectStateTreeTask()
{
	bShouldCallTick = false;
	bShouldCopyBoundPropertiesOnTick = true;
	bShouldCopyBoundPropertiesOnExitState = true;
}

EStateTreeRunStatus FArcMassInjectStateTreeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.StateTree)
	{
		return EStateTreeRunStatus::Failed;
	}

	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FMassEntityView View(MassCtx.GetEntityManager(), MassCtx.GetEntity());
	FMassStateTreeInstanceFragment* Frag = View.GetFragmentDataPtr<FMassStateTreeInstanceFragment>();
	if (!Frag)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	UMassStateTreeSubsystem* Subsystem = MassCtx.GetWorld()->GetSubsystem<UMassStateTreeSubsystem>();
	UMassSignalSubsystem* Signal = MassCtx.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
	
	FStateTreeReferenceOverrides* Overrides = Subsystem->GetLinkedStateTreeOverrides(Frag->InstanceHandle);
	InstanceData.StateTreeRef.SetStateTree(InstanceData.StateTree.Get());

	InstanceData.LinkedStateTreeOverrides.Reset();
	
	Overrides->AddOverride(InstanceData.StateTag, InstanceData.StateTreeRef);

	Signal->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {MassCtx.GetEntity()});
	return EStateTreeRunStatus::Succeeded;
}
