// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeFilter.h"
#include "ArcKnowledgeScorer.h"
#include "ArcKnowledgeQueryDefinition.h"
#include "ArcKnowledgeEntryDefinition.h"
#include "AsyncMessageWorldSubsystem.h"
#include "MassEntityHandle.h"
#include "Engine/World.h"

DECLARE_STATS_GROUP(TEXT("ArcKnowledge"), STATGROUP_ArcKnowledge, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("QueryKnowledge"), STAT_ArcKnowledge_QueryKnowledge, STATGROUP_ArcKnowledge);
DECLARE_CYCLE_STAT(TEXT("Tick"), STAT_ArcKnowledge_Tick, STATGROUP_ArcKnowledge);

// ====================================================================
// USubsystem
// ====================================================================

void UArcKnowledgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	SpatialHash.CellSize = SpatialHashCellSize;

	UAsyncMessageWorldSubsystem* AsyncMessage = Collection.InitializeDependency<UAsyncMessageWorldSubsystem>();
	EventBroadcaster.Initialize(AsyncMessage, EventBroadcastConfig);
}

void UArcKnowledgeSubsystem::Deinitialize()
{
	EventBroadcaster.Shutdown();

	SpatialHash.Clear();
	Entries.Empty();
	TagIndex.Empty();
	SourceEntityIndex.Empty();

	Super::Deinitialize();
}

// ====================================================================
// Tick
// ====================================================================

void UArcKnowledgeSubsystem::Tick(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_ArcKnowledge_Tick);

	ExpirationTimeAccumulator += DeltaTime;
	if (ExpirationTimeAccumulator < ExpirationTickInterval)
	{
		return;
	}
	ExpirationTimeAccumulator = 0.0f;

	const double CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	TArray<FArcKnowledgeHandle> ToRemove;

	for (const auto& Pair : Entries)
	{
		if (Pair.Value.Lifetime > 0.0f && (CurrentTime - Pair.Value.Timestamp) >= static_cast<double>(Pair.Value.Lifetime))
		{
			ToRemove.Add(Pair.Key);
		}
	}

	for (const FArcKnowledgeHandle& Handle : ToRemove)
	{
		RemoveKnowledge(Handle);
	}
}

TStatId UArcKnowledgeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArcKnowledgeSubsystem, STATGROUP_ArcKnowledge);
}

// ====================================================================
// Knowledge Index
// ====================================================================

FArcKnowledgeHandle UArcKnowledgeSubsystem::RegisterKnowledge(FArcKnowledgeEntry& Entry)
{
	const FArcKnowledgeHandle Handle = FArcKnowledgeHandle::Make();
	Entry.Handle = Handle;

	if (Entry.Timestamp == 0.0 && GetWorld())
	{
		Entry.Timestamp = GetWorld()->GetTimeSeconds();
	}

	Entries.Add(Handle, Entry);
	AddToTagIndex(Handle, Entry.Tags);
	SpatialHash.Add(Handle, Entry.Location);

	if (Entry.SourceEntity.IsValid())
	{
		SourceEntityIndex.FindOrAdd(Entry.SourceEntity).Add(Handle);
	}

	BroadcastKnowledgeEvent(EArcKnowledgeEventType::Registered, Entry);

	return Handle;
}

void UArcKnowledgeSubsystem::UpdateKnowledge(FArcKnowledgeHandle Handle, const FArcKnowledgeEntry& Updated)
{
	FArcKnowledgeEntry* Existing = Entries.Find(Handle);
	if (!Existing)
	{
		return;
	}

	const FVector OldLocation = Existing->Location;

	// Remove old tag index entries
	RemoveFromTagIndex(Handle, Existing->Tags);

	// Remove old source entity index
	if (Existing->SourceEntity.IsValid())
	{
		if (TArray<FArcKnowledgeHandle>* SourceEntries = SourceEntityIndex.Find(Existing->SourceEntity))
		{
			SourceEntries->Remove(Handle);
			if (SourceEntries->IsEmpty())
			{
				SourceEntityIndex.Remove(Existing->SourceEntity);
			}
		}
	}

	// Update
	*Existing = Updated;
	Existing->Handle = Handle;

	// Re-add to indices
	AddToTagIndex(Handle, Existing->Tags);
	SpatialHash.Update(Handle, OldLocation, Existing->Location);

	if (Existing->SourceEntity.IsValid())
	{
		SourceEntityIndex.FindOrAdd(Existing->SourceEntity).Add(Handle);
	}

	BroadcastKnowledgeEvent(EArcKnowledgeEventType::Updated, *Existing);
}

