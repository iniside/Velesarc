// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTacticsConfigLint.h"

#include "Abilities/ArcAbilityDefinition.h"
#include "Considerations/ArcTacticsConsiderations.h"
#include "Decision/ArcTacticsBindings.h"
#include "Decision/ArcTacticsConfig.h"
#include "Decision/ArcTacticsNode.h"
#include "PropertyBindingPath.h"
#include "Steps/ArcTacticsUseAbilityStep.h"
#include "StructUtils/PropertyBag.h"
#include "UtilityAI/ArcUtilityConsideration.h"
#include "UtilityAI/ArcUtilityScoring.h"

namespace ArcTacticsConfigLintPrivate
{
	using namespace ArcTacticsConfigLint;

	constexpr int32 RangeIntervals = 128;
	constexpr float FlatCurveTolerance = 0.01f;

	struct FBindingIndex
	{
		TMap<FGuid, TArray<FName>> Written;
		TMap<FGuid, TArray<FName>> Read;
	};

	enum class EInputKind : uint8
	{
		Constant,
		Gate,
		OwnedTags,
		Range
	};

	struct FConsiderationRange
	{
		const FArcTacticsNodeDefinition* Definition = nullptr;
		const FArcUtilityConsideration* Consideration = nullptr;
		EInputKind Kind = EInputKind::Range;
		TArray<float> Inputs;
		bool bInputRange = false;
		bool bCurveBound = false;
		bool bValid = false;
		float BestInput = 0.0f;
		float WorstInput = 0.0f;
		float BestOutput = 0.0f;
		float WorstOutput = 0.0f;
	};

	struct FBranch
	{
		FGuid NodeID;
		FGuid ParametersID;
		const FArcTacticsBranchParameters* Parameters = nullptr;
		const TArray<FArcTacticsNodeDefinition>* Considerations = nullptr;
		const TArray<FArcTacticsNodeDefinition>* Steps = nullptr;
		bool bStandalone = false;
	};

	FName GetRootName(const FPropertyBindingPath& Path)
	{
		return Path.IsPathEmpty() ? NAME_None : Path.GetSegment(0).GetName();
	}

	bool Contains(const TMap<FGuid, TArray<FName>>& Map, const FGuid& ID, const FName Root)
	{
		const TArray<FName>* Roots = Map.Find(ID);
		return Roots && (Roots->Contains(NAME_None) || Roots->Contains(Root));
	}

	FBindingIndex BuildBindingIndex(const UArcTacticsConfig& Config)
	{
		FBindingIndex Index;
		for (const FArcTacticsBinding& Binding : Config.EditorBindings.GetBindings())
		{
			const FPropertyBindingPath& WrittenPath = Binding.IsOutputBinding() ? Binding.GetSourcePath() : Binding.GetTargetPath();
			const FPropertyBindingPath& ReadPath = Binding.IsOutputBinding() ? Binding.GetTargetPath() : Binding.GetSourcePath();
			Index.Written.FindOrAdd(WrittenPath.GetStructID()).AddUnique(GetRootName(WrittenPath));
			Index.Read.FindOrAdd(ReadPath.GetStructID()).AddUnique(GetRootName(ReadPath));
		}
		return Index;
	}

	bool IsWritten(const FBindingIndex& Bindings, const FGuid& ID, const FName Root)
	{
		return Contains(Bindings.Written, ID, Root);
	}

	FString GetName(const UArcTacticsConfig& Config, const FGuid& ID)
	{
		return Config.GetAuthoringName(ID).ToString();
	}

	bool IsGate(const UScriptStruct* Type)
	{
		return Type
			&& (Type->IsChildOf(FArcTacticsCostConsideration::StaticStruct())
				|| Type->IsChildOf(FArcTacticsCooldownConsideration::StaticStruct())
				|| Type->IsChildOf(FArcTacticsHasEligibleTargetConsideration::StaticStruct())
				|| Type->IsChildOf(FArcTacticsCombatSlotsAvailableConsideration::StaticStruct())
				|| Type->IsChildOf(FArcTacticsActionHistoryConsideration::StaticStruct()));
	}

	void AddRange(const float Low, const float High, TArray<float>& OutInputs)
	{
		for (int32 Step = 0; Step <= RangeIntervals; ++Step)
		{
			OutInputs.Add(FMath::Lerp(Low, High, static_cast<float>(Step) / static_cast<float>(RangeIntervals)));
		}
	}

