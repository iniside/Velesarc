// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulator/ArcTacticsSimulator.h"

#include "Considerations/ArcTacticsConsiderations.h"
#include "Decision/ArcTacticsConfig.h"
#include "Decision/ArcTacticsDecision.h"
#include "Math/RandomStream.h"
#include "PropertyBindingBinding.h"
#include "PropertyBindingPath.h"
#include "UtilityAI/ArcUtilityConsideration.h"
#include "UtilityAI/ArcUtilityScoring.h"

namespace ArcTacticsSimulatorPrivate
{
	const FName SimulatedVetoReason(TEXT("SimulatedVeto"));
	const FName InvalidConsiderationDataReason(TEXT("InvalidConsiderationData"));
	const FName WeightSegmentName(TEXT("Weight"));
	const FName PrioritySegmentName(TEXT("Priority"));
	const FName ResponseCurveSegmentName(TEXT("ResponseCurve"));

	void GatherBoundIDs(
		const UArcTacticsConfig& Config,
		TSet<FGuid>& OutConsiderationIDs,
		TSet<FGuid>& OutWeightParametersIDs,
		TSet<FGuid>& OutPriorityParametersIDs)
	{
		Config.EditorBindings.ForEachBinding([&OutConsiderationIDs, &OutWeightParametersIDs, &OutPriorityParametersIDs](
			const FPropertyBindingBinding& Binding)
		{
			const FPropertyBindingPath& TargetPath = Binding.GetTargetPath();
			if (TargetPath.NumSegments() == 0)
			{
				return;
			}
			const FName FirstSegment = TargetPath.GetSegment(0).GetName();
			if (FirstSegment == WeightSegmentName)
			{
				OutWeightParametersIDs.Add(TargetPath.GetStructID());
			}
			if (FirstSegment == PrioritySegmentName)
			{
				OutPriorityParametersIDs.Add(TargetPath.GetStructID());
			}
			if (FirstSegment == ResponseCurveSegmentName)
			{
				OutConsiderationIDs.Add(TargetPath.GetStructID());
			}
		});
	}
}

void FArcTacticsSimulator::Rebuild(const UArcTacticsConfig* InConfig)
{
	Config = InConfig;
	TArray<FBranchView> Branches;
	GatherBranches(Branches);
	RebuildRows(Branches);
	Recompute();
}

void FArcTacticsSimulator::Recompute()
{
	const UArcTacticsConfig* ConfigPtr = Config.Get();
	TArray<FBranchView> Branches;
	GatherBranches(Branches);
	if (!MatchesStructure(Branches))
	{
		RebuildRows(Branches);
	}
	if (!ConfigPtr)
	{
		NothingPickedChance = 1.0f;
		return;
	}

	MinScore = ConfigPtr->DecisionSettings.MinScore;

	TSet<FGuid> BoundConsiderationIDs;
	TSet<FGuid> BoundWeightParametersIDs;
	TSet<FGuid> BoundPriorityParametersIDs;
	ArcTacticsSimulatorPrivate::GatherBoundIDs(
		*ConfigPtr, BoundConsiderationIDs, BoundWeightParametersIDs, BoundPriorityParametersIDs);

	TArray<FRowEvaluation> Evaluations;
	Evaluations.Reserve(Branches.Num());
	for (int32 RowIndex = 0; RowIndex < Branches.Num(); ++RowIndex)
	{
		FArcTacticsSimulatorRow& Row = Rows[RowIndex];
		Row.Name = ConfigPtr->GetAuthoringName(Branches[RowIndex].ActionID);
		Row.SelectorName = Row.bFromSelector ? ConfigPtr->GetAuthoringName(Row.NodeID) : FText::GetEmpty();
		Row.Priority = Branches[RowIndex].Priority;
		Row.bWeightBound = BoundWeightParametersIDs.Contains(Row.ParametersID);
		Row.bPriorityBound = BoundPriorityParametersIDs.Contains(Row.ParametersID);
		Row.bHasBoundDefaults = Row.bWeightBound || Row.bPriorityBound;
		for (FArcTacticsSimulatorConsideration& Consideration : Row.Considerations)
		{
			Consideration.Name = ConfigPtr->GetAuthoringName(Consideration.NodeID);
			Consideration.bBound = BoundConsiderationIDs.Contains(Consideration.NodeID);
			Row.bHasBoundDefaults |= Consideration.bBound;
		}
		const FRowEvaluation Evaluation = EvaluateBranch(Branches[RowIndex], States, &Row);
		Row.Outcome = Evaluation.Outcome;
		Row.Reason = Evaluation.Reason;
		Row.Score = Evaluation.Score;
		Evaluations.Add(Evaluation);
	}

	TArray<float> Chances;
	NothingPickedChance = ComputeChances(Evaluations, Chances);
	for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
	{
		Rows[RowIndex].Chance = Chances[RowIndex];
	}
}