void UArcKnowledgeSubsystem::RemoveKnowledge(FArcKnowledgeHandle Handle)
{
	const FArcKnowledgeEntry* Entry = Entries.Find(Handle);
	if (!Entry)
	{
		return;
	}

	// Capture data before deletion for the event broadcast
	BroadcastKnowledgeEvent(EArcKnowledgeEventType::Removed, *Entry);

	RemoveFromTagIndex(Handle, Entry->Tags);
	SpatialHash.Remove(Handle, Entry->Location);

	if (Entry->SourceEntity.IsValid())
	{
		if (TArray<FArcKnowledgeHandle>* SourceEntries = SourceEntityIndex.Find(Entry->SourceEntity))
		{
			SourceEntries->Remove(Handle);
			if (SourceEntries->IsEmpty())
			{
				SourceEntityIndex.Remove(Entry->SourceEntity);
			}
		}
	}

	Entries.Remove(Handle);
}

void UArcKnowledgeSubsystem::RegisterKnowledgeBatch(TArray<FArcKnowledgeEntry>& InEntries, TArray<FArcKnowledgeHandle>& OutHandles)
{
	OutHandles.Reserve(InEntries.Num());
	for (FArcKnowledgeEntry& Entry : InEntries)
	{
		OutHandles.Add(RegisterKnowledge(Entry));
	}
}

void UArcKnowledgeSubsystem::RegisterFromDefinition(const UArcKnowledgeEntryDefinition* Definition, const FVector& Location, FMassEntityHandle SourceEntity, TArray<FArcKnowledgeHandle>& OutHandles)
{
	if (!Definition)
	{
		return;
	}

	// Register primary entry from definition tags
	if (!Definition->Tags.IsEmpty())
	{
		FArcKnowledgeEntry PrimaryEntry;
		PrimaryEntry.Tags = Definition->Tags;
		PrimaryEntry.Location = Location;
		PrimaryEntry.Payload = Definition->Payload;
		PrimaryEntry.SpatialBroadcastRadius = Definition->SpatialBroadcastRadius;
		PrimaryEntry.SourceEntity = SourceEntity;
		OutHandles.Add(RegisterKnowledge(PrimaryEntry));
	}

	// Register all initial knowledge entries from the definition
	for (FArcKnowledgeEntry InitEntry : Definition->InitialKnowledge)
	{
		if (InitEntry.Location.IsZero())
		{
			InitEntry.Location = Location;
		}
		if (SourceEntity.IsValid() && !InitEntry.SourceEntity.IsValid())
		{
			InitEntry.SourceEntity = SourceEntity;
		}
		OutHandles.Add(RegisterKnowledge(InitEntry));
	}
}

void UArcKnowledgeSubsystem::RefreshKnowledge(FArcKnowledgeHandle Handle, float NewRelevance)
{
	FArcKnowledgeEntry* Entry = Entries.Find(Handle);
	if (!Entry)
	{
		return;
	}

	Entry->Relevance = FMath::Clamp(NewRelevance, 0.0f, 1.0f);
	if (GetWorld())
	{
		Entry->Timestamp = GetWorld()->GetTimeSeconds();
	}
}

const FArcKnowledgeEntry* UArcKnowledgeSubsystem::GetKnowledgeEntry(FArcKnowledgeHandle Handle) const
{
	return Entries.Find(Handle);
}

// ====================================================================
// Knowledge Mutation
// ====================================================================

