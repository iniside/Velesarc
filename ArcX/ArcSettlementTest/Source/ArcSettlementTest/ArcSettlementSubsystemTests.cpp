// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

#include "NativeGameplayTags.h"
#include "InstancedStruct.h"

#include "ArcSettlementSubsystem.h"
#include "ArcSettlementTypes.h"
#include "ArcKnowledgeEntry.h"
#include "ArcKnowledgeQuery.h"
#include "ArcKnowledgeFilter.h"
#include "ArcKnowledgeScorer.h"
#include "ArcSettlementData.h"
#include "ArcSettlementDefinition.h"
#include "ArcRegionDefinition.h"

// ---- Test gameplay tags ----
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Resource_Iron, "Test.Resource.Iron");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Resource_Wood, "Test.Resource.Wood");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Resource_Stone, "Test.Resource.Stone");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Event_Robbery, "Test.Event.Robbery");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Capability_Forge, "Test.Capability.Forge");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Quest_Deliver, "Test.Quest.Deliver");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Settlement_Town, "Test.Settlement.Town");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Settlement_Village, "Test.Settlement.Village");

// ===================================================================
// Helpers
// ===================================================================

namespace ArcSettlementTestHelpers
{
	UArcSettlementSubsystem* GetSubsystem(FActorTestSpawner& Spawner)
	{
		UWorld* World = &Spawner.GetWorld();
		return World ? World->GetSubsystem<UArcSettlementSubsystem>() : nullptr;
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

	UArcSettlementDefinition* CreateSettlementDef(FActorTestSpawner& Spawner, FText Name, float Radius = 5000.0f)
	{
		UArcSettlementDefinition* Def = NewObject<UArcSettlementDefinition>();
		Def->DisplayName = Name;
		Def->BoundingRadius = Radius;
		return Def;
	}

	UArcRegionDefinition* CreateRegionDef(FActorTestSpawner& Spawner, FText Name)
	{
		UArcRegionDefinition* Def = NewObject<UArcRegionDefinition>();
		Def->DisplayName = Name;
		return Def;
	}
}

// ===================================================================
// Knowledge CRUD — Register, Update, Remove, Batch, Refresh
// ===================================================================

TEST_CLASS(ArcSettlement_KnowledgeCRUD, "ArcSettlement.Knowledge.CRUD")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(Register_ReturnsValidHandle)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Entry = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(Entry);

		ASSERT_THAT(IsTrue(Handle.IsValid(), TEXT("RegisterKnowledge should return a valid handle")));
	}

	TEST_METHOD(Register_EntryRetrievable)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Entry = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(100, 200, 0));
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(Entry);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved, TEXT("Registered entry should be retrievable")));
		ASSERT_THAT(IsTrue(Retrieved->Tags.HasTagExact(TAG_Test_Resource_Iron)));
		ASSERT_THAT(IsNear(100.0f, static_cast<float>(Retrieved->Location.X), 0.01f));
	}

	TEST_METHOD(Register_HandleStoredOnEntry)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Entry = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(Entry);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(AreEqual(Handle, Retrieved->Handle, TEXT("Entry's handle should match returned handle")));
	}

	TEST_METHOD(Register_MultipleEntriesUnique)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Entry1 = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeEntry Entry2 = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Wood);
		FArcKnowledgeHandle H1 = Sub->RegisterKnowledge(Entry1);
		FArcKnowledgeHandle H2 = Sub->RegisterKnowledge(Entry2);

		ASSERT_THAT(AreNotEqual(H1, H2, TEXT("Different registrations should produce different handles")));
	}

	TEST_METHOD(Remove_EntryNoLongerRetrievable)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Entry = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(Entry);
		Sub->RemoveKnowledge(Handle);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNull(Retrieved, TEXT("Removed entry should not be retrievable")));
	}

	TEST_METHOD(Remove_InvalidHandle_NoEffect)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		// Should not crash
		Sub->RemoveKnowledge(FArcKnowledgeHandle());
		Sub->RemoveKnowledge(FArcKnowledgeHandle(99999));
	}

	TEST_METHOD(Update_ChangesEntryData)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Entry = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(100, 0, 0));
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(Entry);

		// Update to new location and tag
		FArcKnowledgeEntry Updated = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Wood, FVector(500, 0, 0));
		Sub->UpdateKnowledge(Handle, Updated);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(IsTrue(Retrieved->Tags.HasTagExact(TAG_Test_Resource_Wood), TEXT("Tags should be updated")));
		ASSERT_THAT(IsFalse(Retrieved->Tags.HasTagExact(TAG_Test_Resource_Iron), TEXT("Old tag should be gone")));
		ASSERT_THAT(IsNear(500.0f, static_cast<float>(Retrieved->Location.X), 0.01f, TEXT("Location should be updated")));
	}

	TEST_METHOD(Update_PreservesHandle)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Entry = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(Entry);

		FArcKnowledgeEntry Updated = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Wood);
		Sub->UpdateKnowledge(Handle, Updated);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(AreEqual(Handle, Retrieved->Handle, TEXT("Handle should be preserved after update")));
	}

	TEST_METHOD(Update_InvalidHandle_NoEffect)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Updated = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		// Should not crash
		Sub->UpdateKnowledge(FArcKnowledgeHandle(99999), Updated);
	}

	TEST_METHOD(BatchRegister_AllRetrievable)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		TArray<FArcKnowledgeEntry> Entries;
		Entries.Add(ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron));
		Entries.Add(ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Wood));
		Entries.Add(ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Stone));

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
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Entry = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector::ZeroVector, 0.5f);
		Entry.Timestamp = 10.0;
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(Entry);

		Sub->RefreshKnowledge(Handle, 0.9f);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(IsNear(0.9f, Retrieved->Relevance, 0.001f, TEXT("Relevance should be refreshed")));
	}

	TEST_METHOD(Refresh_ClampedTo01)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Entry = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
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

