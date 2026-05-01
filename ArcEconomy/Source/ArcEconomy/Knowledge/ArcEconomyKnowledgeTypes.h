// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "NativeGameplayTags.h"
#include "ArcKnowledgePayload.h"
#include "ArcEconomyKnowledgeTypes.generated.h"

class UArcItemDefinition;

namespace ArcEconomy::Tags
{
	ARCECONOMY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Knowledge_Economy_Demand);
	ARCECONOMY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Knowledge_Economy_Supply);
	ARCECONOMY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Knowledge_Economy_Market);
	ARCECONOMY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Knowledge_Economy_Transport);
}

/** Knowledge payload for demand: a settlement needs a specific item. */
USTRUCT(BlueprintType)
struct ARCECONOMY_API FArcEconomyDemandPayload : public FArcKnowledgePayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Demand")
	TObjectPtr<UArcItemDefinition> ItemDefinition = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Demand")
	int32 QuantityNeeded = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Demand")
	FMassEntityHandle SettlementHandle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Demand")
	float OfferingPrice = 0.0f;

	/** For tag-based ingredient demands: match supply items by tags instead of item def.
	 *  Used when ItemDefinition is null. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Demand")
	FGameplayTagContainer RequiredTags;
};

/** Knowledge payload for supply: a settlement has items available. */
USTRUCT(BlueprintType)
struct ARCECONOMY_API FArcEconomySupplyPayload : public FArcKnowledgePayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supply")
	TObjectPtr<UArcItemDefinition> ItemDefinition = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supply")
	int32 QuantityAvailable = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supply")
	FMassEntityHandle SettlementHandle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supply")
	float AskingPrice = 0.0f;
};

/** Knowledge payload for market: a snapshot of current prices. */
USTRUCT(BlueprintType)
struct ARCECONOMY_API FArcEconomyMarketPayload : public FArcKnowledgePayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Market")
	TMap<TObjectPtr<UArcItemDefinition>, float> PriceSnapshot;
};
