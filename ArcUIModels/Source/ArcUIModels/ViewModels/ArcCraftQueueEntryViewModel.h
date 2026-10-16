// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcNamedPrimaryAssetId.h"
#include "MVVMViewModelBase.h"
#include "ArcCraft/Station/ArcCraftStationTypes.h"

#include "ArcCraftQueueEntryViewModel.generated.h"

struct FArcCraftQueueEntry;
struct FArcRecipeDefinitionData;

UCLASS(BlueprintType)
class ARCUIMODELS_API UArcCraftQueueEntryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FArcNamedPrimaryAssetId RecipeId;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText DisplayName;

	/** Craft progress in [0, 1]. Only meaningful for the active entry. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	float Progress = 0.0f;

	/** Total items requested for this queue entry. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	int32 Amount = 0;

	/** How many have completed so far. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	int32 CompletedAmount = 0;

	/** True when this is the front-of-queue entry currently being crafted. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bIsActive = false;

	/** True when the crafted output cannot be deposited and is held at the station. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bOutputBlocked = false;

	/** True when this entry represents a locally-staged (not yet committed) craft plan. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bIsStaged = false;

	// New FieldNotify property — formatted "HH:MM:SS" remaining.
	// Refreshed at most once per simulated whole-second to avoid per-tick FText churn.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText TimeRemaining;

	const FArcNamedPrimaryAssetId& GetRecipeId() const { return RecipeId; }
	const FText& GetDisplayName() const { return DisplayName; }
	float GetProgress() const { return Progress; }
	int32 GetAmount() const { return Amount; }
	int32 GetCompletedAmount() const { return CompletedAmount; }
	bool GetbIsActive() const { return bIsActive; }
	bool GetbOutputBlocked() const { return bOutputBlocked; }
	bool GetbIsStaged() const { return bIsStaged; }
	const FText& GetTimeRemaining() const { return TimeRemaining; }

	// Plain (non-FieldNotify) anchor state — populated by RefreshFrom, consumed by
	// UpdateSimulatedProgress. Not UPROPERTY so the editor cannot accidentally override it.
	float  AnchorElapsedTime       = 0.0f;
	double AnchorWallTimeLocal     = 0.0;
	int64  AnchorStartTimestamp    = 0;
	float  CachedCraftTime         = 0.0f;
	EArcCraftStationTimeMode CachedTimeMode = EArcCraftStationTimeMode::AutoTick;
	int32  LastDisplayedRemainingSeconds = -1;

	// Called by FArcCraftingStructModel::Tick on the active entry only.
	// LocalNowSeconds = FPlatformTime::Seconds() captured once per tick by caller.
	// UtcNowUnixSeconds = FDateTime::UtcNow().ToUnixTimestamp() captured once per tick.
	void UpdateSimulatedProgress(double LocalNowSeconds, int64 UtcNowUnixSeconds);

	/**
	 * Diff-updates all FieldNotify properties from a queue entry snapshot.
	 *
	 * @param Entry          The queue entry to reflect.
	 * @param Recipe         The entry's recipe record (display name and CraftTime); null when missing.
	 * @param bIsActiveEntry Whether this entry is currently the active (front-of-queue) entry.
	 */
	void RefreshFrom(
		const FArcCraftQueueEntry& Entry,
		const FArcRecipeDefinitionData* Recipe,
		bool bIsActiveEntry);
};
