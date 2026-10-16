// Copyright Lukasz Baran. All Rights Reserved.

#include "Behaviors/ArcCueBehavior_StateTree.h"

#include "Engine/World.h"
#include "StateTree.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeEvents.h"
#include "Subsystems/WorldSubsystem.h"

#include "ArcCueDefinition.h"
#include "ArcCuesModule.h"
#include "Cues/ArcCueSourceContext.h"
#include "Cues/ArcCueStateTreeSchema.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcCueBehavior_StateTree)

namespace ArcAbilityKit::CueStateTreeBehavior
{
	static void CopyCueContext(const FArcCueContext& Context, FArcCueStateTreeContext& OutContext)
	{
		OutContext.CueHandle = Context.Handle;
		OutContext.CueDefinition = Context.Definition;
		OutContext.OwnerEntity = Context.Parameters.OwnerEntity;
		OutContext.SourceEntity = Context.Parameters.SourceEntity;
		OutContext.TargetEntity = Context.Parameters.TargetEntity;
		OutContext.Location = Context.Parameters.Location;
		OutContext.Magnitude = Context.Parameters.Magnitude;
		OutContext.Tags = Context.Parameters.Tags;
		OutContext.bHasSourceContext = false;
		OutContext.SourceContext = FArcCueSourceContext();

		if (const FArcCueSourceContext* SourceContext = Context.Parameters.SourceContext.GetPtr<FArcCueSourceContext>())
		{
			OutContext.bHasSourceContext = true;
			OutContext.SourceContext = *SourceContext;
		}
	}

	static bool SetContextRequirements(
		FStateTreeExecutionContext& ExecutionContext,
		FArcCueStateTreeContext& CueContext,
		UWorld& World)
	{
		ExecutionContext.SetContextDataByName(
			ArcAbilityKit::CueStateTree::ContextName,
			FStateTreeDataView(FStructView::Make(CueContext)));

		ExecutionContext.SetCollectExternalDataCallback(FOnCollectStateTreeExternalData::CreateLambda(
			[WorldPtr = &World](
				const FStateTreeExecutionContext& Context,
				const UStateTree* StateTree,
				TArrayView<const FStateTreeExternalDataDesc> ExternalDescs,
				TArrayView<FStateTreeDataView> OutDataViews)
			{
				for (int32 Index = 0; Index < ExternalDescs.Num(); ++Index)
				{
					const FStateTreeExternalDataDesc& Desc = ExternalDescs[Index];
					if (Desc.Struct != nullptr && Desc.Struct->IsChildOf(UWorldSubsystem::StaticClass()))
					{
						UWorldSubsystem* Subsystem = WorldPtr->GetSubsystemBase(
							Cast<UClass>(const_cast<UStruct*>(ToRawPtr(Desc.Struct))));
						if (Subsystem != nullptr)
						{
							OutDataViews[Index] = FStateTreeDataView(Subsystem);
						}
					}
				}
				return true;
			}));

		return ExecutionContext.AreContextDataViewsValid();
	}

	static bool IsCompatibleTree(const UStateTree& StateTree)
	{
		return Cast<UArcCueStateTreeSchema>(StateTree.GetSchema()) != nullptr;
	}

	static EStateTreeRunStatus StartTree(
		UWorld& World,
		const UStateTree& StateTree,
		FStateTreeInstanceData& InstanceData,
		FArcCueStateTreeContext& CueContext,
		FConstStructView GlobalParameters)
	{
		if (!IsCompatibleTree(StateTree))
		{
			UE_LOGF(LogArcCues, Warning, "[CueStateTree] Start skipped: incompatible schema on StateTree=%ls", *GetPathNameSafe(&StateTree));
			return EStateTreeRunStatus::Failed;
		}

		FStateTreeExecutionContext ExecutionContext(World, StateTree, InstanceData);
		if (!ExecutionContext.IsValid() || !SetContextRequirements(ExecutionContext, CueContext, World))
		{
			return EStateTreeRunStatus::Failed;
		}
		return ExecutionContext.Start(GlobalParameters);
	}

	static EStateTreeRunStatus TickTree(
		UWorld& World,
		const UStateTree& StateTree,
		FStateTreeInstanceData& InstanceData,
		FArcCueStateTreeContext& CueContext,
		float DeltaTime)
	{
		FStateTreeExecutionContext ExecutionContext(World, StateTree, InstanceData);
		if (!ExecutionContext.IsValid() || !SetContextRequirements(ExecutionContext, CueContext, World))
		{
			return EStateTreeRunStatus::Failed;
		}
		return ExecutionContext.Tick(DeltaTime);
	}

	static void SendEvent(
		UWorld& World,
		const UStateTree& StateTree,
		FStateTreeInstanceData& InstanceData,
		FArcCueStateTreeContext& CueContext,
		const FStateTreeEvent& Event)
	{
		FStateTreeExecutionContext ExecutionContext(World, StateTree, InstanceData);
		if (ExecutionContext.IsValid() && SetContextRequirements(ExecutionContext, CueContext, World))
		{
			ExecutionContext.SendEvent(Event.Tag, Event.Payload, Event.Origin);
		}
	}

