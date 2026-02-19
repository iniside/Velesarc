// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MassEntityHandle.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTypes.h"
#include "MassEntityConcepts.h"
#include "MassProcessor.h"
#include "MassSignalProcessorBase.h"
#include "Subsystems/WorldSubsystem.h"

#include "ArcMassDecayingTags.generated.h"

// ---------------------------------------------------------------------------
// Signal namespace
// ---------------------------------------------------------------------------

namespace UE::ArcMass::Signals
{
	const FName DecayingTagAdded = FName(TEXT("DecayingTagAdded"));
}

// ---------------------------------------------------------------------------
// Pending request (non-USTRUCT, internal queue item)
// ---------------------------------------------------------------------------

struct FArcMassDecayingTagRequest
{
	FMassEntityHandle Entity;
	FGameplayTag Tag;
	float Duration = 0.f;

	/** Optional influence grid bridge — set GridIndex to INDEX_NONE to skip. */
	int32 InfluenceGridIndex = INDEX_NONE;
	int32 InfluenceChannel = INDEX_NONE;
	float InfluenceStrength = 0.f;
};

// ---------------------------------------------------------------------------
// Tag entry (individual decaying tag on an entity)
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct ARCMASS_API FArcMassDecayingTagEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DecayingTags")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DecayingTags")
	float RemainingTime = 0.f;

	/** Original duration when the tag was added/reset. Useful for computing normalized decay ratio. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DecayingTags")
	float InitialDuration = 0.f;
};

// ---------------------------------------------------------------------------
// Fragment
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcMassDecayingTagFragment : public FMassFragment
{
	GENERATED_BODY()

	TArray<FArcMassDecayingTagEntry> Entries;

	FArcMassDecayingTagEntry* FindEntry(const FGameplayTag& Tag);
	const FArcMassDecayingTagEntry* FindEntry(const FGameplayTag& Tag) const;

	bool HasTag(const FGameplayTag& Tag) const;
	float GetRemainingTime(const FGameplayTag& Tag) const;

	/** Add a new tag or reset the timer if it already exists. */
	void AddOrResetTag(const FGameplayTag& Tag, float Duration);

	/** Remove entries whose RemainingTime <= 0. Returns number removed. */
	int32 RemoveExpiredEntries();
};

template<>
struct TMassFragmentTraits<FArcMassDecayingTagFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// ---------------------------------------------------------------------------
// Subsystem — request queue + query API
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMassDecayingTagSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// -- Mutation API (enqueues request + raises signal) ---------------------

	/** Request adding (or resetting) a decaying gameplay tag on an entity. */
	UFUNCTION(BlueprintCallable, Category = "ArcMass|DecayingTags")
	void RequestAddDecayingTag(const FMassEntityHandle& Entity, FGameplayTag Tag, float Duration);

	/** C++ overload with optional influence grid bridge. */
	void RequestAddDecayingTag(const FMassEntityHandle& Entity, FGameplayTag Tag, float Duration,
		int32 InfluenceGridIndex, int32 InfluenceChannel, float InfluenceStrength);

	// -- Query API ----------------------------------------------------------

	/** Check if an entity currently has a specific decaying tag. */
	UFUNCTION(BlueprintCallable, Category = "ArcMass|DecayingTags", meta = (WorldContext = "WorldContextObject"))
	static bool EntityHasDecayingTag(const UObject* WorldContextObject, const FMassEntityHandle& Entity, FGameplayTag Tag);

	/** Get remaining time for a decaying tag. Returns 0 if not found. */
	UFUNCTION(BlueprintCallable, Category = "ArcMass|DecayingTags", meta = (WorldContext = "WorldContextObject"))
	static float GetDecayingTagRemainingTime(const UObject* WorldContextObject, const FMassEntityHandle& Entity, FGameplayTag Tag);

	/** Get all active decaying tags on an entity. */
	UFUNCTION(BlueprintCallable, Category = "ArcMass|DecayingTags", meta = (WorldContext = "WorldContextObject"))
	static TArray<FGameplayTag> GetAllDecayingTags(const UObject* WorldContextObject, const FMassEntityHandle& Entity);

	// -- Internal (used by signal processor) --------------------------------

	TArray<FArcMassDecayingTagRequest>& GetPendingRequests() { return PendingRequests; }

private:
	TArray<FArcMassDecayingTagRequest> PendingRequests;
};

// ---------------------------------------------------------------------------
// Signal processor — handles add/reset requests
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMassDecayingTagSignalProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcMassDecayingTagSignalProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context,
		FMassSignalNameLookup& EntitySignals) override;
};

// ---------------------------------------------------------------------------
// Decay processor — ticks down timers, removes expired entries
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMassDecayingTagDecayProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcMassDecayingTagDecayProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;

#if WITH_GAMEPLAY_DEBUGGER
	FMassEntityQuery DebugQuery;
#endif
};

// ---------------------------------------------------------------------------
// Trait — opts entities into the decaying tags system
// ---------------------------------------------------------------------------

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Decaying Tags"))
class ARCMASS_API UArcMassDecayingTagTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