void FArcTacticsSimulator::SetInput(const FGuid& ConsiderationID, const float Input)
{
	if (FArcTacticsSimulatorInputState* State = States.Find(ConsiderationID))
	{
		State->Input = FMath::Clamp(Input, 0.0f, 1.0f);
		Recompute();
	}
}

void FArcTacticsSimulator::SetVeto(const FGuid& ConsiderationID, const bool bVeto)
{
	if (FArcTacticsSimulatorInputState* State = States.Find(ConsiderationID))
	{
		State->bVeto = bVeto;
		Recompute();
	}
}

void FArcTacticsSimulator::SetSelectionMode(const EArcTacticsSelectionMode Mode)
{
	Selection.Mode = Mode;
	ClearMonteCarlo();
	Recompute();
}

void FArcTacticsSimulator::SetTopPercent(const float TopPercent)
{
	Selection.TopPercent = FMath::Clamp(TopPercent, 0.0f, 1.0f);
	ClearMonteCarlo();
	Recompute();
}

void FArcTacticsSimulator::SetLocked(const FGuid& ConsiderationID, const bool bLocked)
{
	if (FArcTacticsSimulatorInputState* State = States.Find(ConsiderationID))
	{
		State->bLocked = bLocked;
	}
}

const FArcTacticsSimulatorInputState* FArcTacticsSimulator::FindState(const FGuid& ConsiderationID) const
{
	return States.Find(ConsiderationID);
}

void FArcTacticsSimulator::ResetInputs()
{
	TArray<FBranchView> Branches;
	GatherBranches(Branches);
	for (const FBranchView& Branch : Branches)
	{
		for (const FArcTacticsNodeDefinition& Definition : *Branch.Considerations)
		{
			FArcTacticsSimulatorInputState& State = States.FindOrAdd(Definition.ID);
			State = FArcTacticsSimulatorInputState();
			State.Input = GetDefaultInput(Definition);
		}
	}
	Recompute();
}

void FArcTacticsSimulator::RunMonteCarlo()
{
	Recompute();
	if (!Config.Get())
	{
		MonteCarloNothingPickedChance = 1.0f;
		bHasMonteCarlo = true;
		return;
	}
	TArray<FBranchView> Branches;
	GatherBranches(Branches);

	TArray<double> ChanceSums;
	ChanceSums.SetNumZeroed(Branches.Num());
	TArray<int32> EligibleCounts;
	EligibleCounts.SetNumZeroed(Branches.Num());
	double NothingPickedSum = 0.0;

	FRandomStream RandomStream(MonteCarloSeed);
	TMap<FGuid, FArcTacticsSimulatorInputState> SampleStates = States;
	TArray<FRowEvaluation> Evaluations;
	TArray<float> Chances;
	for (int32 SampleIndex = 0; SampleIndex < MonteCarloSamples; ++SampleIndex)
	{
		for (const FArcTacticsSimulatorRow& Row : Rows)
		{
			for (const FArcTacticsSimulatorConsideration& Consideration : Row.Considerations)
			{
				FArcTacticsSimulatorInputState* State = SampleStates.Find(Consideration.NodeID);
				if (State && !State->bLocked)
				{
					State->Input = RandomStream.FRand();
				}
			}
		}
		Evaluations.Reset();
		for (const FBranchView& Branch : Branches)
		{
			Evaluations.Add(EvaluateBranch(Branch, SampleStates, nullptr));
		}
		NothingPickedSum += ComputeChances(Evaluations, Chances);
		for (int32 RowIndex = 0; RowIndex < Branches.Num(); ++RowIndex)
		{
			ChanceSums[RowIndex] += Chances[RowIndex];
			EligibleCounts[RowIndex] += Evaluations[RowIndex].Outcome == EArcTacticsSimulatorOutcome::Accepted ? 1 : 0;
		}
	}

	for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
	{
		Rows[RowIndex].MonteCarloMeanChance = static_cast<float>(ChanceSums[RowIndex] / MonteCarloSamples);
		Rows[RowIndex].MonteCarloEligibleShare =
			static_cast<float>(EligibleCounts[RowIndex]) / static_cast<float>(MonteCarloSamples);
	}
	MonteCarloNothingPickedChance = static_cast<float>(NothingPickedSum / MonteCarloSamples);
	bHasMonteCarlo = true;
}

