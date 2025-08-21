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

#include "ArcAISTT_UtilityStateSelection.h"

#include "EngineUtils.h"
#include "StateTreeExecutionContext.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_StateTree_Event, "StateTree.Event.Test", "Test Event for state tree.");

float FArcStateTreeExecutionContext::ArcEvaluateUtility(FStateTreeStateHandle NextState)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(ArcStateTree_EvaluateUtility);
	FStateTreeExecutionState& Exec = GetExecState();
//
	const FStateTreeExecutionFrame* ParentFrame = nullptr;
	const FStateTreeExecutionFrame* CurrentFrame = FindFrame(GetStateTree(), FStateTreeStateHandle(0), Exec.ActiveFrames, ParentFrame);
	FStateSelectionResult OutSelectionResult;
	
	TGuardValue<const FStateSelectionResult*> GuardValue(CurrentSelectionResult, &OutSelectionResult);

	if (Exec.ActiveFrames.IsEmpty())
	{
		return 0.f;
	}
	
	if (!NextState.IsValid())
	{
		return 0.f;
	}

	// Walk towards the root from current state.
	TArray<FStateTreeStateHandle, TInlineAllocator<FStateTreeActiveStates::MaxStates>> PathToNextState;
	FStateTreeStateHandle CurrState = NextState;
	while (CurrState.IsValid())
	{
		if (PathToNextState.Num() == FStateTreeActiveStates::MaxStates)
		{
			return 0.f;
		}
		// Store the states that are in between the 'NextState' and common ancestor. 
		PathToNextState.Push(CurrState);
		CurrState = CurrentFrame->StateTree->GetStates()[CurrState.Index].Parent;
	}

	Algo::Reverse(PathToNextState);

	const UStateTree* NextStateTree = CurrentFrame->StateTree;
	const FStateTreeStateHandle NextRootState = PathToNextState[0]; 

	// Find the frame that the next state belongs to.
	int32 CurrentFrameIndex = INDEX_NONE;
	int32 CurrentStateTreeIndex = INDEX_NONE;

	for (int32 FrameIndex = Exec.ActiveFrames.Num() - 1; FrameIndex >= 0; FrameIndex--)
	{
		const FStateTreeExecutionFrame& Frame = Exec.ActiveFrames[FrameIndex]; 
		if (Frame.StateTree == NextStateTree)
		{
			CurrentStateTreeIndex = FrameIndex;
			if (Frame.RootState == NextRootState)
			{
				CurrentFrameIndex = FrameIndex;
				break;
			}
		}
	}

	// Copy common frames over.
	// ReferenceCurrentFrame is the original of the last copied frame. It will be used to keep track if we are following the current active frames and states.
	const FStateTreeExecutionFrame* CurrentFrameInActiveFrames  = nullptr;
	if (CurrentFrameIndex != INDEX_NONE)
	{
		const int32 NumCommonFrames = CurrentFrameIndex + 1;
		OutSelectionResult = FStateSelectionResult(MakeArrayView(Exec.ActiveFrames.GetData(), NumCommonFrames));
		CurrentFrameInActiveFrames  = &Exec.ActiveFrames[CurrentFrameIndex];
	}
	else if (CurrentStateTreeIndex != INDEX_NONE)
	{
		// If we could not find a common frame, we assume that we jumped to different subtree in same asset.
		const int32 NumCommonFrames = CurrentStateTreeIndex + 1;
		OutSelectionResult = FStateSelectionResult(MakeArrayView(Exec.ActiveFrames.GetData(), NumCommonFrames));
		CurrentFrameInActiveFrames  = &Exec.ActiveFrames[CurrentStateTreeIndex];
	}
	else
	{
		return 0.f;
	}
	
	// Append in between state in reverse order, they were collected from leaf towards the root.
	// Note: NextState will be added by SelectStateInternal() if conditions pass.
	const int32 LastFrameIndex = OutSelectionResult.FramesNum() - 1;
	FStateTreeExecutionFrame& LastFrame = OutSelectionResult.GetSelectedFrames()[LastFrameIndex];

	// Find index of the first state to be evaluated.
	int32 FirstNewStateIndex = 0;
	if (CurrentFrameIndex != INDEX_NONE)
	{
		// If LastFrame.ActiveStates is a subset of PathToNextState (e.g when someone use "TryEnter" selection behavior and then make a transition to it's child or if one is reentering the same state).
		// In such case loop below won't break on anything and FirstNewStateIndex will be incorrectly 0, thus we initialize it to be right after the shorter range.
		FirstNewStateIndex = FMath::Max(0, FMath::Min(PathToNextState.Num(), LastFrame.ActiveStates.Num()) - 1);
		for (int32 Index = 0; Index < FMath::Min(PathToNextState.Num(), LastFrame.ActiveStates.Num()); ++Index)
		{
			if (LastFrame.ActiveStates[Index] != PathToNextState[Index])
			{
				FirstNewStateIndex = Index;
				break;
			}
		}
	}

	LastFrame.ActiveStates.SetNum(FirstNewStateIndex);

	// Existing state's data is safe to access during select.
	//LastFrame.NumCurrentlyActiveStates = static_cast<uint8>(LastFrame.ActiveStates.Num());

	FStateSelectionResult InitialSelection;
	
	// We take copy of the last frame and assign it later, as SelectStateInternal() might change the array and invalidate the pointer.
	const FStateTreeExecutionFrame* CurrentParentFrame = LastFrameIndex > 0 ? &OutSelectionResult.GetSelectedFrames()[LastFrameIndex - 1] : nullptr;

	// Path from the first new state up to the NextState
	TConstArrayView<FStateTreeStateHandle> NewStatesPathToNextState(&PathToNextState[FirstNewStateIndex], PathToNextState.Num() - FirstNewStateIndex);

	const FCompactStateTreeState& CurrentState = CurrentFrame->StateTree->GetStates()[NextState.Index];
	//EvaluateUtility(CurrentParentFrame, *CurrentFrame, CurrentState.UtilityConsiderationsBegin, CurrentState.UtilityConsiderationsNum, CurrentState.Weight);

	int32 ConsiderationEndIdx = CurrentState.UtilityConsiderationsNum + CurrentState.UtilityConsiderationsBegin;
	if (CurrentState.UtilityConsiderationsNum > 0)
	{
		FStateTreeInstanceData InInstanceData;
		float FinalScore = 1.0f;
		
		for (int32 i = CurrentState.UtilityConsiderationsBegin; i < ConsiderationEndIdx; i++)
		{
			const FStateTreeConsiderationBase* Const = CurrentFrame->StateTree->GetNodes()[i].GetPtr<const FStateTreeConsiderationBase>();
			FStateTreeDataHandle Test = Const->InstanceDataHandle;
			
			const FStateTreeDataView ConditionInstanceView = GetDataView(CurrentParentFrame, *CurrentFrame, Const->InstanceDataHandle);
			FArcNodeInstanceDataScope Scope(*this, Const->InstanceDataHandle, ConditionInstanceView);

			if (Const->BindingsBatch.IsValid())
			{
				// Use validated copy, since we test in situations where the sources are not always valid (e.g. considerations may try to access inactive parent state). 
				if (!CopyBatchWithValidation(CurrentParentFrame, *CurrentFrame, ConditionInstanceView, Const->BindingsBatch))
				{
					// If the source data cannot be accessed, the whole expression evaluates to zero.
					break;
				}
			}
			
			float Score = Const->GetNormalizedScore(*this);
			FinalScore *= Score;
			if (Score <= 0.05)
			{
				break;
			}
		}

		return FinalScore;
	}

	return 0.f;
}

