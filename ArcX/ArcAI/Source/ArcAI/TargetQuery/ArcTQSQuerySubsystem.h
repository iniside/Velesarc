// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSTypes.h"
#include "ArcTQSQueryInstance.h"
#include "StructUtils/InstancedStruct.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcTQSQuerySubsystem.generated.h"

class UArcTQSQueryDefinition;

DECLARE_DELEGATE_OneParam(FArcTQSQueryFinished, FArcTQSQueryInstance& /*CompletedQuery*/);

/**
 * Debug snapshot of a completed TQS query.
 * Stored per-entity in the subsystem so the Gameplay Debugger can visualize results.
 */
struct FArcTQSDebugQueryData
{
	// All items generated (including filtered-out ones, with bValid = false)
	TArray<FArcTQSTargetItem> AllItems;

	// Final selected results
	TArray<FArcTQSTargetItem> Results;

	// Query origin
	FVector QuerierLocation = FVector::ZeroVector;

	// Context locations the generator ran around
	TArray<FVector> ContextLocations;

	// Timing
	double ExecutionTimeMs = 0.0;

	// Query status
	EArcTQSQueryStatus Status = EArcTQSQueryStatus::Pending;

	// Selection mode used
	EArcTQSSelectionMode SelectionMode = EArcTQSSelectionMode::HighestScore;

	// Number of items generated / passed filters
	int32 TotalGenerated = 0;
	int32 TotalValid = 0;

	// Timestamp for auto-expiry
	double Timestamp = 0.0;
};

/**
 * Manages all running TQS queries, distributing frame time budget across them.
 * Queries are time-sliced: each gets a portion of the per-frame budget, with higher
 * priority queries processed first.
 *
 * Results are delivered via callback (FArcTQSQueryFinished) — the subsystem does not
 * store completed queries. Callers must consume results in the callback.
 *
 * Debug data from completed queries is stored per-entity for the Gameplay Debugger.
 */
UCLASS()
class ARCAI_API UArcTQSQuerySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/**
	 * Submit a query from a DataAsset definition. Copies generator/steps/selection into the instance.
	 * @param Definition - the query definition data asset
	 * @param Context - query context with querier info
	 * @param OnFinished - callback when query completes (results are delivered here)
	 * @param Priority - higher = processed first (default 0)
	 * @return query ID handle, or INDEX_NONE on failure
	 */
	int32 RunQuery(
		const UArcTQSQueryDefinition* Definition,
		const FArcTQSQueryContext& Context,
		FArcTQSQueryFinished OnFinished,
		int32 Priority = 0);

	/**
	 * Submit a query from raw data (generator, steps, selection config).
	 * Used by State Tree tasks with inline instanced structs.
	 * Takes ownership of the provided data via move.
	 */
	int32 RunQuery(
		FInstancedStruct&& InContextProvider,
		FInstancedStruct&& InGenerator,
		TArray<FInstancedStruct>&& InSteps,
		EArcTQSSelectionMode InSelectionMode,
		int32 InTopN,
		float InMinPassingScore,
		const FArcTQSQueryContext& Context,
		FArcTQSQueryFinished OnFinished,
		int32 Priority = 0);

	// Abort a running query
	bool AbortQuery(int32 QueryId);

	// Check if a query is still running
	bool IsQueryRunning(int32 QueryId) const;

	// --- Debug API ---

	// Get debug data for a specific entity's last completed query
	const FArcTQSDebugQueryData* GetDebugData(FMassEntityHandle Entity) const;

#if !UE_BUILD_SHIPPING
	// Get all stored debug data (for overview display)
	const TMap<FMassEntityHandle, FArcTQSDebugQueryData>& GetAllDebugData() const { return DebugQueryData; }
#endif

	// Max time per frame for all TQS queries (seconds)
	UPROPERTY(EditAnywhere, Category = "ArcTQS")
	float MaxAllowedTestingTime = 0.005f; // 5ms budget

	// How long to keep debug data before auto-expiry (seconds)
	static constexpr double DebugDataExpiryTime = 10.0;

private:
	int32 SubmitInstance(TSharedPtr<FArcTQSQueryInstance> Instance, FArcTQSQueryFinished OnFinished);

	TArray<TSharedPtr<FArcTQSQueryInstance>> RunningQueries;
	int32 NextQueryId = 1;

#if !UE_BUILD_SHIPPING
	// Store debug snapshot from a completed query
	void StoreDebugData(const FArcTQSQueryInstance& CompletedQuery);

	// Remove expired debug entries
	void CleanupDebugData();

	TMap<FMassEntityHandle, FArcTQSDebugQueryData> DebugQueryData;
#endif
};