void UArcKnowledgeSubsystem::AddTagToKnowledge(FArcKnowledgeHandle Handle, FGameplayTag Tag)
{
	FArcKnowledgeEntry* Entry = Entries.Find(Handle);
	if (!Entry || !Tag.IsValid())
	{
		return;
	}

	if (Entry->Tags.HasTagExact(Tag))
	{
		return;
	}

	Entry->Tags.AddTag(Tag);
	TagIndex.FindOrAdd(Tag).AddUnique(Handle);

	BroadcastKnowledgeEvent(EArcKnowledgeEventType::Updated, *Entry);
}

void UArcKnowledgeSubsystem::RemoveTagFromKnowledge(FArcKnowledgeHandle Handle, FGameplayTag Tag)
{
	FArcKnowledgeEntry* Entry = Entries.Find(Handle);
	if (!Entry || !Tag.IsValid())
	{
		return;
	}

	if (!Entry->Tags.HasTagExact(Tag))
	{
		return;
	}

	Entry->Tags.RemoveTag(Tag);

	if (TArray<FArcKnowledgeHandle>* Handles = TagIndex.Find(Tag))
	{
		Handles->Remove(Handle);
		if (Handles->IsEmpty())
		{
			TagIndex.Remove(Tag);
		}
	}

	BroadcastKnowledgeEvent(EArcKnowledgeEventType::Updated, *Entry);
}

void UArcKnowledgeSubsystem::SetKnowledgeTags(FArcKnowledgeHandle Handle, const FGameplayTagContainer& NewTags)
{
	FArcKnowledgeEntry* Entry = Entries.Find(Handle);
	if (!Entry)
	{
		return;
	}

	RemoveFromTagIndex(Handle, Entry->Tags);
	Entry->Tags = NewTags;
	AddToTagIndex(Handle, Entry->Tags);

	BroadcastKnowledgeEvent(EArcKnowledgeEventType::Updated, *Entry);
}

void UArcKnowledgeSubsystem::SetKnowledgePayload(FArcKnowledgeHandle Handle, const FInstancedStruct& NewPayload)
{
	FArcKnowledgeEntry* Entry = Entries.Find(Handle);
	if (!Entry)
	{
		return;
	}

	Entry->Payload = NewPayload;

	if (GetWorld())
	{
		Entry->Timestamp = GetWorld()->GetTimeSeconds();
	}

	BroadcastKnowledgeEvent(EArcKnowledgeEventType::Updated, *Entry);
}

// ====================================================================
// Source Entity Index
// ====================================================================

void UArcKnowledgeSubsystem::RemoveKnowledgeBySource(FMassEntityHandle SourceEntity)
{
	const TArray<FArcKnowledgeHandle>* Handles = SourceEntityIndex.Find(SourceEntity);
	if (!Handles)
	{
		return;
	}

	// Copy to avoid mutation during iteration (RemoveKnowledge modifies SourceEntityIndex)
	TArray<FArcKnowledgeHandle> HandlesCopy = *Handles;
	for (const FArcKnowledgeHandle& Handle : HandlesCopy)
	{
		RemoveKnowledge(Handle);
	}
}

void UArcKnowledgeSubsystem::GetKnowledgeBySource(FMassEntityHandle SourceEntity, TArray<FArcKnowledgeHandle>& OutHandles) const
{
	if (const TArray<FArcKnowledgeHandle>* Handles = SourceEntityIndex.Find(SourceEntity))
	{
		OutHandles = *Handles;
	}
}

// ====================================================================
// Tag Index
// ====================================================================

void UArcKnowledgeSubsystem::AddToTagIndex(FArcKnowledgeHandle Handle, const FGameplayTagContainer& Tags)
{
	for (const FGameplayTag& Tag : Tags)
	{
		TagIndex.FindOrAdd(Tag).AddUnique(Handle);
	}
}

void UArcKnowledgeSubsystem::RemoveFromTagIndex(FArcKnowledgeHandle Handle, const FGameplayTagContainer& Tags)
{
	for (const FGameplayTag& Tag : Tags)
	{
		if (TArray<FArcKnowledgeHandle>* Handles = TagIndex.Find(Tag))
		{
			Handles->Remove(Handle);
			if (Handles->IsEmpty())
			{
				TagIndex.Remove(Tag);
			}
		}
	}
}