TEST_CLASS(ArcSettlement_KnowledgeQuery, "ArcSettlement.Knowledge.Query")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(EmptyQuery_ReturnsAllEntries)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry E1 = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeEntry E2 = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Wood);
		Sub->RegisterKnowledge(E1);
		Sub->RegisterKnowledge(E2);

		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;
		FArcKnowledgeQueryContext Ctx = ArcSettlementTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(2, Results.Num(), TEXT("Empty tag query should match all entries")));
	}

	TEST_METHOD(TagQuery_FiltersCorrectly)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry E1 = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeEntry E2 = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Wood);
		FArcKnowledgeEntry E3 = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Event_Robbery);
		Sub->RegisterKnowledge(E1);
		Sub->RegisterKnowledge(E2);
		Sub->RegisterKnowledge(E3);

		FArcKnowledgeQuery Query;
		Query.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Test_Resource_Iron));
		Query.MaxResults = 10;
		FArcKnowledgeQueryContext Ctx = ArcSettlementTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Should only match the Iron entry")));
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Iron)));
	}

	TEST_METHOD(MaxResults_LimitsOutput)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		for (int32 i = 0; i < 10; ++i)
		{
			FArcKnowledgeEntry E = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(i * 100.0f, 0, 0));
			Sub->RegisterKnowledge(E);
		}

		FArcKnowledgeQuery Query;
		Query.MaxResults = 3;
		FArcKnowledgeQueryContext Ctx = ArcSettlementTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(3, Results.Num(), TEXT("Should return at most MaxResults entries")));
	}

	TEST_METHOD(DistanceFilter_RejectsDistant)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Near = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(100, 0, 0));
		FArcKnowledgeEntry Far = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Wood, FVector(50000, 0, 0));
		Sub->RegisterKnowledge(Near);
		Sub->RegisterKnowledge(Far);

		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;

		FInstancedStruct FilterStruct;
		FilterStruct.InitializeAs<FArcKnowledgeFilter_MaxDistance>();
		FilterStruct.GetMutable<FArcKnowledgeFilter_MaxDistance>().MaxDistance = 1000.0f;
		Query.Filters.Add(MoveTemp(FilterStruct));

		FArcKnowledgeQueryContext Ctx = ArcSettlementTestHelpers::MakeContext(FVector::ZeroVector);

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Only near entry should pass distance filter")));
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Iron)));
	}

	TEST_METHOD(MinRelevanceFilter_RejectsLowRelevance)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry High = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector::ZeroVector, 0.8f);
		FArcKnowledgeEntry Low = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Wood, FVector::ZeroVector, 0.05f);
		Sub->RegisterKnowledge(High);
		Sub->RegisterKnowledge(Low);

		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;

		FInstancedStruct FilterStruct;
		FilterStruct.InitializeAs<FArcKnowledgeFilter_MinRelevance>();
		FilterStruct.GetMutable<FArcKnowledgeFilter_MinRelevance>().MinRelevance = 0.1f;
		Query.Filters.Add(MoveTemp(FilterStruct));

		FArcKnowledgeQueryContext Ctx = ArcSettlementTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Only high-relevance entry should pass")));
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Iron)));
	}

	TEST_METHOD(MaxAgeFilter_RejectsOld)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
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

		FArcKnowledgeQueryContext Ctx = ArcSettlementTestHelpers::MakeContext(FVector::ZeroVector, 100.0);

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Only fresh entry should pass age filter")));
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Iron)));
	}

	TEST_METHOD(NotClaimedFilter_RejectsClaimedEntries)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Action1 = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Quest_Deliver);
		FArcKnowledgeEntry Action2 = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Quest_Deliver);
		FArcKnowledgeHandle H1 = Sub->PostAction(Action1);
		FArcKnowledgeHandle H2 = Sub->PostAction(Action2);

		// Claim one
		Sub->ClaimAction(H1, FMassEntityHandle());

		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;

		FInstancedStruct FilterStruct;
		FilterStruct.InitializeAs<FArcKnowledgeFilter_NotClaimed>();
		Query.Filters.Add(MoveTemp(FilterStruct));

		FArcKnowledgeQueryContext Ctx = ArcSettlementTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Only unclaimed action should pass")));
		ASSERT_THAT(AreEqual(H2, Results[0].Entry.Handle, TEXT("Should be the unclaimed action")));
	}

	TEST_METHOD(DistanceScorer_CloserScoresHigher)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Near = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(100, 0, 0));
		FArcKnowledgeEntry Far = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Wood, FVector(5000, 0, 0));
		Sub->RegisterKnowledge(Near);
		Sub->RegisterKnowledge(Far);

		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;

		FInstancedStruct ScorerStruct;
		ScorerStruct.InitializeAs<FArcKnowledgeScorer_Distance>();
		ScorerStruct.GetMutable<FArcKnowledgeScorer_Distance>().MaxDistance = 10000.0f;
		Query.Scorers.Add(MoveTemp(ScorerStruct));

		FArcKnowledgeQueryContext Ctx = ArcSettlementTestHelpers::MakeContext(FVector::ZeroVector);

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(2, Results.Num()));
		// Results sorted by score descending (HighestScore mode)
		ASSERT_THAT(IsTrue(Results[0].Score > Results[1].Score, TEXT("Closer entry should score higher")));
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Iron), TEXT("Iron (near) should be first")));
	}

	TEST_METHOD(RelevanceScorer_HigherRelevanceScoresHigher)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry High = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector::ZeroVector, 0.9f);
		FArcKnowledgeEntry Low = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Wood, FVector::ZeroVector, 0.2f);
		Sub->RegisterKnowledge(High);
		Sub->RegisterKnowledge(Low);

		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;

		FInstancedStruct ScorerStruct;
		ScorerStruct.InitializeAs<FArcKnowledgeScorer_Relevance>();
		Query.Scorers.Add(MoveTemp(ScorerStruct));

		FArcKnowledgeQueryContext Ctx = ArcSettlementTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(2, Results.Num()));
		ASSERT_THAT(IsTrue(Results[0].Score > Results[1].Score, TEXT("Higher relevance should score higher")));
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Iron)));
	}

	TEST_METHOD(FreshnessScorer_NewerScoresHigher)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
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

		FArcKnowledgeQueryContext Ctx = ArcSettlementTestHelpers::MakeContext(FVector::ZeroVector, 100.0);

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(2, Results.Num()));
		ASSERT_THAT(IsTrue(Results[0].Score > Results[1].Score, TEXT("More recent entry should score higher")));
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Iron)));
	}

	TEST_METHOD(MultipleFilters_AllMustPass)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		// Near + high relevance
		FArcKnowledgeEntry Good = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(100, 0, 0), 0.9f);
		// Near + low relevance
		FArcKnowledgeEntry NearLow = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Wood, FVector(200, 0, 0), 0.01f);
		// Far + high relevance
		FArcKnowledgeEntry FarHigh = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Stone, FVector(50000, 0, 0), 0.9f);
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

		FArcKnowledgeQueryContext Ctx = ArcSettlementTestHelpers::MakeContext(FVector::ZeroVector);

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Only entry passing both filters should survive")));
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Iron)));
	}

	TEST_METHOD(MultipleScorers_Multiplicative)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		// Near + high relevance (should score highest)
		FArcKnowledgeEntry Best = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector(100, 0, 0), 0.9f);
		// Far + high relevance
		FArcKnowledgeEntry FarHigh = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Wood, FVector(8000, 0, 0), 0.9f);
		// Near + low relevance
		FArcKnowledgeEntry NearLow = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Stone, FVector(100, 0, 0), 0.1f);
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

		FArcKnowledgeQueryContext Ctx = ArcSettlementTestHelpers::MakeContext(FVector::ZeroVector);

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(3, Results.Num()));
		// Best (near+high) should be first
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Iron),
			TEXT("Near + high relevance should rank first with multiplicative scoring")));
	}

	TEST_METHOD(ZeroScore_ExcludedFromResults)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		// Zero relevance entry
		FArcKnowledgeEntry ZeroRel = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector::ZeroVector, 0.0f);
		FArcKnowledgeEntry Normal = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Wood, FVector::ZeroVector, 0.5f);
		Sub->RegisterKnowledge(ZeroRel);
		Sub->RegisterKnowledge(Normal);

		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;

		FInstancedStruct RelScorer;
		RelScorer.InitializeAs<FArcKnowledgeScorer_Relevance>();
		Query.Scorers.Add(MoveTemp(RelScorer));

		FArcKnowledgeQueryContext Ctx = ArcSettlementTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Zero-score entry should be excluded")));
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Wood)));
	}

	TEST_METHOD(NoMatchingEntries_EmptyResults)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry E = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		Sub->RegisterKnowledge(E);

		FArcKnowledgeQuery Query;
		Query.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Test_Event_Robbery));
		Query.MaxResults = 10;
		FArcKnowledgeQueryContext Ctx = ArcSettlementTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(0, Results.Num(), TEXT("Non-matching tag query should return empty")));
	}

	TEST_METHOD(QueryAfterRemoval_EntryNotReturned)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry E1 = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeEntry E2 = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Wood);
		FArcKnowledgeHandle H1 = Sub->RegisterKnowledge(E1);
		Sub->RegisterKnowledge(E2);

		Sub->RemoveKnowledge(H1);

		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;
		FArcKnowledgeQueryContext Ctx = ArcSettlementTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Removed entry should not appear in queries")));
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Wood)));
	}
};

