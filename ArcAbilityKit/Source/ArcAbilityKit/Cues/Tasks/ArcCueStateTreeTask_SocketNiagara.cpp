// Copyright Lukasz Baran. All Rights Reserved.

#include "Cues/Tasks/ArcCueStateTreeTask_SocketNiagara.h"

#include "Engine/World.h"
#include "MassEntitySubsystem.h"
#include "NiagaraComponent.h"
#include "StateTreeExecutionContext.h"

#include "Cues/ArcCueStateTreeContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcCueStateTreeTask_SocketNiagara)

namespace ArcAbilityKit::CueStateTreeTaskSocketNiagara
{
	static FArcCueBehavior_SocketNiagara MakeBehavior(const FArcCueStateTreeTask_SocketNiagaraInstanceData& InstanceData)
	{
		FArcCueBehavior_SocketNiagara Behavior;
		Behavior.NiagaraSystem = InstanceData.NiagaraSystem;
		Behavior.MeshComponentTag = InstanceData.MeshComponentTag;
		Behavior.SocketName = InstanceData.SocketName;
		Behavior.LocalOffset = InstanceData.LocalOffset;
		Behavior.Control.bStartOnApplication = true;
		Behavior.Control.InitialParameters = InstanceData.InitialParameters;
		return Behavior;
	}

	static void PopulateCueContext(
		FStateTreeExecutionContext& Context,
		FMassEntityHandle TargetEntity,
		FArcCueParameters& OutParameters,
		FArcCueHandle& OutHandle,
		const UArcCueDefinition*& OutDefinition)
	{
		const FStateTreeDataView ContextData = Context.GetContextDataByName(ArcAbilityKit::CueStateTree::ContextName);
		const FArcCueStateTreeContext* CueStateTreeContext = ContextData.GetPtr<FArcCueStateTreeContext>();
		if (CueStateTreeContext != nullptr)
		{
			OutParameters.OwnerEntity = CueStateTreeContext->OwnerEntity;
			OutParameters.SourceEntity = CueStateTreeContext->SourceEntity;
			OutParameters.Location = CueStateTreeContext->Location;
			OutParameters.Magnitude = CueStateTreeContext->Magnitude;
			OutParameters.Tags = CueStateTreeContext->Tags;
			if (CueStateTreeContext->bHasSourceContext)
			{
				OutParameters.SourceContext = FInstancedStruct::Make<FArcCueSourceContext>(CueStateTreeContext->SourceContext);
			}
			OutHandle = CueStateTreeContext->CueHandle;
			OutDefinition = CueStateTreeContext->CueDefinition.Get();
		}

		OutParameters.TargetEntity = TargetEntity;
	}

	static FMassEntityManager* ResolveEntityManager(UWorld* World)
	{
		UMassEntitySubsystem* MassSubsystem = World != nullptr ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
		return MassSubsystem != nullptr ? &MassSubsystem->GetMutableEntityManager() : nullptr;
	}
}

EStateTreeRunStatus FArcCueStateTreeTask_SocketNiagara::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	FArcCueParameters Parameters;
	FArcCueHandle Handle;
	const UArcCueDefinition* Definition = nullptr;
	ArcAbilityKit::CueStateTreeTaskSocketNiagara::PopulateCueContext(
		Context,
		InstanceData.TargetEntity,
		Parameters,
		Handle,
		Definition);

	UWorld* World = Context.GetWorld();
	FArcCueContext CueContext(Parameters);
	CueContext.Handle = Handle;
	CueContext.EntityManager = ArcAbilityKit::CueStateTreeTaskSocketNiagara::ResolveEntityManager(World);
	CueContext.World = World;
	CueContext.Definition = Definition;

	const FArcCueBehavior_SocketNiagara Behavior =
		ArcAbilityKit::CueStateTreeTaskSocketNiagara::MakeBehavior(InstanceData);
	TInstancedStruct<FArcCueBehaviorState> SpawnedState = Behavior.OnApplication(CueContext);
	FArcCueBehaviorState_SocketNiagara* SocketState =
		SpawnedState.GetMutablePtr<FArcCueBehaviorState_SocketNiagara>();
	if (SocketState == nullptr || !IsValid(SocketState->Component))
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.BehaviorState = *SocketState;
	SocketState->Component = nullptr;
	return EStateTreeRunStatus::Running;
}

void FArcCueStateTreeTask_SocketNiagara::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	FArcCueParameters Parameters;
	FArcCueHandle Handle;
	const UArcCueDefinition* Definition = nullptr;
	ArcAbilityKit::CueStateTreeTaskSocketNiagara::PopulateCueContext(
		Context,
		InstanceData.TargetEntity,
		Parameters,
		Handle,
		Definition);

	UWorld* World = Context.GetWorld();
	FArcCueContext CueContext(Parameters);
	CueContext.Handle = Handle;
	CueContext.EntityManager = ArcAbilityKit::CueStateTreeTaskSocketNiagara::ResolveEntityManager(World);
	CueContext.World = World;
	CueContext.Definition = Definition;

	const FArcCueBehavior_SocketNiagara Behavior =
		ArcAbilityKit::CueStateTreeTaskSocketNiagara::MakeBehavior(InstanceData);
	Behavior.OnRemoved(CueContext, InstanceData.BehaviorState);
	InstanceData.BehaviorState = FArcCueBehaviorState_SocketNiagara();
}
