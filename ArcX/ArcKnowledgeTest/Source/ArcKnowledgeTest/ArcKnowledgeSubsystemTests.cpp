// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

#include "NativeGameplayTags.h"
#include "InstancedStruct.h"

#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntry.h"
#include "ArcKnowledgeQuery.h"
#include "ArcKnowledgeFilter.h"
#include "ArcKnowledgeScorer.h"
#include "ArcKnowledgePayload.h"
#include "ArcKnowledgeTestPayload.h"

// ---- Test gameplay tags ----
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Resource_Iron, "Test.Resource.Iron");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Resource_Wood, "Test.Resource.Wood");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Resource_Stone, "Test.Resource.Stone");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Event_Robbery, "Test.Event.Robbery");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Quest_Deliver, "Test.Quest.Deliver");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Area_Vacancy, "Test.Area.Vacancy");

// ===================================================================
// Helpers
// ===================================================================

namespace ArcKnowledgeTestHelpers
{
	UArcKnowledgeSubsystem* GetSubsystem(FActorTestSpawner& Spawner)
	{
		UWorld* World = &Spawner.GetWorld();
		return World ? World->GetSubsystem<UArcKnowledgeSubsystem>() : nullptr;
	}

	FArcKnowledgeEntry MakeEntry(FGameplayTag Tag, FVector Location = FVector::ZeroVector, float Relevance = 1.0f)
	{
		FArcKnowledgeEntry Entry;
		Entry.Tags.AddTag(Tag);
		Entry.Location = Location;
		Entry.Relevance = Relevance;
		return Entry;
	}

	FArcKnowledgeEntry MakeEntryMultiTag(const FGameplayTagContainer& Tags, FVector Location = FVector::ZeroVector, float Relevance = 1.0f)
	{
		FArcKnowledgeEntry Entry;
		Entry.Tags = Tags;
		Entry.Location = Location;
		Entry.Relevance = Relevance;
		return Entry;
	}

	FArcKnowledgeQueryContext MakeContext(FVector Origin = FVector::ZeroVector, double Time = 100.0)
	{
		FArcKnowledgeQueryContext Ctx;
		Ctx.QueryOrigin = Origin;
		Ctx.CurrentTime = Time;
		return Ctx;
	}

}

// ===================================================================
// Knowledge CRUD — Register, Update, Remove, Batch, Refresh
// ===================================================================

TEST_CLASS(ArcKnowledge_KnowledgeCRUD, "ArcKnowledge.Knowledge.CRUD")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(Register_ReturnsValidHandle)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Entry = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(Entry);

		ASSERT_THAT(IsTrue(Handle.IsValid(), TEXT("RegisterKnowledge should return a valid handle")));
	}

	TEST_METHOD(Register_EntryRetrievable)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Entry = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(100, 200, 0));
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(Entry);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved, TEXT("Registered entry should be retrievable")));
		ASSERT_THAT(IsTrue(Retrieved->Tags.HasTagExact(TAG_Test_Resource_Iron)));
		ASSERT_THAT(IsNear(100.0f, static_cast<float>(Retrieved->Location.X), 0.01f));
	}

	TEST_METHOD(Register_HandleStoredOnEntry)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Entry = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(Entry);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(AreEqual(Handle, Retrieved->Handle, TEXT("Entry's handle should match returned handle")));
	}

	TEST_METHOD(Register_MultipleEntriesUnique)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Entry1 = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeEntry Entry2 = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood);
		FArcKnowledgeHandle H1 = Sub->RegisterKnowledge(Entry1);
		FArcKnowledgeHandle H2 = Sub->RegisterKnowledge(Entry2);

		ASSERT_THAT(AreNotEqual(H1, H2, TEXT("Different registrations should produce different handles")));
	}

	TEST_METHOD(Remove_EntryNoLongerRetrievable)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Entry = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(Entry);
		Sub->RemoveKnowledge(Handle);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNull(Retrieved, TEXT("Removed entry should not be retrievable")));
	}

	TEST_METHOD(Remove_InvalidHandle_NoEffect)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		// Should not crash
		Sub->RemoveKnowledge(FArcKnowledgeHandle());
		Sub->RemoveKnowledge(FArcKnowledgeHandle(99999));
	}

	TEST_METHOD(Update_ChangesEntryData)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Entry = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(100, 0, 0));
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(Entry);

		// Update to new location and tag
		FArcKnowledgeEntry Updated = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood, FVector(500, 0, 0));
		Sub->UpdateKnowledge(Handle, Updated);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(IsTrue(Retrieved->Tags.HasTagExact(TAG_Test_Resource_Wood), TEXT("Tags should be updated")));
		ASSERT_THAT(IsFalse(Retrieved->Tags.HasTagExact(TAG_Test_Resource_Iron), TEXT("Old tag should be gone")));
		ASSERT_THAT(IsNear(500.0f, static_cast<float>(Retrieved->Location.X), 0.01f, TEXT("Location should be updated")));
	}

	TEST_METHOD(Update_PreservesHandle)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Entry = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(Entry);

		FArcKnowledgeEntry Updated = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood);
		Sub->UpdateKnowledge(Handle, Updated);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(AreEqual(Handle, Retrieved->Handle, TEXT("Handle should be preserved after update")));
	}

	TEST_METHOD(Update_InvalidHandle_NoEffect)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Updated = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		// Should not crash
		Sub->UpdateKnowledge(FArcKnowledgeHandle(99999), Updated);
	}

	TEST_METHOD(BatchRegister_AllRetrievable)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		TArray<FArcKnowledgeEntry> Entries;
		Entries.Add(ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron));
		Entries.Add(ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood));
		Entries.Add(ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Stone));

		TArray<FArcKnowledgeHandle> Handles;
		Sub->RegisterKnowledgeBatch(Entries, Handles);

		ASSERT_THAT(AreEqual(3, Handles.Num(), TEXT("Batch should return 3 handles")));
		for (const FArcKnowledgeHandle& H : Handles)
		{
			ASSERT_THAT(IsTrue(H.IsValid()));
			ASSERT_THAT(IsNotNull(Sub->GetKnowledgeEntry(H)));
		}
	}

	TEST_METHOD(Refresh_UpdatesRelevanceAndTimestamp)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Entry = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector::ZeroVector, 0.5f);
		Entry.Timestamp = 10.0;
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(Entry);

		Sub->RefreshKnowledge(Handle, 0.9f);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(IsNear(0.9f, Retrieved->Relevance, 0.001f, TEXT("Relevance should be refreshed")));
	}

	TEST_METHOD(Refresh_ClampedTo01)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Entry = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(Entry);

		Sub->RefreshKnowledge(Handle, 5.0f);
		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(IsNear(1.0f, Retrieved->Relevance, 0.001f, TEXT("Relevance should be clamped to 1.0")));

		Sub->RefreshKnowledge(Handle, -1.0f);
		Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(IsNear(0.0f, Retrieved->Relevance, 0.001f, TEXT("Relevance should be clamped to 0.0")));
	}
};