	static void StopTree(
		UWorld& World,
		const UStateTree& StateTree,
		FStateTreeInstanceData& InstanceData,
		FArcCueStateTreeContext& CueContext)
	{
		FStateTreeExecutionContext ExecutionContext(World, StateTree, InstanceData);
		if (ExecutionContext.IsValid() && SetContextRequirements(ExecutionContext, CueContext, World))
		{
			ExecutionContext.Stop();
		}
	}
}

void FArcCueBehavior_StateTree::OnExecute(const FArcCueContext& Context) const
{
	UWorld* World = Context.World;
	const UStateTree* StateTree = StateTreeRef.GetStateTree();
	if (World == nullptr || StateTree == nullptr)
	{
		return;
	}

	FArcCueBehaviorState_StateTree StateValue;
	ArcAbilityKit::CueStateTreeBehavior::CopyCueContext(Context, StateValue.CueContext);
	StateValue.LastRunStatus = ArcAbilityKit::CueStateTreeBehavior::StartTree(
		*World,
		*StateTree,
		StateValue.InstanceData,
		StateValue.CueContext,
		StateTreeRef.GetGlobalParameters());
	if (StateValue.LastRunStatus == EStateTreeRunStatus::Running)
	{
		ArcAbilityKit::CueStateTreeBehavior::StopTree(*World, *StateTree, StateValue.InstanceData, StateValue.CueContext);
	}
}

TInstancedStruct<FArcCueBehaviorState> FArcCueBehavior_StateTree::OnApplication(const FArcCueContext& Context) const
{
	UWorld* World = Context.World;
	const UStateTree* StateTree = StateTreeRef.GetStateTree();
	if (World == nullptr || StateTree == nullptr)
	{
		return {};
	}

	FArcCueBehaviorState_StateTree StateValue;
	ArcAbilityKit::CueStateTreeBehavior::CopyCueContext(Context, StateValue.CueContext);
	StateValue.LastRunStatus = ArcAbilityKit::CueStateTreeBehavior::StartTree(
		*World,
		*StateTree,
		StateValue.InstanceData,
		StateValue.CueContext,
		StateTreeRef.GetGlobalParameters());
	if (StateValue.LastRunStatus == EStateTreeRunStatus::Failed)
	{
		return {};
	}

	return TInstancedStruct<FArcCueBehaviorState>::Make<FArcCueBehaviorState_StateTree>(StateValue);
}

void FArcCueBehavior_StateTree::OnStateTreeEvent(
	const FArcCueContext& Context,
	const FStateTreeEvent& Event,
	TInstancedStruct<FArcCueBehaviorState>& State) const
{
	FArcCueBehaviorState_StateTree* TypedState = State.GetMutablePtr<FArcCueBehaviorState_StateTree>();
	UWorld* World = Context.World;
	const UStateTree* StateTree = StateTreeRef.GetStateTree();
	if (TypedState == nullptr || World == nullptr || StateTree == nullptr
		|| TypedState->LastRunStatus != EStateTreeRunStatus::Running)
	{
		return;
	}

	ArcAbilityKit::CueStateTreeBehavior::CopyCueContext(Context, TypedState->CueContext);
	ArcAbilityKit::CueStateTreeBehavior::SendEvent(
		*World,
		*StateTree,
		TypedState->InstanceData,
		TypedState->CueContext,
		Event);
}

void FArcCueBehavior_StateTree::OnTick(const FArcCueContext& Context, FArcCueBehaviorState& State, float DeltaTime) const
{
	FArcCueBehaviorState_StateTree& TypedState = static_cast<FArcCueBehaviorState_StateTree&>(State);
	UWorld* World = Context.World;
	const UStateTree* StateTree = StateTreeRef.GetStateTree();
	if (World == nullptr || StateTree == nullptr || TypedState.LastRunStatus != EStateTreeRunStatus::Running)
	{
		return;
	}

	ArcAbilityKit::CueStateTreeBehavior::CopyCueContext(Context, TypedState.CueContext);
	TypedState.LastRunStatus = ArcAbilityKit::CueStateTreeBehavior::TickTree(
		*World,
		*StateTree,
		TypedState.InstanceData,
		TypedState.CueContext,
		DeltaTime);
}

void FArcCueBehavior_StateTree::OnRemoved(const FArcCueContext& Context, FArcCueBehaviorState& State) const
{
	FArcCueBehaviorState_StateTree& TypedState = static_cast<FArcCueBehaviorState_StateTree&>(State);
	UWorld* World = Context.World;
	const UStateTree* StateTree = StateTreeRef.GetStateTree();
	if (World != nullptr && StateTree != nullptr)
	{
		ArcAbilityKit::CueStateTreeBehavior::CopyCueContext(Context, TypedState.CueContext);
		ArcAbilityKit::CueStateTreeBehavior::StopTree(
			*World,
			*StateTree,
			TypedState.InstanceData,
			TypedState.CueContext);
	}
	TypedState.LastRunStatus = EStateTreeRunStatus::Unset;
}
