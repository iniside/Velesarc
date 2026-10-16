// Copyright Lukasz Baran. All Rights Reserved.

#include "Decision/ArcTacticsDecision.h"

#include "Abilities/ArcAbilityDefinition.h"
#include "Actions/ArcTacticsAction.h"
#include "Considerations/ArcTacticsConsiderations.h"
#include "Debugger/ArcTacticsDebug.h"
#include "Debugger/ArcTacticsTrace.h"
#include "Decision/ArcTacticsConfig.h"
#include "Decision/ArcTacticsSelectors.h"
#include "Engine/World.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Mass/ArcTacticsInstanceSubsystem.h"
#include "Mass/EntityView.h"
#include "Misc/ScopeExit.h"
#include "UtilityAI/ArcUtilityConsideration.h"
#include "UtilityAI/ArcUtilityScoring.h"
#include "UtilityAI/ArcUtilityTypes.h"
#include "UObject/UnrealType.h"

namespace ArcTacticsDecisionPrivate
{
	const FArcTacticsNode* GetNode(
		const FArcTacticsCompiledData& CompiledData,
		const FArcTacticsNodeHandle Handle)
	{
		return CompiledData.Nodes.IsValidIndex(Handle.Get())
			? CompiledData.Nodes[Handle.Get()].Node.GetPtr<FArcTacticsNode>()
			: nullptr;
	}

	FArcTQSFunctionContext MakeFunctionContext(const FArcTacticsTaskContext& Context, const FMassEntityHandle Self)
	{
		FArcTQSFunctionContext FunctionContext;
		FunctionContext.EntityManager = Context.EntityManager;
		FunctionContext.World = Context.World;
		FunctionContext.Self = Self;
		return FunctionContext;
	}

	bool ApplyNodeBindings(
		FArcTacticsInstanceData& Instance,
		const FArcTacticsCompiledData& CompiledData,
		const FArcTQSFunctionContext& FunctionContext,
		const FArcTacticsNodeHandle NodeHandle,
		const FPropertyBindingDataView TargetView,
		const FArcTacticsOption* Scope = nullptr,
		const bool bOutputs = false)
	{
		const int32 NodeIndex = NodeHandle.Get();
		if (!CompiledData.Nodes.IsValidIndex(NodeIndex))
		{
			return false;
		}
		const FArcTacticsCompiledNode& Node = CompiledData.Nodes[NodeIndex];
		return Instance.ApplyBindings(
			CompiledData,
			bOutputs ? Node.OutputBindingsBatch : Node.BindingsBatch,
			TargetView,
			FunctionContext,
			Scope);
	}

	bool ApplyScratchNodeBindings(
		FArcTacticsInstanceData& Instance,
		const FArcTacticsCompiledData& CompiledData,
		const FArcTQSFunctionContext& FunctionContext,
		const FArcTacticsNodeHandle NodeHandle,
		const bool bOutputs = false)
	{
		const int32 NodeIndex = NodeHandle.Get();
		return CompiledData.Nodes.IsValidIndex(NodeIndex)
			&& ApplyNodeBindings(
				Instance,
				CompiledData,
				FunctionContext,
				NodeHandle,
				Instance.GetMutableDataView(CompiledData.Nodes[NodeIndex].InstanceData),
				nullptr,
				bOutputs);
	}

	bool ResetBranch(
		FArcTacticsInstanceData& Instance,
		const FArcTacticsCompiledData& CompiledData,
		const FArcTQSFunctionContext& FunctionContext,
		const FArcTacticsDataHandle ParametersHandle,
		const FArcTacticsDataHandle ParameterBagHandle,
		const FArcTacticsBranchParameters& DefaultParameters,
		const FInstancedPropertyBag& DefaultParameterBag,
		const FPropertyBindingIndex16 ParameterBagBindings,
		const FPropertyBindingIndex16 ParametersBindings,
		FName& OutReason)
	{
		const int32 BranchIndex = ParametersHandle.GetIndex();
		if (ParametersHandle.GetKind() != EArcTacticsBindingEndpointKind::BranchParameters
			|| ParameterBagHandle.GetKind() != EArcTacticsBindingEndpointKind::BranchParameterBag
			|| ParameterBagHandle.GetIndex() != BranchIndex
			|| !Instance.BranchParameters.IsValidIndex(BranchIndex)
			|| !Instance.BranchParameterBags.IsValidIndex(BranchIndex))
		{
			OutReason = TEXT("InvalidBranchStorage");
			return false;
		}
		Instance.BranchParameters[BranchIndex] = DefaultParameters;
		Instance.BranchParameterBags[BranchIndex] = DefaultParameterBag;
		if (!Instance.ApplyBindings(
				CompiledData, ParameterBagBindings,
				Instance.GetMutableDataView(ParameterBagHandle), FunctionContext))
		{
			OutReason = TEXT("BranchBagInitializationFailed");
			return false;
		}
		if (!Instance.ApplyBindings(
				CompiledData, ParametersBindings,
				Instance.GetMutableDataView(ParametersHandle), FunctionContext))
		{
			OutReason = TEXT("BranchParameterBindingFailed");
			return false;
		}
		return true;
	}

	void CaptureScope(
		const FArcTacticsInstanceData& Instance,
		const int32 BranchIndex,
		FArcTacticsOption& Option)
	{
		Option.Parameters = Instance.BranchParameters[BranchIndex];
		Option.ParameterBag = Instance.BranchParameterBags[BranchIndex];
	}

	const FArcTacticsSelectorNode* GetSelectorNode(
		const FArcTacticsCompiledData& CompiledData,
		const FArcTacticsCompiledSelector& Selector)
	{
		return CompiledData.Nodes.IsValidIndex(Selector.Selector.Get())
			? CompiledData.Nodes[Selector.Selector.Get()].Node.GetPtr<FArcTacticsSelectorNode>()
			: nullptr;
	}

