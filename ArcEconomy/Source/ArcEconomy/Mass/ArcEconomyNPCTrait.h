// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcEconomyFragments.h"
#include "Mass/ArcAreaAssignmentFragments.h"
#include "ArcEconomyNPCTrait.generated.h"

/** Standard economy NPC trait. Adds all role fragments to every NPC. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Economy NPC", Category = "Economy"))
class ARCECONOMY_API UArcEconomyNPCTrait : public UMassEntityTraitBase
{
    GENERATED_BODY()

public:
    virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
    virtual bool ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext, const UWorld& World, FAdditionalTraitRequirements& OutTraitRequirements) const override;
};

/** Caravan variant — same fragments plus FArcCaravanTag. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Economy Caravan NPC", Category = "Economy"))
class ARCECONOMY_API UArcEconomyCaravanNPCTrait : public UArcEconomyNPCTrait
{
    GENERATED_BODY()

public:
    virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