// ====================================================================
// Knowledge Queries
// ====================================================================

void UArcKnowledgeSubsystem::QueryKnowledge(const FArcKnowledgeQuery& Query, const FArcKnowledgeQueryContext& Context, TArray<FArcKnowledgeQueryResult>& OutResults) const
{
	ExecuteQuery(Query.TagQuery, Query.Filters, Query.Scorers, Query.MaxResults, Query.SelectionMode, Context, OutResults);
}

void UArcKnowledgeSubsystem::QueryKnowledgeFromDefinition(const UArcKnowledgeQueryDefinition* Definition, const FArcKnowledgeQueryContext& Context, TArray<FArcKnowledgeQueryResult>& OutResults) const
{
	if (!Definition)
	{
		return;
	}

	ExecuteQuery(Definition->TagQuery, Definition->Filters, Definition->Scorers, Definition->MaxResults, Definition->SelectionMode, Context, OutResults);
}

void UArcKnowledgeSubsystem::GatherCandidates(const FGameplayTagQuery& TagQuery, const FArcKnowledgeQueryContext& Context, float SpatialRadiusHint, TArray<FArcKnowledgeHandle>& OutCandidates) const
{
	const bool bHasSpatialHint = SpatialRadiusHint > 0.0f && !Context.QueryOrigin.IsZero();

	if (bHasSpatialHint)
	{
		// Use spatial hash for initial candidate set, then verify tags
		TArray<FArcKnowledgeHandle> SpatialCandidates;
		SpatialHash.QuerySphere(Context.QueryOrigin, SpatialRadiusHint, SpatialCandidates);

		if (TagQuery.IsEmpty())
		{
			OutCandidates = MoveTemp(SpatialCandidates);
		}
		else
		{
			OutCandidates.Reserve(SpatialCandidates.Num());
			for (const FArcKnowledgeHandle& Handle : SpatialCandidates)
			{
				if (const FArcKnowledgeEntry* Entry = Entries.Find(Handle))
				{
					if (TagQuery.Matches(Entry->Tags))
					{
						OutCandidates.Add(Handle);
					}
				}
			}
		}
		return;
	}

	if (TagQuery.IsEmpty())
	{
		// No tag filter, no spatial hint — all entries are candidates
		Entries.GetKeys(OutCandidates);
		return;
	}

	// Tag-only query — iterate all entries and check tags
	OutCandidates.Reserve(Entries.Num());
	for (const auto& Pair : Entries)
	{
		if (TagQuery.Matches(Pair.Value.Tags))
		{
			OutCandidates.Add(Pair.Key);
		}
	}
}