	FName FirstExclusionDiagnostic(const FArcTacticsInstanceSlot& Slot)
	{
		for (const TUniquePtr<FArcTacticsPackage>& Package : Slot.Packages)
		{
			if (Package->bActive && Package->bExcluded)
			{
				return Package->Instance.Diagnostic;
			}
		}
		return NAME_None;
	}

	bool AddCandidate(
		FArcTacticsInstanceSlot& Slot,
		const int32 PackageIndex,
		FArcTacticsPackage& Package,
		FArcTacticsTaskContext& TaskContext,
		const FArcTacticsNodeHandle ActionHandle,
		const TConstArrayView<FArcTacticsNodeHandle> Steps,
		const TConstArrayView<FArcTacticsNodeHandle> ActiveSteps,
		const TConstArrayView<FArcTacticsNodeHandle> Considerations,
		const FArcTacticsDataHandle ParametersHandle,
		const FArcTacticsDataHandle ParameterBagHandle,
		const FArcTacticsBranchParameters& DefaultParameters,
		const FInstancedPropertyBag& DefaultParameterBag,
		const FPropertyBindingIndex16 ParameterBagBindings,
		const FPropertyBindingIndex16 ParametersBindings,
		const FName Name,
		const int32 SourceIndex,
		const bool bFromSelector,
		const FArcTacticsCandidateIdentity& Identity,
		const int32 ItemIndex)
	{
		FArcTacticsInstanceData& Instance = Package.Instance;
		const FArcTacticsCompiledData& CompiledData = *Package.Definitions;
		const FArcTQSFunctionContext FunctionContext = MakeFunctionContext(TaskContext, Slot.Context.Self);
		FArcTacticsCandidate& Candidate = Slot.Candidates.AddDefaulted_GetRef();
		Candidate.Order = Slot.Candidates.Num() - 1;
		Candidate.Option.Action = ActionHandle;
		Candidate.Option.Name = Name;
		Candidate.Option.SourceIndex = SourceIndex;
		Candidate.Option.bFromSelector = bFromSelector;
		Candidate.Option.Identity = Identity;
		Candidate.Option.ItemIndex = ItemIndex;
		Candidate.Option.PackageIndex = PackageIndex;
		Candidate.Option.PackageSerial = Package.Serial;

		FArcUtilityPairResult PairResult;
		FArcUtilityPairTrace* PairTraceSink = nullptr;
#if WITH_ARCTACTICS_TRACE
		const int32 CandidateIndex = Candidate.Order;
		int32 TraceBreakIndex = INDEX_NONE;
		TOptional<int32> TracePriority;
		TArray<FArcUtilityConsiderationSnapshot> TraceSnapshots;
		FArcUtilityPairTrace PairTrace;
		const bool bTraceCandidate = TRACE_ARCTACTICS_ENABLED();
		const bool bCaptureCandidate = TaskContext.World
			&& ArcTacticsDebug::IsCapturing(*TaskContext.World, Slot.Context.Self);
		if (bTraceCandidate || bCaptureCandidate)
		{
			PairTrace.Snapshots = &TraceSnapshots;
			PairTraceSink = &PairTrace;
		}
		ON_SCOPE_EXIT
		{
			if (!bTraceCandidate && !bCaptureCandidate)
			{
				return;
			}
			const bool bVetoBreak = TraceBreakIndex != INDEX_NONE
				&& PairResult.Outcome == EArcUtilityPairOutcome::Vetoed;
			const int32 EarlyOutIndex = bVetoBreak ? INDEX_NONE : TraceBreakIndex;
			const int32 VetoIndex = bVetoBreak ? TraceBreakIndex : INDEX_NONE;
			if (bTraceCandidate)
			{
				ArcTacticsTrace::OutputCandidate(
					TaskContext.World, TaskContext.EntityManager, Slot, CompiledData, Slot.Candidates[CandidateIndex],
					Considerations, TraceSnapshots, EarlyOutIndex, VetoIndex, TracePriority);
			}
			if (bCaptureCandidate)
			{
				ArcTacticsDebug::CaptureCandidate(
					*TaskContext.World, Slot, CompiledData, Slot.Candidates[CandidateIndex],
					Considerations, TraceSnapshots, EarlyOutIndex, VetoIndex, TracePriority);
			}
		};
#endif

		FName BranchFailure;
		if (!ResetBranch(
			Instance, CompiledData, FunctionContext, ParametersHandle, ParameterBagHandle,
			DefaultParameters, DefaultParameterBag, ParameterBagBindings, ParametersBindings,
			BranchFailure))
		{
			Candidate.Outcome = EArcTacticsCandidateOutcome::BindingFailed;
			Candidate.RejectionReason = BranchFailure;
			return false;
		}
		const int32 BranchIndex = ParametersHandle.GetIndex();
		const float Weight = Instance.BranchParameters[BranchIndex].Weight;
#if WITH_ARCTACTICS_TRACE
		TracePriority = Instance.BranchParameters[BranchIndex].Priority;
#endif
		if (Slot.ActiveAction.IsSet()
			&& ((Slot.ActiveAction.PackageSerial == Package.Serial
					&& FArcTacticsInstanceData::GetBranchIndex(CompiledData, Slot.ActiveAction) == BranchIndex)
				|| Instance.BranchParameters[BranchIndex].Priority <= Slot.ActiveAction.Parameters.Priority))
		{
			Candidate.Outcome = EArcTacticsCandidateOutcome::Vetoed;
			Candidate.RejectionReason = TEXT("BelowActivePriority");
			return false;
		}
		const double* CooldownEndTime = Slot.CooldownEndTimes.Find(
			FArcTacticsCooldownKey(Package.Config.Get(), BranchIndex));
		if (CooldownEndTime && TaskContext.World
			&& TaskContext.World->GetTimeSeconds() < *CooldownEndTime)
		{
			Candidate.Outcome = EArcTacticsCandidateOutcome::Vetoed;
			Candidate.RejectionReason = TEXT("ActionCooldown");
			return false;
		}
		EArcUtilityPairProgress Progress = ArcUtilityScoring::BeginPair(
			Weight, Considerations.Num(), Instance.DecisionSettings.MinScore, PairResult);
		if (Progress == EArcUtilityPairProgress::Invalid)
		{
			Candidate.Outcome = EArcTacticsCandidateOutcome::BindingFailed;
			Candidate.RejectionReason = PairResult.VetoReason;
			return false;
		}

		FArcUtilityContext UtilityContext;
		UtilityContext.QuerierEntity = Slot.Context.Self;
		UtilityContext.World = TaskContext.World;
		UtilityContext.EntityManager = TaskContext.EntityManager;
		for (int32 ConsiderationIndex = 0; ConsiderationIndex < Considerations.Num(); ++ConsiderationIndex)
		{
			const FArcTacticsNodeHandle ConsiderationHandle = Considerations[ConsiderationIndex];
			const int32 NodeIndex = ConsiderationHandle.Get();
			if (!Instance.ResetNodeData(CompiledData, ConsiderationHandle)
				|| !ApplyScratchNodeBindings(Instance, CompiledData, FunctionContext, ConsiderationHandle)
				|| !CompiledData.Nodes.IsValidIndex(NodeIndex))
			{
				Candidate.Outcome = EArcTacticsCandidateOutcome::BindingFailed;
				Candidate.RejectionReason = TEXT("ConsiderationBindingFailed");
				return false;
			}
			const FArcTacticsConsiderationNode* Node =
				CompiledData.Nodes[NodeIndex].Node.GetPtr<FArcTacticsConsiderationNode>();
			FStructView ConsiderationView = Instance.NodeInstanceData[NodeIndex];
			FArcUtilityConsideration* Consideration =
				ConsiderationView.GetPtr<FArcUtilityConsideration>();
			if (!Node || !Consideration)
			{
				Candidate.Outcome = EArcTacticsCandidateOutcome::BindingFailed;
				Candidate.RejectionReason = TEXT("InvalidConsiderationData");
				return false;
			}
			const FArcUtilityConsiderationResult Result =
				Node->Score(ConsiderationView, FArcUtilityTarget(), UtilityContext);
			Progress = ArcUtilityScoring::AccumulateConsideration(
				*Consideration, Result, ConsiderationIndex, FString(),
				PairResult, PairTraceSink);
			if (Result.IsAccepted() && Progress != EArcUtilityPairProgress::Invalid
				&& !ApplyScratchNodeBindings(
					Instance, CompiledData, FunctionContext, ConsiderationHandle, true))
			{
				Candidate.Outcome = EArcTacticsCandidateOutcome::BindingFailed;
				Candidate.RejectionReason = TEXT("ConsiderationOutputBindingFailed");
				return false;
			}
			if (Progress != EArcUtilityPairProgress::Continue)
			{
#if WITH_ARCTACTICS_TRACE
				TraceBreakIndex = ConsiderationIndex;
#endif
				break;
			}
		}
		if (Progress == EArcUtilityPairProgress::Continue)
		{
			Progress = ArcUtilityScoring::FinishPair(
				Weight, Considerations.Num(), Instance.DecisionSettings.MinScore, PairResult);
		}
		Candidate.Option.Score = PairResult.Score;
		Candidate.RejectionReason = PairResult.VetoReason;
		if (Progress == EArcUtilityPairProgress::Invalid)
		{
			Candidate.Outcome = EArcTacticsCandidateOutcome::BindingFailed;
			return false;
		}
		if (PairResult.Outcome != EArcUtilityPairOutcome::Accepted)
		{
			Candidate.Outcome = PairResult.Outcome == EArcUtilityPairOutcome::Vetoed
				? EArcTacticsCandidateOutcome::Vetoed
				: EArcTacticsCandidateOutcome::BelowMinScore;
			return false;
		}

		if (!Instance.ResetNodeData(CompiledData, ActionHandle)
			|| !ApplyScratchNodeBindings(Instance, CompiledData, FunctionContext, ActionHandle))
		{
			Candidate.Outcome = EArcTacticsCandidateOutcome::BindingFailed;
			Candidate.RejectionReason = TEXT("ActionBindingFailed");
			return false;
		}
		TArray<FConstStructView, TInlineAllocator<8>> StepViews;
		for (const FArcTacticsNodeHandle StepHandle : Steps)
		{
			if (!Instance.ResetNodeData(CompiledData, StepHandle)
				|| !ApplyScratchNodeBindings(Instance, CompiledData, FunctionContext, StepHandle))
			{
				Candidate.Outcome = EArcTacticsCandidateOutcome::BindingFailed;
				Candidate.RejectionReason = TEXT("StepBindingFailed");
				return false;
			}
			StepViews.Add(Instance.NodeInstanceData[StepHandle.Get()]);
		}
		TArray<FConstStructView, TInlineAllocator<4>> ActiveStepViews;
		for (const FArcTacticsNodeHandle ActiveStepHandle : ActiveSteps)
		{
			if (!Instance.ResetNodeData(CompiledData, ActiveStepHandle)
				|| !ApplyScratchNodeBindings(Instance, CompiledData, FunctionContext, ActiveStepHandle))
			{
				Candidate.Outcome = EArcTacticsCandidateOutcome::BindingFailed;
				Candidate.RejectionReason = TEXT("ActiveStepBindingFailed");
				return false;
			}
			ActiveStepViews.Add(Instance.NodeInstanceData[ActiveStepHandle.Get()]);
		}
		const FPropertyBindingDataView ActionView = Instance.GetDataView(
			CompiledData.Nodes[ActionHandle.Get()].InstanceData);
		Candidate.Option.ActionData.InitializeAs(
			CastChecked<const UScriptStruct>(ActionView.GetStruct()),
			static_cast<const uint8*>(ActionView.GetMemory()));
		Candidate.Option.StepData = TConstArrayView<FConstStructView>(StepViews);
		Candidate.Option.ActiveStepData = TConstArrayView<FConstStructView>(ActiveStepViews);
		CaptureScope(Instance, BranchIndex, Candidate.Option);
		Candidate.Outcome = EArcTacticsCandidateOutcome::Accepted;
		return true;
	}

