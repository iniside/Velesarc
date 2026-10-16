// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Decision/ArcTacticsCompiledData.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UArcTacticsConfig;
struct FArcTacticsNodeDefinition;

enum class EArcTacticsSimulatorOutcome : uint8
{
	Accepted,
	BelowMinScore,
	Vetoed,
	Invalid
};

struct FArcTacticsSimulatorInputState
{
	float Input = 0.5f;
	bool bVeto = false;
	bool bLocked = false;
};

struct FArcTacticsSimulatorConsideration
{
	FGuid NodeID;
	FText Name;
	bool bBound = false;
	bool bEvaluated = false;
	float CurvedScore = 0.0f;
};

struct FArcTacticsSimulatorRow
{
	FGuid NodeID;
	FGuid ParametersID;
	FText Name;
	FText SelectorName;
	bool bFromSelector = false;
	int32 SourceIndex = INDEX_NONE;
	int32 Priority = 0;
	bool bWeightBound = false;
	bool bPriorityBound = false;
	bool bHasBoundDefaults = false;
	TArray<FArcTacticsSimulatorConsideration> Considerations;
	EArcTacticsSimulatorOutcome Outcome = EArcTacticsSimulatorOutcome::Invalid;
	FName Reason;
	float Score = 0.0f;
	float Chance = 0.0f;
	float MonteCarloMeanChance = 0.0f;
	float MonteCarloEligibleShare = 0.0f;
};

class ARCTACTICSEDITOR_API FArcTacticsSimulator
{
public:
	static constexpr int32 MonteCarloSamples = 2000;
	static constexpr int32 MonteCarloSeed = 1316;

	void Rebuild(const UArcTacticsConfig* InConfig);
	void Recompute();

	void SetInput(const FGuid& ConsiderationID, float Input);
	void SetVeto(const FGuid& ConsiderationID, bool bVeto);
	void SetLocked(const FGuid& ConsiderationID, bool bLocked);
	void SetSelectionMode(EArcTacticsSelectionMode Mode);
	void SetTopPercent(float TopPercent);
	const FArcTacticsSimulatorInputState* FindState(const FGuid& ConsiderationID) const;
	void ResetInputs();

	void RunMonteCarlo();
	void ClearMonteCarlo();
	bool HasMonteCarlo() const { return bHasMonteCarlo; }

	const UArcTacticsConfig* GetConfig() const { return Config.Get(); }
	const TArray<FArcTacticsSimulatorRow>& GetRows() const { return Rows; }
	const FArcTacticsSimulatorRow* FindRow(const FGuid& RowID) const;
	float GetNothingPickedChance() const { return NothingPickedChance; }
	float GetMonteCarloNothingPickedChance() const { return MonteCarloNothingPickedChance; }
	EArcTacticsSelectionMode GetSelectionMode() const { return Selection.Mode; }
	float GetTopPercent() const { return Selection.TopPercent; }
	float GetMinScore() const { return MinScore; }

private:
	struct FBranchView
	{
		FGuid NodeID;
		FGuid ActionID;
		FGuid ParametersID;
		const TArray<FArcTacticsNodeDefinition>* Considerations = nullptr;
		float Weight = 0.0f;
		int32 Priority = 0;
		bool bFromSelector = false;
		int32 SourceIndex = INDEX_NONE;
	};

	struct FRowEvaluation
	{
		EArcTacticsSimulatorOutcome Outcome = EArcTacticsSimulatorOutcome::Invalid;
		FName Reason;
		float Score = 0.0f;
		int32 Priority = 0;
	};

	void GatherBranches(TArray<FBranchView>& OutBranches) const;
	bool MatchesStructure(TConstArrayView<FBranchView> Branches) const;
	void RebuildRows(TConstArrayView<FBranchView> Branches);
	float GetDefaultInput(const FArcTacticsNodeDefinition& Definition) const;
	FRowEvaluation EvaluateBranch(
		const FBranchView& Branch,
		const TMap<FGuid, FArcTacticsSimulatorInputState>& InputStates,
		FArcTacticsSimulatorRow* OutTracedRow) const;
	float ComputeChances(TConstArrayView<FRowEvaluation> Evaluations, TArray<float>& OutChances) const;

	TWeakObjectPtr<const UArcTacticsConfig> Config;
	TMap<FGuid, FArcTacticsSimulatorInputState> States;
	TArray<FArcTacticsSimulatorRow> Rows;
	FArcTacticsSelectionSettings Selection;
	float MinScore = 0.0f;
	float NothingPickedChance = 1.0f;
	float MonteCarloNothingPickedChance = 0.0f;
	bool bHasMonteCarlo = false;
};
