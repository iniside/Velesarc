// Copyright Lukasz Baran. All Rights Reserved.

#include "GameplayDebuggerCategory_ArcKnowledge.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntry.h"
#include "Engine/World.h"

namespace ArcKnowledgeDebug
{
	static FColor TagsToColor(const FGameplayTagContainer& Tags)
	{
		// Simple hash-based color for visual distinction
		const uint32 Hash = GetTypeHash(Tags.ToString());
		return FColor(
			50 + (Hash & 0xFF) % 200,
			50 + ((Hash >> 8) & 0xFF) % 200,
			50 + ((Hash >> 16) & 0xFF) % 200
		);
	}
} // namespace ArcKnowledgeDebug

FGameplayDebuggerCategory_ArcKnowledge::FGameplayDebuggerCategory_ArcKnowledge()
	: Super()
{
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_ArcKnowledge::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_ArcKnowledge());
}

void FGameplayDebuggerCategory_ArcKnowledge::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	const UWorld* World = GetDataWorld(OwnerPC, DebugActor);
	if (!World)
	{
		return;
	}

	const UArcKnowledgeSubsystem* Subsystem = UWorld::GetSubsystem<UArcKnowledgeSubsystem>(World);
	if (!Subsystem)
	{
		AddTextLine(FString::Printf(TEXT("{red}Knowledge Subsystem not available")));
		return;
	}

	// Query all knowledge entries (empty tag query matches everything)
	FArcKnowledgeQuery AllQuery;
	AllQuery.MaxResults = 500;

	FArcKnowledgeQueryContext QueryContext;
	QueryContext.QueryOrigin = FVector::ZeroVector;
	QueryContext.World = const_cast<UWorld*>(World);
	QueryContext.CurrentTime = World->GetTimeSeconds();

	TArray<FArcKnowledgeQueryResult> AllResults;
	Subsystem->QueryKnowledge(AllQuery, QueryContext, AllResults);

	// Count claimed entries
	int32 ClaimedCount = 0;
	for (const FArcKnowledgeQueryResult& Result : AllResults)
	{
		if (Result.Entry.bClaimed)
		{
			ClaimedCount++;
		}
	}

	// ---- Header ----
	AddTextLine(FString::Printf(TEXT("{white}Arc Knowledge Debug")));
	AddTextLine(FString::Printf(TEXT("{white}Total entries: {yellow}%d"), AllResults.Num()));
	AddTextLine(FString::Printf(TEXT("{white}Claimed advertisements: {yellow}%d"), ClaimedCount));

	// ---- Draw all knowledge entry locations ----
	for (const FArcKnowledgeQueryResult& Result : AllResults)
	{
		const FColor EntryColor = Result.Entry.bClaimed
			? FColor(180, 80, 80)  // Red-ish for claimed
			: ArcKnowledgeDebug::TagsToColor(Result.Entry.Tags);

		AddShape(FGameplayDebuggerShape::MakePoint(
			Result.Entry.Location + FVector(0, 0, 30.0f),
			10.0f, EntryColor));
	}
}

void FGameplayDebuggerCategory_ArcKnowledge::DrawData(
	APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	FGameplayDebuggerCategory::DrawData(OwnerPC, CanvasContext);
}

#endif // WITH_GAMEPLAY_DEBUGGER