EStateTreeRunStatus FArcAISTT_UtilityStateSelectionTask::EnterState(FStateTreeExecutionContext& Context
																	, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.CurrentState = Transition.CurrentState;
	InstanceData.SourceState = Transition.SourceState;
	InstanceData.TargetState = Transition.TargetState;
	InstanceData.bStateSelected = false;
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcAISTT_UtilityStateSelectionTask::Tick(FStateTreeExecutionContext& Context
	, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.bStateSelected)
	{
		return EStateTreeRunStatus::Running;
	}
	
	const UStateTree* ST = Context.GetStateTree();

	const FInstancedStructContainer& Nodes = ST->GetNodes();
	
	int32 NumStates = InstanceData.ConsideredStates.Num();
	if (NumStates == 0)
	{
		return EStateTreeRunStatus::Running;
	}

	TArray<ConState> ConStates;
	ConStates.Reset(InstanceData.ConsideredStates.Num() * InstanceData.InActors.Num());

	for (const FStateTreeStateLink& StateLink : InstanceData.ConsideredStates)
	{
		const FCompactStateTreeState* StatePtr = Context.GetStateFromHandle(StateLink.StateHandle);
		
		float FinalScore = 1.f;
		for (AActor* A : InstanceData.InActors)
		{
			if (A)
			{
				InstanceData.CurrentActor = A;
				if (StatePtr->UtilityConsiderationsNum > 0)
				{
					FArcStateTreeExecutionContext ParallelTreeContext(Context, *ST, *Context.GetMutableInstanceData());

					FinalScore = ParallelTreeContext.ArcEvaluateUtility(StateLink.StateHandle);
					ConStates.Add( {StateLink.StateHandle, FinalScore, A});
				}		
			}
		}	
	}

	ConStates.Sort([](const ConState& A, const ConState& B) { return A.Score > B.Score; });
	
	FArcAIUtilityStateEvent Data;
	Data.TargetActor = ConStates[0].Target.Get();
	FConstStructView View = FConstStructView::Make(Data);

	InstanceData.SelectedActor = ConStates[0].Target.Get();
	
	Context.SendEvent(TAG_StateTree_Event, View);
	Context.RequestTransition(ConStates[0].Handle, EStateTreeTransitionPriority::Critical);
	//Context.ForceTransition(ForceTransition);
	InstanceData.bStateSelected = true;
	return EStateTreeRunStatus::Running;
}

float FArcActorConsideration::GetScore(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.Input)
	{
		return 1.f;
	}

	return 0.f;
}

