// Copyright Lukasz Baran. All Rights Reserved.

#include "GameplayDebuggerCategory_ArcSettlement.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "ArcSettlementSubsystem.h"
#include "ArcSettlementData.h"
#include "ArcKnowledgeEntry.h"
#include "ArcKnowledgeFilter.h"
#include "Engine/World.h"

namespace ArcSettlementDebug
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
} // namespace ArcSettlementDebug

FGameplayDebuggerCategory_ArcSettlement::FGameplayDebuggerCategory_ArcSettlement()
	: Super()
{
	BindKeyPress(EKeys::S.GetFName(), FGameplayDebuggerInputModifier::Shift, this,
		&FGameplayDebuggerCategory_ArcSettlement::OnCycleSettlement, EGameplayDebuggerInputMode::Replicated);
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_ArcSettlement::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_ArcSettlement());
}

void FGameplayDebuggerCategory_ArcSettlement::OnCycleSettlement()
{
	bCycleSettlement = true;
}

void FGameplayDebuggerCategory_ArcSettlement::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	const UWorld* World = GetDataWorld(OwnerPC, DebugActor);
	if (!World)
	{
		return;
	}

	const UArcSettlementSubsystem* Subsystem = UWorld::GetSubsystem<UArcSettlementSubsystem>(World);
	if (!Subsystem)
	{
		AddTextLine(FString::Printf(TEXT("{red}Settlement Subsystem not available")));
		return;
	}

	const auto& AllSettlements = Subsystem->GetAllSettlements();

	// Build sorted settlement list for stable cycling
	TArray<FArcSettlementHandle> SettlementHandles;
	AllSettlements.GetKeys(SettlementHandles);
	SettlementHandles.Sort([](const FArcSettlementHandle& A, const FArcSettlementHandle& B)
	{
		return A.GetValue() < B.GetValue();
	});

	// Handle settlement cycling
	if (bCycleSettlement)
	{
		bCycleSettlement = false;
		if (SettlementHandles.Num() > 0)
		{
			SelectedSettlementIndex = (SelectedSettlementIndex + 1) % SettlementHandles.Num();
			SelectedSettlement = SettlementHandles[SelectedSettlementIndex];
		}
	}

	// Validate selected settlement still exists
	if (SelectedSettlement.IsValid() && !AllSettlements.Contains(SelectedSettlement))
	{
		SelectedSettlement = FArcSettlementHandle();
		SelectedSettlementIndex = 0;
	}

	// Auto-select first settlement if none selected
	if (!SelectedSettlement.IsValid() && SettlementHandles.Num() > 0)
	{
		SelectedSettlement = SettlementHandles[0];
		SelectedSettlementIndex = 0;
	}

	// ---- Header ----
	AddTextLine(FString::Printf(TEXT("{white}Arc Settlement Debug  |  {cyan}Shift+S{white} to cycle settlements")));
	AddTextLine(FString::Printf(TEXT("{white}Settlements: {yellow}%d"), SettlementHandles.Num()));

	// ---- Draw all settlements as circles ----
	for (const auto& Pair : AllSettlements)
	{
		const FArcSettlementData& Data = Pair.Value;
		const bool bSelected = (Pair.Key == SelectedSettlement);
		const FColor Color = bSelected ? FColor::Green : FColor::Cyan;
		const float Thickness = bSelected ? 4.0f : 2.0f;

		// Settlement circle
		AddShape(FGameplayDebuggerShape::MakeCircle(
			Data.Location + FVector(0, 0, 50.0f),
			FVector::UpVector,
			Data.BoundingRadius,
			Color,
			FString::Printf(TEXT("%s [%d]"), *Data.DisplayName.ToString(), Data.Handle.GetValue())));

		// Center marker
		AddShape(FGameplayDebuggerShape::MakePoint(
			Data.Location + FVector(0, 0, 100.0f),
			15.0f, Color));
	}

	// ---- Selected settlement details ----
	if (SelectedSettlement.IsValid())
	{
		FArcSettlementData SelectedData;
		if (Subsystem->GetSettlementData(SelectedSettlement, SelectedData))
		{
			AddTextLine(FString::Printf(TEXT("")));
			AddTextLine(FString::Printf(TEXT("{green}Selected: {yellow}%s{white} (Handle: %d)"),
				*SelectedData.DisplayName.ToString(), SelectedData.Handle.GetValue()));
			AddTextLine(FString::Printf(TEXT("{white}Location: ({yellow}%.0f, %.0f, %.0f{white})  Radius: {yellow}%.0f"),
				SelectedData.Location.X, SelectedData.Location.Y, SelectedData.Location.Z,
				SelectedData.BoundingRadius));
			AddTextLine(FString::Printf(TEXT("{white}Tags: {cyan}%s"),
				*SelectedData.Tags.ToString()));

			// Region info
			const FArcRegionHandle Region = Subsystem->FindRegionForSettlement(SelectedSettlement);
			if (Region.IsValid())
			{
				FArcRegionData RegionData;
				if (Subsystem->GetRegionData(Region, RegionData))
				{
					AddTextLine(FString::Printf(TEXT("{white}Region: {yellow}%s{white} (Handle: %d)"),
						*RegionData.DisplayName.ToString(), RegionData.Handle.GetValue()));
				}
			}
			else
			{
				AddTextLine(FString::Printf(TEXT("{grey}Region: none")));
			}

			// Knowledge entries for this settlement — gather from subsystem
			// We iterate visible entries and count those belonging to this settlement
			int32 KnowledgeCount = 0;
			int32 ActionCount = 0;
			int32 ClaimedCount = 0;

			// Use a simple approach: query all entries with no tag filter, same settlement
			FArcKnowledgeQuery CountQuery;
			// Empty tag query matches everything
			CountQuery.MaxResults = 100;

			FInstancedStruct SameSettlementFilter;
			SameSettlementFilter.InitializeAs<FArcKnowledgeFilter_SameSettlement>();
			CountQuery.Filters.Add(MoveTemp(SameSettlementFilter));

			FArcKnowledgeQueryContext CountContext;
			CountContext.QueryOrigin = SelectedData.Location;
			CountContext.World = const_cast<UWorld*>(World);
			CountContext.CurrentTime = World->GetTimeSeconds();
			CountContext.QuerierSettlement = SelectedSettlement;

			TArray<FArcKnowledgeQueryResult> KnowledgeResults;
			Subsystem->QueryKnowledge(CountQuery, CountContext, KnowledgeResults);

			KnowledgeCount = KnowledgeResults.Num();

			for (const FArcKnowledgeQueryResult& Result : KnowledgeResults)
			{
				if (Result.Entry.bClaimed)
				{
					ClaimedCount++;
				}
			}

			AddTextLine(FString::Printf(TEXT("{white}Knowledge entries: {yellow}%d{white}  (Claimed actions: {yellow}%d{white})"),
				KnowledgeCount, ClaimedCount));

			// Draw knowledge entry locations
			for (const FArcKnowledgeQueryResult& Result : KnowledgeResults)
			{
				const FColor EntryColor = Result.Entry.bClaimed
					? FColor(180, 80, 80)  // Red-ish for claimed
					: ArcSettlementDebug::TagsToColor(Result.Entry.Tags);

				AddShape(FGameplayDebuggerShape::MakePoint(
					Result.Entry.Location + FVector(0, 0, 30.0f),
					10.0f, EntryColor));
			}
		}
	}
}

void FGameplayDebuggerCategory_ArcSettlement::DrawData(
	APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	FGameplayDebuggerCategory::DrawData(OwnerPC, CanvasContext);
}

#endif // WITH_GAMEPLAY_DEBUGGER
