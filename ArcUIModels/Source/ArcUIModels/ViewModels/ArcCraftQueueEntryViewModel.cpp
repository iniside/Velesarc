// Copyright Lukasz Baran. All Rights Reserved.

#include "ViewModels/ArcCraftQueueEntryViewModel.h"

#include "ArcCraft/Station/ArcCraftStationTypes.h"
#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"

namespace ArcCraftQueueEntryViewModelInternal
{
	FText FormatTimeRemaining(float Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::CeilToInt(Seconds));
		const int32 Hours = Total / 3600;
		const int32 Mins  = (Total % 3600) / 60;
		const int32 Secs  = Total % 60;
		return FText::FromString(FString::Printf(TEXT("%02d:%02d:%02d"), Hours, Mins, Secs));
	}

	int32 WholeSecondsRemaining(float Seconds)
	{
		return FMath::Max(0, FMath::CeilToInt(Seconds));
	}
}

void UArcCraftQueueEntryViewModel::RefreshFrom(
	const FArcCraftQueueEntry& Entry,
	const FArcRecipeDefinitionData* Recipe,
	bool bIsActiveEntry)
{
	// --- RecipeId ---
	FArcNamedPrimaryAssetId NewRecipeId;
	NewRecipeId.AssetId = Entry.RecipeId;
	UE_MVVM_SET_PROPERTY_VALUE(RecipeId, NewRecipeId);

	// --- DisplayName ---
	FText NewDisplayName;
	if (Recipe != nullptr)
	{
		NewDisplayName = Recipe->GetDisplayName();
	}
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, NewDisplayName);

	// --- Progress + anchor + TimeRemaining ---
	float NewProgress = 0.0f;
	float InitialRemaining = 0.0f;

	if (bIsActiveEntry && Recipe != nullptr && Recipe->CraftTime > 0.0f)
	{
		NewProgress = FMath::Clamp(Entry.ElapsedTickTime / Recipe->CraftTime, 0.0f, 1.0f);
		InitialRemaining = FMath::Max(0.0f, Recipe->CraftTime - Entry.ElapsedTickTime);

		AnchorElapsedTime    = Entry.ElapsedTickTime;
		AnchorWallTimeLocal  = FPlatformTime::Seconds();
		AnchorStartTimestamp = Entry.StartTimestamp;
		CachedCraftTime      = Recipe->CraftTime;
		CachedTimeMode       = Entry.TimeMode;
	}
	else
	{
		AnchorElapsedTime    = 0.0f;
		AnchorWallTimeLocal  = 0.0;
		AnchorStartTimestamp = 0;
		CachedCraftTime      = 0.0f;
		CachedTimeMode       = EArcCraftStationTimeMode::AutoTick;
	}

	UE_MVVM_SET_PROPERTY_VALUE(Progress, NewProgress);

	const int32 InitialWholeRemaining =
		ArcCraftQueueEntryViewModelInternal::WholeSecondsRemaining(InitialRemaining);
	if (InitialWholeRemaining != LastDisplayedRemainingSeconds)
	{
		LastDisplayedRemainingSeconds = InitialWholeRemaining;
		const FText NewText = ArcCraftQueueEntryViewModelInternal::FormatTimeRemaining(InitialRemaining);
		UE_MVVM_SET_PROPERTY_VALUE(TimeRemaining, NewText);
	}

	// --- Amount / CompletedAmount ---
	UE_MVVM_SET_PROPERTY_VALUE(Amount, Entry.Amount);
	UE_MVVM_SET_PROPERTY_VALUE(CompletedAmount, Entry.CompletedAmount);

	// --- bIsActive ---
	UE_MVVM_SET_PROPERTY_VALUE(bIsActive, bIsActiveEntry);

	// --- bOutputBlocked ---
	UE_MVVM_SET_PROPERTY_VALUE(bOutputBlocked, Entry.bOutputBlocked);

	// --- bIsStaged --- committed entries are never staged
	UE_MVVM_SET_PROPERTY_VALUE(bIsStaged, false);
}

void UArcCraftQueueEntryViewModel::UpdateSimulatedProgress(
	double LocalNowSeconds,
	int64 UtcNowUnixSeconds)
{
	if (!bIsActive || CachedCraftTime <= 0.0f)
	{
		return;
	}

	float SimulatedElapsed = AnchorElapsedTime;
	if (CachedTimeMode == EArcCraftStationTimeMode::AutoTick)
	{
		if (AnchorWallTimeLocal > 0.0)
		{
			SimulatedElapsed += static_cast<float>(LocalNowSeconds - AnchorWallTimeLocal);
		}
	}
	else // InteractionCheck — use UTC anchor (StartTimestamp) since the server's
	     // completion check is wall-clock based.
	{
		if (AnchorStartTimestamp > 0)
		{
			SimulatedElapsed = static_cast<float>(UtcNowUnixSeconds - AnchorStartTimestamp);
		}
	}

	SimulatedElapsed = FMath::Clamp(SimulatedElapsed, 0.0f, CachedCraftTime);

	const float NewProgress = SimulatedElapsed / CachedCraftTime;
	const float Remaining   = CachedCraftTime - SimulatedElapsed;

	UE_MVVM_SET_PROPERTY_VALUE(Progress, NewProgress);

	const int32 WholeRemaining =
		ArcCraftQueueEntryViewModelInternal::WholeSecondsRemaining(Remaining);
	if (WholeRemaining != LastDisplayedRemainingSeconds)
	{
		LastDisplayedRemainingSeconds = WholeRemaining;
		const FText NewText = ArcCraftQueueEntryViewModelInternal::FormatTimeRemaining(Remaining);
		UE_MVVM_SET_PROPERTY_VALUE(TimeRemaining, NewText);
	}
}