// ===================================================================
// Knowledge Queries — Tag matching, Filters, Scorers
// ===================================================================

TEST_CLASS(ArcKnowledge_KnowledgeQuery, "ArcKnowledge.Knowledge.Query")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(EmptyQuery_ReturnsAllEntries)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry E1 = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeEntry E2 = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood);
		Sub->RegisterKnowledge(E1);
		Sub->RegisterKnowledge(E2);

		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;
		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(2, Results.Num(), TEXT("Empty tag query should match all entries")));
	}

	TEST_METHOD(TagQuery_FiltersCorrectly)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry E1 = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeEntry E2 = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood);
		FArcKnowledgeEntry E3 = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Event_Robbery);
		Sub->RegisterKnowledge(E1);
		Sub->RegisterKnowledge(E2);
		Sub->RegisterKnowledge(E3);

		FArcKnowledgeQuery Query;
		Query.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Test_Resource_Iron));
		Query.MaxResults = 10;
		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Should only match the Iron entry")));
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Iron)));
	}

	TEST_METHOD(MaxResults_LimitsOutput)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		for (int32 i = 0; i < 10; ++i)
		{
			FArcKnowledgeEntry E = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(i * 100.0f, 0, 0));
			Sub->RegisterKnowledge(E);
		}

		FArcKnowledgeQuery Query;
		Query.MaxResults = 3;
		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(3, Results.Num(), TEXT("Should return at most MaxResults entries")));
	}

	TEST_METHOD(DistanceFilter_RejectsDistant)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Near = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(100, 0, 0));
		FArcKnowledgeEntry Far = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood, FVector(50000, 0, 0));
		Sub->RegisterKnowledge(Near);
		Sub->RegisterKnowledge(Far);

		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;

		FInstancedStruct FilterStruct;
		FilterStruct.InitializeAs<FArcKnowledgeFilter_MaxDistance>();
		FilterStruct.GetMutable<FArcKnowledgeFilter_MaxDistance>().MaxDistance = 1000.0f;
		Query.Filters.Add(MoveTemp(FilterStruct));

		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext(FVector::ZeroVector);

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Only near entry should pass distance filter")));
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Iron)));
	}

	TEST_METHOD(MinRelevanceFilter_RejectsLowRelevance)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry High = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector::ZeroVector, 0.8f);
		FArcKnowledgeEntry Low = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood, FVector::ZeroVector, 0.05f);
		Sub->RegisterKnowledge(High);
		Sub->RegisterKnowledge(Low);

		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;

		FInstancedStruct FilterStruct;
		FilterStruct.InitializeAs<FArcKnowledgeFilter_MinRelevance>();
		FilterStruct.GetMutable<FArcKnowledgeFilter_MinRelevance>().MinRelevance = 0.1f;
		Query.Filters.Add(MoveTemp(FilterStruct));

		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Only high-relevance entry should pass")));
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Iron)));
	}

	TEST_METHOD(MaxAgeFilter_RejectsOld)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Fresh;
		Fresh.Tags.AddTag(TAG_Test_Resource_Iron);
		Fresh.Timestamp = 95.0;  // 5 seconds ago at time=100
		Sub->RegisterKnowledge(Fresh);

		FArcKnowledgeEntry Old;
		Old.Tags.AddTag(TAG_Test_Resource_Wood);
		Old.Timestamp = 10.0;  // 90 seconds ago at time=100
		Sub->RegisterKnowledge(Old);

		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;

		FInstancedStruct FilterStruct;
		FilterStruct.InitializeAs<FArcKnowledgeFilter_MaxAge>();
		FilterStruct.GetMutable<FArcKnowledgeFilter_MaxAge>().MaxAgeSeconds = 30.0f;
		Query.Filters.Add(MoveTemp(FilterStruct));

		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext(FVector::ZeroVector, 100.0);

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Only fresh entry should pass age filter")));
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Iron)));
	}

	TEST_METHOD(NotClaimedFilter_RejectsClaimedEntries)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Ad1 = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Quest_Deliver);
		FArcKnowledgeEntry Ad2 = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Quest_Deliver);
		FArcKnowledgeHandle H1 = Sub->PostAdvertisement(Ad1);
		FArcKnowledgeHandle H2 = Sub->PostAdvertisement(Ad2);

		// Claim one
		Sub->ClaimAdvertisement(H1, FMassEntityHandle());

		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;

		FInstancedStruct FilterStruct;
		FilterStruct.InitializeAs<FArcKnowledgeFilter_NotClaimed>();
		Query.Filters.Add(MoveTemp(FilterStruct));

		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Only unclaimed advertisement should pass")));
		ASSERT_THAT(AreEqual(H2, Results[0].Entry.Handle, TEXT("Should be the unclaimed advertisement")));
	}

	TEST_METHOD(DistanceScorer_CloserScoresHigher)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Near = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(100, 0, 0));
		FArcKnowledgeEntry Far = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood, FVector(5000, 0, 0));
		Sub->RegisterKnowledge(Near);
		Sub->RegisterKnowledge(Far);

		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;

		FInstancedStruct ScorerStruct;
		ScorerStruct.InitializeAs<FArcKnowledgeScorer_Distance>();
		ScorerStruct.GetMutable<FArcKnowledgeScorer_Distance>().MaxDistance = 10000.0f;
		Query.Scorers.Add(MoveTemp(ScorerStruct));

		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext(FVector::ZeroVector);

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(2, Results.Num()));
		// Results sorted by score descending (HighestScore mode)
		ASSERT_THAT(IsTrue(Results[0].Score > Results[1].Score, TEXT("Closer entry should score higher")));
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Iron), TEXT("Iron (near) should be first")));
	}

	TEST_METHOD(RelevanceScorer_HigherRelevanceScoresHigher)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry High = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector::ZeroVector, 0.9f);
		FArcKnowledgeEntry Low = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood, FVector::ZeroVector, 0.2f);
		Sub->RegisterKnowledge(High);
		Sub->RegisterKnowledge(Low);

		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;

		FInstancedStruct ScorerStruct;
		ScorerStruct.InitializeAs<FArcKnowledgeScorer_Relevance>();
		Query.Scorers.Add(MoveTemp(ScorerStruct));

		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(2, Results.Num()));
		ASSERT_THAT(IsTrue(Results[0].Score > Results[1].Score, TEXT("Higher relevance should score higher")));
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Iron)));
	}

	TEST_METHOD(FreshnessScorer_NewerScoresHigher)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Recent;
		Recent.Tags.AddTag(TAG_Test_Resource_Iron);
		Recent.Timestamp = 95.0;
		Sub->RegisterKnowledge(Recent);

		FArcKnowledgeEntry Old;
		Old.Tags.AddTag(TAG_Test_Resource_Wood);
		Old.Timestamp = 10.0;
		Sub->RegisterKnowledge(Old);

		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;

		FInstancedStruct ScorerStruct;
		ScorerStruct.InitializeAs<FArcKnowledgeScorer_Freshness>();
		ScorerStruct.GetMutable<FArcKnowledgeScorer_Freshness>().HalfLifeSeconds = 60.0f;
		Query.Scorers.Add(MoveTemp(ScorerStruct));

		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext(FVector::ZeroVector, 100.0);

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(2, Results.Num()));
		ASSERT_THAT(IsTrue(Results[0].Score > Results[1].Score, TEXT("More recent entry should score higher")));
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Iron)));
	}

	TEST_METHOD(MultipleFilters_AllMustPass)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		// Near + high relevance
		FArcKnowledgeEntry Good = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(100, 0, 0), 0.9f);
		// Near + low relevance
		FArcKnowledgeEntry NearLow = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood, FVector(200, 0, 0), 0.01f);
		// Far + high relevance
		FArcKnowledgeEntry FarHigh = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Stone, FVector(50000, 0, 0), 0.9f);
		Sub->RegisterKnowledge(Good);
		Sub->RegisterKnowledge(NearLow);
		Sub->RegisterKnowledge(FarHigh);

		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;

		FInstancedStruct DistFilter;
		DistFilter.InitializeAs<FArcKnowledgeFilter_MaxDistance>();
		DistFilter.GetMutable<FArcKnowledgeFilter_MaxDistance>().MaxDistance = 1000.0f;
		Query.Filters.Add(MoveTemp(DistFilter));

		FInstancedStruct RelFilter;
		RelFilter.InitializeAs<FArcKnowledgeFilter_MinRelevance>();
		RelFilter.GetMutable<FArcKnowledgeFilter_MinRelevance>().MinRelevance = 0.5f;
		Query.Filters.Add(MoveTemp(RelFilter));

		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext(FVector::ZeroVector);

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Only entry passing both filters should survive")));
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Iron)));
	}

	TEST_METHOD(MultipleScorers_Multiplicative)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		// Near + high relevance (should score highest)
		FArcKnowledgeEntry Best = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(100, 0, 0), 0.9f);
		// Far + high relevance
		FArcKnowledgeEntry FarHigh = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood, FVector(8000, 0, 0), 0.9f);
		// Near + low relevance
		FArcKnowledgeEntry NearLow = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Stone, FVector(100, 0, 0), 0.1f);
		Sub->RegisterKnowledge(Best);
		Sub->RegisterKnowledge(FarHigh);
		Sub->RegisterKnowledge(NearLow);

		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;

		FInstancedStruct DistScorer;
		DistScorer.InitializeAs<FArcKnowledgeScorer_Distance>();
		DistScorer.GetMutable<FArcKnowledgeScorer_Distance>().MaxDistance = 10000.0f;
		Query.Scorers.Add(MoveTemp(DistScorer));

		FInstancedStruct RelScorer;
		RelScorer.InitializeAs<FArcKnowledgeScorer_Relevance>();
		Query.Scorers.Add(MoveTemp(RelScorer));

		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext(FVector::ZeroVector);

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(3, Results.Num()));
		// Best (near+high) should be first
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Iron),
			TEXT("Near + high relevance should rank first with multiplicative scoring")));
	}

	TEST_METHOD(ZeroScore_ExcludedFromResults)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		// Zero relevance entry
		FArcKnowledgeEntry ZeroRel = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector::ZeroVector, 0.0f);
		FArcKnowledgeEntry Normal = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood, FVector::ZeroVector, 0.5f);
		Sub->RegisterKnowledge(ZeroRel);
		Sub->RegisterKnowledge(Normal);

		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;

		FInstancedStruct RelScorer;
		RelScorer.InitializeAs<FArcKnowledgeScorer_Relevance>();
		Query.Scorers.Add(MoveTemp(RelScorer));

		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Zero-score entry should be excluded")));
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Wood)));
	}

	TEST_METHOD(NoMatchingEntries_EmptyResults)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry E = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		Sub->RegisterKnowledge(E);

		FArcKnowledgeQuery Query;
		Query.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Test_Event_Robbery));
		Query.MaxResults = 10;
		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(0, Results.Num(), TEXT("Non-matching tag query should return empty")));
	}

	TEST_METHOD(QueryAfterRemoval_EntryNotReturned)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry E1 = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeEntry E2 = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood);
		FArcKnowledgeHandle H1 = Sub->RegisterKnowledge(E1);
		Sub->RegisterKnowledge(E2);

		Sub->RemoveKnowledge(H1);

		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;
		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Removed entry should not appear in queries")));
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Wood)));
	}
};

