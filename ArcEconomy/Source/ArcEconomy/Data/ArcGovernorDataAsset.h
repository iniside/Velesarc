// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ArcEconomyTypes.h"
#include "ArcGovernorDataAsset.generated.h"

/**
 * Tuning parameters for AI settlement governors.
 * Fed into StateTree instance data via data extension at compilation.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Arc Governor Config"))
class ARCECONOMY_API UArcGovernorDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    /** Target ratio of workers to transporters. E.g. 4.0 = 4 workers per 1 transporter. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workforce")
    float TargetWorkerTransporterRatio = 4.0f;

    /** Items in output inventories before rebalancing toward more transporters. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workforce")
    int32 OutputBacklogThreshold = 20;

    /** Minimum price gap ratio between settlements to justify a caravan. E.g. 2.0 = 2x price difference. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trade")
    float TradePriceDifferentialThreshold = 2.0f;

    /** Seconds between trade route evaluations (expensive spatial knowledge query). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trade")
    float TradeEvalInterval = 2.0f;

    /** Default output buffer size applied to buildings that don't override it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storage")
    int32 DefaultOutputBufferSize = 10;

    /** Default settlement-wide storage cap applied when no override is set. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storage")
    int32 DefaultTotalStorageCap = 500;

    /**
     * Maps NPC role to required SmartObject Activity Tags.
     * A slot matches a role if ALL tags in the container are present on the slot's Activity Tags.
     * Worker, Transporter, and Gatherer should be configured — Idle has no slot mapping.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Slots")
    TMap<EArcEconomyNPCRole, FGameplayTagContainer> SlotRoleMapping;
};