	void AddRejectedSelectorCandidate(
		FArcTacticsInstanceSlot& Slot,
		const int32 PackageIndex,
		const FArcTacticsPackage& Package,
		const FArcTacticsTaskContext& TaskContext,
		const FArcTacticsCompiledSelector& Selector,
		const int32 SelectorIndex,
		const int32 ItemIndex,
		const FName Reason)
	{
		FArcTacticsCandidate& Candidate = Slot.Candidates.AddDefaulted_GetRef();
		Candidate.Order = Slot.Candidates.Num() - 1;
		Candidate.Option.Action = Selector.Action;
		Candidate.Option.Name = Selector.Name;
		Candidate.Option.SourceIndex = SelectorIndex;
		Candidate.Option.bFromSelector = true;
		Candidate.Option.ItemIndex = ItemIndex;
		Candidate.Option.PackageIndex = PackageIndex;
		Candidate.Option.PackageSerial = Package.Serial;
		Candidate.Outcome = EArcTacticsCandidateOutcome::BindingFailed;
		Candidate.RejectionReason = Reason;
#if WITH_ARCTACTICS_TRACE
		if (TRACE_ARCTACTICS_ENABLED())
		{
			ArcTacticsTrace::OutputCandidate(
				TaskContext.World, TaskContext.EntityManager, Slot, *Package.Definitions, Candidate, Selector.Considerations,
				TConstArrayView<FArcUtilityConsiderationSnapshot>(), INDEX_NONE, INDEX_NONE, TOptional<int32>());
		}
		if (TaskContext.World && ArcTacticsDebug::IsCapturing(*TaskContext.World, Slot.Context.Self))
		{
			ArcTacticsDebug::CaptureCandidate(
				*TaskContext.World, Slot, *Package.Definitions, Candidate, Selector.Considerations,
				TConstArrayView<FArcUtilityConsiderationSnapshot>(), INDEX_NONE, INDEX_NONE, TOptional<int32>());
		}
#else
		(void)TaskContext;
#endif
	}

