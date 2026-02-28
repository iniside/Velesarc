// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcSettlementTypes.h"
#include "ArcKnowledgeEntry.h"
#include "ArcKnowledgeQuery.h"
#include "ArcSettlementData.h"
#include "ArcSettlementSubsystem.generated.h"

class UArcKnowledgeQueryDefinition;
class UArcSettlementDefinition;
class UArcRegionDefinition;

/**
 * Central subsystem for the meta-knowledge layer.
 * Owns the knowledge index, settlement registry, and query execution.
 *
 * The subsystem is an information broker — it indexes facts about the world,
 * answers queries, and holds action postings. It never decides anything for NPCs.
 */
UCLASS()
class ARCSETTLEMENT_API UArcSettlementSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// ---------- USubsystem ----------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---------- UTickableWorldSubsystem ----------
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

	// ====================================================================
	// Knowledge Index
	// ====================================================================

	/** Register a new knowledge entry. Returns a handle for future updates/removal. */
	UFUNCTION(BlueprintCallable, Category = "ArcSettlement|Knowledge")
	FArcKnowledgeHandle RegisterKnowledge(UPARAM(ref) FArcKnowledgeEntry& Entry);

	/** Update an existing knowledge entry. */
	UFUNCTION(BlueprintCallable, Category = "ArcSettlement|Knowledge")
	void UpdateKnowledge(FArcKnowledgeHandle Handle, const FArcKnowledgeEntry& Updated);

	/** Remove a knowledge entry. */
	UFUNCTION(BlueprintCallable, Category = "ArcSettlement|Knowledge")
	void RemoveKnowledge(FArcKnowledgeHandle Handle);

	/** Register multiple entries at once. */
	void RegisterKnowledgeBatch(TArray<FArcKnowledgeEntry>& Entries, TArray<FArcKnowledgeHandle>& OutHandles);

	/** Refresh the timestamp and relevance of an existing entry. */
	UFUNCTION(BlueprintCallable, Category = "ArcSettlement|Knowledge")
	void RefreshKnowledge(FArcKnowledgeHandle Handle, float NewRelevance = 1.0f);

	/** Get an entry by handle (returns nullptr if not found). */
	const FArcKnowledgeEntry* GetKnowledgeEntry(FArcKnowledgeHandle Handle) const;

	// ====================================================================
	// Knowledge Mutation (partial updates)
	// ====================================================================

	/** Add a tag to an existing knowledge entry. Updates tag index. */
	UFUNCTION(BlueprintCallable, Category = "ArcSettlement|Knowledge")
	void AddTagToKnowledge(FArcKnowledgeHandle Handle, FGameplayTag Tag);

	/** Remove a tag from an existing knowledge entry. Updates tag index. */
	UFUNCTION(BlueprintCallable, Category = "ArcSettlement|Knowledge")
	void RemoveTagFromKnowledge(FArcKnowledgeHandle Handle, FGameplayTag Tag);

	/** Replace just the tags on an entry. Rebuilds tag index for this entry. */
	UFUNCTION(BlueprintCallable, Category = "ArcSettlement|Knowledge")
	void SetKnowledgeTags(FArcKnowledgeHandle Handle, const FGameplayTagContainer& NewTags);

	/** Replace just the payload on an entry. Updates timestamp. */
	UFUNCTION(BlueprintCallable, Category = "ArcSettlement|Knowledge")
	void SetKnowledgePayload(FArcKnowledgeHandle Handle, const FInstancedStruct& NewPayload);

	// ====================================================================
	// Source Entity Index (ownership-based cleanup)
	// ====================================================================

	/** Remove all knowledge entries registered by a specific source entity. */
	UFUNCTION(BlueprintCallable, Category = "ArcSettlement|Knowledge")
	void RemoveKnowledgeBySource(FMassEntityHandle SourceEntity);

	/** Get all knowledge handles registered by a source entity. */
	void GetKnowledgeBySource(FMassEntityHandle SourceEntity, TArray<FArcKnowledgeHandle>& OutHandles) const;

	// ====================================================================
	// Knowledge Queries
	// ====================================================================

	/** Execute a query using inline parameters. */
	UFUNCTION(BlueprintCallable, Category = "ArcSettlement|Knowledge")
	void QueryKnowledge(const FArcKnowledgeQuery& Query, const FArcKnowledgeQueryContext& Context, TArray<FArcKnowledgeQueryResult>& OutResults) const;

	/** Execute a query from a reusable data asset definition. */
	UFUNCTION(BlueprintCallable, Category = "ArcSettlement|Knowledge")
	void QueryKnowledgeFromDefinition(const UArcKnowledgeQueryDefinition* Definition, const FArcKnowledgeQueryContext& Context, TArray<FArcKnowledgeQueryResult>& OutResults) const;

	// ====================================================================
	// Actions (knowledge entries with action semantics)
	// ====================================================================

	/** Post an action (quest/request) for other entities to pick up. */
	UFUNCTION(BlueprintCallable, Category = "ArcSettlement|Actions")
	FArcKnowledgeHandle PostAction(UPARAM(ref) FArcKnowledgeEntry& ActionEntry);

	/** Claim an action so others don't pick it up. */
	UFUNCTION(BlueprintCallable, Category = "ArcSettlement|Actions")
	bool ClaimAction(FArcKnowledgeHandle Handle, FMassEntityHandle Claimer);

	/** Mark an action as completed and remove it. */
	UFUNCTION(BlueprintCallable, Category = "ArcSettlement|Actions")
	void CompleteAction(FArcKnowledgeHandle Handle);

	/** Cancel a claim, making the action available again. */
	UFUNCTION(BlueprintCallable, Category = "ArcSettlement|Actions")
	void CancelAction(FArcKnowledgeHandle Handle);

	// ====================================================================
	// Settlements
	// ====================================================================

	/** Create a settlement from a definition at a world location. */
	UFUNCTION(BlueprintCallable, Category = "ArcSettlement|Settlement")
	FArcSettlementHandle CreateSettlement(const UArcSettlementDefinition* Definition, FVector Location);

	/** Remove a settlement and all its associated knowledge entries. */
	UFUNCTION(BlueprintCallable, Category = "ArcSettlement|Settlement")
	void DestroySettlement(FArcSettlementHandle Handle);

	/** Get settlement data by handle. */
	UFUNCTION(BlueprintCallable, Category = "ArcSettlement|Settlement")
	bool GetSettlementData(FArcSettlementHandle Handle, FArcSettlementData& OutData) const;

	/** Find which settlement a world location belongs to. */
	UFUNCTION(BlueprintCallable, Category = "ArcSettlement|Settlement")
	FArcSettlementHandle FindSettlementAt(const FVector& WorldLocation) const;

	/** Find all settlements within a radius. */
	UFUNCTION(BlueprintCallable, Category = "ArcSettlement|Settlement")
	void QuerySettlementsInRadius(const FVector& Center, float Radius, TArray<FArcSettlementHandle>& OutHandles) const;

	/** Get all registered settlements. */
	const TMap<FArcSettlementHandle, FArcSettlementData>& GetAllSettlements() const { return Settlements; }

	// ====================================================================
	// Regions
	// ====================================================================

	/** Create a region. */
	UFUNCTION(BlueprintCallable, Category = "ArcSettlement|Region")
	FArcRegionHandle CreateRegion(const UArcRegionDefinition* Definition, FVector Center, float Radius);

	/** Remove a region. */
	UFUNCTION(BlueprintCallable, Category = "ArcSettlement|Region")
	void DestroyRegion(FArcRegionHandle Handle);

	/** Get region data by handle. */
	UFUNCTION(BlueprintCallable, Category = "ArcSettlement|Region")
	bool GetRegionData(FArcRegionHandle Handle, FArcRegionData& OutData) const;

	/** Find which region a settlement belongs to. */
	FArcRegionHandle FindRegionForSettlement(FArcSettlementHandle SettlementHandle) const;

	// ====================================================================
	// Configuration
	// ====================================================================

	/** Relevance decay rate per second. Entries below MinRelevanceThreshold are auto-removed. */
	UPROPERTY(EditAnywhere, Category = "Configuration")
	float RelevanceDecayRate = 0.01f;

	/** Entries with relevance below this are auto-removed during tick. */
	UPROPERTY(EditAnywhere, Category = "Configuration")
	float MinRelevanceThreshold = 0.01f;

	/** Interval in seconds between relevance decay ticks. */
	UPROPERTY(EditAnywhere, Category = "Configuration")
	float DecayTickInterval = 5.0f;

