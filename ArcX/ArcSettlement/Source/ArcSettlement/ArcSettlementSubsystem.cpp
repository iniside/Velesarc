// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSettlementSubsystem.h"
#include "ArcKnowledgeFilter.h"
#include "ArcKnowledgeScorer.h"
#include "ArcKnowledgeQueryDefinition.h"
#include "ArcSettlementDefinition.h"
#include "ArcRegionDefinition.h"
#include "MassEntityHandle.h"
#include "Engine/World.h"

DECLARE_STATS_GROUP(TEXT("ArcSettlement"), STATGROUP_ArcSettlement, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("QueryKnowledge"), STAT_ArcSettlement_QueryKnowledge, STATGROUP_ArcSettlement);
DECLARE_CYCLE_STAT(TEXT("Tick"), STAT_ArcSettlement_Tick, STATGROUP_ArcSettlement);

// ====================================================================
// USubsystem
// ====================================================================

void UArcSettlementSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UArcSettlementSubsystem::Deinitialize()
{
	Entries.Empty();
	TagIndex.Empty();
	SettlementKnowledgeIndex.Empty();
	SourceEntityIndex.Empty();
	Settlements.Empty();
	Regions.Empty();

	Super::Deinitialize();
}

// ====================================================================
// Tick
// ====================================================================

void UArcSettlementSubsystem::Tick(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_ArcSettlement_Tick);

	DecayTimeAccumulator += DeltaTime;
	if (DecayTimeAccumulator < DecayTickInterval)
	{
		return;
	}
	DecayTimeAccumulator = 0.0f;

	if (RelevanceDecayRate <= 0.0f)
	{
		return;
	}

	const float DecayAmount = RelevanceDecayRate * DecayTickInterval;
	TArray<FArcKnowledgeHandle> ToRemove;

	for (auto& Pair : Entries)
	{
		Pair.Value.Relevance -= DecayAmount;
		if (Pair.Value.Relevance < MinRelevanceThreshold)
		{
			ToRemove.Add(Pair.Key);
		}
	}

	for (const FArcKnowledgeHandle& Handle : ToRemove)
	{
		RemoveKnowledge(Handle);
	}
}

TStatId UArcSettlementSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArcSettlementSubsystem, STATGROUP_ArcSettlement);
}

// ====================================================================
// Knowledge Index
// ====================================================================

FArcKnowledgeHandle UArcSettlementSubsystem::RegisterKnowledge(FArcKnowledgeEntry& Entry)
{
	const FArcKnowledgeHandle Handle = FArcKnowledgeHandle::Make();
	Entry.Handle = Handle;

	if (Entry.Timestamp == 0.0 && GetWorld())
	{
		Entry.Timestamp = GetWorld()->GetTimeSeconds();
	}

	Entries.Add(Handle, Entry);
	AddToTagIndex(Handle, Entry.Tags);

	if (Entry.Settlement.IsValid())
	{
		SettlementKnowledgeIndex.FindOrAdd(Entry.Settlement).Add(Handle);
	}

	if (Entry.SourceEntity.IsValid())
	{
		SourceEntityIndex.FindOrAdd(Entry.SourceEntity).Add(Handle);
	}

	return Handle;
}

void UArcSettlementSubsystem::UpdateKnowledge(FArcKnowledgeHandle Handle, const FArcKnowledgeEntry& Updated)
{
	FArcKnowledgeEntry* Existing = Entries.Find(Handle);
	if (!Existing)
	{
		return;
	}

	// Remove old tag index entries
	RemoveFromTagIndex(Handle, Existing->Tags);

	// Remove old settlement index
	if (Existing->Settlement.IsValid())
	{
		if (TArray<FArcKnowledgeHandle>* SettlementEntries = SettlementKnowledgeIndex.Find(Existing->Settlement))
		{
			SettlementEntries->Remove(Handle);
		}
	}

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

	if (Existing->Settlement.IsValid())
	{
		SettlementKnowledgeIndex.FindOrAdd(Existing->Settlement).Add(Handle);
	}

	if (Existing->SourceEntity.IsValid())
	{
		SourceEntityIndex.FindOrAdd(Existing->SourceEntity).Add(Handle);
	}
}