void UArcKnowledgeSubsystem::ExecuteQuery(
	const FGameplayTagQuery& TagQuery,
	const TArray<FInstancedStruct>& Filters,
	const TArray<FInstancedStruct>& Scorers,
	int32 MaxResults,
	EArcKnowledgeSelectionMode SelectionMode,
	const FArcKnowledgeQueryContext& Context,
	TArray<FArcKnowledgeQueryResult>& OutResults) const
{
	SCOPE_CYCLE_COUNTER(STAT_ArcKnowledge_QueryKnowledge);

	OutResults.Reset();

	// Extract spatial radius hint from filters for spatial pre-filtering
	float SpatialRadiusHint = 0.0f;
	for (const FInstancedStruct& FilterStruct : Filters)
	{
		if (const FArcKnowledgeFilter* Filter = FilterStruct.GetPtr<FArcKnowledgeFilter>())
		{
			const float Radius = Filter->GetSpatialRadius();
			if (Radius > 0.0f)
			{
				// Use the smallest spatial radius for tightest bounds
				SpatialRadiusHint = (SpatialRadiusHint > 0.0f) ? FMath::Min(SpatialRadiusHint, Radius) : Radius;
			}
		}
	}

	// Phase 1: Gather candidates via tag matching + spatial pre-filtering
	TArray<FArcKnowledgeHandle> Candidates;
	GatherCandidates(TagQuery, Context, SpatialRadiusHint, Candidates);

	if (Candidates.IsEmpty())
	{
		return;
	}

	// Phase 2: Filter
	TArray<FArcKnowledgeQueryResult> ScoredResults;
	ScoredResults.Reserve(Candidates.Num());

	for (const FArcKnowledgeHandle& Handle : Candidates)
	{
		const FArcKnowledgeEntry* Entry = Entries.Find(Handle);
		if (!Entry)
		{
			continue;
		}

		bool bPassedFilters = true;
		for (const FInstancedStruct& FilterStruct : Filters)
		{
			if (const FArcKnowledgeFilter* Filter = FilterStruct.GetPtr<FArcKnowledgeFilter>())
			{
				if (!Filter->PassesFilter(*Entry, Context))
				{
					bPassedFilters = false;
					break;
				}
			}
		}

		if (!bPassedFilters)
		{
			continue;
		}

		// Phase 3: Score
		float FinalScore = 1.0f;
		for (const FInstancedStruct& ScorerStruct : Scorers)
		{
			if (const FArcKnowledgeScorer* Scorer = ScorerStruct.GetPtr<FArcKnowledgeScorer>())
			{
				float RawScore = FMath::Clamp(Scorer->Score(*Entry, Context), 0.0f, 1.0f);

				// Apply weight exponent (compensatory scoring)
				if (Scorer->Weight != 1.0f)
				{
					RawScore = FMath::Pow(RawScore, Scorer->Weight);
				}

				FinalScore *= RawScore;
			}
		}

		if (FinalScore > 0.0f)
		{
			FArcKnowledgeQueryResult& Result = ScoredResults.AddDefaulted_GetRef();
			Result.Entry = *Entry;
			Result.Score = FinalScore;
		}
	}

	if (ScoredResults.IsEmpty())
	{
		return;
	}

	// Phase 4: Selection
	switch (SelectionMode)
	{
	case EArcKnowledgeSelectionMode::HighestScore:
	case EArcKnowledgeSelectionMode::TopN:
	{
		ScoredResults.Sort([](const FArcKnowledgeQueryResult& A, const FArcKnowledgeQueryResult& B)
		{
			return A.Score > B.Score;
		});
		const int32 Count = FMath::Min(MaxResults, ScoredResults.Num());
		OutResults.Append(ScoredResults.GetData(), Count);
		break;
	}
	case EArcKnowledgeSelectionMode::RandomWeighted:
	{
		// Weighted random selection without replacement
		float TotalWeight = 0.0f;
		for (const FArcKnowledgeQueryResult& R : ScoredResults)
		{
			TotalWeight += R.Score;
		}

		const int32 Count = FMath::Min(MaxResults, ScoredResults.Num());
		for (int32 i = 0; i < Count && !ScoredResults.IsEmpty(); ++i)
		{
			float Roll = FMath::FRand() * TotalWeight;
			int32 SelectedIndex = 0;
			for (int32 j = 0; j < ScoredResults.Num(); ++j)
			{
				Roll -= ScoredResults[j].Score;
				if (Roll <= 0.0f)
				{
					SelectedIndex = j;
					break;
				}
			}
			OutResults.Add(ScoredResults[SelectedIndex]);
			TotalWeight -= ScoredResults[SelectedIndex].Score;
			ScoredResults.RemoveAtSwap(SelectedIndex);
		}
		break;
	}
	}
}

// ====================================================================
// Advertisements
// ====================================================================

FArcKnowledgeHandle UArcKnowledgeSubsystem::PostAdvertisement(FArcKnowledgeEntry& AdvertisementEntry)
{
	const FArcKnowledgeHandle Handle = RegisterKnowledge(AdvertisementEntry);

	// Fire AdvertisementPosted in addition to the Registered event from RegisterKnowledge
	if (const FArcKnowledgeEntry* Entry = Entries.Find(Handle))
	{
		BroadcastKnowledgeEvent(EArcKnowledgeEventType::AdvertisementPosted, *Entry);
	}

	return Handle;
}