	void BuildInputs(const FArcTacticsNodeDefinition& Definition, const FBindingIndex& Bindings, FConsiderationRange& Range)
	{
		const FInstancedStruct& Data = Definition.InstanceData;
		if (const FArcTacticsConstantConsideration* Constant = Data.GetPtr<FArcTacticsConstantConsideration>())
		{
			Range.Kind = EInputKind::Constant;
			if (!IsWritten(Bindings, Definition.ID, GET_MEMBER_NAME_CHECKED(FArcTacticsConstantConsideration, Value)))
			{
				Range.Inputs.Add(Constant->Value);
				return;
			}
		}
		else if (IsGate(Data.GetScriptStruct()))
		{
			Range.Kind = EInputKind::Gate;
			Range.Inputs.Add(1.0f);
			return;
		}
		else if (const FArcTacticsOwnedTagsConsideration* OwnedTags = Data.GetPtr<FArcTacticsOwnedTagsConsideration>())
		{
			Range.Kind = EInputKind::OwnedTags;
			if (!IsWritten(Bindings, Definition.ID, GET_MEMBER_NAME_CHECKED(FArcTacticsOwnedTagsConsideration, MatchScore))
				&& !IsWritten(Bindings, Definition.ID, GET_MEMBER_NAME_CHECKED(FArcTacticsOwnedTagsConsideration, MismatchScore)))
			{
				Range.Inputs.Add(OwnedTags->MatchScore);
				Range.Inputs.Add(OwnedTags->MismatchScore);
				return;
			}
		}
		else if (const FArcTacticsDistanceConsideration* Distance = Data.GetPtr<FArcTacticsDistanceConsideration>())
		{
			Range.Kind = EInputKind::Range;
			if (!IsWritten(Bindings, Definition.ID, GET_MEMBER_NAME_CHECKED(FArcTacticsDistanceConsideration, NearScore))
				&& !IsWritten(Bindings, Definition.ID, GET_MEMBER_NAME_CHECKED(FArcTacticsDistanceConsideration, FarScore)))
			{
				Range.bInputRange = true;
				AddRange(
					FMath::Min(Distance->NearScore, Distance->FarScore),
					FMath::Max(Distance->NearScore, Distance->FarScore),
					Range.Inputs);
				return;
			}
		}
		else
		{
			Range.Kind = EInputKind::Range;
		}
		Range.bInputRange = true;
		AddRange(0.0f, 1.0f, Range.Inputs);
	}

	bool EvaluateCurve(const FArcUtilityConsideration& Consideration, const float Input, float& OutOutput)
	{
		FArcUtilityPairResult Result;
		if (ArcUtilityScoring::BeginPair(1.0f, 1, 0.0f, Result) == EArcUtilityPairProgress::Invalid
			|| ArcUtilityScoring::AccumulateConsideration(
				Consideration, FArcUtilityConsiderationResult::Accept(Input), 0, FString(), Result)
				== EArcUtilityPairProgress::Invalid)
		{
			return false;
		}
		OutOutput = Result.Product;
		return true;
	}

	FConsiderationRange AnalyzeConsideration(const FArcTacticsNodeDefinition& Definition, const FBindingIndex& Bindings)
	{
		FConsiderationRange Range;
		Range.Definition = &Definition;
		Range.Consideration = Definition.InstanceData.GetPtr<FArcUtilityConsideration>();
		if (!Range.Consideration)
		{
			return Range;
		}
		Range.bCurveBound = IsWritten(
			Bindings, Definition.ID, GET_MEMBER_NAME_CHECKED(FArcUtilityConsideration, ResponseCurve));
		BuildInputs(Definition, Bindings, Range);
		for (const float Input : Range.Inputs)
		{
			float Output = 0.0f;
			if (!EvaluateCurve(*Range.Consideration, Input, Output))
			{
				continue;
			}
			if (!Range.bValid || Output > Range.BestOutput)
			{
				Range.BestOutput = Output;
				Range.BestInput = Input;
			}
			if (!Range.bValid || Output < Range.WorstOutput)
			{
				Range.WorstOutput = Output;
				Range.WorstInput = Input;
			}
			Range.bValid = true;
		}
		return Range;
	}