// ===================================================================
// Advertisements — Post, Claim, Cancel, Complete
// ===================================================================

TEST_CLASS(ArcKnowledge_Advertisements, "ArcKnowledge.Advertisements")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(PostAdvertisement_CreatesQueryableEntry)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Advert = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Quest_Deliver);
		FArcKnowledgeHandle Handle = Sub->PostAdvertisement(Advert);

		ASSERT_THAT(IsTrue(Handle.IsValid()));

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(IsFalse(Retrieved->bClaimed, TEXT("New advertisement should not be claimed")));
	}

	TEST_METHOD(ClaimAdvertisement_MarksClaimed)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Advert = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Quest_Deliver);
		FArcKnowledgeHandle Handle = Sub->PostAdvertisement(Advert);

		bool bClaimed = Sub->ClaimAdvertisement(Handle, FMassEntityHandle());
		ASSERT_THAT(IsTrue(bClaimed, TEXT("First claim should succeed")));

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(IsTrue(Retrieved->bClaimed, TEXT("Entry should be marked claimed")));
	}

	TEST_METHOD(ClaimAdvertisement_DoubleClaimFails)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Advert = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Quest_Deliver);
		FArcKnowledgeHandle Handle = Sub->PostAdvertisement(Advert);

		Sub->ClaimAdvertisement(Handle, FMassEntityHandle());
		bool bSecondClaim = Sub->ClaimAdvertisement(Handle, FMassEntityHandle());
		ASSERT_THAT(IsFalse(bSecondClaim, TEXT("Double claim should fail")));
	}

	TEST_METHOD(ClaimAdvertisement_InvalidHandle_Fails)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		bool bClaimed = Sub->ClaimAdvertisement(FArcKnowledgeHandle(99999), FMassEntityHandle());
		ASSERT_THAT(IsFalse(bClaimed, TEXT("Claiming invalid handle should fail")));
	}

	TEST_METHOD(CancelAdvertisement_ReleasesClaim)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Advert = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Quest_Deliver);
		FArcKnowledgeHandle Handle = Sub->PostAdvertisement(Advert);

		Sub->ClaimAdvertisement(Handle, FMassEntityHandle());
		Sub->CancelAdvertisement(Handle);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(IsFalse(Retrieved->bClaimed, TEXT("Cancel should release claim")));
	}

	TEST_METHOD(CancelAdvertisement_CanReClaimAfterCancel)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Advert = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Quest_Deliver);
		FArcKnowledgeHandle Handle = Sub->PostAdvertisement(Advert);

		Sub->ClaimAdvertisement(Handle, FMassEntityHandle());
		Sub->CancelAdvertisement(Handle);
		bool bReClaimed = Sub->ClaimAdvertisement(Handle, FMassEntityHandle());

		ASSERT_THAT(IsTrue(bReClaimed, TEXT("Should be able to re-claim after cancel")));
	}

	TEST_METHOD(CompleteAdvertisement_RemovesEntry)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Advert = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Quest_Deliver);
		FArcKnowledgeHandle Handle = Sub->PostAdvertisement(Advert);

		Sub->CompleteAdvertisement(Handle);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNull(Retrieved, TEXT("Completed advertisement should be removed")));
	}
};