bool UArcKnowledgeSubsystem::ClaimAdvertisement(FArcKnowledgeHandle Handle, FMassEntityHandle Claimer)
{
	FArcKnowledgeEntry* Entry = Entries.Find(Handle);
	if (!Entry || Entry->bClaimed)
	{
		return false;
	}

	Entry->bClaimed = true;
	Entry->ClaimedBy = Claimer;

	BroadcastKnowledgeEvent(EArcKnowledgeEventType::AdvertisementClaimed, *Entry);

	return true;
}

void UArcKnowledgeSubsystem::CompleteAdvertisement(FArcKnowledgeHandle Handle)
{
	// Fire AdvertisementCompleted before RemoveKnowledge (which fires Removed and deletes)
	if (const FArcKnowledgeEntry* Entry = Entries.Find(Handle))
	{
		BroadcastKnowledgeEvent(EArcKnowledgeEventType::AdvertisementCompleted, *Entry);
	}

	RemoveKnowledge(Handle);
}

void UArcKnowledgeSubsystem::CancelAdvertisement(FArcKnowledgeHandle Handle)
{
	FArcKnowledgeEntry* Entry = Entries.Find(Handle);
	if (!Entry)
	{
		return;
	}

	Entry->bClaimed = false;
	Entry->ClaimedBy = FMassEntityHandle();
}

// ====================================================================
// Events
// ====================================================================

void UArcKnowledgeSubsystem::BroadcastKnowledgeEvent(EArcKnowledgeEventType Type, const FArcKnowledgeEntry& Entry)
{
	FArcKnowledgeChangedEvent Event;
	Event.EventType = Type;
	Event.Handle = Entry.Handle;
	Event.Tags = Entry.Tags;
	Event.Location = Entry.Location;

	EventBroadcaster.BroadcastEvent(Event, Entry.SpatialBroadcastRadius);
}

// ====================================================================
// Spatial Queries
// ====================================================================

void UArcKnowledgeSubsystem::QueryKnowledgeInRadius(const FVector& Center, float Radius, TArray<FArcKnowledgeHandle>& OutHandles, const FGameplayTagQuery& OptionalTagFilter) const
{
	TArray<FArcKnowledgeHandle> SpatialResults;
	SpatialHash.QuerySphere(Center, Radius, SpatialResults);

	if (OptionalTagFilter.IsEmpty())
	{
		OutHandles = MoveTemp(SpatialResults);
		return;
	}

	OutHandles.Reserve(SpatialResults.Num());
	for (const FArcKnowledgeHandle& Handle : SpatialResults)
	{
		if (const FArcKnowledgeEntry* Entry = Entries.Find(Handle))
		{
			if (OptionalTagFilter.Matches(Entry->Tags))
			{
				OutHandles.Add(Handle);
			}
		}
	}
}

FArcKnowledgeHandle UArcKnowledgeSubsystem::FindNearestKnowledge(const FVector& Location, float MaxRadius, const FGameplayTagQuery& TagFilter) const
{
	TArray<FArcKnowledgeSpatialHash::FEntry> SpatialEntries;
	SpatialHash.QuerySphereWithDistance(Location, MaxRadius, SpatialEntries);

	FArcKnowledgeHandle BestHandle;
	float BestDistSq = TNumericLimits<float>::Max();

	for (const FArcKnowledgeSpatialHash::FEntry& SpatialEntry : SpatialEntries)
	{
		const float DistSq = FVector::DistSquared(Location, SpatialEntry.Location);
		if (DistSq >= BestDistSq)
		{
			continue;
		}

		if (!TagFilter.IsEmpty())
		{
			if (const FArcKnowledgeEntry* Entry = Entries.Find(SpatialEntry.Handle))
			{
				if (!TagFilter.Matches(Entry->Tags))
				{
					continue;
				}
			}
			else
			{
				continue;
			}
		}

		BestDistSq = DistSq;
		BestHandle = SpatialEntry.Handle;
	}

	return BestHandle;
}
