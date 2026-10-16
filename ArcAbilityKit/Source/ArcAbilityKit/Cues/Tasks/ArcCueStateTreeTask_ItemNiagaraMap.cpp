// Copyright Lukasz Baran. All Rights Reserved.

#include "Cues/Tasks/ArcCueStateTreeTask_ItemNiagaraMap.h"

#include "Engine/World.h"
#include "Items/ArcItemDefinitionData.h"
#include "MassEntitySubsystem.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "StateTreeExecutionContext.h"

#include "Cues/ArcCueStateTreeContext.h"
#include "ItemFragments/ArcItemFragment_NiagaraSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcCueStateTreeTask_ItemNiagaraMap)

namespace ArcAbilityKit::CueStateTreeTaskItemNiagaraMap
{
	static const FArcCueStateTreeContext* PopulateCueContext(
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
		return CueStateTreeContext;
	}

	static FMassEntityManager* ResolveEntityManager(UWorld* World)
	{
		UMassEntitySubsystem* MassSubsystem = World != nullptr ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
		return MassSubsystem != nullptr ? &MassSubsystem->GetMutableEntityManager() : nullptr;
	}

	static FArcCueBehavior_SocketNiagara MakeBehavior(
		UNiagaraSystem* NiagaraSystem,
		const FArcCueItemNiagaraMapAttachment& Attachment)
	{
		FArcCueBehavior_SocketNiagara Behavior;
		Behavior.NiagaraSystem = NiagaraSystem;
		Behavior.MeshComponentTag = Attachment.MeshComponentTag;
		Behavior.SocketName = Attachment.SocketName;
		Behavior.LocalOffset = Attachment.LocalOffset;
		Behavior.Control.bStartOnApplication = true;
		Behavior.Control.InitialParameters = Attachment.InitialParameters;
		return Behavior;
	}

	static void Cleanup(
		const FArcCueContext& CueContext,
		TArray<FArcCueBehaviorState_SocketNiagara>& BehaviorStates)
	{
		FArcCueBehavior_SocketNiagara Behavior;
		for (FArcCueBehaviorState_SocketNiagara& BehaviorState : BehaviorStates)
		{
			Behavior.OnRemoved(CueContext, BehaviorState);
		}
		BehaviorStates.Reset();
	}
}