private:
	// Internal query execution
	void ExecuteQuery(
		const FGameplayTagQuery& TagQuery,
		const TArray<FInstancedStruct>& Filters,
		const TArray<FInstancedStruct>& Scorers,
		int32 MaxResults,
		EArcKnowledgeSelectionMode SelectionMode,
		const FArcKnowledgeQueryContext& Context,
		TArray<FArcKnowledgeQueryResult>& OutResults) const;

	// Tag index management
	void AddToTagIndex(FArcKnowledgeHandle Handle, const FGameplayTagContainer& Tags);
	void RemoveFromTagIndex(FArcKnowledgeHandle Handle, const FGameplayTagContainer& Tags);

	// Gather candidate handles from the tag index that might match the query
	void GatherCandidates(const FGameplayTagQuery& TagQuery, TArray<FArcKnowledgeHandle>& OutCandidates) const;

	// Settlement-knowledge association
	void RemoveKnowledgeForSettlement(FArcSettlementHandle SettlementHandle);

	// Auto-assign settlements to regions based on spatial overlap
	void AssignSettlementToRegions(FArcSettlementHandle SettlementHandle, const FVector& Location);

	// ---------- Storage ----------

	/** All knowledge entries, keyed by handle. */
	TMap<FArcKnowledgeHandle, FArcKnowledgeEntry> Entries;

	/** Tag -> handles index for fast query lookups. */
	TMap<FGameplayTag, TArray<FArcKnowledgeHandle>> TagIndex;

	/** Settlement -> knowledge handles index. */
	TMap<FArcSettlementHandle, TArray<FArcKnowledgeHandle>> SettlementKnowledgeIndex;

	/** Source entity -> knowledge handles index.
	  * Only populated for entries whose SourceEntity is valid. */
	TMap<FMassEntityHandle, TArray<FArcKnowledgeHandle>> SourceEntityIndex;

	/** All settlements. */
	TMap<FArcSettlementHandle, FArcSettlementData> Settlements;

	/** All regions. */
	TMap<FArcRegionHandle, FArcRegionData> Regions;

	/** Time accumulator for relevance decay. */
	float DecayTimeAccumulator = 0.0f;
};
