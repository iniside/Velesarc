// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ArcKnowledgeEntry.h"
#include "ArcSettlementDefinition.generated.h"

/**
 * Data asset defining a settlement type.
 * Placed via AArcSettlementVolume or created programmatically.
 */
UCLASS(BlueprintType, Blueprintable)
class ARCSETTLEMENT_API UArcSettlementDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settlement")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settlement")
	FGameplayTagContainer SettlementTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settlement", meta = (ClampMin = 100.0))
	float BoundingRadius = 5000.0f;

	/** Initial knowledge entries to register when the settlement is created. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settlement")
	TArray<FArcKnowledgeEntry> InitialKnowledge;
};