void UArcSettlementSubsystem::RemoveKnowledge(FArcKnowledgeHandle Handle)
{
	const FArcKnowledgeEntry* Entry = Entries.Find(Handle);
	if (!Entry)
	{
		return;
	}

	RemoveFromTagIndex(Handle, Entry->Tags);

	if (Entry->Settlement.IsValid())
	{
		if (TArray<FArcKnowledgeHandle>* SettlementEntries = SettlementKnowledgeIndex.Find(Entry->Settlement))
		{
			SettlementEntries->Remove(Handle);
		}
	}

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

void UArcSettlementSubsystem::RegisterKnowledgeBatch(TArray<FArcKnowledgeEntry>& InEntries, TArray<FArcKnowledgeHandle>& OutHandles)
{
	OutHandles.Reserve(InEntries.Num());
	for (FArcKnowledgeEntry& Entry : InEntries)
	{
		OutHandles.Add(RegisterKnowledge(Entry));
	}
}

void UArcSettlementSubsystem::RefreshKnowledge(FArcKnowledgeHandle Handle, float NewRelevance)
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

const FArcKnowledgeEntry* UArcSettlementSubsystem::GetKnowledgeEntry(FArcKnowledgeHandle Handle) const
{
	return Entries.Find(Handle);
}

// ====================================================================
// Knowledge Mutation
// ====================================================================

void UArcSettlementSubsystem::AddTagToKnowledge(FArcKnowledgeHandle Handle, FGameplayTag Tag)
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
}

void UArcSettlementSubsystem::RemoveTagFromKnowledge(FArcKnowledgeHandle Handle, FGameplayTag Tag)
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
}

void UArcSettlementSubsystem::SetKnowledgeTags(FArcKnowledgeHandle Handle, const FGameplayTagContainer& NewTags)
{
	FArcKnowledgeEntry* Entry = Entries.Find(Handle);
	if (!Entry)
	{
		return;
	}

	RemoveFromTagIndex(Handle, Entry->Tags);
	Entry->Tags = NewTags;
	AddToTagIndex(Handle, Entry->Tags);
}

void UArcSettlementSubsystem::SetKnowledgePayload(FArcKnowledgeHandle Handle, const FInstancedStruct& NewPayload)
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
}

// ====================================================================
// Source Entity Index
// ====================================================================

void UArcSettlementSubsystem::RemoveKnowledgeBySource(FMassEntityHandle SourceEntity)
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

void UArcSettlementSubsystem::GetKnowledgeBySource(FMassEntityHandle SourceEntity, TArray<FArcKnowledgeHandle>& OutHandles) const
{
	if (const TArray<FArcKnowledgeHandle>* Handles = SourceEntityIndex.Find(SourceEntity))
	{
		OutHandles = *Handles;
	}
}

// ====================================================================
// Tag Index
// ====================================================================

void UArcSettlementSubsystem::AddToTagIndex(FArcKnowledgeHandle Handle, const FGameplayTagContainer& Tags)
{
	for (const FGameplayTag& Tag : Tags)
	{
		TagIndex.FindOrAdd(Tag).AddUnique(Handle);
	}
}

void UArcSettlementSubsystem::RemoveFromTagIndex(FArcKnowledgeHandle Handle, const FGameplayTagContainer& Tags)
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

void UArcSettlementSubsystem::QueryKnowledge(const FArcKnowledgeQuery& Query, const FArcKnowledgeQueryContext& Context, TArray<FArcKnowledgeQueryResult>& OutResults) const
{
	ExecuteQuery(Query.TagQuery, Query.Filters, Query.Scorers, Query.MaxResults, Query.SelectionMode, Context, OutResults);
}

void UArcSettlementSubsystem::QueryKnowledgeFromDefinition(const UArcKnowledgeQueryDefinition* Definition, const FArcKnowledgeQueryContext& Context, TArray<FArcKnowledgeQueryResult>& OutResults) const
{
	if (!Definition)
	{
		return;
	}

	ExecuteQuery(Definition->TagQuery, Definition->Filters, Definition->Scorers, Definition->MaxResults, Definition->SelectionMode, Context, OutResults);
}