	void BuildCandidates(
		FArcTacticsInstanceSlot& Slot,
		const int32 PackageIndex,
		FArcTacticsPackage& Package,
		FArcTacticsTaskContext& Context)
	{
		FArcTacticsInstanceData& Instance = Package.Instance;
		const FArcTacticsCompiledData& CompiledData = *Package.Definitions;
		const FArcTQSFunctionContext FunctionContext = MakeFunctionContext(Context, Slot.Context.Self);
		for (int32 ActionIndex = 0; ActionIndex < CompiledData.Actions.Num(); ++ActionIndex)
		{
			const FArcTacticsCompiledAction& Action = CompiledData.Actions[ActionIndex];
			FArcTacticsCandidateIdentity Identity;
			Identity.Type = EArcTacticsValueIdentity::None;
			AddCandidate(
				Slot, PackageIndex, Package, Context, Action.Action, Action.Steps, Action.ActiveSteps, Action.Considerations,
				Action.Parameters, Action.ParameterBag,
				Action.DefaultParameters, Action.DefaultParameterBag,
				Action.ParameterBagInitializationBindingsBatch,
				Action.ParametersInitializationBindingsBatch,
				Action.Name, ActionIndex, false, Identity, INDEX_NONE);
		}

		for (int32 SelectorIndex = 0; SelectorIndex < CompiledData.Selectors.Num(); ++SelectorIndex)
		{
			const FArcTacticsCompiledSelector& Selector = CompiledData.Selectors[SelectorIndex];
			const FArcTacticsSelectorNode* SelectorNode = GetSelectorNode(CompiledData, Selector);
			const int32 SelectorNodeIndex = Selector.Selector.Get();
			if (!SelectorNode
				|| !Instance.ResetNodeData(CompiledData, Selector.Selector)
				|| !ApplyScratchNodeBindings(Instance, CompiledData, FunctionContext, Selector.Selector))
			{
				AddRejectedSelectorCandidate(
					Slot, PackageIndex, Package, Context, Selector, SelectorIndex, INDEX_NONE, TEXT("SelectorBindingFailed"));
				continue;
			}
			const int32 ItemCount = SelectorNode->GetItemCount(Instance.NodeInstanceData[SelectorNodeIndex]);
			for (int32 ItemIndex = 0; ItemIndex < ItemCount; ++ItemIndex)
			{
				FArcTacticsCandidateIdentity Identity;
				if (!SelectorNode->SelectItem(Instance.NodeInstanceData[SelectorNodeIndex], ItemIndex, Identity))
				{
					AddRejectedSelectorCandidate(
						Slot, PackageIndex, Package, Context, Selector, SelectorIndex, ItemIndex, TEXT("SelectorItemFailed"));
					continue;
				}
				AddCandidate(
					Slot, PackageIndex, Package, Context, Selector.Action, Selector.Steps, Selector.ActiveSteps, Selector.Considerations,
					Selector.Parameters, Selector.ParameterBag,
					Selector.DefaultParameters, Selector.DefaultParameterBag,
					Selector.ParameterBagInitializationBindingsBatch,
					Selector.ParametersInitializationBindingsBatch,
					Selector.Name, SelectorIndex, true, Identity, ItemIndex);
			}
		}
	}

