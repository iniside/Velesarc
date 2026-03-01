// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ArcKnowledgeEntry.h"
#include "ArcKnowledgeEntryDefinition.generated.h"

/**
 * Reusable data asset holding a bundle of predefined knowledge entries.
 * Referenced by volumes, traits, or spawned programmatically to register
 * a set of knowledge entries at a given location.
 *
 * Generic replacement for the old UArcSettlementDefinition — no domain assumptions.
 */
UCLASS(BlueprintType, Blueprintable)
class ARCKNOWLEDGE_API UArcKnowledgeEntryDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Human-readable name for this knowledge bundle (e.g., "Mining Outpost", "Trade Route"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knowledge")
	FText DisplayName;

	/** Tags applied to the primary knowledge entry registered by this definition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knowledge")
	FGameplayTagContainer Tags;

	/** Spatial extent for the knowledge area. Used by volumes for their sphere radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knowledge", meta = (ClampMin = 100.0))
	float BoundingRadius = 5000.0f;

	/** Optional payload for the primary knowledge entry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knowledge", meta = (BaseStruct = "/Script/ArcKnowledge.ArcKnowledgePayload", ExcludeBaseStruct))
	FInstancedStruct Payload;

	/** Spatial broadcast radius for events on this area's knowledge. 0 = use system default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knowledge", meta = (ClampMin = "0.0"))
	float SpatialBroadcastRadius = 0.0f;

	/** Additional knowledge entries to register alongside the primary entry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knowledge")
	TArray<FArcKnowledgeEntry> InitialKnowledge;
};
