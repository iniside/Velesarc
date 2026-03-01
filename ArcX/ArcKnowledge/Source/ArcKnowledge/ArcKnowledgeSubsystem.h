// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcKnowledgeTypes.h"
#include "ArcKnowledgeEntry.h"
#include "ArcKnowledgeQuery.h"
#include "ArcKnowledgeEvent.h"
#include "ArcKnowledgeEventBroadcaster.h"
#include "ArcKnowledgeSpatialHash.h"
#include "ArcKnowledgeSubsystem.generated.h"

class UArcKnowledgeQueryDefinition;
class UArcKnowledgeEntryDefinition;

/**
 * Central subsystem for the meta-knowledge layer.
 * Owns the knowledge index and query execution.
 *
 * The subsystem is an information broker — it indexes facts about the world,
 * answers queries, and holds advertisement postings. It never decides anything for NPCs.
 */
UCLASS()
class ARCKNOWLEDGE_API UArcKnowledgeSubsystem : public UTickableWorldSubsystem
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
	UFUNCTION(BlueprintCallable, Category = "ArcKnowledge|Knowledge")
	FArcKnowledgeHandle RegisterKnowledge(UPARAM(ref) FArcKnowledgeEntry& Entry);

	/** Update an existing knowledge entry. */
	UFUNCTION(BlueprintCallable, Category = "ArcKnowledge|Knowledge")
	void UpdateKnowledge(FArcKnowledgeHandle Handle, const FArcKnowledgeEntry& Updated);

	/** Remove a knowledge entry. */
	UFUNCTION(BlueprintCallable, Category = "ArcKnowledge|Knowledge")
	void RemoveKnowledge(FArcKnowledgeHandle Handle);

	/** Register multiple entries at once. */
	void RegisterKnowledgeBatch(TArray<FArcKnowledgeEntry>& Entries, TArray<FArcKnowledgeHandle>& OutHandles);

	/** Register all entries from a definition at a given location.
	  * Registers the primary entry (from definition tags/payload) + all InitialKnowledge entries.
	  * Entries with zero location get the provided location. SourceEntity is set on all entries. */
	UFUNCTION(BlueprintCallable, Category = "ArcKnowledge|Knowledge")
	void RegisterFromDefinition(const UArcKnowledgeEntryDefinition* Definition, const FVector& Location, FMassEntityHandle SourceEntity, TArray<FArcKnowledgeHandle>& OutHandles);

	/** Refresh the timestamp and relevance of an existing entry. */
	UFUNCTION(BlueprintCallable, Category = "ArcKnowledge|Knowledge")
	void RefreshKnowledge(FArcKnowledgeHandle Handle, float NewRelevance = 1.0f);

	/** Get an entry by handle (returns nullptr if not found). */
	const FArcKnowledgeEntry* GetKnowledgeEntry(FArcKnowledgeHandle Handle) const;

	// ====================================================================
	// Knowledge Mutation (partial updates)
	// ====================================================================

	/** Add a tag to an existing knowledge entry. Updates tag index. */
	UFUNCTION(BlueprintCallable, Category = "ArcKnowledge|Knowledge")
	void AddTagToKnowledge(FArcKnowledgeHandle Handle, FGameplayTag Tag);

	/** Remove a tag from an existing knowledge entry. Updates tag index. */
	UFUNCTION(BlueprintCallable, Category = "ArcKnowledge|Knowledge")
	void RemoveTagFromKnowledge(FArcKnowledgeHandle Handle, FGameplayTag Tag);

	/** Replace just the tags on an entry. Rebuilds tag index for this entry. */
	UFUNCTION(BlueprintCallable, Category = "ArcKnowledge|Knowledge")
	void SetKnowledgeTags(FArcKnowledgeHandle Handle, const FGameplayTagContainer& NewTags);

	/** Replace just the payload on an entry. Updates timestamp. */
	UFUNCTION(BlueprintCallable, Category = "ArcKnowledge|Knowledge")
	void SetKnowledgePayload(FArcKnowledgeHandle Handle, const FInstancedStruct& NewPayload);

	// ====================================================================
	// Source Entity Index (ownership-based cleanup)
	// ====================================================================

	/** Remove all knowledge entries registered by a specific source entity. */
	UFUNCTION(BlueprintCallable, Category = "ArcKnowledge|Knowledge")
	void RemoveKnowledgeBySource(FMassEntityHandle SourceEntity);

	/** Get all knowledge handles registered by a source entity. */
	void GetKnowledgeBySource(FMassEntityHandle SourceEntity, TArray<FArcKnowledgeHandle>& OutHandles) const;

	// ====================================================================
	// Knowledge Queries
	// ====================================================================

	/** Execute a query using inline parameters. */
	UFUNCTION(BlueprintCallable, Category = "ArcKnowledge|Knowledge")
	void QueryKnowledge(const FArcKnowledgeQuery& Query, const FArcKnowledgeQueryContext& Context, TArray<FArcKnowledgeQueryResult>& OutResults) const;

	/** Execute a query from a reusable data asset definition. */
	UFUNCTION(BlueprintCallable, Category = "ArcKnowledge|Knowledge")
	void QueryKnowledgeFromDefinition(const UArcKnowledgeQueryDefinition* Definition, const FArcKnowledgeQueryContext& Context, TArray<FArcKnowledgeQueryResult>& OutResults) const;

	// ====================================================================
	// Spatial Queries
	// ====================================================================

	/** Find all knowledge entries within radius of a point. Optionally filter by tags. */
	UFUNCTION(BlueprintCallable, Category = "ArcKnowledge|Knowledge")
	void QueryKnowledgeInRadius(const FVector& Center, float Radius, TArray<FArcKnowledgeHandle>& OutHandles, const FGameplayTagQuery& OptionalTagFilter) const;

	/** Find the nearest knowledge entry matching tags to a point. Returns invalid handle if none found. */
	UFUNCTION(BlueprintCallable, Category = "ArcKnowledge|Knowledge")
	FArcKnowledgeHandle FindNearestKnowledge(const FVector& Location, float MaxRadius, const FGameplayTagQuery& TagFilter) const;

	// ====================================================================
	// Advertisements (knowledge entries with advertisement semantics)
	// ====================================================================

	/** Post an advertisement for other entities to pick up. */
	UFUNCTION(BlueprintCallable, Category = "ArcKnowledge|Advertisements")
	FArcKnowledgeHandle PostAdvertisement(UPARAM(ref) FArcKnowledgeEntry& AdvertisementEntry);

	/** Claim an advertisement so others don't pick it up. */
	UFUNCTION(BlueprintCallable, Category = "ArcKnowledge|Advertisements")
	bool ClaimAdvertisement(FArcKnowledgeHandle Handle, FMassEntityHandle Claimer);

	/** Mark an advertisement as completed and remove it. */
	UFUNCTION(BlueprintCallable, Category = "ArcKnowledge|Advertisements")
	void CompleteAdvertisement(FArcKnowledgeHandle Handle);

	/** Cancel a claim, making the advertisement available again. */
	UFUNCTION(BlueprintCallable, Category = "ArcKnowledge|Advertisements")
	void CancelAdvertisement(FArcKnowledgeHandle Handle);

	// ====================================================================
	// Events
	// ====================================================================

	/** Get the event broadcaster for external listener management. */
	FArcKnowledgeEventBroadcaster& GetEventBroadcaster() { return EventBroadcaster; }
	const FArcKnowledgeEventBroadcaster& GetEventBroadcaster() const { return EventBroadcaster; }

	// ====================================================================
	// Configuration
	// ====================================================================

	/** Configuration for knowledge event broadcasting. */
	UPROPERTY(EditAnywhere, Category = "Configuration")
	FArcKnowledgeEventBroadcastConfig EventBroadcastConfig;

	/** Interval in seconds between expiration checks for entries with finite Lifetime. */
	UPROPERTY(EditAnywhere, Category = "Configuration")
	float ExpirationTickInterval = 5.0f;

	/** Grid cell size for the spatial hash in world units. Larger = fewer cells, less precision. */
	UPROPERTY(EditAnywhere, Category = "Configuration")
	float SpatialHashCellSize = 1000.0f;

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

	// Gather candidate handles. Uses spatial hash when a spatial radius hint is available.
	void GatherCandidates(const FGameplayTagQuery& TagQuery, const FArcKnowledgeQueryContext& Context, float SpatialRadiusHint, TArray<FArcKnowledgeHandle>& OutCandidates) const;

	// Fire a knowledge event through the broadcaster
	void BroadcastKnowledgeEvent(EArcKnowledgeEventType Type, const FArcKnowledgeEntry& Entry);

	// ---------- Storage ----------

	/** All knowledge entries, keyed by handle. */
	TMap<FArcKnowledgeHandle, FArcKnowledgeEntry> Entries;

	/** Tag -> handles index for fast query lookups. */
	TMap<FGameplayTag, TArray<FArcKnowledgeHandle>> TagIndex;

	/** Source entity -> knowledge handles index.
	  * Only populated for entries whose SourceEntity is valid. */
	TMap<FMassEntityHandle, TArray<FArcKnowledgeHandle>> SourceEntityIndex;

	/** Spatial hash for location-based queries. Maintained inline during CRUD. */
	FArcKnowledgeSpatialHash SpatialHash;

	/** Time accumulator for expiration checks. */
	float ExpirationTimeAccumulator = 0.0f;

	/** Event broadcaster for knowledge change notifications. */
	FArcKnowledgeEventBroadcaster EventBroadcaster;
};
