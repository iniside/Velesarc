// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InstancedStruct.h"
#include "MassEntityTypes.h"
#include "ArcKnowledgeTypes.h"
#include "ArcAdvertisementInstruction.h"
#include "MassEntityHandle.h"
#include "ArcKnowledgeEntry.generated.h"

/**
 * A knowledge entry is a fact about the world.
 * Generic — the system doesn't distinguish resource availability from events from capabilities.
 * Those are all just different tag combinations with different payloads.
 */
USTRUCT(BlueprintType)
struct ARCKNOWLEDGE_API FArcKnowledgeEntry
{
	GENERATED_BODY()

	/** What this fact is about (e.g., "Resource.Iron", "Event.Robbery", "Capability.Forge"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knowledge")
	FGameplayTagContainer Tags;

	/** Where in the world this fact applies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knowledge")
	FVector Location = FVector::ZeroVector;

	/** Who or what produced this fact (optional). */
	UPROPERTY(BlueprintReadWrite, Category = "Knowledge")
	FMassEntityHandle SourceEntity;

	/** 0-1 relevance for query scoring. Not decayed automatically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knowledge", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float Relevance = 1.0f;

	/** When this entry was registered or last updated (world time). */
	UPROPERTY(BlueprintReadOnly, Category = "Knowledge")
	double Timestamp = 0.0;

	/** How long this entry lives in seconds. 0 = infinite (default). Measured from Timestamp. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knowledge", meta = (ClampMin = "0.0"))
	float Lifetime = 0.0f;

	/** Optional arbitrary data (quantity, threat level, price, etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knowledge", meta = (BaseStruct = "/Script/ArcKnowledge.ArcKnowledgePayload", ExcludeBaseStruct))
	FInstancedStruct Payload;

	/** Broadcast radius for spatial events. 0 = use system default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knowledge", meta = (ClampMin = "0.0"))
	float SpatialBroadcastRadius = 0.0f;

	/** Optional behavioral instruction for advertisements.
	  * Tells the claiming entity HOW to fulfill this advertisement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knowledge", meta = (BaseStruct = "/Script/ArcKnowledge.ArcAdvertisementInstruction", ExcludeBaseStruct))
	FInstancedStruct Instruction;

	/** Internal handle — set by the subsystem on registration. */
	UPROPERTY(BlueprintReadOnly, Category = "Knowledge")
	FArcKnowledgeHandle Handle;

	/** Whether this entry has been claimed (for advertisement entries). */
	UPROPERTY(BlueprintReadOnly, Category = "Knowledge")
	bool bClaimed = false;

	/** Who claimed this entry (for advertisement entries). */
	UPROPERTY(BlueprintReadOnly, Category = "Knowledge")
	FMassEntityHandle ClaimedBy;
};