	int32 SelectByMode(FArcTacticsInstanceSlot& Slot)
	{
		TArray<int32> Eligible;
		int32 TopPriority = MIN_int32;
		for (int32 CandidateIndex = 0; CandidateIndex < Slot.Candidates.Num(); ++CandidateIndex)
		{
			const FArcTacticsCandidate& Candidate = Slot.Candidates[CandidateIndex];
			if (Candidate.Outcome != EArcTacticsCandidateOutcome::Accepted)
			{
				continue;
			}
			const int32 Priority = Candidate.Option.Parameters.Priority;
			if (Eligible.IsEmpty() || Priority > TopPriority)
			{
				Eligible.Reset();
				TopPriority = Priority;
			}
			if (Priority == TopPriority)
			{
				Eligible.Add(CandidateIndex);
			}
		}
		if (Eligible.IsEmpty())
		{
			return INDEX_NONE;
		}
		Eligible.Sort([&Slot](const int32 LeftIndex, const int32 RightIndex)
		{
			const FArcTacticsCandidate& Left = Slot.Candidates[LeftIndex];
			const FArcTacticsCandidate& Right = Slot.Candidates[RightIndex];
			return Left.Option.Score == Right.Option.Score
				? Left.Order < Right.Order
				: Left.Option.Score > Right.Option.Score;
		});

		int32 SelectedIndex = INDEX_NONE;
		if (Slot.Settings.Selection.Mode == EArcTacticsSelectionMode::HighestScore)
		{
			SelectedIndex = Eligible[0];
		}
		else if (Slot.Settings.Selection.Mode == EArcTacticsSelectionMode::RandomFromTopPercent)
		{
			const float TopPercent = Slot.Settings.Selection.TopPercent;
			const int32 PoolSize = FMath::Clamp(
				FMath::CeilToInt(static_cast<float>(Eligible.Num()) * TopPercent), 1, Eligible.Num());
			SelectedIndex = Eligible[Slot.RandomStream.RandRange(0, PoolSize - 1)];
		}
		else
		{
			double TotalScore = 0.0;
			for (const int32 CandidateIndex : Eligible)
			{
				TotalScore += FMath::Max(
					0.0, static_cast<double>(Slot.Candidates[CandidateIndex].Option.Score));
			}
			if (TotalScore > 0.0)
			{
				double Draw = static_cast<double>(Slot.RandomStream.FRand()) * TotalScore;
				for (const int32 CandidateIndex : Eligible)
				{
					Draw -= FMath::Max(
						0.0, static_cast<double>(Slot.Candidates[CandidateIndex].Option.Score));
					if (Draw <= 0.0)
					{
						SelectedIndex = CandidateIndex;
						break;
					}
				}
				if (SelectedIndex == INDEX_NONE)
				{
					SelectedIndex = Eligible.Last();
				}
			}
		}
		return SelectedIndex;
	}
}

void ArcTacticsDecision::ComputeSelectionChances(
	const TConstArrayView<float> Scores,
	const TConstArrayView<int32> Priorities,
	const FArcTacticsSelectionSettings& Settings,
	TArray<float>& OutChances)
{
	OutChances.Reset();
	OutChances.SetNumZeroed(Scores.Num());
	if (Scores.IsEmpty() || Priorities.Num() != Scores.Num())
	{
		return;
	}
	int32 TopPriority = Priorities[0];
	for (const int32 Priority : Priorities)
	{
		TopPriority = FMath::Max(TopPriority, Priority);
	}
	TArray<int32> Eligible;
	Eligible.Reserve(Scores.Num());
	for (int32 ScoreIndex = 0; ScoreIndex < Scores.Num(); ++ScoreIndex)
	{
		if (Priorities[ScoreIndex] == TopPriority)
		{
			Eligible.Add(ScoreIndex);
		}
	}
	Eligible.Sort([&Scores](const int32 LeftIndex, const int32 RightIndex)
	{
		return Scores[LeftIndex] == Scores[RightIndex]
			? LeftIndex < RightIndex
			: Scores[LeftIndex] > Scores[RightIndex];
	});

	if (Settings.Mode == EArcTacticsSelectionMode::HighestScore)
	{
		OutChances[Eligible[0]] = 1.0f;
	}
	else if (Settings.Mode == EArcTacticsSelectionMode::RandomFromTopPercent)
	{
		const int32 PoolSize = FMath::Clamp(
			FMath::CeilToInt(static_cast<float>(Eligible.Num()) * Settings.TopPercent), 1, Eligible.Num());
		for (int32 PoolIndex = 0; PoolIndex < PoolSize; ++PoolIndex)
		{
			OutChances[Eligible[PoolIndex]] = 1.0f / static_cast<float>(PoolSize);
		}
	}
	else
	{
		double TotalScore = 0.0;
		for (const int32 ScoreIndex : Eligible)
		{
			TotalScore += FMath::Max(0.0, static_cast<double>(Scores[ScoreIndex]));
		}
		if (TotalScore > 0.0)
		{
			for (const int32 ScoreIndex : Eligible)
			{
				OutChances[ScoreIndex] = static_cast<float>(
					FMath::Max(0.0, static_cast<double>(Scores[ScoreIndex])) / TotalScore);
			}
		}
	}
}


namespace ArcTacticsDecisionPrivate
{
	bool SelectCapturedItem(const FArcTacticsCompiledData& CompiledData, FArcTacticsOption& Option)
	{
		if (!CompiledData.Selectors.IsValidIndex(Option.SourceIndex))
		{
			return false;
		}
		const FArcTacticsCompiledSelector& Selector = CompiledData.Selectors[Option.SourceIndex];
		const FArcTacticsSelectorNode* SelectorNode = GetSelectorNode(CompiledData, Selector);
		const int32 SelectorNodeIndex = Selector.Selector.Get();
		FArcTacticsCandidateIdentity Identity;
		return SelectorNode
			&& Option.CapturedNodeData.IsValidIndex(SelectorNodeIndex)
			&& SelectorNode->SelectItem(Option.CapturedNodeData[SelectorNodeIndex], Option.ItemIndex, Identity)
			&& Identity == Option.Identity;
	}