void UArcSettlementSubsystem::GatherCandidates(const FGameplayTagQuery& TagQuery, TArray<FArcKnowledgeHandle>& OutCandidates) const
{
	if (TagQuery.IsEmpty())
	{
		// No tag filter — all entries are candidates
		Entries.GetKeys(OutCandidates);
		return;
	}

	// For tag queries, we need to check each entry's tags against the query.
	// The tag index helps us narrow down: gather entries that have at least one relevant tag,
	// then verify against the full query.
	// For small scale this is fine — iterate all entries.
	OutCandidates.Reserve(Entries.Num());
	for (const auto& Pair : Entries)
	{
		if (TagQuery.Matches(Pair.Value.Tags))
		{
			OutCandidates.Add(Pair.Key);
		}
	}
}

void UArcSettlementSubsystem::ExecuteQuery(
	const FGameplayTagQuery& TagQuery,
	const TArray<FInstancedStruct>& Filters,
	const TArray<FInstancedStruct>& Scorers,
	int32 MaxResults,
	EArcKnowledgeSelectionMode SelectionMode,
	const FArcKnowledgeQueryContext& Context,
	TArray<FArcKnowledgeQueryResult>& OutResults) const
{
	SCOPE_CYCLE_COUNTER(STAT_ArcSettlement_QueryKnowledge);

	OutResults.Reset();

	// Phase 1: Gather candidates via tag matching
	TArray<FArcKnowledgeHandle> Candidates;
	GatherCandidates(TagQuery, Candidates);

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
// Actions
// ====================================================================

FArcKnowledgeHandle UArcSettlementSubsystem::PostAction(FArcKnowledgeEntry& ActionEntry)
{
	return RegisterKnowledge(ActionEntry);
}

bool UArcSettlementSubsystem::ClaimAction(FArcKnowledgeHandle Handle, FMassEntityHandle Claimer)
{
	FArcKnowledgeEntry* Entry = Entries.Find(Handle);
	if (!Entry || Entry->bClaimed)
	{
		return false;
	}

	Entry->bClaimed = true;
	Entry->ClaimedBy = Claimer;
	return true;
}

void UArcSettlementSubsystem::CompleteAction(FArcKnowledgeHandle Handle)
{
	RemoveKnowledge(Handle);
}

void UArcSettlementSubsystem::CancelAction(FArcKnowledgeHandle Handle)
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
// Settlements
// ====================================================================

FArcSettlementHandle UArcSettlementSubsystem::CreateSettlement(const UArcSettlementDefinition* Definition, FVector Location)
{
	const FArcSettlementHandle Handle = FArcSettlementHandle::Make();

	FArcSettlementData Data;
	Data.Handle = Handle;
	Data.Location = Location;

	if (Definition)
	{
		Data.DefinitionId = Definition->GetPrimaryAssetId();
		Data.DisplayName = Definition->DisplayName;
		Data.Tags = Definition->SettlementTags;
		Data.BoundingRadius = Definition->BoundingRadius;
	}

	Settlements.Add(Handle, MoveTemp(Data));

	// Auto-assign to regions
	AssignSettlementToRegions(Handle, Location);

	// Register initial knowledge from definition
	if (Definition)
	{
		for (const FArcKnowledgeEntry& InitEntry : Definition->InitialKnowledge)
		{
			FArcKnowledgeEntry Entry = InitEntry;
			Entry.Settlement = Handle;
			if (Entry.Location.IsZero())
			{
				Entry.Location = Location;
			}
			RegisterKnowledge(Entry);
		}
	}

	return Handle;
}

void UArcSettlementSubsystem::DestroySettlement(FArcSettlementHandle Handle)
{
	RemoveKnowledgeForSettlement(Handle);

	// Remove from regions
	for (auto& RegionPair : Regions)
	{
		RegionPair.Value.Settlements.Remove(Handle);
	}

	Settlements.Remove(Handle);
	SettlementKnowledgeIndex.Remove(Handle);
}

bool UArcSettlementSubsystem::GetSettlementData(FArcSettlementHandle Handle, FArcSettlementData& OutData) const
{
	if (const FArcSettlementData* Data = Settlements.Find(Handle))
	{
		OutData = *Data;
		return true;
	}
	return false;
}

FArcSettlementHandle UArcSettlementSubsystem::FindSettlementAt(const FVector& WorldLocation) const
{
	float BestDistSq = MAX_FLT;
	FArcSettlementHandle BestHandle;

	for (const auto& Pair : Settlements)
	{
		const float DistSq = FVector::DistSquared(WorldLocation, Pair.Value.Location);
		if (DistSq <= FMath::Square(Pair.Value.BoundingRadius) && DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestHandle = Pair.Key;
		}
	}

	return BestHandle;
}

void UArcSettlementSubsystem::QuerySettlementsInRadius(const FVector& Center, float Radius, TArray<FArcSettlementHandle>& OutHandles) const
{
	const float RadiusSq = FMath::Square(Radius);
	for (const auto& Pair : Settlements)
	{
		if (FVector::DistSquared(Center, Pair.Value.Location) <= RadiusSq)
		{
			OutHandles.Add(Pair.Key);
		}
	}
}

void UArcSettlementSubsystem::RemoveKnowledgeForSettlement(FArcSettlementHandle SettlementHandle)
{
	if (const TArray<FArcKnowledgeHandle>* KnowledgeHandles = SettlementKnowledgeIndex.Find(SettlementHandle))
	{
		// Copy to avoid mutation during iteration
		TArray<FArcKnowledgeHandle> HandlesCopy = *KnowledgeHandles;
		for (const FArcKnowledgeHandle& Handle : HandlesCopy)
		{
			RemoveKnowledge(Handle);
		}
	}
}

void UArcSettlementSubsystem::AssignSettlementToRegions(FArcSettlementHandle SettlementHandle, const FVector& Location)
{
	for (auto& RegionPair : Regions)
	{
		FArcRegionData& Region = RegionPair.Value;
		if (FVector::DistSquared(Location, Region.Center) <= FMath::Square(Region.Radius))
		{
			Region.Settlements.AddUnique(SettlementHandle);

			if (FArcSettlementData* Settlement = Settlements.Find(SettlementHandle))
			{
				Settlement->Region = RegionPair.Key;
			}
		}
	}
}

// ====================================================================
// Regions
// ====================================================================

FArcRegionHandle UArcSettlementSubsystem::CreateRegion(const UArcRegionDefinition* Definition, FVector Center, float Radius)
{
	const FArcRegionHandle Handle = FArcRegionHandle::Make();

	FArcRegionData Data;
	Data.Handle = Handle;
	Data.Center = Center;
	Data.Radius = Radius;

	if (Definition)
	{
		Data.DefinitionId = Definition->GetPrimaryAssetId();
		Data.DisplayName = Definition->DisplayName;
	}

	Regions.Add(Handle, MoveTemp(Data));

	// Check existing settlements for auto-assignment
	for (const auto& SettlementPair : Settlements)
	{
		if (FVector::DistSquared(SettlementPair.Value.Location, Center) <= FMath::Square(Radius))
		{
			Regions[Handle].Settlements.AddUnique(SettlementPair.Key);
		}
	}

	return Handle;
}

void UArcSettlementSubsystem::DestroyRegion(FArcRegionHandle Handle)
{
	if (const FArcRegionData* Region = Regions.Find(Handle))
	{
		// Clear region reference from settlements
		for (const FArcSettlementHandle& SettlementHandle : Region->Settlements)
		{
			if (FArcSettlementData* Settlement = Settlements.Find(SettlementHandle))
			{
				if (Settlement->Region == Handle)
				{
					Settlement->Region = FArcRegionHandle();
				}
			}
		}
	}

	Regions.Remove(Handle);
}

bool UArcSettlementSubsystem::GetRegionData(FArcRegionHandle Handle, FArcRegionData& OutData) const
{
	if (const FArcRegionData* Data = Regions.Find(Handle))
	{
		OutData = *Data;
		return true;
	}
	return false;
}

FArcRegionHandle UArcSettlementSubsystem::FindRegionForSettlement(FArcSettlementHandle SettlementHandle) const
{
	if (const FArcSettlementData* Settlement = Settlements.Find(SettlementHandle))
	{
		return Settlement->Region;
	}
	return FArcRegionHandle();
}
