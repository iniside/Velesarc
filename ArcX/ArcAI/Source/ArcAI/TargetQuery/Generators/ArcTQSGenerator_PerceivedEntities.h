// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSGenerator.h"
#include "ArcTQSGenerator_PerceivedEntities.generated.h"

/**
 * Which perception senses to read perceived entities from.
 */
UENUM(BlueprintType)
enum class EArcTQSPerceptionSense : uint8
{
	Sight,
	Hearing,
	/** Merge both sight and hearing results (deduplicates). */
	Both
};

/**
 * Generator that produces target items from the querier entity's perception results.
 * Reads FArcMassSightPerceptionResult and/or FArcMassHearingPerceptionResult fragments
 * on the querier entity and creates a MassEntity-type target item per perceived entity.
 *
 * Does not use context locations — overrides GenerateItems() directly.
 */
USTRUCT(DisplayName = "Perceived Entities")
struct ARCAI_API FArcTQSGenerator_PerceivedEntities : public FArcTQSGenerator
{
	GENERATED_BODY()

	// Which perception sense(s) to read from.
	UPROPERTY(EditAnywhere, Category = "Generator")
	EArcTQSPerceptionSense Sense = EArcTQSPerceptionSense::Sight;

	// If true, use LastKnownLocation from perception result instead of
	// resolving the entity's current transform. Useful for hearing where
	// the location may be an approximation.
	UPROPERTY(EditAnywhere, Category = "Generator")
	bool bUseLastKnownLocation = false;

	virtual void GenerateItems(const FArcTQSQueryContext& QueryContext, TArray<FArcTQSTargetItem>& OutItems) const override;
};
