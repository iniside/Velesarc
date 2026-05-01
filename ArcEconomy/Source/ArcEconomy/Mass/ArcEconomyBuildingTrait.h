// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcEconomyFragments.h"
#include "Mass/ArcAreaFragments.h"
#include "ArcEconomyBuildingTrait.generated.h"

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Economy Building", Category = "Economy"))
class ARCECONOMY_API UArcEconomyBuildingTrait : public UMassEntityTraitBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Building")
    FArcBuildingFragment BuildingConfig;

    UPROPERTY(EditAnywhere, Category = "Economy")
    FArcBuildingEconomyConfig EconomyConfig;

    virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
    virtual bool ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext, const UWorld& World, FAdditionalTraitRequirements& OutTraitRequirements) const override;
};