// ===================================================================
// Expiration — Lifetime-based entry removal on Tick
// ===================================================================

TEST_CLASS(ArcKnowledge_Expiration, "ArcKnowledge.Expiration")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(ZeroLifetime_NeverExpires)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		// Default Lifetime = 0 means infinite. Use a past timestamp to prove it won't expire.
		FArcKnowledgeEntry E = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		E.Timestamp = -1000.0;  // Far in the past
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);

		// Configure fast expiration checks and tick past interval
		Sub->ExpirationTickInterval = 1.0f;
		Sub->Tick(2.0f);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved, TEXT("Zero-lifetime entry should never expire")));
	}

	TEST_METHOD(FiniteLifetime_RemovedAfterExpiration)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		Sub->ExpirationTickInterval = 1.0f;

		// Timestamp in the past so (WorldTime=0 - Timestamp=-10) = 10, which exceeds Lifetime=5
		FArcKnowledgeEntry E = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		E.Lifetime = 5.0f;
		E.Timestamp = -10.0;
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);

		// Tick past the ExpirationTickInterval so the check runs
		Sub->Tick(2.0f);

		ASSERT_THAT(IsNull(Sub->GetKnowledgeEntry(Handle),
			TEXT("Entry past its lifetime should be expired and removed")));
	}

	TEST_METHOD(FiniteLifetime_SurvivesBeforeExpiration)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		Sub->ExpirationTickInterval = 1.0f;

		// Timestamp in the past but NOT past lifetime: (WorldTime=0 - Timestamp=-2) = 2, < Lifetime=5
		FArcKnowledgeEntry E = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		E.Lifetime = 5.0f;
		E.Timestamp = -2.0;
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);

		Sub->Tick(2.0f);

		ASSERT_THAT(IsNotNull(Sub->GetKnowledgeEntry(Handle),
			TEXT("Entry within its lifetime should survive")));
	}

	TEST_METHOD(ExpirationInterval_OnlyChecksAfterInterval)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		Sub->ExpirationTickInterval = 10.0f;

		// Entry is already expired by time: (0 - (-10)) = 10 >= Lifetime=1
		FArcKnowledgeEntry E = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		E.Lifetime = 1.0f;
		E.Timestamp = -10.0;
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);

		// Small tick — accumulator reaches 2.0, less than interval of 10.0 → check does NOT run
		Sub->Tick(2.0f);

		// Entry should still exist because expiration check hasn't triggered yet
		ASSERT_THAT(IsNotNull(Sub->GetKnowledgeEntry(Handle),
			TEXT("Expired entry should survive when expiration interval hasn't elapsed")));

		// Tick past the full interval (accumulator now 2+9 = 11 >= 10) → check runs
		Sub->Tick(9.0f);

		ASSERT_THAT(IsNull(Sub->GetKnowledgeEntry(Handle),
			TEXT("Expired entry should be removed after expiration interval elapses")));
	}

	TEST_METHOD(MultipleEntries_OnlyExpiredRemoved)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		Sub->ExpirationTickInterval = 1.0f;

		FArcKnowledgeEntry Infinite = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		Infinite.Lifetime = 0.0f;  // Never expires
		Infinite.Timestamp = -1000.0;  // Very old — but Lifetime=0 means infinite
		FArcKnowledgeHandle HInfinite = Sub->RegisterKnowledge(Infinite);

		FArcKnowledgeEntry LongLived = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood);
		LongLived.Lifetime = 999.0f;
		LongLived.Timestamp = -2.0;  // Age=2, well within Lifetime=999
		FArcKnowledgeHandle HLong = Sub->RegisterKnowledge(LongLived);

		FArcKnowledgeEntry Expired = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Stone);
		Expired.Lifetime = 3.0f;
		Expired.Timestamp = -10.0;  // Age=10, exceeds Lifetime=3
		FArcKnowledgeHandle HExpired = Sub->RegisterKnowledge(Expired);

		Sub->Tick(2.0f);

		ASSERT_THAT(IsNotNull(Sub->GetKnowledgeEntry(HInfinite), TEXT("Infinite entry should survive")));
		ASSERT_THAT(IsNotNull(Sub->GetKnowledgeEntry(HLong), TEXT("Long-lived entry should survive")));
		ASSERT_THAT(IsNull(Sub->GetKnowledgeEntry(HExpired), TEXT("Expired entry should be removed")));
	}
};

// ===================================================================
// Tag Index Integrity — verify index stays consistent after mutations
// ===================================================================

