// Copyright Lukasz Baran. All Rights Reserved.

#include "GameplayDebuggerCategory_ArcKnowledge.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeRTree.h"
#include "ArcKnowledgeEntry.h"
#include "GameFramework/Actor.h"
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

	// Get debug center for spatial queries
	FVector DebugCenter = FVector::ZeroVector;
	if (DebugActor)
	{
		DebugCenter = DebugActor->GetActorLocation();
	}

	constexpr float DebugRadius = 10000.0f;
	constexpr int32 MaxDebugEntries = 500;

	// ---- Dynamic entries: query via radius to avoid triggering Phase 2b promotion ----
	TArray<FArcKnowledgeHandle> DynamicHandles;
	Subsystem->QueryKnowledgeInRadius(DebugCenter, DebugRadius, DynamicHandles, FGameplayTagQuery());

	int32 ClaimedCount = 0;
	int32 DynamicDrawn = 0;
	for (const FArcKnowledgeHandle& Handle : DynamicHandles)
	{
		const FArcKnowledgeEntry* Entry = Subsystem->GetKnowledgeEntry(Handle);
		if (!Entry)
		{
			continue;
		}

		if (Entry->bClaimed)
		{
			ClaimedCount++;
		}

		if (DynamicDrawn < MaxDebugEntries)
		{
			const FColor EntryColor = Entry->bClaimed
				? FColor(180, 80, 80)
				: ArcKnowledgeDebug::TagsToColor(Entry->Tags);

			AddShape(FGameplayDebuggerShape::MakePoint(
				Entry->Location + FVector(0, 0, 30.0f),
				10.0f, EntryColor));
			DynamicDrawn++;
		}
	}

	// ---- Static entries: query R-tree directly, no promotion ----
	const int32 StaticRTreeCount = Subsystem->GetStaticRTree().GetEntryCount();
	TArray<FArcKnowledgeRTreeLeafEntry> StaticLeafEntries;
	Subsystem->GetStaticRTree().QuerySphere(DebugCenter, DebugRadius, FArcKnowledgeTagBitmask(), StaticLeafEntries);

	int32 StaticDrawn = 0;
	for (const FArcKnowledgeRTreeLeafEntry& LeafEntry : StaticLeafEntries)
	{
		if (StaticDrawn >= MaxDebugEntries)
		{
			break;
		}

		AddShape(FGameplayDebuggerShape::MakePoint(
			LeafEntry.Position + FVector(0, 0, 30.0f),
			6.0f, FColor(80, 180, 80)));
		StaticDrawn++;
	}

	// ---- Header ----
	AddTextLine(FString::Printf(TEXT("{white}Arc Knowledge Debug")));
	AddTextLine(FString::Printf(TEXT("{white}Dynamic nearby: {yellow}%d  {white}Claimed: {yellow}%d"), DynamicHandles.Num(), ClaimedCount));
	AddTextLine(FString::Printf(TEXT("{white}Static nearby: {green}%d  {white}R-tree total: {green}%d"), StaticLeafEntries.Num(), StaticRTreeCount));
}

void FGameplayDebuggerCategory_ArcKnowledge::DrawData(
	APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	FGameplayDebuggerCategory::DrawData(OwnerPC, CanvasContext);
}

#endif // WITH_GAMEPLAY_DEBUGGER