	struct FSelectionReport
	{
		int32 SelectedOrder = INDEX_NONE;
	};

	bool IsIncluded(const FArcTacticsPackage& Package)
	{
		return Package.bActive && !Package.bExcluded && Package.Definitions;
	}

	void ExcludePackage(FArcTacticsPackage& Package, const FName Diagnostic)
	{
		Package.Instance.bGlobalTaskRunning = false;
		Package.Instance.Diagnostic = Diagnostic;
		Package.bExcluded = true;
	}

	void Evaluate(
		FArcTacticsInstanceSlot& Slot,
		FArcTacticsTaskContext& Context,
		FSelectionReport& OutReport)
	{
		bool bAnyIncluded = false;
		for (int32 PackageIndex = 0; PackageIndex < Slot.Packages.Num(); ++PackageIndex)
		{
			FArcTacticsPackage& Package = *Slot.Packages[PackageIndex];
			if (!IsIncluded(Package))
			{
				continue;
			}
			FArcTacticsInstanceData& Instance = Package.Instance;
			const FArcTacticsCompiledData& CompiledData = *Package.Definitions;
			Instance.DecisionSettings = CompiledData.DefaultDecisionSettings;
			if (!Instance.ApplyBindings(
					CompiledData,
					CompiledData.DecisionSettingsBindingsBatch,
					Instance.GetMutableDataView(
						FArcTacticsDataHandle(EArcTacticsBindingEndpointKind::DecisionSettings)),
					MakeFunctionContext(Context, Slot.Context.Self)))
			{
				ExcludePackage(Package, TEXT("DecisionSettingsBindingFailed"));
				continue;
			}
			if (!Instance.DecisionSettings.IsValid())
			{
				ExcludePackage(Package, TEXT("InvalidDecisionSettings"));
				continue;
			}
			bAnyIncluded = true;
			BuildCandidates(Slot, PackageIndex, Package, Context);
		}
		Slot.SelectedAction.Reset();
		if (!bAnyIncluded)
		{
			Slot.Candidates.Reset();
			Slot.Diagnostic = FirstExclusionDiagnostic(Slot);
			Slot.DecisionStatus = EArcTacticsDecisionStatus::Failed;
			return;
		}
		const int32 SelectedIndex = SelectByMode(Slot);
		if (Slot.Candidates.IsValidIndex(SelectedIndex))
		{
			const FArcTacticsCandidate& Selected = Slot.Candidates[SelectedIndex];
			const FArcTacticsPackage& Package = *Slot.Packages[Selected.Option.PackageIndex];
			Slot.SelectedAction = Selected.Option;
			Slot.SelectedAction.GlobalParameters = Package.Instance.GlobalParameters;
			Slot.SelectedAction.CapturedNodeData = Package.Instance.NodeInstanceData;
			Slot.SelectedAction.Serial = 0;
			if (Slot.SelectedAction.bFromSelector
				&& !SelectCapturedItem(*Package.Definitions, Slot.SelectedAction))
			{
				Slot.SelectedAction.Reset();
				Slot.Diagnostic = TEXT("SelectorItemCaptureFailed");
			}
			if (Slot.SelectedAction.IsSet())
			{
				OutReport.SelectedOrder = Selected.Order;
			}
		}
		Slot.DecisionStatus = EArcTacticsDecisionStatus::Succeeded;
	}

	bool RunPackageTasks(FArcTacticsPackage& Package, FArcTacticsTaskContext& Context)
	{
		FArcTacticsInstanceData& Instance = Package.Instance;
		const TSharedPtr<const FArcTacticsCompiledData> Definitions = Package.Definitions;
		const FArcTacticsCompiledData& CompiledData = *Definitions;
		if (!Instance.IsCompatible(CompiledData))
		{
			ExcludePackage(Package, TEXT("IncompatibleCompiledData"));
			return false;
		}
		const FArcTQSFunctionContext FunctionContext = MakeFunctionContext(Context, Context.Entity);
		while (CompiledData.GlobalTasks.IsValidIndex(Instance.GlobalTaskIndex))
		{
			const FArcTacticsNodeHandle TaskHandle = CompiledData.GlobalTasks[Instance.GlobalTaskIndex];
			const FArcTacticsNode* Node = GetNode(CompiledData, TaskHandle);
			const FArcTacticsGlobalTask* Task = Node
				? CompiledData.Nodes[TaskHandle.Get()].Node.GetPtr<FArcTacticsGlobalTask>()
				: nullptr;
			const FArcTacticsDataHandle InstanceDataHandle = CompiledData.Nodes.IsValidIndex(TaskHandle.Get())
				? CompiledData.Nodes[TaskHandle.Get()].InstanceData
				: FArcTacticsDataHandle();
			FStructView InstanceData = InstanceDataHandle.GetKind() == EArcTacticsBindingEndpointKind::Node
				&& Instance.NodeInstanceData.IsValidIndex(InstanceDataHandle.GetIndex())
				? Instance.NodeInstanceData[InstanceDataHandle.GetIndex()]
				: FStructView();
			if (!Task || !InstanceData.IsValid())
			{
				ExcludePackage(Package, TEXT("InvalidGlobalTaskData"));
				return false;
			}

			const bool bWasRunning = Instance.bGlobalTaskRunning;
			if ((!bWasRunning && !Instance.ResetNodeData(CompiledData, TaskHandle))
				|| !ApplyScratchNodeBindings(Instance, CompiledData, FunctionContext, TaskHandle))
			{
				if (bWasRunning)
				{
					Task->Cancel(Context, InstanceData);
				}
				ExcludePackage(Package, TEXT("GlobalTaskBindingFailed"));
				return false;
			}
			InstanceData = Instance.NodeInstanceData[InstanceDataHandle.GetIndex()];
			EArcTacticsTaskStatus TaskStatus = EArcTacticsTaskStatus::Failed;
			Context.FailureReason = NAME_None;
			if (bWasRunning)
			{
				TaskStatus = Task->Tick(Context, InstanceData);
			}
			else
			{
				TaskStatus = Task->Enter(Context, InstanceData);
			}

			if (TaskStatus != EArcTacticsTaskStatus::Failed
				&& !ApplyScratchNodeBindings(Instance, CompiledData, FunctionContext, TaskHandle, true))
			{
				Task->Cancel(Context, InstanceData);
				ExcludePackage(Package, TEXT("GlobalTaskOutputBindingFailed"));
				return false;
			}
			if (TaskStatus == EArcTacticsTaskStatus::Running)
			{
				Instance.bGlobalTaskRunning = true;
				return true;
			}
			Instance.bGlobalTaskRunning = false;
			if (TaskStatus == EArcTacticsTaskStatus::Failed)
			{
				ExcludePackage(Package, Context.FailureReason.IsNone()
					? FName(TEXT("GlobalTaskFailed"))
					: Context.FailureReason);
				return false;
			}
			++Instance.GlobalTaskIndex;
		}
		return false;
	}