void FArcTacticsSimulator::ClearMonteCarlo()
{
	for (FArcTacticsSimulatorRow& Row : Rows)
	{
		Row.MonteCarloMeanChance = 0.0f;
		Row.MonteCarloEligibleShare = 0.0f;
	}
	MonteCarloNothingPickedChance = 0.0f;
	bHasMonteCarlo = false;
}

const FArcTacticsSimulatorRow* FArcTacticsSimulator::FindRow(const FGuid& RowID) const
{
	return Rows.FindByPredicate([&RowID](const FArcTacticsSimulatorRow& Row)
	{
		return Row.NodeID == RowID;
	});
}

void FArcTacticsSimulator::GatherBranches(TArray<FBranchView>& OutBranches) const
{
	OutBranches.Reset();
	const UArcTacticsConfig* ConfigPtr = Config.Get();
	if (!ConfigPtr)
	{
		return;
	}
	for (int32 ActionIndex = 0; ActionIndex < ConfigPtr->Actions.Num(); ++ActionIndex)
	{
		const FArcTacticsActionDefinition& Action = ConfigPtr->Actions[ActionIndex];
		FBranchView& Branch = OutBranches.AddDefaulted_GetRef();
		Branch.NodeID = Action.Action.ID;
		Branch.ActionID = Action.Action.ID;
		Branch.ParametersID = Action.ParametersID;
		Branch.Considerations = &Action.Considerations;
		Branch.Weight = Action.Parameters.Weight;
		Branch.Priority = Action.Parameters.Priority;
		Branch.SourceIndex = ActionIndex;
	}
	for (int32 SelectorIndex = 0; SelectorIndex < ConfigPtr->Selectors.Num(); ++SelectorIndex)
	{
		const FArcTacticsSelectorDefinition& Selector = ConfigPtr->Selectors[SelectorIndex];
		FBranchView& Branch = OutBranches.AddDefaulted_GetRef();
		Branch.NodeID = Selector.Selector.ID;
		Branch.ActionID = Selector.Action.ID;
		Branch.ParametersID = Selector.ParametersID;
		Branch.Considerations = &Selector.Considerations;
		Branch.Weight = Selector.Parameters.Weight;
		Branch.Priority = Selector.Parameters.Priority;
		Branch.bFromSelector = true;
		Branch.SourceIndex = SelectorIndex;
	}
}

bool FArcTacticsSimulator::MatchesStructure(const TConstArrayView<FBranchView> Branches) const
{
	if (Branches.Num() != Rows.Num())
	{
		return false;
	}
	for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
	{
		const FArcTacticsSimulatorRow& Row = Rows[RowIndex];
		const FBranchView& Branch = Branches[RowIndex];
		if (Row.NodeID != Branch.NodeID
			|| Row.ParametersID != Branch.ParametersID
			|| Row.Considerations.Num() != Branch.Considerations->Num())
		{
			return false;
		}
		for (int32 ConsiderationIndex = 0; ConsiderationIndex < Row.Considerations.Num(); ++ConsiderationIndex)
		{
			if (Row.Considerations[ConsiderationIndex].NodeID != (*Branch.Considerations)[ConsiderationIndex].ID)
			{
				return false;
			}
		}
	}
	return true;
}