TEST_CLASS(ArcKnowledge_TagIndex, "ArcKnowledge.Knowledge.TagIndex")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(Update_TagsReIndexed)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		// Register with Iron tag
		FArcKnowledgeEntry E = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);

		// Update to Wood tag
		FArcKnowledgeEntry Updated = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood);
		Sub->UpdateKnowledge(Handle, Updated);

		// Query for Iron — should not find it
		FArcKnowledgeQuery IronQuery;
		IronQuery.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Test_Resource_Iron));
		IronQuery.MaxResults = 10;
		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> IronResults;
		Sub->QueryKnowledge(IronQuery, Ctx, IronResults);
		ASSERT_THAT(AreEqual(0, IronResults.Num(), TEXT("Old tag should not be queryable after update")));

		// Query for Wood — should find it
		FArcKnowledgeQuery WoodQuery;
		WoodQuery.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Test_Resource_Wood));
		WoodQuery.MaxResults = 10;

		TArray<FArcKnowledgeQueryResult> WoodResults;
		Sub->QueryKnowledge(WoodQuery, Ctx, WoodResults);
		ASSERT_THAT(AreEqual(1, WoodResults.Num(), TEXT("New tag should be queryable after update")));
	}

	TEST_METHOD(Remove_TagIndexCleanedUp)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry E = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);
		Sub->RemoveKnowledge(Handle);

		FArcKnowledgeQuery Query;
		Query.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Test_Resource_Iron));
		Query.MaxResults = 10;
		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);
		ASSERT_THAT(AreEqual(0, Results.Num(), TEXT("Removed entry should not appear in tag queries")));
	}

};

// ===================================================================
// Spatial Queries — QueryKnowledgeInRadius, FindNearestKnowledge
// ===================================================================

TEST_CLASS(ArcKnowledge_SpatialQuery, "ArcKnowledge.Knowledge.SpatialQuery")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(QueryInRadius_ReturnsEntriesInRange)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Near = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(500, 0, 0));
		FArcKnowledgeEntry Far = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood, FVector(50000, 0, 0));
		Sub->RegisterKnowledge(Near);
		Sub->RegisterKnowledge(Far);

		TArray<FArcKnowledgeHandle> Results;
		Sub->QueryKnowledgeInRadius(FVector::ZeroVector, 2000.0f, Results, FGameplayTagQuery());

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Only near entry should be in radius")));
	}

	TEST_METHOD(QueryInRadius_EmptyIndex_ReturnsEmpty)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		TArray<FArcKnowledgeHandle> Results;
		Sub->QueryKnowledgeInRadius(FVector::ZeroVector, 5000.0f, Results, FGameplayTagQuery());

		ASSERT_THAT(AreEqual(0, Results.Num(), TEXT("Empty index should return no results")));
	}

	TEST_METHOD(QueryInRadius_ZeroRadius_ReturnsExactLocation)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry AtOrigin = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector::ZeroVector);
		FArcKnowledgeEntry Nearby = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood, FVector(100, 0, 0));
		Sub->RegisterKnowledge(AtOrigin);
		Sub->RegisterKnowledge(Nearby);

		TArray<FArcKnowledgeHandle> Results;
		Sub->QueryKnowledgeInRadius(FVector::ZeroVector, 0.0f, Results, FGameplayTagQuery());

		// Zero radius — only exact location match
		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Zero radius should only match exact location")));
	}

	TEST_METHOD(QueryInRadius_WithTagFilter)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Iron = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(100, 0, 0));
		FArcKnowledgeEntry Wood = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood, FVector(200, 0, 0));
		Sub->RegisterKnowledge(Iron);
		Sub->RegisterKnowledge(Wood);

		FGameplayTagQuery TagFilter = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Test_Resource_Iron));

		TArray<FArcKnowledgeHandle> Results;
		Sub->QueryKnowledgeInRadius(FVector::ZeroVector, 5000.0f, Results, TagFilter);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Should only return Iron entry matching tag filter")));
	}

	TEST_METHOD(FindNearest_ReturnsClosest)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Close = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(100, 0, 0));
		FArcKnowledgeEntry Far = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood, FVector(5000, 0, 0));
		FArcKnowledgeHandle HClose = Sub->RegisterKnowledge(Close);
		Sub->RegisterKnowledge(Far);

		FArcKnowledgeHandle Nearest = Sub->FindNearestKnowledge(FVector::ZeroVector, 10000.0f, FGameplayTagQuery());

		ASSERT_THAT(IsTrue(Nearest.IsValid(), TEXT("Should find a nearest entry")));
		ASSERT_THAT(AreEqual(HClose, Nearest, TEXT("Should return the closest entry")));
	}

	TEST_METHOD(FindNearest_WithTagFilter)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		// Iron is closer but we filter for Wood
		FArcKnowledgeEntry Iron = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(100, 0, 0));
		FArcKnowledgeEntry Wood = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood, FVector(500, 0, 0));
		Sub->RegisterKnowledge(Iron);
		FArcKnowledgeHandle HWood = Sub->RegisterKnowledge(Wood);

		FGameplayTagQuery WoodFilter = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Test_Resource_Wood));
		FArcKnowledgeHandle Nearest = Sub->FindNearestKnowledge(FVector::ZeroVector, 10000.0f, WoodFilter);

		ASSERT_THAT(AreEqual(HWood, Nearest, TEXT("Should return nearest matching tag filter")));
	}

	TEST_METHOD(FindNearest_NoneInRange_ReturnsInvalid)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Far = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(50000, 0, 0));
		Sub->RegisterKnowledge(Far);

		FArcKnowledgeHandle Nearest = Sub->FindNearestKnowledge(FVector::ZeroVector, 1000.0f, FGameplayTagQuery());

		ASSERT_THAT(IsFalse(Nearest.IsValid(), TEXT("Should return invalid handle when nothing in range")));
	}

	TEST_METHOD(SpatialHash_MaintainedAfterRemove)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry E = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(100, 0, 0));
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);

		Sub->RemoveKnowledge(Handle);

		TArray<FArcKnowledgeHandle> Results;
		Sub->QueryKnowledgeInRadius(FVector::ZeroVector, 5000.0f, Results, FGameplayTagQuery());

		ASSERT_THAT(AreEqual(0, Results.Num(), TEXT("Removed entry should not appear in spatial queries")));
	}

	TEST_METHOD(SpatialHash_MaintainedAfterUpdate)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry E = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(100, 0, 0));
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);

		// Move entry far away
		FArcKnowledgeEntry Updated = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(50000, 0, 0));
		Sub->UpdateKnowledge(Handle, Updated);

		// Should not find it near origin
		TArray<FArcKnowledgeHandle> NearResults;
		Sub->QueryKnowledgeInRadius(FVector::ZeroVector, 1000.0f, NearResults, FGameplayTagQuery());
		ASSERT_THAT(AreEqual(0, NearResults.Num(), TEXT("Updated entry should not be at old location")));

		// Should find it at new location
		TArray<FArcKnowledgeHandle> FarResults;
		Sub->QueryKnowledgeInRadius(FVector(50000, 0, 0), 1000.0f, FarResults, FGameplayTagQuery());
		ASSERT_THAT(AreEqual(1, FarResults.Num(), TEXT("Updated entry should be at new location")));
	}

	TEST_METHOD(DistanceFilter_UsesSpatialPreFiltering)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		// Place entries in a spread-out pattern
		FArcKnowledgeEntry Near = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(500, 0, 0));
		FArcKnowledgeEntry Far = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood, FVector(50000, 0, 0));
		Sub->RegisterKnowledge(Near);
		Sub->RegisterKnowledge(Far);

		// Query with distance filter — should use spatial hash internally
		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;

		FInstancedStruct FilterStruct;
		FilterStruct.InitializeAs<FArcKnowledgeFilter_MaxDistance>();
		FilterStruct.GetMutable<FArcKnowledgeFilter_MaxDistance>().MaxDistance = 2000.0f;
		Query.Filters.Add(MoveTemp(FilterStruct));

		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext(FVector::ZeroVector);

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Distance filter query should return only near entry")));
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Iron)));
	}

	TEST_METHOD(QueryInRadius_AtBoundary)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		// Place entry exactly at the boundary distance
		FArcKnowledgeEntry E = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(1000, 0, 0));
		Sub->RegisterKnowledge(E);

		TArray<FArcKnowledgeHandle> Results;
		Sub->QueryKnowledgeInRadius(FVector::ZeroVector, 1000.0f, Results, FGameplayTagQuery());

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Entry exactly at boundary should be included")));
	}
};