	EArcTacticsDecisionStatus RunTasksAndEvaluate(
		FArcTacticsInstanceSlot& Slot,
		FArcTacticsTaskContext& Context,
		FSelectionReport& OutReport)
	{
		Context.Entity = Slot.Context.Self;
		Context.DecisionGeneration = Slot.DecisionGeneration;
		while (Slot.Packages.IsValidIndex(Slot.PackageCursor))
		{
			FArcTacticsPackage& Package = *Slot.Packages[Slot.PackageCursor];
			if (IsIncluded(Package) && RunPackageTasks(Package, Context))
			{
				return EArcTacticsDecisionStatus::Running;
			}
			++Slot.PackageCursor;
		}

		Evaluate(Slot, Context, OutReport);
		return Slot.DecisionStatus;
	}

	void CaptureDecision(
		const FArcTacticsTaskContext& Context,
		const FArcTacticsInstanceSlot& Slot,
		const int32 SelectedOrder)
	{
#if WITH_ARCTACTICS_TRACE
		if (Context.World && ArcTacticsDebug::IsCapturing(*Context.World, Slot.Context.Self))
		{
			ArcTacticsDebug::CaptureDecision(*Context.World, Slot, SelectedOrder);
		}
#else
		(void)Context;
		(void)Slot;
		(void)SelectedOrder;
#endif
	}

	EArcTacticsDecisionStatus RunTasks(
		FArcTacticsInstanceSlot& Slot,
		FArcTacticsTaskContext& Context)
	{
		FSelectionReport Report;
		const EArcTacticsDecisionStatus Status = RunTasksAndEvaluate(Slot, Context, Report);
		if (Status != EArcTacticsDecisionStatus::Running)
		{
			TRACE_ARCTACTICS_DECISION(Context, Slot, Report.SelectedOrder);
			CaptureDecision(Context, Slot, Report.SelectedOrder);
		}
		return Status;
	}

	bool ApplyActiveActionBindings(
		FArcTacticsInstanceSlot& Slot,
		FArcTacticsPackage& Package,
		const FArcTQSFunctionContext& FunctionContext,
		const bool bOutputs)
	{
		FArcTacticsInstanceData& Instance = Package.Instance;
		if (!Slot.ActiveAction.IsSet())
		{
			Instance.Diagnostic = TEXT("MissingActiveAction");
			return false;
		}
		const int32 NodeIndex = Slot.ActiveAction.Action.Get();
		if (!Package.Definitions
			|| !Package.Definitions->Nodes.IsValidIndex(NodeIndex)
			|| !Package.Definitions->DefaultInstanceData.IsValidIndex(NodeIndex)
			|| Slot.ActiveAction.ActionData.GetScriptStruct()
				!= Package.Definitions->DefaultInstanceData[NodeIndex].GetScriptStruct())
		{
			Instance.Diagnostic = TEXT("InvalidActiveAction");
			return false;
		}
		const FPropertyBindingDataView TargetView(
			Slot.ActiveAction.ActionData.GetScriptStruct(),
			Slot.ActiveAction.ActionData.GetMutableMemory());
		if (!ApplyNodeBindings(
			Instance, *Package.Definitions, FunctionContext, Slot.ActiveAction.Action,
			TargetView, &Slot.ActiveAction, bOutputs))
		{
			Instance.Diagnostic = bOutputs ? TEXT("ActiveActionOutputBindingFailed") : TEXT("ActiveActionBindingFailed");
			return false;
		}
		return true;
	}
}

EArcTacticsDecisionStatus ArcTacticsDecision::BeginDecision(
	FArcTacticsInstanceSlot& Slot,
	FArcTacticsTaskContext& Context)
{
	if (Slot.DecisionStatus == EArcTacticsDecisionStatus::Running)
	{
		return Slot.DecisionStatus;
	}
	if (!Slot.HasActivePackage())
	{
		Slot.Candidates.Reset();
		Slot.SelectedAction.Reset();
		Slot.DecisionStatus = EArcTacticsDecisionStatus::Idle;
		return Slot.DecisionStatus;
	}
	if (!Slot.ResetForDecision(ArcTacticsDecisionPrivate::MakeFunctionContext(Context, Slot.Context.Self)))
	{
		TRACE_ARCTACTICS_DECISION(Context, Slot, INDEX_NONE);
		ArcTacticsDecisionPrivate::CaptureDecision(Context, Slot, INDEX_NONE);
		return Slot.DecisionStatus;
	}
	return ArcTacticsDecisionPrivate::RunTasks(Slot, Context);
}