EStateTreeRunStatus FArcCueStateTreeTask_ItemNiagaraMap::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	FArcCueParameters Parameters;
	FArcCueHandle Handle;
	const UArcCueDefinition* Definition = nullptr;
	const FArcCueStateTreeContext* CueStateTreeContext =
		ArcAbilityKit::CueStateTreeTaskItemNiagaraMap::PopulateCueContext(
			Context,
			InstanceData.TargetEntity,
			Parameters,
			Handle,
			Definition);

	UWorld* World = Context.GetWorld();
	FArcCueContext CueContext(Parameters);
	CueContext.Handle = Handle;
	CueContext.EntityManager = ArcAbilityKit::CueStateTreeTaskItemNiagaraMap::ResolveEntityManager(World);
	CueContext.World = World;
	CueContext.Definition = Definition;

	ArcAbilityKit::CueStateTreeTaskItemNiagaraMap::Cleanup(CueContext, InstanceData.BehaviorStates);

	if (InstanceData.Attachments.IsEmpty())
	{
		UE_LOGF(LogStateTree, Warning, "Item Niagara Map has no attachment entries (index=none, tag=none, socket=none).");
		return EStateTreeRunStatus::Failed;
	}

	const FArcCueItemNiagaraMapAttachment& FirstAttachment = InstanceData.Attachments[0];
	if (CueStateTreeContext == nullptr)
	{
		UE_LOGF(LogStateTree, Warning,
			"Item Niagara Map cue context is unavailable (first index=0, tag='%ls', socket='%ls').",
			*FirstAttachment.EffectTag.ToString(), *FirstAttachment.SocketName.ToString());
		return EStateTreeRunStatus::Failed;
	}

	const FArcItemDefinitionData* ItemDefinition = FArcItemDefinitionData::Find(CueStateTreeContext->SourceContext.SourceItemDefinition);
	if (ItemDefinition == nullptr)
	{
		UE_LOGF(LogStateTree, Warning,
			"Item Niagara Map source item definition is null (first index=0, tag='%ls', socket='%ls').",
			*FirstAttachment.EffectTag.ToString(), *FirstAttachment.SocketName.ToString());
		return EStateTreeRunStatus::Failed;
	}

	const FArcItemFragment_NiagaraSystemMap* NiagaraMap =
		ItemDefinition->FindFragment<FArcItemFragment_NiagaraSystemMap>();
	if (NiagaraMap == nullptr)
	{
		UE_LOGF(LogStateTree, Warning,
			"Item Niagara Map item definition '%ls' has no Niagara system map fragment (first index=0, tag='%ls', socket='%ls').",
			*CueStateTreeContext->SourceContext.SourceItemDefinition.ToString(), *FirstAttachment.EffectTag.ToString(), *FirstAttachment.SocketName.ToString());
		return EStateTreeRunStatus::Failed;
	}

	for (int32 AttachmentIndex = 0; AttachmentIndex < InstanceData.Attachments.Num(); ++AttachmentIndex)
	{
		const FArcCueItemNiagaraMapAttachment& Attachment = InstanceData.Attachments[AttachmentIndex];
		if (!Attachment.EffectTag.IsValid())
		{
			UE_LOGF(LogStateTree, Warning,
				"Item Niagara Map attachment index=%d has an invalid effect tag (tag='%ls', socket='%ls').",
				AttachmentIndex, *Attachment.EffectTag.ToString(), *Attachment.SocketName.ToString());
			ArcAbilityKit::CueStateTreeTaskItemNiagaraMap::Cleanup(CueContext, InstanceData.BehaviorStates);
			return EStateTreeRunStatus::Failed;
		}

		if (Attachment.SocketName.IsNone())
		{
			UE_LOGF(LogStateTree, Warning,
				"Item Niagara Map attachment index=%d has no socket name (tag='%ls', socket='%ls').",
				AttachmentIndex, *Attachment.EffectTag.ToString(), *Attachment.SocketName.ToString());
			ArcAbilityKit::CueStateTreeTaskItemNiagaraMap::Cleanup(CueContext, InstanceData.BehaviorStates);
			return EStateTreeRunStatus::Failed;
		}

		const TSoftObjectPtr<UNiagaraSystem>* SystemReference = NiagaraMap->Systems.Find(Attachment.EffectTag);
		if (SystemReference == nullptr)
		{
			UE_LOGF(LogStateTree, Warning,
				"Item Niagara Map attachment index=%d has no system for exact tag '%ls' (socket='%ls').",
				AttachmentIndex, *Attachment.EffectTag.ToString(), *Attachment.SocketName.ToString());
			ArcAbilityKit::CueStateTreeTaskItemNiagaraMap::Cleanup(CueContext, InstanceData.BehaviorStates);
			return EStateTreeRunStatus::Failed;
		}

		UNiagaraSystem* NiagaraSystem = SystemReference->LoadSynchronous();
		if (!IsValid(NiagaraSystem))
		{
			UE_LOGF(LogStateTree, Warning,
				"Item Niagara Map attachment index=%d could not load the system for tag '%ls' (socket='%ls').",
				AttachmentIndex, *Attachment.EffectTag.ToString(), *Attachment.SocketName.ToString());
			ArcAbilityKit::CueStateTreeTaskItemNiagaraMap::Cleanup(CueContext, InstanceData.BehaviorStates);
			return EStateTreeRunStatus::Failed;
		}

		const FArcCueBehavior_SocketNiagara Behavior =
			ArcAbilityKit::CueStateTreeTaskItemNiagaraMap::MakeBehavior(NiagaraSystem, Attachment);
		TInstancedStruct<FArcCueBehaviorState> SpawnedState = Behavior.OnApplication(CueContext);
		FArcCueBehaviorState_SocketNiagara* SocketState =
			SpawnedState.GetMutablePtr<FArcCueBehaviorState_SocketNiagara>();
		if (SocketState == nullptr || !IsValid(SocketState->Component))
		{
			UE_LOGF(LogStateTree, Warning,
				"Item Niagara Map attachment index=%d failed to spawn tag '%ls' on socket '%ls'.",
				AttachmentIndex, *Attachment.EffectTag.ToString(), *Attachment.SocketName.ToString());
			ArcAbilityKit::CueStateTreeTaskItemNiagaraMap::Cleanup(CueContext, InstanceData.BehaviorStates);
			return EStateTreeRunStatus::Failed;
		}

		InstanceData.BehaviorStates.Add(*SocketState);
		SocketState->Component = nullptr;
	}

	return EStateTreeRunStatus::Running;
}

void FArcCueStateTreeTask_ItemNiagaraMap::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	FArcCueParameters Parameters;
	FArcCueHandle Handle;
	const UArcCueDefinition* Definition = nullptr;
	ArcAbilityKit::CueStateTreeTaskItemNiagaraMap::PopulateCueContext(
		Context,
		InstanceData.TargetEntity,
		Parameters,
		Handle,
		Definition);

	UWorld* World = Context.GetWorld();
	FArcCueContext CueContext(Parameters);
	CueContext.Handle = Handle;
	CueContext.EntityManager = ArcAbilityKit::CueStateTreeTaskItemNiagaraMap::ResolveEntityManager(World);
	CueContext.World = World;
	CueContext.Definition = Definition;

	ArcAbilityKit::CueStateTreeTaskItemNiagaraMap::Cleanup(CueContext, InstanceData.BehaviorStates);
}