// ===================================================================
// Knowledge Mutation — AddTag, RemoveTag, SetTags, SetPayload
// ===================================================================

TEST_CLASS(ArcKnowledge_Mutation, "ArcKnowledge.Knowledge.Mutation")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(AddTag_TagAppearsOnEntry)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry E = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);

		Sub->AddTagToKnowledge(Handle, TAG_Test_Resource_Wood);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(IsTrue(Retrieved->Tags.HasTagExact(TAG_Test_Resource_Iron), TEXT("Original tag should remain")));
		ASSERT_THAT(IsTrue(Retrieved->Tags.HasTagExact(TAG_Test_Resource_Wood), TEXT("Added tag should be present")));
	}

	TEST_METHOD(AddTag_IndexUpdated)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry E = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);

		Sub->AddTagToKnowledge(Handle, TAG_Test_Resource_Wood);

		// Query for Wood — should find the entry via tag index
		FArcKnowledgeQuery Query;
		Query.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Test_Resource_Wood));
		Query.MaxResults = 10;
		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Entry should be found via newly added tag")));
	}

	TEST_METHOD(AddTag_DuplicateTag_NoEffect)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry E = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);

		// Add same tag again — should not crash or duplicate
		Sub->AddTagToKnowledge(Handle, TAG_Test_Resource_Iron);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(IsTrue(Retrieved->Tags.HasTagExact(TAG_Test_Resource_Iron)));
	}

	TEST_METHOD(AddTag_InvalidHandle_NoEffect)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		// Should not crash
		Sub->AddTagToKnowledge(FArcKnowledgeHandle(99999), TAG_Test_Resource_Iron);
	}

	TEST_METHOD(RemoveTag_TagRemovedFromEntry)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FGameplayTagContainer Tags;
		Tags.AddTag(TAG_Test_Resource_Iron);
		Tags.AddTag(TAG_Test_Resource_Wood);
		FArcKnowledgeEntry E = ArcKnowledgeTestHelpers::MakeEntryMultiTag(Tags);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);

		Sub->RemoveTagFromKnowledge(Handle, TAG_Test_Resource_Iron);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(IsFalse(Retrieved->Tags.HasTagExact(TAG_Test_Resource_Iron), TEXT("Removed tag should be gone")));
		ASSERT_THAT(IsTrue(Retrieved->Tags.HasTagExact(TAG_Test_Resource_Wood), TEXT("Other tag should remain")));
	}

	TEST_METHOD(RemoveTag_IndexUpdated)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FGameplayTagContainer Tags;
		Tags.AddTag(TAG_Test_Resource_Iron);
		Tags.AddTag(TAG_Test_Resource_Wood);
		FArcKnowledgeEntry E = ArcKnowledgeTestHelpers::MakeEntryMultiTag(Tags);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);

		Sub->RemoveTagFromKnowledge(Handle, TAG_Test_Resource_Iron);

		// Query for Iron — should NOT find it anymore
		FArcKnowledgeQuery Query;
		Query.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Test_Resource_Iron));
		Query.MaxResults = 10;
		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(0, Results.Num(), TEXT("Entry should not appear in queries for removed tag")));
	}

	TEST_METHOD(RemoveTag_TagNotPresent_NoEffect)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry E = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);

		// Remove tag that isn't on the entry — should not crash
		Sub->RemoveTagFromKnowledge(Handle, TAG_Test_Resource_Wood);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(IsTrue(Retrieved->Tags.HasTagExact(TAG_Test_Resource_Iron), TEXT("Original tag should be unaffected")));
	}

	TEST_METHOD(SetTags_ReplacesAllTags)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FGameplayTagContainer OriginalTags;
		OriginalTags.AddTag(TAG_Test_Resource_Iron);
		OriginalTags.AddTag(TAG_Test_Resource_Wood);
		FArcKnowledgeEntry E = ArcKnowledgeTestHelpers::MakeEntryMultiTag(OriginalTags);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);

		FGameplayTagContainer NewTags;
		NewTags.AddTag(TAG_Test_Resource_Stone);
		NewTags.AddTag(TAG_Test_Event_Robbery);
		Sub->SetKnowledgeTags(Handle, NewTags);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(IsFalse(Retrieved->Tags.HasTagExact(TAG_Test_Resource_Iron), TEXT("Old tags should be gone")));
		ASSERT_THAT(IsFalse(Retrieved->Tags.HasTagExact(TAG_Test_Resource_Wood), TEXT("Old tags should be gone")));
		ASSERT_THAT(IsTrue(Retrieved->Tags.HasTagExact(TAG_Test_Resource_Stone), TEXT("New tags should be present")));
		ASSERT_THAT(IsTrue(Retrieved->Tags.HasTagExact(TAG_Test_Event_Robbery), TEXT("New tags should be present")));
	}

	TEST_METHOD(SetTags_IndexFullyRebuilt)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry E = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);

		FGameplayTagContainer NewTags;
		NewTags.AddTag(TAG_Test_Resource_Stone);
		Sub->SetKnowledgeTags(Handle, NewTags);

		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext();

		// Old tag should not find it
		FArcKnowledgeQuery OldQuery;
		OldQuery.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Test_Resource_Iron));
		OldQuery.MaxResults = 10;
		TArray<FArcKnowledgeQueryResult> OldResults;
		Sub->QueryKnowledge(OldQuery, Ctx, OldResults);
		ASSERT_THAT(AreEqual(0, OldResults.Num(), TEXT("Old tag should not match after SetTags")));

		// New tag should find it
		FArcKnowledgeQuery NewQuery;
		NewQuery.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Test_Resource_Stone));
		NewQuery.MaxResults = 10;
		TArray<FArcKnowledgeQueryResult> NewResults;
		Sub->QueryKnowledge(NewQuery, Ctx, NewResults);
		ASSERT_THAT(AreEqual(1, NewResults.Num(), TEXT("New tag should match after SetTags")));
	}

	TEST_METHOD(SetPayload_ReplacesPayload)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry E = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);

		// Entry starts with no payload
		const FArcKnowledgeEntry* Before = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Before));
		ASSERT_THAT(IsFalse(Before->Payload.IsValid(), TEXT("Entry should start with no payload")));

		// Set a typed payload
		FInstancedStruct NewPayload;
		NewPayload.InitializeAs<FArcKnowledgeTestPayload>();
		NewPayload.GetMutable<FArcKnowledgeTestPayload>().TestValue = 42;
		Sub->SetKnowledgePayload(Handle, NewPayload);

		const FArcKnowledgeEntry* After = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(After));
		ASSERT_THAT(IsTrue(After->Payload.IsValid(), TEXT("Payload should be set")));

		const FArcKnowledgeTestPayload* Payload = After->Payload.GetPtr<FArcKnowledgeTestPayload>();
		ASSERT_THAT(IsNotNull(Payload, TEXT("Payload should be of correct type")));
		ASSERT_THAT(AreEqual(42, Payload->TestValue, TEXT("Payload value should match")));
	}

	TEST_METHOD(SetPayload_InvalidHandle_NoEffect)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FInstancedStruct NewPayload;
		NewPayload.InitializeAs<FArcKnowledgeTestPayload>();
		// Should not crash
		Sub->SetKnowledgePayload(FArcKnowledgeHandle(99999), NewPayload);
	}
};