EArcTacticsDecisionStatus ArcTacticsDecision::TickDecision(
	FArcTacticsInstanceSlot& Slot,
	FArcTacticsTaskContext& Context)
{
	if (Slot.DecisionStatus != EArcTacticsDecisionStatus::Running)
	{
		return Slot.DecisionStatus;
	}
	return ArcTacticsDecisionPrivate::RunTasks(Slot, Context);
}

void ArcTacticsDecision::CancelDecision(
	FArcTacticsInstanceSlot& Slot,
	FArcTacticsTaskContext& Context)
{
	const bool bWasRunning = Slot.DecisionStatus == EArcTacticsDecisionStatus::Running;
	FArcTacticsPackage* Package = bWasRunning && Slot.Packages.IsValidIndex(Slot.PackageCursor)
		? Slot.Packages[Slot.PackageCursor].Get()
		: nullptr;
	const TSharedPtr<const FArcTacticsCompiledData> Definitions = Package ? Package->Definitions : nullptr;
	if (Package && Definitions
		&& Package->Instance.bGlobalTaskRunning
		&& Definitions->GlobalTasks.IsValidIndex(Package->Instance.GlobalTaskIndex))
	{
		const FArcTacticsCompiledData& CompiledData = *Definitions;
		FArcTacticsInstanceData& Instance = Package->Instance;
		const FArcTacticsNodeHandle TaskHandle = CompiledData.GlobalTasks[Instance.GlobalTaskIndex];
		const FArcTacticsGlobalTask* Task = CompiledData.Nodes.IsValidIndex(TaskHandle.Get())
			? CompiledData.Nodes[TaskHandle.Get()].Node.GetPtr<FArcTacticsGlobalTask>()
			: nullptr;
		const FArcTacticsDataHandle InstanceDataHandle = CompiledData.Nodes.IsValidIndex(TaskHandle.Get())
			? CompiledData.Nodes[TaskHandle.Get()].InstanceData
			: FArcTacticsDataHandle();
		FStructView InstanceData = InstanceDataHandle.GetKind() == EArcTacticsBindingEndpointKind::Node
			&& Instance.NodeInstanceData.IsValidIndex(InstanceDataHandle.GetIndex())
			? Instance.NodeInstanceData[InstanceDataHandle.GetIndex()]
			: FStructView();
		if (Task && InstanceData.IsValid())
		{
			Context.Entity = Slot.Context.Self;
			Context.DecisionGeneration = Slot.DecisionGeneration;
			Task->Cancel(Context, InstanceData);
		}
	}
	for (const TUniquePtr<FArcTacticsPackage>& Each : Slot.Packages)
	{
		Each->Instance.bGlobalTaskRunning = false;
	}
	Slot.Candidates.Reset();
	Slot.SelectedAction.Reset();
	Slot.DecisionStatus = EArcTacticsDecisionStatus::Cancelled;
	if (bWasRunning)
	{
		TRACE_ARCTACTICS_DECISION(Context, Slot, INDEX_NONE);
		ArcTacticsDecisionPrivate::CaptureDecision(Context, Slot, INDEX_NONE);
	}
}

bool ArcTacticsDecision::ApplyActiveActionInputs(
	FArcTacticsInstanceSlot& Slot,
	FArcTacticsPackage& Package,
	const FArcTQSFunctionContext& FunctionContext)
{
	return ArcTacticsDecisionPrivate::ApplyActiveActionBindings(Slot, Package, FunctionContext, false);
}

bool ArcTacticsDecision::PublishActiveActionOutputs(
	FArcTacticsInstanceSlot& Slot,
	FArcTacticsPackage& Package,
	const FArcTQSFunctionContext& FunctionContext)
{
	return ArcTacticsDecisionPrivate::ApplyActiveActionBindings(Slot, Package, FunctionContext, true);
}


const FArcActiveAbility* ArcTacticsDecision::ResolveAbility(
	FMassEntityManager& EntityManager,
	const FMassEntityHandle Entity,
	const UArcAbilityDefinition* Definition,
	const bool bRequireInactive,
	FName& OutRejectionReason)
{
	const FMassEntityView EntityView(EntityManager, Entity);
	const FArcAbilityCollectionFragment* Collection = EntityView.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>();
	if (!Definition || !Collection)
	{
		OutRejectionReason = TEXT("AbilityGrantMissing");
		return nullptr;
	}
	const FArcActiveAbility* Ability = nullptr;
	for (const FArcActiveAbility& Candidate : Collection->Abilities)
	{
		if (Candidate.Definition.Get() != Definition)
		{
			continue;
		}
		if (Ability)
		{
			OutRejectionReason = TEXT("AmbiguousAbilityOwnership");
			return nullptr;
		}
		Ability = &Candidate;
	}
	if (!Ability || !Ability->Handle.IsValid())
	{
		OutRejectionReason = TEXT("AbilityGrantMissing");
		return nullptr;
	}
	if (!Ability->InputTag.IsValid())
	{
		OutRejectionReason = TEXT("InvalidInputTag");
		return nullptr;
	}
	int32 InputTagMatches = 0;
	for (const FArcActiveAbility& Candidate : Collection->Abilities)
	{
		InputTagMatches += Candidate.InputTag == Ability->InputTag ? 1 : 0;
	}
	if (InputTagMatches != 1)
	{
		OutRejectionReason = TEXT("AmbiguousAbilityOwnership");
		return nullptr;
	}
	if (bRequireInactive && Ability->bIsActive)
	{
		OutRejectionReason = TEXT("AbilityAlreadyActive");
		return nullptr;
	}
	return Ability;
}
