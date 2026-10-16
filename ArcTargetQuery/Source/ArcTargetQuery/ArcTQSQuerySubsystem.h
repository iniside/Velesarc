// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSTypes.h"
#include "ArcTQSQueryInstance.h"
#include "ArcTQSQueryExecution.h"
#include "ArcTQSQueryReference.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/ObjectKey.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcTQSQuerySubsystem.generated.h"

class UArcTQSQueryDefinition;

DECLARE_DELEGATE_OneParam(FArcTQSQueryFinished, FArcTQSQueryInstance& /*CompletedQuery*/);

struct FArcTQSDebugQueryData
{
	FArcTQSQueryResult Result;
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
class ARCTARGETQUERY_API UArcTQSQuerySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UArcTQSQuerySubsystem();
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual bool IsTickableInEditor() const override { return true; }
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	int32 RunQuery(
		const FArcTQSQueryReference& Reference,
		const FArcTQSQueryContext& Context,
		FArcTQSQueryFinished OnFinished,
		int32 Priority = 0,
		const FArcTQSSelectionOverride* Override = nullptr);

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

	TSet<FObjectKey> LoggedNotExecutable;
	TSet<FObjectKey> LoggedLayoutMismatch;

#if !UE_BUILD_SHIPPING
	// Store debug snapshot from a completed query
	void StoreDebugData(const FArcTQSQueryInstance& CompletedQuery);

	// Remove expired debug entries
	void CleanupDebugData();

	TMap<FMassEntityHandle, FArcTQSDebugQueryData> DebugQueryData;
#endif
};