// ===================================================================
// Source Entity Index — GetBySource, RemoveBySource
// ===================================================================

TEST_CLASS(ArcKnowledge_SourceEntityIndex, "ArcKnowledge.Knowledge.SourceEntityIndex")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(GetBySource_ReturnsEntriesForSource)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		// Use synthetic entity handles for testing (both Index and SerialNumber must be non-zero for IsValid)
		FMassEntityHandle SourceA(1, 1);
		FMassEntityHandle SourceB(2, 1);

		FArcKnowledgeEntry E1 = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		E1.SourceEntity = SourceA;
		FArcKnowledgeHandle H1 = Sub->RegisterKnowledge(E1);

		FArcKnowledgeEntry E2 = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood);
		E2.SourceEntity = SourceA;
		FArcKnowledgeHandle H2 = Sub->RegisterKnowledge(E2);

		FArcKnowledgeEntry E3 = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Stone);
		E3.SourceEntity = SourceB;
		Sub->RegisterKnowledge(E3);

		TArray<FArcKnowledgeHandle> Handles;
		Sub->GetKnowledgeBySource(SourceA, Handles);

		ASSERT_THAT(AreEqual(2, Handles.Num(), TEXT("Should return 2 entries for SourceA")));
		ASSERT_THAT(IsTrue(Handles.Contains(H1)));
		ASSERT_THAT(IsTrue(Handles.Contains(H2)));
	}

	TEST_METHOD(GetBySource_NoEntries_ReturnsEmpty)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FMassEntityHandle UnknownSource(99, 1);

		TArray<FArcKnowledgeHandle> Handles;
		Sub->GetKnowledgeBySource(UnknownSource, Handles);

		ASSERT_THAT(AreEqual(0, Handles.Num(), TEXT("Unknown source should return empty")));
	}

	TEST_METHOD(RemoveBySource_RemovesAllEntriesForSource)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FMassEntityHandle SourceA(1, 1);
		FMassEntityHandle SourceB(2, 1);

		FArcKnowledgeEntry E1 = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		E1.SourceEntity = SourceA;
		FArcKnowledgeHandle H1 = Sub->RegisterKnowledge(E1);

		FArcKnowledgeEntry E2 = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood);
		E2.SourceEntity = SourceA;
		FArcKnowledgeHandle H2 = Sub->RegisterKnowledge(E2);

		FArcKnowledgeEntry E3 = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Stone);
		E3.SourceEntity = SourceB;
		FArcKnowledgeHandle H3 = Sub->RegisterKnowledge(E3);

		Sub->RemoveKnowledgeBySource(SourceA);

		ASSERT_THAT(IsNull(Sub->GetKnowledgeEntry(H1), TEXT("SourceA entry 1 should be removed")));
		ASSERT_THAT(IsNull(Sub->GetKnowledgeEntry(H2), TEXT("SourceA entry 2 should be removed")));
		ASSERT_THAT(IsNotNull(Sub->GetKnowledgeEntry(H3), TEXT("SourceB entry should survive")));
	}

	TEST_METHOD(RemoveBySource_IndexCleanedUp)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FMassEntityHandle SourceA(1, 1);

		FArcKnowledgeEntry E = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		E.SourceEntity = SourceA;
		Sub->RegisterKnowledge(E);

		Sub->RemoveKnowledgeBySource(SourceA);

		// Source index should be empty after removal
		TArray<FArcKnowledgeHandle> Handles;
		Sub->GetKnowledgeBySource(SourceA, Handles);
		ASSERT_THAT(AreEqual(0, Handles.Num(), TEXT("Source index should be cleaned up after RemoveBySource")));
	}

	TEST_METHOD(RemoveBySource_UnknownSource_NoEffect)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FMassEntityHandle UnknownSource(99, 1);

		// Should not crash
		Sub->RemoveKnowledgeBySource(UnknownSource);
	}

	TEST_METHOD(SourceIndex_UpdatedOnRemove)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FMassEntityHandle Source(1, 1);

		FArcKnowledgeEntry E1 = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		E1.SourceEntity = Source;
		FArcKnowledgeHandle H1 = Sub->RegisterKnowledge(E1);

		FArcKnowledgeEntry E2 = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood);
		E2.SourceEntity = Source;
		FArcKnowledgeHandle H2 = Sub->RegisterKnowledge(E2);

		// Remove one entry manually
		Sub->RemoveKnowledge(H1);

		TArray<FArcKnowledgeHandle> Handles;
		Sub->GetKnowledgeBySource(Source, Handles);
		ASSERT_THAT(AreEqual(1, Handles.Num(), TEXT("Source index should reflect manual removal")));
		ASSERT_THAT(IsTrue(Handles.Contains(H2), TEXT("Remaining entry should be H2")));
	}
};