// ===================================================================
// Actions — Post, Claim, Cancel, Complete
// ===================================================================

TEST_CLASS(ArcSettlement_Actions, "ArcSettlement.Actions")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(PostAction_CreatesQueryableEntry)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Action = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Quest_Deliver);
		FArcKnowledgeHandle Handle = Sub->PostAction(Action);

		ASSERT_THAT(IsTrue(Handle.IsValid()));

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(IsFalse(Retrieved->bClaimed, TEXT("New action should not be claimed")));
	}

	TEST_METHOD(ClaimAction_MarksClaimed)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Action = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Quest_Deliver);
		FArcKnowledgeHandle Handle = Sub->PostAction(Action);

		bool bClaimed = Sub->ClaimAction(Handle, FMassEntityHandle());
		ASSERT_THAT(IsTrue(bClaimed, TEXT("First claim should succeed")));

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(IsTrue(Retrieved->bClaimed, TEXT("Entry should be marked claimed")));
	}

	TEST_METHOD(ClaimAction_DoubleClaimFails)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Action = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Quest_Deliver);
		FArcKnowledgeHandle Handle = Sub->PostAction(Action);

		Sub->ClaimAction(Handle, FMassEntityHandle());
		bool bSecondClaim = Sub->ClaimAction(Handle, FMassEntityHandle());
		ASSERT_THAT(IsFalse(bSecondClaim, TEXT("Double claim should fail")));
	}

	TEST_METHOD(ClaimAction_InvalidHandle_Fails)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		bool bClaimed = Sub->ClaimAction(FArcKnowledgeHandle(99999), FMassEntityHandle());
		ASSERT_THAT(IsFalse(bClaimed, TEXT("Claiming invalid handle should fail")));
	}

	TEST_METHOD(CancelAction_ReleasesClaim)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Action = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Quest_Deliver);
		FArcKnowledgeHandle Handle = Sub->PostAction(Action);

		Sub->ClaimAction(Handle, FMassEntityHandle());
		Sub->CancelAction(Handle);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(IsFalse(Retrieved->bClaimed, TEXT("Cancel should release claim")));
	}

	TEST_METHOD(CancelAction_CanReClaimAfterCancel)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Action = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Quest_Deliver);
		FArcKnowledgeHandle Handle = Sub->PostAction(Action);

		Sub->ClaimAction(Handle, FMassEntityHandle());
		Sub->CancelAction(Handle);
		bool bReClaimed = Sub->ClaimAction(Handle, FMassEntityHandle());

		ASSERT_THAT(IsTrue(bReClaimed, TEXT("Should be able to re-claim after cancel")));
	}

	TEST_METHOD(CompleteAction_RemovesEntry)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry Action = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Quest_Deliver);
		FArcKnowledgeHandle Handle = Sub->PostAction(Action);

		Sub->CompleteAction(Handle);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNull(Retrieved, TEXT("Completed action should be removed")));
	}
};