	bool ScoreBranch(
		const TArray<FConsiderationRange>& Ranges,
		const float Weight,
		const float MinScore,
		const bool bBest,
		FArcUtilityPairResult& OutResult)
	{
		const int32 NumConsiderations = Ranges.Num();
		if (ArcUtilityScoring::BeginPair(Weight, NumConsiderations, MinScore, OutResult) == EArcUtilityPairProgress::Invalid)
		{
			return false;
		}
		for (int32 Index = 0; Index < NumConsiderations; ++Index)
		{
			const FConsiderationRange& Range = Ranges[Index];
			const EArcUtilityPairProgress Progress = ArcUtilityScoring::AccumulateConsideration(
				*Range.Consideration,
				FArcUtilityConsiderationResult::Accept(bBest ? Range.BestInput : Range.WorstInput),
				Index,
				FString(),
				OutResult);
			if (Progress == EArcUtilityPairProgress::Invalid)
			{
				return false;
			}
			if (Progress == EArcUtilityPairProgress::Complete)
			{
				return true;
			}
		}
		return ArcUtilityScoring::FinishPair(Weight, NumConsiderations, MinScore, OutResult)
			!= EArcUtilityPairProgress::Invalid;
	}

	FString DescribeInputs(const FConsiderationRange& Range)
	{
		if (Range.Inputs.IsEmpty())
		{
			return FString();
		}
		if (Range.bInputRange)
		{
			return FString::Printf(TEXT("[%.3f, %.3f]"), Range.Inputs[0], Range.Inputs.Last());
		}
		TArray<FString> Values;
		for (const float Input : Range.Inputs)
		{
			Values.Add(FString::Printf(TEXT("%.3f"), Input));
		}
		return FString::Printf(TEXT("{%s}"), *FString::Join(Values, TEXT(", ")));
	}

	void Add(TArray<FFinding>& Findings, const ESeverity Severity, const TCHAR* Code, const FString& Node, const FString& Message)
	{
		FFinding& Finding = Findings.AddDefaulted_GetRef();
		Finding.Severity = Severity;
		Finding.Code = Code;
		Finding.Node = Node;
		Finding.Message = Message;
	}

	TArray<FBranch> CollectBranches(const UArcTacticsConfig& Config)
	{
		TArray<FBranch> Branches;
		for (const FArcTacticsActionDefinition& Action : Config.Actions)
		{
			FBranch& Branch = Branches.AddDefaulted_GetRef();
			Branch.NodeID = Action.Action.ID;
			Branch.ParametersID = Action.ParametersID;
			Branch.Parameters = &Action.Parameters;
			Branch.Considerations = &Action.Considerations;
			Branch.Steps = &Action.Steps;
			Branch.bStandalone = true;
		}
		for (const FArcTacticsSelectorDefinition& Selector : Config.Selectors)
		{
			FBranch& Branch = Branches.AddDefaulted_GetRef();
			Branch.NodeID = Selector.Selector.ID;
			Branch.ParametersID = Selector.ParametersID;
			Branch.Parameters = &Selector.Parameters;
			Branch.Considerations = &Selector.Considerations;
			Branch.Steps = &Selector.Steps;
			Branch.bStandalone = false;
		}
		return Branches;
	}

