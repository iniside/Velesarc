// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcKnowledgePayload.h"
#include "ArcKnowledgeTypes.h"
#include "Engine/DeveloperSettings.h"
#include "Mass/EntityHandle.h"
#include "NativeGameplayTags.h"

#include "ArcSharedTargetKnowledge.generated.h"

class UArcKnowledgeSubsystem;

/** Marks a knowledge entry as one team's record of where a target was last seen. */
ARCAI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AI_Knowledge_SharedTarget);

/**
 * Payload of a shared target knowledge entry. The target itself is the entry's SourceEntity,
 * its last known location is the entry's Location and the sighting time is the entry's Timestamp.
 */
USTRUCT(BlueprintType)
struct ARCAI_API FArcSharedTargetPayload : public FArcKnowledgePayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SharedTarget")
	int32 TeamId = INDEX_NONE;
};

/** Owns how long shared target records live. Retention belongs to the shared layer, not to any
 *  one perceiver's eyes: several allies with different sight configs write the same entry. */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "ArcAI Shared Target Knowledge"))
class ARCAI_API UArcSharedTargetKnowledgeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return FName("Arc Game"); }
	virtual FName GetSectionName() const override { return FName("Shared Target Knowledge"); }

	/** Seconds a side retains a sighting after it was made, before the knowledge sweep reclaims it.
	 *  This is the storage budget only: how long a record stays *relevant* is the reader's window,
	 *  which is clamped against this value and is therefore never longer than it. */
	UPROPERTY(config, EditAnywhere, Category = "Shared Target", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float RetentionSeconds = 60.0f;
};

namespace ArcSharedTargetKnowledge
{
	/** Seconds a side retains a sighting. Readers clamp their own staleness window against this. */
	ARCAI_API double GetRetentionSeconds();

	/** Find TeamId's shared target entry about Target. GetKnowledgeBySource returns a mixed bucket
	 *  (the target's own entries too), so both the marker tag and the payload team are checked. */
	ARCAI_API FArcKnowledgeHandle FindTeamEntry(
		const UArcKnowledgeSubsystem& Subsystem,
		FMassEntityHandle Target,
		int32 TeamId);

	/**
	 * Register or refresh TeamId's record of Target. Publishes only when the sighting is newer than
	 * the stored one and either the target moved or the record is at least RefreshInterval old.
	 * Returns true when the store was written.
	 */
	ARCAI_API bool PublishSighting(
		UArcKnowledgeSubsystem& Subsystem,
		FMassEntityHandle Target,
		int32 TeamId,
		const FVector& Location,
		double SightingTime,
		double RefreshInterval);
}