// ===================================================================
// Payload Type Filter — filter entries by FInstancedStruct payload type
// ===================================================================

TEST_CLASS(ArcKnowledge_PayloadFilter, "ArcKnowledge.Knowledge.PayloadFilter")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(PayloadTypeFilter_MatchesCorrectType)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		// Entry with payload
		FArcKnowledgeEntry WithPayload = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		WithPayload.Payload.InitializeAs<FArcKnowledgeTestPayload>();
		WithPayload.Payload.GetMutable<FArcKnowledgeTestPayload>().TestValue = 10;
		Sub->RegisterKnowledge(WithPayload);

		// Entry without payload
		FArcKnowledgeEntry NoPayload = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood);
		Sub->RegisterKnowledge(NoPayload);

		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;

		FInstancedStruct FilterStruct;
		FilterStruct.InitializeAs<FArcKnowledgeFilter_PayloadType>();
		FilterStruct.GetMutable<FArcKnowledgeFilter_PayloadType>().RequiredPayloadType = FArcKnowledgeTestPayload::StaticStruct();
		Query.Filters.Add(MoveTemp(FilterStruct));

		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Only entry with matching payload type should pass")));
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Iron)));
	}

	TEST_METHOD(PayloadTypeFilter_NullType_PassesAll)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry E1 = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeEntry E2 = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Wood);
		Sub->RegisterKnowledge(E1);
		Sub->RegisterKnowledge(E2);

		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;

		FInstancedStruct FilterStruct;
		FilterStruct.InitializeAs<FArcKnowledgeFilter_PayloadType>();
		// RequiredPayloadType defaults to nullptr — should pass all
		Query.Filters.Add(MoveTemp(FilterStruct));

		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(2, Results.Num(), TEXT("Null payload type filter should pass all entries")));
	}

	TEST_METHOD(PayloadTypeFilter_BaseTypeMatchesDerived)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		// Entry with derived payload
		FArcKnowledgeEntry WithDerived = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		WithDerived.Payload.InitializeAs<FArcKnowledgeTestPayload>();
		Sub->RegisterKnowledge(WithDerived);

		// Filter for base type — should match derived
		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;

		FInstancedStruct FilterStruct;
		FilterStruct.InitializeAs<FArcKnowledgeFilter_PayloadType>();
		FilterStruct.GetMutable<FArcKnowledgeFilter_PayloadType>().RequiredPayloadType = FArcKnowledgePayload::StaticStruct();
		Query.Filters.Add(MoveTemp(FilterStruct));

		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Base payload type filter should match derived payload")));
	}

	TEST_METHOD(AdvertisementWithPayload_PayloadPreserved)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		// Post an advertisement with a typed payload (simulates vacancy posting pattern)
		FArcKnowledgeEntry Advert = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Area_Vacancy);
		Advert.Location = FVector(1000, 2000, 0);
		Advert.Payload.InitializeAs<FArcKnowledgeTestPayload>();
		Advert.Payload.GetMutable<FArcKnowledgeTestPayload>().TestValue = 7;
		FArcKnowledgeHandle Handle = Sub->PostAdvertisement(Advert);

		// Retrieve and verify payload survived round-trip
		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(IsTrue(Retrieved->Payload.IsValid(), TEXT("Advertisement payload should be preserved")));

		const FArcKnowledgeTestPayload* Payload = Retrieved->Payload.GetPtr<FArcKnowledgeTestPayload>();
		ASSERT_THAT(IsNotNull(Payload, TEXT("Payload should be correct type")));
		ASSERT_THAT(AreEqual(7, Payload->TestValue, TEXT("Payload data should be preserved")));
	}

	TEST_METHOD(AdvertisementWithPayload_QueryableByTagAndPayloadType)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		// Post two advertisements: one with payload, one without
		FArcKnowledgeEntry VacancyAd = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Area_Vacancy);
		VacancyAd.Payload.InitializeAs<FArcKnowledgeTestPayload>();
		VacancyAd.Payload.GetMutable<FArcKnowledgeTestPayload>().TestValue = 3;
		Sub->PostAdvertisement(VacancyAd);

		FArcKnowledgeEntry PlainAd = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Area_Vacancy);
		Sub->PostAdvertisement(PlainAd);

		// Query with tag + payload type filter + not-claimed filter
		FArcKnowledgeQuery Query;
		Query.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Test_Area_Vacancy));
		Query.MaxResults = 10;

		FInstancedStruct PayloadFilter;
		PayloadFilter.InitializeAs<FArcKnowledgeFilter_PayloadType>();
		PayloadFilter.GetMutable<FArcKnowledgeFilter_PayloadType>().RequiredPayloadType = FArcKnowledgeTestPayload::StaticStruct();
		Query.Filters.Add(MoveTemp(PayloadFilter));

		FInstancedStruct NotClaimedFilter;
		NotClaimedFilter.InitializeAs<FArcKnowledgeFilter_NotClaimed>();
		Query.Filters.Add(MoveTemp(NotClaimedFilter));

		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Should find only the vacancy ad with payload")));

		const FArcKnowledgeTestPayload* Payload = Results[0].Entry.Payload.GetPtr<FArcKnowledgeTestPayload>();
		ASSERT_THAT(IsNotNull(Payload));
		ASSERT_THAT(AreEqual(3, Payload->TestValue, TEXT("Payload data should match")));
	}

	TEST_METHOD(RemoveAdvertisement_PayloadCleanedUp)
	{
		UArcKnowledgeSubsystem* Sub = ArcKnowledgeTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Advert = ArcKnowledgeTestHelpers::MakeEntry(TAG_Test_Area_Vacancy);
		Advert.Payload.InitializeAs<FArcKnowledgeTestPayload>();
		FArcKnowledgeHandle Handle = Sub->PostAdvertisement(Advert);

		Sub->RemoveKnowledge(Handle);

		// Query should not find it
		FArcKnowledgeQuery Query;
		Query.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Test_Area_Vacancy));
		Query.MaxResults = 10;

		FInstancedStruct PayloadFilter;
		PayloadFilter.InitializeAs<FArcKnowledgeFilter_PayloadType>();
		PayloadFilter.GetMutable<FArcKnowledgeFilter_PayloadType>().RequiredPayloadType = FArcKnowledgeTestPayload::StaticStruct();
		Query.Filters.Add(MoveTemp(PayloadFilter));

		FArcKnowledgeQueryContext Ctx = ArcKnowledgeTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(0, Results.Num(), TEXT("Removed advertisement should not appear in queries")));
	}
};