	void LintScores(
		const UArcTacticsConfig& Config,
		const TArray<FBranch>& Branches,
		const FBindingIndex& Bindings,
		TArray<FFinding>& Findings)
	{
		const float MinScore = Config.DecisionSettings.MinScore;
		const bool bMinScoreBound = IsWritten(
			Bindings, Config.DecisionSettingsID, GET_MEMBER_NAME_CHECKED(FArcTacticsDecisionSettingsParameters, MinScore));
		bool bAnyPriorityBound = false;
		for (const FBranch& Branch : Branches)
		{
			bAnyPriorityBound |= IsWritten(
				Bindings, Branch.ParametersID, GET_MEMBER_NAME_CHECKED(FArcTacticsBranchParameters, Priority));
		}

		for (const FBranch& Branch : Branches)
		{
			const FString BranchName = GetName(Config, Branch.NodeID);
			TArray<FConsiderationRange> Ranges;
			bool bAllValid = true;
			bool bAnyCurveBound = false;
			bool bAllConstant = true;
			for (const FArcTacticsNodeDefinition& Definition : *Branch.Considerations)
			{
				FConsiderationRange& Range = Ranges.Add_GetRef(AnalyzeConsideration(Definition, Bindings));
				bAllValid &= Range.bValid;
				bAnyCurveBound |= Range.bCurveBound;
				bAllConstant &= Range.Kind == EInputKind::Constant;
				if (Range.bValid && !Range.bCurveBound
					&& (Range.Kind == EInputKind::Range || Range.Kind == EInputKind::OwnedTags)
					&& Range.BestOutput - Range.WorstOutput < FlatCurveTolerance)
				{
					Add(Findings, ESeverity::Warning, TEXT("FlatCurve"), GetName(Config, Definition.ID), FString::Printf(
						TEXT("The response curve output varies by only %.4f (%.3f to %.3f) over its input set %s, so this consideration does not tell candidates apart."),
						Range.BestOutput - Range.WorstOutput,
						Range.WorstOutput,
						Range.BestOutput,
						*DescribeInputs(Range)));
				}
			}

			const bool bWeightBound = IsWritten(
				Bindings, Branch.ParametersID, GET_MEMBER_NAME_CHECKED(FArcTacticsBranchParameters, Weight));
			if (bMinScoreBound || bWeightBound || bAnyCurveBound || !bAllValid)
			{
				continue;
			}
			const float Weight = Branch.Parameters->Weight;

			FArcUtilityPairResult Best;
			if (ScoreBranch(Ranges, Weight, MinScore, true, Best)
				&& Best.Outcome == EArcUtilityPairOutcome::BelowMinScore)
			{
				FArcUtilityPairResult Unbounded;
				const FString BestScore = ScoreBranch(Ranges, Weight, 0.0f, true, Unbounded)
					? FString::Printf(TEXT("%.3f"), Unbounded.Score)
					: FString(TEXT("?"));
				Add(Findings, ESeverity::Warning, TEXT("Unreachable"), BranchName, FString::Printf(
					TEXT("The best possible score %s (weight %.3f, every consideration at its curve maximum) is below MinScore %.3f, so this branch is never selected."),
					*BestScore,
					Weight,
					MinScore));
			}

			const bool bCooldownBound = IsWritten(
				Bindings, Branch.ParametersID, GET_MEMBER_NAME_CHECKED(FArcTacticsBranchParameters, Cooldown));
			if (!Branch.bStandalone || !bAllConstant || bAnyPriorityBound || bCooldownBound
				|| Branch.Parameters->Cooldown > 0.0f)
			{
				continue;
			}
			bool bLowerPriorityExists = false;
			for (const FBranch& Other : Branches)
			{
				bLowerPriorityExists |= &Other != &Branch && Other.Parameters->Priority < Branch.Parameters->Priority;
			}
			FArcUtilityPairResult Worst;
			if (bLowerPriorityExists
				&& ScoreBranch(Ranges, Weight, MinScore, false, Worst)
				&& Worst.Outcome == EArcUtilityPairOutcome::Accepted)
			{
				Add(Findings, ESeverity::Warning, TEXT("AlwaysEligible"), BranchName, FString::Printf(
					TEXT("Priority %d with no cooldown and a worst score %.3f at or above MinScore %.3f: lower-priority branches never run while this action is a candidate. They are vetoed while it runs, and it wins every decision when it is not running."),
					Branch.Parameters->Priority,
					Worst.Score,
					MinScore));
			}
		}
	}

	void LintGlobalOutputs(const UArcTacticsConfig& Config, const FBindingIndex& Bindings, TArray<FFinding>& Findings)
	{
		for (const FArcTacticsNodeDefinition& Task : Config.GlobalTasks)
		{
			const UScriptStruct* DataType = Task.InstanceData.GetScriptStruct();
			if (!DataType)
			{
				continue;
			}
			for (TFieldIterator<FProperty> It(DataType); It; ++It)
			{
				const FProperty* Property = *It;
				if (ArcTacticsNode::ResolvePropertyRole(*Property) != EArcTacticsPropertyRole::Output
					|| Contains(Bindings.Read, Task.ID, Property->GetFName()))
				{
					continue;
				}
				Add(Findings, ESeverity::Info, TEXT("UnusedOutput"), GetName(Config, Task.ID), FString::Printf(
					TEXT("Output '%s' is not read by any binding."),
					*Property->GetName()));
			}
		}
	}

	void LintGlobalParameters(const UArcTacticsConfig& Config, const FBindingIndex& Bindings, TArray<FFinding>& Findings)
	{
		const UPropertyBag* BagStruct = Config.GlobalParameters.GetPropertyBagStruct();
		if (!BagStruct)
		{
			return;
		}
		for (const FPropertyBagPropertyDesc& Desc : BagStruct->GetPropertyDescs())
		{
			if (Contains(Bindings.Read, Config.GlobalParametersID, Desc.Name))
			{
				continue;
			}
			Add(Findings, ESeverity::Info, TEXT("UnreadParameter"), TEXT("GlobalParameters"), FString::Printf(
				TEXT("Global parameter '%s' is not read by any binding."),
				*Desc.Name.ToString()));
		}
	}