void FArcTacticsSimulator::RebuildRows(const TConstArrayView<FBranchView> Branches)
{
	Rows.Reset();
	ClearMonteCarlo();
	const UArcTacticsConfig* ConfigPtr = Config.Get();
	TMap<FGuid, FArcTacticsSimulatorInputState> SurvivingStates;
	for (const FBranchView& Branch : Branches)
	{
		FArcTacticsSimulatorRow& Row = Rows.AddDefaulted_GetRef();
		Row.NodeID = Branch.NodeID;
		Row.ParametersID = Branch.ParametersID;
		Row.Name = ConfigPtr->GetAuthoringName(Branch.ActionID);
		Row.SelectorName = Branch.bFromSelector ? ConfigPtr->GetAuthoringName(Branch.NodeID) : FText::GetEmpty();
		Row.bFromSelector = Branch.bFromSelector;
		Row.SourceIndex = Branch.SourceIndex;
		for (const FArcTacticsNodeDefinition& Definition : *Branch.Considerations)
		{
			FArcTacticsSimulatorConsideration& Consideration = Row.Considerations.AddDefaulted_GetRef();
			Consideration.NodeID = Definition.ID;
			Consideration.Name = ConfigPtr->GetAuthoringName(Definition.ID);
			if (const FArcTacticsSimulatorInputState* Existing = States.Find(Definition.ID))
			{
				SurvivingStates.Add(Definition.ID, *Existing);
			}
			else
			{
				FArcTacticsSimulatorInputState State;
				State.Input = GetDefaultInput(Definition);
				SurvivingStates.Add(Definition.ID, State);
			}
		}
	}
	States = MoveTemp(SurvivingStates);
}

float FArcTacticsSimulator::GetDefaultInput(const FArcTacticsNodeDefinition& Definition) const
{
	const FArcTacticsConstantConsideration* Constant = Definition.InstanceData.GetPtr<FArcTacticsConstantConsideration>();
	return Constant ? FMath::Clamp(Constant->Value, 0.0f, 1.0f) : 0.5f;
}

