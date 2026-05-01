// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcEconomyBuildingTrait.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityManager.h"
#include "Mass/ArcAreaFragments.h"
#include "Mass/ArcMassItemFragments.h"

void UArcEconomyBuildingTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
    FArcBuildingFragment& BuildingFragment = BuildContext.AddFragment_GetRef<FArcBuildingFragment>();
    FArcBuildingWorkforceFragment& Workforce = BuildContext.AddFragment_GetRef<FArcBuildingWorkforceFragment>();

    BuildingFragment = BuildingConfig;

    // Add economy config as shared fragment (mutable for recipe cache)
    FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
    const FSharedStruct EconomyConfigFragment = EntityManager.GetOrCreateSharedFragment(EconomyConfig);
    BuildContext.AddSharedFragment(EconomyConfigFragment);

    // Initialize workforce slots from economy config
    const int32 SlotCount = EconomyConfig.MaxProductionSlots;
    Workforce.Slots.SetNum(SlotCount);
    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        Workforce.Slots[Index].RequiredWorkerCount = EconomyConfig.WorkersPerSlot;
    }

    // Add item storage fragment for buildings with consumption needs
    if (EconomyConfig.ConsumptionNeeds.Num() > 0)
    {
        BuildContext.AddFragment_GetRef<FArcMassItemSpecArrayFragment>();
    }
}

bool UArcEconomyBuildingTrait::ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext, const UWorld& World, FAdditionalTraitRequirements& OutTraitRequirements) const
{
    if (!BuildContext.HasTag<FArcAreaTag>())
    {
        UE_LOG(LogTemp, Error, TEXT("UArcEconomyBuildingTrait requires UArcAreaTrait — FArcAreaTag not found on template."));
        return false;
    }

    return true;
}