// ===================================================================
// Settlements — Create, Destroy, Lookup, Spatial
// ===================================================================

TEST_CLASS(ArcSettlement_Settlements, "ArcSettlement.Settlements")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(CreateSettlement_ReturnsValidHandle)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		UArcSettlementDefinition* Def = ArcSettlementTestHelpers::CreateSettlementDef(Spawner, FText::FromString("TestTown"));
		FArcSettlementHandle Handle = Sub->CreateSettlement(Def, FVector(1000, 2000, 0));

		ASSERT_THAT(IsTrue(Handle.IsValid()));
	}

	TEST_METHOD(CreateSettlement_DataRetrievable)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		UArcSettlementDefinition* Def = ArcSettlementTestHelpers::CreateSettlementDef(Spawner, FText::FromString("TestTown"), 3000.0f);
		FArcSettlementHandle Handle = Sub->CreateSettlement(Def, FVector(1000, 2000, 0));

		FArcSettlementData Data;
		bool bFound = Sub->GetSettlementData(Handle, Data);

		ASSERT_THAT(IsTrue(bFound, TEXT("GetSettlementData should succeed for valid handle")));
		ASSERT_THAT(AreEqual(Handle, Data.Handle));
		ASSERT_THAT(IsNear(1000.0f, static_cast<float>(Data.Location.X), 0.01f));
		ASSERT_THAT(IsNear(3000.0f, Data.BoundingRadius, 0.01f));
	}

	TEST_METHOD(CreateSettlement_NullDefinition_StillCreates)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcSettlementHandle Handle = Sub->CreateSettlement(nullptr, FVector(0, 0, 0));
		ASSERT_THAT(IsTrue(Handle.IsValid(), TEXT("Null definition should still create a settlement")));
	}

	TEST_METHOD(DestroySettlement_RemovesFromRegistry)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		UArcSettlementDefinition* Def = ArcSettlementTestHelpers::CreateSettlementDef(Spawner, FText::FromString("TestTown"));
		FArcSettlementHandle Handle = Sub->CreateSettlement(Def, FVector::ZeroVector);
		Sub->DestroySettlement(Handle);

		FArcSettlementData Data;
		bool bFound = Sub->GetSettlementData(Handle, Data);
		ASSERT_THAT(IsFalse(bFound, TEXT("Destroyed settlement should not be findable")));
	}

	TEST_METHOD(DestroySettlement_RemovesAssociatedKnowledge)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		UArcSettlementDefinition* Def = ArcSettlementTestHelpers::CreateSettlementDef(Spawner, FText::FromString("TestTown"));
		FArcSettlementHandle SHandle = Sub->CreateSettlement(Def, FVector::ZeroVector);

		// Register knowledge linked to this settlement
		FArcKnowledgeEntry E1 = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		E1.Settlement = SHandle;
		FArcKnowledgeHandle KH = Sub->RegisterKnowledge(E1);

		// Also register unaffiliated knowledge
		FArcKnowledgeEntry E2 = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Wood);
		FArcKnowledgeHandle KH2 = Sub->RegisterKnowledge(E2);

		Sub->DestroySettlement(SHandle);

		ASSERT_THAT(IsNull(Sub->GetKnowledgeEntry(KH), TEXT("Settlement-linked knowledge should be removed")));
		ASSERT_THAT(IsNotNull(Sub->GetKnowledgeEntry(KH2), TEXT("Unaffiliated knowledge should survive")));
	}

	TEST_METHOD(FindSettlementAt_FindsCorrect)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		UArcSettlementDefinition* Def = ArcSettlementTestHelpers::CreateSettlementDef(Spawner, FText::FromString("Town A"), 5000.0f);
		FArcSettlementHandle HA = Sub->CreateSettlement(Def, FVector(0, 0, 0));

		UArcSettlementDefinition* Def2 = ArcSettlementTestHelpers::CreateSettlementDef(Spawner, FText::FromString("Town B"), 5000.0f);
		FArcSettlementHandle HB = Sub->CreateSettlement(Def2, FVector(20000, 0, 0));

		FArcSettlementHandle Found = Sub->FindSettlementAt(FVector(100, 100, 0));
		ASSERT_THAT(AreEqual(HA, Found, TEXT("Should find Town A at location near its center")));

		FArcSettlementHandle Found2 = Sub->FindSettlementAt(FVector(20100, 0, 0));
		ASSERT_THAT(AreEqual(HB, Found2, TEXT("Should find Town B at location near its center")));
	}

	TEST_METHOD(FindSettlementAt_OutsideBounds_ReturnsInvalid)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		UArcSettlementDefinition* Def = ArcSettlementTestHelpers::CreateSettlementDef(Spawner, FText::FromString("Town"), 1000.0f);
		Sub->CreateSettlement(Def, FVector(0, 0, 0));

		FArcSettlementHandle Found = Sub->FindSettlementAt(FVector(50000, 0, 0));
		ASSERT_THAT(IsFalse(Found.IsValid(), TEXT("Location outside all bounds should return invalid handle")));
	}

	TEST_METHOD(QuerySettlementsInRadius)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		UArcSettlementDefinition* Def = ArcSettlementTestHelpers::CreateSettlementDef(Spawner, FText::FromString("Near"));
		FArcSettlementHandle HNear = Sub->CreateSettlement(Def, FVector(500, 0, 0));

		UArcSettlementDefinition* Def2 = ArcSettlementTestHelpers::CreateSettlementDef(Spawner, FText::FromString("Far"));
		Sub->CreateSettlement(Def2, FVector(50000, 0, 0));

		TArray<FArcSettlementHandle> Found;
		Sub->QuerySettlementsInRadius(FVector::ZeroVector, 5000.0f, Found);

		ASSERT_THAT(AreEqual(1, Found.Num(), TEXT("Only settlement within radius should be returned")));
		ASSERT_THAT(AreEqual(HNear, Found[0]));
	}

	TEST_METHOD(SameSettlementFilter_WithQuery)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		UArcSettlementDefinition* DefA = ArcSettlementTestHelpers::CreateSettlementDef(Spawner, FText::FromString("Town A"));
		FArcSettlementHandle SA = Sub->CreateSettlement(DefA, FVector(0, 0, 0));

		UArcSettlementDefinition* DefB = ArcSettlementTestHelpers::CreateSettlementDef(Spawner, FText::FromString("Town B"));
		FArcSettlementHandle SB = Sub->CreateSettlement(DefB, FVector(20000, 0, 0));

		FArcKnowledgeEntry E1 = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		E1.Settlement = SA;
		Sub->RegisterKnowledge(E1);

		FArcKnowledgeEntry E2 = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Wood);
		E2.Settlement = SB;
		Sub->RegisterKnowledge(E2);

		FArcKnowledgeQuery Query;
		Query.MaxResults = 10;

		FInstancedStruct FilterStruct;
		FilterStruct.InitializeAs<FArcKnowledgeFilter_SameSettlement>();
		Query.Filters.Add(MoveTemp(FilterStruct));

		FArcKnowledgeQueryContext Ctx = ArcSettlementTestHelpers::MakeContext();
		Ctx.QuerierSettlement = SA;

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Only entries from querier's settlement should pass")));
		ASSERT_THAT(IsTrue(Results[0].Entry.Tags.HasTagExact(TAG_Test_Resource_Iron)));
	}

	TEST_METHOD(InitialKnowledge_RegisteredOnCreate)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		UArcSettlementDefinition* Def = ArcSettlementTestHelpers::CreateSettlementDef(Spawner, FText::FromString("Town"));
		FArcKnowledgeEntry InitEntry;
		InitEntry.Tags.AddTag(TAG_Test_Capability_Forge);
		Def->InitialKnowledge.Add(InitEntry);

		FArcSettlementHandle Handle = Sub->CreateSettlement(Def, FVector(1000, 0, 0));

		// Query for the initial knowledge
		FArcKnowledgeQuery Query;
		Query.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Test_Capability_Forge));
		Query.MaxResults = 10;
		FArcKnowledgeQueryContext Ctx = ArcSettlementTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Initial knowledge should be registered")));
		ASSERT_THAT(AreEqual(Handle, Results[0].Entry.Settlement, TEXT("Initial knowledge should be assigned to the settlement")));
	}
};