	void LintHistoryTags(
		const UArcTacticsConfig& Config,
		const TArray<FBranch>& Branches,
		const FBindingIndex& Bindings,
		TArray<FFinding>& Findings)
	{
		for (const FBranch& Branch : Branches)
		{
			if (IsWritten(Bindings, Branch.ParametersID, GET_MEMBER_NAME_CHECKED(FArcTacticsBranchParameters, HistoryTag)))
			{
				return;
			}
		}
		for (const FBranch& Branch : Branches)
		{
			for (const FArcTacticsNodeDefinition& Definition : *Branch.Considerations)
			{
				const FArcTacticsActionHistoryConsideration* History =
					Definition.InstanceData.GetPtr<FArcTacticsActionHistoryConsideration>();
				if (!History || !History->Tag.IsValid()
					|| IsWritten(Bindings, Definition.ID, GET_MEMBER_NAME_CHECKED(FArcTacticsActionHistoryConsideration, Tag)))
				{
					continue;
				}
				bool bWritten = false;
				for (const FBranch& Writer : Branches)
				{
					bWritten |= Writer.Parameters->HistoryTag.IsValid()
						&& Writer.Parameters->HistoryTag.MatchesTag(History->Tag);
				}
				if (!bWritten)
				{
					Add(Findings, ESeverity::Warning, TEXT("HistoryTagUnwritten"), GetName(Config, Definition.ID), FString::Printf(
						TEXT("Tag '%s' matches no branch HistoryTag (a recorded child tag would match), so the action history never contains it."),
						*History->Tag.ToString()));
				}
			}
		}
	}

	void LintAbilities(
		const UArcTacticsConfig& Config,
		const TArray<FBranch>& Branches,
		const FBindingIndex& Bindings,
		TArray<FFinding>& Findings)
	{
		for (const FBranch& Branch : Branches)
		{
			TArray<const UArcAbilityDefinition*> Checked;
			bool bCheckBound = false;
			for (const FArcTacticsNodeDefinition& Definition : *Branch.Considerations)
			{
				if (const FArcTacticsCostConsideration* Cost = Definition.InstanceData.GetPtr<FArcTacticsCostConsideration>())
				{
					Checked.Add(Cost->AbilityDefinition.Get());
					bCheckBound |= IsWritten(
						Bindings, Definition.ID, GET_MEMBER_NAME_CHECKED(FArcTacticsCostConsideration, AbilityDefinition));
				}
				else if (const FArcTacticsCooldownConsideration* Cooldown = Definition.InstanceData.GetPtr<FArcTacticsCooldownConsideration>())
				{
					Checked.Add(Cooldown->AbilityDefinition.Get());
					bCheckBound |= IsWritten(
						Bindings, Definition.ID, GET_MEMBER_NAME_CHECKED(FArcTacticsCooldownConsideration, AbilityDefinition));
				}
			}
			if (bCheckBound)
			{
				continue;
			}
			for (const FArcTacticsNodeDefinition& Step : *Branch.Steps)
			{
				const FArcTacticsUseAbilityStepInstanceData* Data =
					Step.InstanceData.GetPtr<FArcTacticsUseAbilityStepInstanceData>();
				if (!Data || !Data->AbilityDefinition || Checked.Contains(Data->AbilityDefinition.Get())
					|| IsWritten(Bindings, Step.ID, GET_MEMBER_NAME_CHECKED(FArcTacticsUseAbilityStepInstanceData, AbilityDefinition)))
				{
					continue;
				}
				Add(Findings, ESeverity::Info, TEXT("AbilityUnchecked"), GetName(Config, Step.ID), FString::Printf(
					TEXT("Ability '%s' has no Can Afford Cost or Cooldown Ready consideration for the same ability in branch '%s'. The action can be picked and then fail."),
					*Data->AbilityDefinition->GetName(),
					*GetName(Config, Branch.NodeID)));
			}
		}
	}
}

TArray<ArcTacticsConfigLint::FFinding> ArcTacticsConfigLint::Run(const UArcTacticsConfig& Config)
{
	using namespace ArcTacticsConfigLintPrivate;
	TArray<FFinding> Findings;
	if (!Config.CompiledData.IsExecutable())
	{
		return Findings;
	}
	const FBindingIndex Bindings = BuildBindingIndex(Config);
	const TArray<FBranch> Branches = CollectBranches(Config);
	LintScores(Config, Branches, Bindings, Findings);
	LintGlobalOutputs(Config, Bindings, Findings);
	LintGlobalParameters(Config, Bindings, Findings);
	LintHistoryTags(Config, Branches, Bindings, Findings);
	LintAbilities(Config, Branches, Bindings, Findings);
	return Findings;
}