FArcTacticsSimulator::FRowEvaluation FArcTacticsSimulator::EvaluateBranch(
	const FBranchView& Branch,
	const TMap<FGuid, FArcTacticsSimulatorInputState>& InputStates,
	FArcTacticsSimulatorRow* OutTracedRow) const
{
	FRowEvaluation Evaluation;
	Evaluation.Priority = Branch.Priority;
	const TArray<FArcTacticsNodeDefinition>& Definitions = *Branch.Considerations;
	TArray<FArcUtilityConsiderationSnapshot> Snapshots;
	if (OutTracedRow)
	{
		for (FArcTacticsSimulatorConsideration& Consideration : OutTracedRow->Considerations)
		{
			Consideration.bEvaluated = false;
			Consideration.CurvedScore = 0.0f;
		}
	}

	for (const FArcTacticsNodeDefinition& Definition : Definitions)
	{
		if (!Definition.InstanceData.GetPtr<FArcUtilityConsideration>())
		{
			Evaluation.Reason = ArcTacticsSimulatorPrivate::InvalidConsiderationDataReason;
			return Evaluation;
		}
	}

	FArcUtilityPairResult PairResult;
	EArcUtilityPairProgress Progress = ArcUtilityScoring::BeginPair(
		Branch.Weight, Definitions.Num(), MinScore, PairResult);
	if (Progress == EArcUtilityPairProgress::Invalid)
	{
		Evaluation.Reason = PairResult.VetoReason;
		return Evaluation;
	}

	FArcUtilityPairTrace Trace;
	Trace.Snapshots = &Snapshots;
	for (int32 ConsiderationIndex = 0; ConsiderationIndex < Definitions.Num(); ++ConsiderationIndex)
	{
		const FArcTacticsNodeDefinition& Definition = Definitions[ConsiderationIndex];
		const FArcUtilityConsideration& Consideration = *Definition.InstanceData.GetPtr<FArcUtilityConsideration>();
		const FArcTacticsSimulatorInputState* State = InputStates.Find(Definition.ID);
		const FArcTacticsSimulatorInputState EffectiveState = State ? *State : FArcTacticsSimulatorInputState();
		const FArcUtilityConsiderationResult Result = EffectiveState.bVeto
			? FArcUtilityConsiderationResult::Reject(ArcTacticsSimulatorPrivate::SimulatedVetoReason)
			: FArcUtilityConsiderationResult::Accept(EffectiveState.Input);
		Progress = ArcUtilityScoring::AccumulateConsideration(
			Consideration, Result, ConsiderationIndex, FString(), PairResult,
			OutTracedRow ? &Trace : nullptr);
		if (Progress != EArcUtilityPairProgress::Continue)
		{
			break;
		}
	}
	if (Progress == EArcUtilityPairProgress::Continue)
	{
		Progress = ArcUtilityScoring::FinishPair(Branch.Weight, Definitions.Num(), MinScore, PairResult);
	}

	if (OutTracedRow)
	{
		for (const FArcUtilityConsiderationSnapshot& Snapshot : Snapshots)
		{
			if (OutTracedRow->Considerations.IsValidIndex(Snapshot.ConsiderationIndex))
			{
				FArcTacticsSimulatorConsideration& Consideration = OutTracedRow->Considerations[Snapshot.ConsiderationIndex];
				Consideration.bEvaluated = true;
				Consideration.CurvedScore = Snapshot.CurvedScore;
			}
		}
	}

	Evaluation.Score = PairResult.Score;
	Evaluation.Reason = PairResult.VetoReason;
	if (Progress == EArcUtilityPairProgress::Invalid)
	{
		Evaluation.Outcome = EArcTacticsSimulatorOutcome::Invalid;
	}
	else if (PairResult.Outcome == EArcUtilityPairOutcome::Accepted)
	{
		Evaluation.Outcome = EArcTacticsSimulatorOutcome::Accepted;
	}
	else
	{
		Evaluation.Outcome = PairResult.Outcome == EArcUtilityPairOutcome::Vetoed
			? EArcTacticsSimulatorOutcome::Vetoed
			: EArcTacticsSimulatorOutcome::BelowMinScore;
	}
	return Evaluation;
}

float FArcTacticsSimulator::ComputeChances(
	const TConstArrayView<FRowEvaluation> Evaluations,
	TArray<float>& OutChances) const
{
	OutChances.Reset();
	OutChances.SetNumZeroed(Evaluations.Num());
	TArray<float> AcceptedScores;
	TArray<int32> AcceptedPriorities;
	TArray<int32> AcceptedRows;
	for (int32 RowIndex = 0; RowIndex < Evaluations.Num(); ++RowIndex)
	{
		if (Evaluations[RowIndex].Outcome == EArcTacticsSimulatorOutcome::Accepted)
		{
			AcceptedScores.Add(Evaluations[RowIndex].Score);
			AcceptedPriorities.Add(Evaluations[RowIndex].Priority);
			AcceptedRows.Add(RowIndex);
		}
	}
	TArray<float> AcceptedChances;
	ArcTacticsDecision::ComputeSelectionChances(AcceptedScores, AcceptedPriorities, Selection, AcceptedChances);
	float ChanceSum = 0.0f;
	for (int32 AcceptedIndex = 0; AcceptedIndex < AcceptedRows.Num(); ++AcceptedIndex)
	{
		OutChances[AcceptedRows[AcceptedIndex]] = AcceptedChances[AcceptedIndex];
		ChanceSum += AcceptedChances[AcceptedIndex];
	}
	return FMath::Max(0.0f, 1.0f - ChanceSum);
}