// ===================================================================
// Regions — Create, Destroy, Auto-Assignment
// ===================================================================

TEST_CLASS(ArcSettlement_Regions, "ArcSettlement.Regions")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(CreateRegion_ReturnsValidHandle)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		UArcRegionDefinition* Def = ArcSettlementTestHelpers::CreateRegionDef(Spawner, FText::FromString("TestRegion"));
		FArcRegionHandle Handle = Sub->CreateRegion(Def, FVector::ZeroVector, 50000.0f);

		ASSERT_THAT(IsTrue(Handle.IsValid()));
	}

	TEST_METHOD(CreateRegion_DataRetrievable)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		UArcRegionDefinition* Def = ArcSettlementTestHelpers::CreateRegionDef(Spawner, FText::FromString("TestRegion"));
		FArcRegionHandle Handle = Sub->CreateRegion(Def, FVector(1000, 2000, 0), 30000.0f);

		FArcRegionData Data;
		bool bFound = Sub->GetRegionData(Handle, Data);

		ASSERT_THAT(IsTrue(bFound));
		ASSERT_THAT(AreEqual(Handle, Data.Handle));
		ASSERT_THAT(IsNear(1000.0f, static_cast<float>(Data.Center.X), 0.01f));
		ASSERT_THAT(IsNear(30000.0f, Data.Radius, 0.01f));
	}

	TEST_METHOD(DestroyRegion_RemovesFromRegistry)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		UArcRegionDefinition* Def = ArcSettlementTestHelpers::CreateRegionDef(Spawner, FText::FromString("TestRegion"));
		FArcRegionHandle Handle = Sub->CreateRegion(Def, FVector::ZeroVector, 50000.0f);
		Sub->DestroyRegion(Handle);

		FArcRegionData Data;
		ASSERT_THAT(IsFalse(Sub->GetRegionData(Handle, Data), TEXT("Destroyed region should not be findable")));
	}

	TEST_METHOD(SettlementAutoAssignedToRegion)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		// Create region first
		UArcRegionDefinition* RegDef = ArcSettlementTestHelpers::CreateRegionDef(Spawner, FText::FromString("Region"));
		FArcRegionHandle RHandle = Sub->CreateRegion(RegDef, FVector::ZeroVector, 50000.0f);

		// Create settlement inside the region
		UArcSettlementDefinition* SetDef = ArcSettlementTestHelpers::CreateSettlementDef(Spawner, FText::FromString("Town"));
		FArcSettlementHandle SHandle = Sub->CreateSettlement(SetDef, FVector(1000, 0, 0));

		// Verify region knows about the settlement
		FArcRegionData RegData;
		Sub->GetRegionData(RHandle, RegData);
		ASSERT_THAT(IsTrue(RegData.Settlements.Contains(SHandle), TEXT("Region should contain the settlement")));

		// Verify settlement knows its region
		FArcRegionHandle FoundRegion = Sub->FindRegionForSettlement(SHandle);
		ASSERT_THAT(AreEqual(RHandle, FoundRegion, TEXT("Settlement should know its region")));
	}

	TEST_METHOD(SettlementOutsideRegion_NotAssigned)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		UArcRegionDefinition* RegDef = ArcSettlementTestHelpers::CreateRegionDef(Spawner, FText::FromString("Region"));
		FArcRegionHandle RHandle = Sub->CreateRegion(RegDef, FVector::ZeroVector, 1000.0f);

		UArcSettlementDefinition* SetDef = ArcSettlementTestHelpers::CreateSettlementDef(Spawner, FText::FromString("FarTown"));
		FArcSettlementHandle SHandle = Sub->CreateSettlement(SetDef, FVector(50000, 0, 0));

		FArcRegionData RegData;
		Sub->GetRegionData(RHandle, RegData);
		ASSERT_THAT(IsFalse(RegData.Settlements.Contains(SHandle), TEXT("Settlement outside region should not be assigned")));

		FArcRegionHandle FoundRegion = Sub->FindRegionForSettlement(SHandle);
		ASSERT_THAT(IsFalse(FoundRegion.IsValid(), TEXT("Settlement should have no region")));
	}

	TEST_METHOD(RegionCreatedAfterSettlement_PicksUpExisting)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		// Create settlement first
		UArcSettlementDefinition* SetDef = ArcSettlementTestHelpers::CreateSettlementDef(Spawner, FText::FromString("Town"));
		FArcSettlementHandle SHandle = Sub->CreateSettlement(SetDef, FVector(500, 0, 0));

		// Create region after — should pick up existing settlement
		UArcRegionDefinition* RegDef = ArcSettlementTestHelpers::CreateRegionDef(Spawner, FText::FromString("Region"));
		FArcRegionHandle RHandle = Sub->CreateRegion(RegDef, FVector::ZeroVector, 50000.0f);

		FArcRegionData RegData;
		Sub->GetRegionData(RHandle, RegData);
		ASSERT_THAT(IsTrue(RegData.Settlements.Contains(SHandle),
			TEXT("Region created after settlement should still find existing settlement inside it")));
	}

	TEST_METHOD(DestroySettlement_RemovedFromRegion)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		UArcRegionDefinition* RegDef = ArcSettlementTestHelpers::CreateRegionDef(Spawner, FText::FromString("Region"));
		FArcRegionHandle RHandle = Sub->CreateRegion(RegDef, FVector::ZeroVector, 50000.0f);

		UArcSettlementDefinition* SetDef = ArcSettlementTestHelpers::CreateSettlementDef(Spawner, FText::FromString("Town"));
		FArcSettlementHandle SHandle = Sub->CreateSettlement(SetDef, FVector(500, 0, 0));

		Sub->DestroySettlement(SHandle);

		FArcRegionData RegData;
		Sub->GetRegionData(RHandle, RegData);
		ASSERT_THAT(IsFalse(RegData.Settlements.Contains(SHandle),
			TEXT("Destroyed settlement should be removed from region")));
	}

	TEST_METHOD(DestroyRegion_ClearsSettlementRegionReference)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		UArcRegionDefinition* RegDef = ArcSettlementTestHelpers::CreateRegionDef(Spawner, FText::FromString("Region"));
		FArcRegionHandle RHandle = Sub->CreateRegion(RegDef, FVector::ZeroVector, 50000.0f);

		UArcSettlementDefinition* SetDef = ArcSettlementTestHelpers::CreateSettlementDef(Spawner, FText::FromString("Town"));
		FArcSettlementHandle SHandle = Sub->CreateSettlement(SetDef, FVector(500, 0, 0));

		// Verify it's assigned
		ASSERT_THAT(AreEqual(RHandle, Sub->FindRegionForSettlement(SHandle)));

		Sub->DestroyRegion(RHandle);

		// Settlement should no longer reference the destroyed region
		FArcRegionHandle FoundRegion = Sub->FindRegionForSettlement(SHandle);
		ASSERT_THAT(IsFalse(FoundRegion.IsValid(), TEXT("Settlement should lose region reference when region is destroyed")));
	}
};

