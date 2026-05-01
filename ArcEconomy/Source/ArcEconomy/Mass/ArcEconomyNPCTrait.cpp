// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcEconomyNPCTrait.h"
#include "MassEntityTemplateRegistry.h"
#include "Mass/ArcMassItemFragments.h"
#include "Mass/ArcAreaAssignmentFragments.h"

void UArcEconomyNPCTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
    BuildContext.AddFragment<FArcEconomyNPCFragment>();
    BuildContext.AddFragment<FArcWorkerFragment>();
    BuildContext.AddFragment<FArcTransporterFragment>();
    BuildContext.AddFragment<FArcGathererFragment>();
    BuildContext.AddFragment<FArcMassItemSpecArrayFragment>();
}

void UArcEconomyCaravanNPCTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
    Super::BuildTemplate(BuildContext, World);
    BuildContext.AddTag<FArcCaravanTag>();
}

bool UArcEconomyNPCTrait::ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext, const UWorld& World, FAdditionalTraitRequirements& OutTraitRequirements) const
{
    if (!BuildContext.HasTag<FArcAreaAssignmentTag>())
    {
        UE_LOG(LogTemp, Error, TEXT("UArcEconomyNPCTrait requires UArcAreaAssignmentTrait — FArcAreaAssignmentTag not found on template."));
        return false;
    }

    return true;
}
