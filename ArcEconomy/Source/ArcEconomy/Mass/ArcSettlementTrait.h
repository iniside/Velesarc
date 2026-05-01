// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcEconomyFragments.h"
#include "Data/ArcGovernorDataAsset.h"
#include "Strategy/ArcPopulationFragment.h"
#include "ArcSettlementTrait.generated.h"

class UMassEntityConfigAsset;
class UArcTQSQueryDefinition;

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Settlement", Category = "Economy"))
class ARCECONOMY_API UArcSettlementTrait : public UMassEntityTraitBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Settlement")
    FArcSettlementFragment SettlementConfig;

    UPROPERTY(EditAnywhere, Category = "Market")
    float InitialK = 0.3f;

    UPROPERTY(EditAnywhere, Category = "Governor")
    TObjectPtr<UArcGovernorDataAsset> GovernorDataAsset = nullptr;

    UPROPERTY(EditAnywhere, Category = "Population")
    int32 InitialMaxPopulation = 20;

    UPROPERTY(EditAnywhere, Category = "Population")
    float GrowthIntervalSeconds = 30.0f;

    UPROPERTY(EditAnywhere, Category = "Population")
    float StarvationGracePeriod = 60.0f;

    UPROPERTY(EditAnywhere, Category = "Population")
    TObjectPtr<UMassEntityConfigAsset> NPCEntityConfig = nullptr;

    UPROPERTY(EditAnywhere, Category = "Population")
    TObjectPtr<UArcTQSQueryDefinition> SpawnLocationQuery = nullptr;

    virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