// ===================================================================
// Relevance Decay — Tick behavior
// ===================================================================

TEST_CLASS(ArcSettlement_RelevanceDecay, "ArcSettlement.RelevanceDecay")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(Tick_DecaysRelevance)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		// Configure fast decay for testing
		Sub->RelevanceDecayRate = 0.5f;
		Sub->DecayTickInterval = 1.0f;
		Sub->MinRelevanceThreshold = 0.01f;

		FArcKnowledgeEntry E = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector::ZeroVector, 1.0f);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);

		// Tick once — should trigger decay
		Sub->Tick(1.0f);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved, TEXT("Entry should still exist after one decay")));
		ASSERT_THAT(IsTrue(Retrieved->Relevance < 1.0f, TEXT("Relevance should have decayed")));
		ASSERT_THAT(IsNear(0.5f, Retrieved->Relevance, 0.01f, TEXT("Relevance should decay by rate * interval")));
	}

	TEST_METHOD(Tick_RemovesBelowThreshold)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		Sub->RelevanceDecayRate = 1.0f;  // Decay by 1.0 per second
		Sub->DecayTickInterval = 1.0f;
		Sub->MinRelevanceThreshold = 0.5f;

		FArcKnowledgeEntry E = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector::ZeroVector, 0.8f);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);

		// After tick: 0.8 - 1.0 = -0.2, below threshold -> removed
		Sub->Tick(1.0f);

		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNull(Retrieved, TEXT("Entry below threshold should be auto-removed")));
	}

	TEST_METHOD(Tick_DecayInterval_OnlyDecaysAfterInterval)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		Sub->RelevanceDecayRate = 0.5f;
		Sub->DecayTickInterval = 5.0f;
		Sub->MinRelevanceThreshold = 0.01f;

		FArcKnowledgeEntry E = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector::ZeroVector, 1.0f);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);

		// Small tick — not enough to trigger decay
		Sub->Tick(1.0f);
		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(IsNear(1.0f, Retrieved->Relevance, 0.001f, TEXT("Relevance should not decay before interval")));

		// Tick enough to trigger
		Sub->Tick(4.0f);
		Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(IsTrue(Retrieved->Relevance < 1.0f, TEXT("Relevance should decay after full interval")));
	}

	TEST_METHOD(Tick_ZeroDecayRate_NoDecay)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		Sub->RelevanceDecayRate = 0.0f;
		Sub->DecayTickInterval = 1.0f;

		FArcKnowledgeEntry E = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector::ZeroVector, 0.5f);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);

		Sub->Tick(10.0f);
		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved));
		ASSERT_THAT(IsNear(0.5f, Retrieved->Relevance, 0.001f, TEXT("Zero decay rate should not change relevance")));
	}

	TEST_METHOD(Refresh_PreventsDecayRemoval)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		Sub->RelevanceDecayRate = 0.3f;
		Sub->DecayTickInterval = 1.0f;
		Sub->MinRelevanceThreshold = 0.01f;

		FArcKnowledgeEntry E = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron, FVector::ZeroVector, 0.5f);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);

		// Tick once (0.5 - 0.3 = 0.2)
		Sub->Tick(1.0f);
		ASSERT_THAT(IsNotNull(Sub->GetKnowledgeEntry(Handle)));

		// Refresh back to full
		Sub->RefreshKnowledge(Handle, 1.0f);

		// Tick again (1.0 - 0.3 = 0.7)
		Sub->Tick(1.0f);
		const FArcKnowledgeEntry* Retrieved = Sub->GetKnowledgeEntry(Handle);
		ASSERT_THAT(IsNotNull(Retrieved, TEXT("Refreshed entry should survive")));
		ASSERT_THAT(IsNear(0.7f, Retrieved->Relevance, 0.01f));
	}
};

