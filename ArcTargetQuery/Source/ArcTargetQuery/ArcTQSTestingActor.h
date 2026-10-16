// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArcTQSTypes.h"
#include "ArcTQSQueryReference.h"
#include "ArcTQSQueryResult.h"
#include "ArcTQSPreviewEntities.h"
#include "ArcTQSTestingActor.generated.h"

class AArcTQSPreviewTargetActor;
class UArcTQSQueryDefinition;
class UArcTQSRenderingComponent;

/**
 * Debug actor for testing TQS queries in the editor and at runtime.
 * Place in the world, configure a query reference, and see
 * the results drawn as color-coded spheres with score labels and arrows.
 *
 * Uses a UDebugDrawComponent-based rendering component so that results
 * are visible in the editor viewport without requiring play-in-editor.
 *
 * Re-runs the query automatically when properties change or the actor is moved.
 */
UCLASS(hidecategories=(Rendering, Physics, Collision, Activation, Cooking, HLOD, Networking, Input))
class ARCTARGETQUERY_API AArcTQSTestingActor : public AActor
{
	GENERATED_BODY()

public:
	AArcTQSTestingActor();

	// --- Query Configuration ---

	UPROPERTY(EditAnywhere, Category = "Query")
	FArcTQSQueryReference Query;

	UPROPERTY(EditAnywhere, Category = "Query")
	TArray<TObjectPtr<AArcTQSPreviewTargetActor>> Targets;

	UPROPERTY(EditAnywhere, Category = "Query")
	TMap<FGuid, TObjectPtr<AArcTQSPreviewTargetActor>> EntityParameterSources;

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

	// --- Methods ---

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }
	virtual void PostRegisterAllComponents() override;
	virtual void UnregisterAllComponents(bool bForReregister = false) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditUndo() override;
#endif

	// Manual trigger from the details panel
	UFUNCTION(CallInEditor, Category = "Query")
	void RunQuery();

private:
	FArcTQSQueryContext BuildQueryContext() const;
	void UpdateDrawing();
	void CreateQuerierEntity();
	void DestroyQuerierEntity();
	void CollectPreviewTargets(TArray<AArcTQSPreviewTargetActor*>& OutTargets) const;
	void BindPreviewTargets();
	void UnbindPreviewTargets();
	void HandlePreviewTargetMoved(AArcTQSPreviewTargetActor* Target);
#if WITH_EDITOR
	void HandleDefinitionCompiled(const UArcTQSQueryDefinition& Definition);

	FDelegateHandle DefinitionCompiledHandle;
#endif

	FArcTQSPreviewEntities PreviewEntities;
	FMassEntityHandle QuerierEntity;
	TArray<TWeakObjectPtr<AArcTQSPreviewTargetActor>> BoundTargets;

	FArcTQSQueryResult LastResult;
	bool bHasData = false;
	double LastQueryTime = 0.0;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	TObjectPtr<UArcTQSRenderingComponent> RenderingComponent;
#endif
};
