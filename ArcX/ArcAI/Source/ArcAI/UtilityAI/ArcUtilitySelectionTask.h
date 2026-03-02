// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeExecutionContext.h"
#include "MassStateTreeTypes.h"
#include "StateTreeAsyncExecutionContext.h"
#include "UtilityAI/ArcUtilityTypes.h"
#include "UtilityAI/ArcUtilityEntry.h"
#include "UtilityAI/ArcUtilityScoringSubsystem.h"
#include "ArcUtilitySelectionTask.generated.h"

class UMassSignalSubsystem;
struct FArcUtilityScoringInstance;

USTRUCT()
struct FArcUtilitySelectionTaskInstanceData
{
	GENERATED_BODY()

	// --- Inputs (bindable from parent state) ---

	UPROPERTY(EditAnywhere, Category = "Input", meta = (Optional))
	TArray<TObjectPtr<AActor>> InActors;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (Optional))
	TArray<FMassEntityHandle> InEntities;

	UPROPERTY(EditAnywhere, Category = "Input", meta = (Optional))
	TArray<FVector> InLocations;

	// --- Configuration ---

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TArray<FArcUtilityEntry> Entries;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	EArcUtilitySelectionMode SelectionMode = EArcUtilitySelectionMode::HighestScore;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float MinScore = 0.1f;

	UPROPERTY(EditAnywhere, Category = "Parameter",
		meta = (EditCondition = "SelectionMode == EArcUtilitySelectionMode::RandomFromTopPercent", ClampMin = 1.0, ClampMax = 100.0))
	float TopPercent = 25.0f;

	// --- Outputs (bindable to parent state) ---

	UPROPERTY(EditAnywhere, Category = "Output")
	TObjectPtr<AActor> SelectedActor;

	UPROPERTY(EditAnywhere, Category = "Output")
	FMassEntityHandle SelectedEntity;

	UPROPERTY(EditAnywhere, Category = "Output")
	FVector SelectedLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Output")
	int32 SelectedEntryIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, Category = "Output")
	float SelectedScore = 0.0f;

	// --- Runtime (not exposed to editor) ---

	int32 RequestId = INDEX_NONE;
	FMassEntityHandle EntityHandle;
};

/**
 * State Tree task that evaluates a matrix of Options x Targets using
 * FArcUtilityConsideration pipelines, time-sliced via UArcUtilityScoringSubsystem.
 *
 * Results are delivered via delegate — the task does NOT tick.
 * On completion, the winning entry's linked state is transitioned to via RequestTransition.
 * If no entry passes MinScore, the task finishes with Failed.
 */
USTRUCT(meta = (DisplayName = "Arc Utility Selection", Category = "Arc|AI"))
struct ARCAI_API FArcUtilitySelectionTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcUtilitySelectionTaskInstanceData;

	FArcUtilitySelectionTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	static void OnScoringCompleted(
		FArcUtilityScoringInstance& CompletedInstance,
		FStateTreeWeakExecutionContext WeakContext,
		UMassSignalSubsystem* SignalSubsystem,
		FMassEntityHandle Entity);

#if WITH_EDITOR
	virtual FName GetIconName() const override
	{
		return FName("StateTreeEditorStyle|Node.Select");
	}
	virtual FColor GetIconColor() const override
	{
		return FColor(220, 160, 40); // warm orange
	}
#endif

private:
	static TArray<FArcUtilityTarget> BuildTargets(const FInstanceDataType& InstanceData);
	static FArcUtilityContext BuildContext(
		FMassStateTreeExecutionContext& MassCtx,
		FMassEntityManager& EntityManager,
		UWorld* World);
};