// ===================================================================
// Tag Index Integrity — verify index stays consistent after mutations
// ===================================================================

TEST_CLASS(ArcSettlement_TagIndex, "ArcSettlement.Knowledge.TagIndex")
{
	FActorTestSpawner Spawner;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
	}

	TEST_METHOD(Update_TagsReIndexed)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		// Register with Iron tag
		FArcKnowledgeEntry E = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);

		// Update to Wood tag
		FArcKnowledgeEntry Updated = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Wood);
		Sub->UpdateKnowledge(Handle, Updated);

		// Query for Iron — should not find it
		FArcKnowledgeQuery IronQuery;
		IronQuery.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Test_Resource_Iron));
		IronQuery.MaxResults = 10;
		FArcKnowledgeQueryContext Ctx = ArcSettlementTestHelpers::MakeContext();

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
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		FArcKnowledgeEntry E = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);
		Sub->RemoveKnowledge(Handle);

		FArcKnowledgeQuery Query;
		Query.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Test_Resource_Iron));
		Query.MaxResults = 10;
		FArcKnowledgeQueryContext Ctx = ArcSettlementTestHelpers::MakeContext();

		TArray<FArcKnowledgeQueryResult> Results;
		Sub->QueryKnowledge(Query, Ctx, Results);
		ASSERT_THAT(AreEqual(0, Results.Num(), TEXT("Removed entry should not appear in tag queries")));
	}

	TEST_METHOD(Update_SettlementIndexRebuilt)
	{
		UArcSettlementSubsystem* Sub = ArcSettlementTestHelpers::GetSubsystem(Spawner);
		ASSERT_THAT(IsNotNull(Sub));

		UArcSettlementDefinition* DefA = ArcSettlementTestHelpers::CreateSettlementDef(Spawner, FText::FromString("A"));
		FArcSettlementHandle SA = Sub->CreateSettlement(DefA, FVector::ZeroVector);

		UArcSettlementDefinition* DefB = ArcSettlementTestHelpers::CreateSettlementDef(Spawner, FText::FromString("B"));
		FArcSettlementHandle SB = Sub->CreateSettlement(DefB, FVector(20000, 0, 0));

		// Register entry for settlement A
		FArcKnowledgeEntry E = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		E.Settlement = SA;
		FArcKnowledgeHandle Handle = Sub->RegisterKnowledge(E);

		// Update to settlement B
		FArcKnowledgeEntry Updated = ArcSettlementTestHelpers::MakeEntry(TAG_Test_Resource_Iron);
		Updated.Settlement = SB;
		Sub->UpdateKnowledge(Handle, Updated);

		// Query with SameSettlement filter for A — should not find it
		FArcKnowledgeQuery QueryA;
		QueryA.MaxResults = 10;
		FInstancedStruct FilterA;
		FilterA.InitializeAs<FArcKnowledgeFilter_SameSettlement>();
		QueryA.Filters.Add(MoveTemp(FilterA));
		FArcKnowledgeQueryContext CtxA = ArcSettlementTestHelpers::MakeContext();
		CtxA.QuerierSettlement = SA;

		TArray<FArcKnowledgeQueryResult> ResultsA;
		Sub->QueryKnowledge(QueryA, CtxA, ResultsA);
		ASSERT_THAT(AreEqual(0, ResultsA.Num(), TEXT("Entry should no longer belong to settlement A")));

		// Query with SameSettlement filter for B — should find it
		FArcKnowledgeQuery QueryB;
		QueryB.MaxResults = 10;
		FInstancedStruct FilterB;
		FilterB.InitializeAs<FArcKnowledgeFilter_SameSettlement>();
		QueryB.Filters.Add(MoveTemp(FilterB));
		FArcKnowledgeQueryContext CtxB = ArcSettlementTestHelpers::MakeContext();
		CtxB.QuerierSettlement = SB;

		TArray<FArcKnowledgeQueryResult> ResultsB;
		Sub->QueryKnowledge(QueryB, CtxB, ResultsB);
		ASSERT_THAT(AreEqual(1, ResultsB.Num(), TEXT("Entry should now belong to settlement B")));
	}
};
