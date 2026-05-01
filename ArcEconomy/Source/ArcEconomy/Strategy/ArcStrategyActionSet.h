// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "UtilityAI/ArcUtilityTypes.h"
#include "ArcStrategyActionSet.generated.h"

/**
 * A single strategic action with its utility considerations.
 * Scored via compensatory multiplication of all considerations.
 */
USTRUCT(BlueprintType)
struct ARCECONOMY_API FArcStrategyAction
{
    GENERATED_BODY()

    /** Action identity — e.g. Strategy.Settlement.Build, Strategy.Faction.DeclareWar. */
    UPROPERTY(EditAnywhere, Category = "Action")
    FGameplayTag ActionType;

    /** Considerations that score this action. Each reads fragments from the querier entity. */
    UPROPERTY(EditAnywhere, Category = "Action", meta = (BaseStruct = "/Script/ArcAI.ArcUtilityConsideration"))
    TArray<FInstancedStruct> Considerations;

    /** Base weight multiplier applied after consideration scoring. */
    UPROPERTY(EditAnywhere, Category = "Action", meta = (ClampMin = 0.01))
    float Weight = 1.0f;

    /** Type-specific execution parameters (what to build, where to attack, etc.). */
    UPROPERTY(EditAnywhere, Category = "Action")
    FInstancedStruct ActionParams;
};

/**
 * Data asset defining the set of strategic actions available to a faction archetype.
 * Assigned per settlement or faction via config fields.
 */
UCLASS(BlueprintType)
class ARCECONOMY_API UArcStrategyActionSet : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Strategy")
    TArray<FArcStrategyAction> Actions;

    UPROPERTY(EditAnywhere, Category = "Strategy")
    EArcUtilitySelectionMode SelectionMode = EArcUtilitySelectionMode::RandomFromTopPercent;

    /** For RandomFromTopPercent: how close to the best score an action must be (0-1). */
    UPROPERTY(EditAnywhere, Category = "Strategy", meta = (ClampMin = 0.0, ClampMax = 1.0))
    float TopPercentThreshold = 0.8f;
};
