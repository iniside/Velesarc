// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArcTQSTypes.h"
#include "ArcTQSQuerySubsystem.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcTQSTestingActor.generated.h"

class UArcTQSQueryDefinition;
class UArcTQSRenderingComponent;
struct FArcTQSQueryInstance;

/**
 * Debug actor for testing TQS queries in the editor and at runtime.
 * Place in the world, configure a query (DataAsset or inline), and see
 * the results drawn as color-coded spheres with score labels and arrows.
 *
 * Uses a UDebugDrawComponent-based rendering component so that results
 * are visible in the editor viewport without requiring play-in-editor.
 *
 * Re-runs the query automatically when properties change or the actor is moved.
 */
UCLASS(hidecategories=(Rendering, Physics, Collision, Activation, Cooking, HLOD, Networking, Input))
class ARCAI_API AArcTQSTestingActor : public AActor
{
	GENERATED_BODY()

public:
	AArcTQSTestingActor();

	// --- Query Configuration ---

	// Toggle between DataAsset reference and inline definition
	UPROPERTY(EditAnywhere, Category = "Query")
	bool bUseInlineDefinition = false;

	// DataAsset-based query definition (shared, reusable)
	UPROPERTY(EditAnywhere, Category = "Query", meta = (EditCondition = "!bUseInlineDefinition"))
	TObjectPtr<UArcTQSQueryDefinition> QueryDefinition;

	// --- Inline definition (when bUseInlineDefinition = true) ---

	// Optional context provider that produces ContextLocations dynamically
	UPROPERTY(EditAnywhere, Category = "Query|Inline",
		meta = (BaseStruct = "/Script/ArcAI.ArcTQSContextProvider", EditCondition = "bUseInlineDefinition"))
	FInstancedStruct InlineContextProvider;

	// Generator that produces the initial target pool
	UPROPERTY(EditAnywhere, Category = "Query|Inline",
		meta = (BaseStruct = "/Script/ArcAI.ArcTQSGenerator", EditCondition = "bUseInlineDefinition"))
	FInstancedStruct InlineGenerator;

	// Ordered pipeline of filter/score steps
	UPROPERTY(EditAnywhere, Category = "Query|Inline",
		meta = (BaseStruct = "/Script/ArcAI.ArcTQSStep", EditCondition = "bUseInlineDefinition"))
	TArray<FInstancedStruct> InlineSteps;

	// How to select from the final scored pool
	UPROPERTY(EditAnywhere, Category = "Query|Inline", meta = (EditCondition = "bUseInlineDefinition"))
	EArcTQSSelectionMode InlineSelectionMode = EArcTQSSelectionMode::HighestScore;

	// For TopN mode: how many results to return
	UPROPERTY(EditAnywhere, Category = "Query|Inline",
		meta = (EditCondition = "bUseInlineDefinition && InlineSelectionMode == EArcTQSSelectionMode::TopN", ClampMin = 1))
	int32 InlineTopN = 5;

	// For AllPassing mode: minimum score threshold
	UPROPERTY(EditAnywhere, Category = "Query|Inline",
		meta = (EditCondition = "bUseInlineDefinition && InlineSelectionMode == EArcTQSSelectionMode::AllPassing"))
	float InlineMinPassingScore = 0.0f;

	// For custom top-percent modes: what percentage of the top items to consider
	UPROPERTY(EditAnywhere, Category = "Query|Inline",
		meta = (EditCondition = "bUseInlineDefinition && (InlineSelectionMode == EArcTQSSelectionMode::RandomFromTopPercent || InlineSelectionMode == EArcTQSSelectionMode::WeightedRandomFromTopPercent)",
			ClampMin = 1.0, ClampMax = 100.0))
	float InlineTopPercent = 25.0f;

	// --- Visualization Options ---

	// Draw score labels on each item
	UPROPERTY(EditAnywhere, Category = "Visualization")
	bool bDrawLabels = true;

	// Draw items that were filtered out (as small grey dots)
	UPROPERTY(EditAnywhere, Category = "Visualization")
	bool bDrawFilteredItems = true;

	// Re-query interval in seconds during play. Set to -1 for manual only.
	UPROPERTY(EditAnywhere, Category = "Visualization")
	float QueryInterval = 0.5f;

	// --- Read-only status ---

	UPROPERTY(VisibleAnywhere, Category = "Status")
	int32 GeneratedCount = 0;

	UPROPERTY(VisibleAnywhere, Category = "Status")
	int32 ValidCount = 0;

	UPROPERTY(VisibleAnywhere, Category = "Status")
	int32 ResultCount = 0;

	UPROPERTY(VisibleAnywhere, Category = "Status")
	float LastExecutionTimeMs = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Status")
	float ApproximateQueryTimeMs = 0.0f;
	
	// --- Methods ---

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
#endif

	// Manual trigger from the details panel
	UFUNCTION(CallInEditor, Category = "Query")
	void RunQuery();

private:
	void OnQueryCompleted(FArcTQSQueryInstance& CompletedQuery);
	FArcTQSQueryContext BuildQueryContext() const;
	void UpdateDrawing();
	void AbortActiveQuery();

	// Cached results for drawing
	FArcTQSDebugQueryData CachedData;
	bool bHasData = false;
	int32 ActiveQueryId = INDEX_NONE;
	double LastQueryTime = 0.0;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	TObjectPtr<UArcTQSRenderingComponent> RenderingComponent;
#endif
};
