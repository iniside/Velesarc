// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "ArcKnowledgeQuery.h"
#include "MassEntityFragments.h"
#include "ArcMassQueryKnowledgeTask.generated.h"

class UArcKnowledgeQueryDefinition;

USTRUCT()
struct FArcMassQueryKnowledgeTaskInstanceData
{
	GENERATED_BODY()

	/** Toggle between DataAsset reference and inline query definition. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bUseInlineDefinition = false;

	/** DataAsset-based query definition (shared, reusable). */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (EditCondition = "!bUseInlineDefinition"))
	TObjectPtr<UArcKnowledgeQueryDefinition> QueryDefinition = nullptr;

	/** Inline tag query for filtering entries. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (EditCondition = "bUseInlineDefinition"))
	FGameplayTagQuery InlineTagQuery;

	/** Inline filter pipeline. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (BaseStruct = "/Script/ArcKnowledge.ArcKnowledgeFilter", EditCondition = "bUseInlineDefinition"))
	TArray<FInstancedStruct> InlineFilters;

	/** Inline scorer pipeline. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (BaseStruct = "/Script/ArcKnowledge.ArcKnowledgeScorer", EditCondition = "bUseInlineDefinition"))
	TArray<FInstancedStruct> InlineScorers;

	/** Inline max results. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (EditCondition = "bUseInlineDefinition", ClampMin = 1))
	int32 InlineMaxResults = 1;

	/** Inline selection mode. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (EditCondition = "bUseInlineDefinition"))
	EArcKnowledgeSelectionMode InlineSelectionMode = EArcKnowledgeSelectionMode::HighestScore;

	// --- Outputs ---

	/** Best single result. */
	UPROPERTY(EditAnywhere, Category = Output)
	FArcKnowledgeQueryResult BestResult;

	/** All results. */
	UPROPERTY(EditAnywhere, Category = Output)
	TArray<FArcKnowledgeQueryResult> ResultArray;

	/** Whether the query found any results. */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bHasResults = false;
};

/**
 * StateTree task that runs a knowledge query and outputs results.
 * Queries are synchronous (fast for small scale).
 * The NPC's StateTree decides what to do with results.
 */
USTRUCT(meta = (DisplayName = "Arc Query Knowledge", Category = "ArcKnowledge"))
struct ARCKNOWLEDGE_API FArcMassQueryKnowledgeTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassQueryKnowledgeTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Find"); }
	virtual FColor GetIconColor() const override { return FColor(100, 200, 100); }
#endif

private:
	TStateTreeExternalDataHandle<FTransformFragment> TransformHandle;
};
