// Copyright Lukasz Baran. All Rights Reserved.

#include "StateTree/ArcGovernorActivateRecipesTask.h"
#include "Mass/ArcEconomyFragments.h"
#include "ArcSettlementSubsystem.h"
#include "ArcCraft/Recipe/ArcRecipeDefinition.h"
#include "Items/ArcItemDefinition.h"
#include "ArcItemEconomyFragment.h"
#include "Core/ArcCoreAssetManager.h"
#include "ArcEconomyUtils.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

bool FArcGovernorActivateRecipesTask::Link(FStateTreeLinker& Linker)
{
    return true;
}

EStateTreeRunStatus FArcGovernorActivateRecipesTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    checkf(IsInGameThread(), TEXT("Governor tasks must run on game thread for cross-entity access"));
    FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
    FMassEntityManager& EntityManager = MassContext.GetEntityManager();
    const FMassEntityHandle SettlementEntity = MassContext.GetEntity();

    UWorld* World = Context.GetWorld();
    UArcSettlementSubsystem* SettlementSub = World ? World->GetSubsystem<UArcSettlementSubsystem>() : nullptr;
    if (!SettlementSub)
    {
        return EStateTreeRunStatus::Failed;
    }

    const TArray<FMassEntityHandle>& BuildingHandles = SettlementSub->GetBuildingsForSettlement(SettlementEntity);
    for (const FMassEntityHandle& BuildingHandle : BuildingHandles)
    {
        if (!EntityManager.IsEntityValid(BuildingHandle))
        {
            continue;
        }

        FArcBuildingWorkforceFragment* Workforce = EntityManager.GetFragmentDataPtr<FArcBuildingWorkforceFragment>(BuildingHandle);
        FArcBuildingEconomyConfig* EconomyConfig = EntityManager.GetSharedFragmentDataPtr<FArcBuildingEconomyConfig>(BuildingHandle);
        if (!Workforce || !EconomyConfig)
        {
            continue;
        }

        const TArray<UArcRecipeDefinition*>& Recipes = ArcEconomy::ResolveAllowedRecipes(*EconomyConfig);
        if (Recipes.Num() == 0)
        {
            continue;
        }

        TArray<UArcRecipeDefinition*> SortedRecipes = Recipes;
        SortedRecipes.Sort([](const UArcRecipeDefinition& A, const UArcRecipeDefinition& B)
        {
            float PriceA = 0.0f;
            float PriceB = 0.0f;
            if (A.OutputItemDefinition.IsValid())
            {
                UArcItemDefinition* ItemA = UArcCoreAssetManager::GetAsset<UArcItemDefinition>(A.OutputItemDefinition.AssetId);
                if (ItemA)
                {
                    const FArcItemEconomyFragment* EconA = ItemA->FindFragment<FArcItemEconomyFragment>();
                    if (EconA)
                    {
                        PriceA = EconA->BasePrice;
                    }
                }
            }
            if (B.OutputItemDefinition.IsValid())
            {
                UArcItemDefinition* ItemB = UArcCoreAssetManager::GetAsset<UArcItemDefinition>(B.OutputItemDefinition.AssetId);
                if (ItemB)
                {
                    const FArcItemEconomyFragment* EconB = ItemB->FindFragment<FArcItemEconomyFragment>();
                    if (EconB)
                    {
                        PriceB = EconB->BasePrice;
                    }
                }
            }
            return PriceA > PriceB;
        });

        TSet<UArcRecipeDefinition*> AssignedRecipes;
        for (const FArcBuildingSlotData& Slot : Workforce->Slots)
        {
            if (Slot.DesiredRecipe)
            {
                AssignedRecipes.Add(Slot.DesiredRecipe);
            }
        }

        int32 FallbackIndex = 0;
        for (int32 SlotIndex = 0; SlotIndex < Workforce->Slots.Num(); ++SlotIndex)
        {
            FArcBuildingSlotData& Slot = Workforce->Slots[SlotIndex];
            if (Slot.DesiredRecipe)
            {
                continue;
            }

            UArcRecipeDefinition* ChosenRecipe = nullptr;
            for (UArcRecipeDefinition* Recipe : SortedRecipes)
            {
                if (!AssignedRecipes.Contains(Recipe))
                {
                    ChosenRecipe = Recipe;
                    break;
                }
            }

            if (!ChosenRecipe)
            {
                ChosenRecipe = SortedRecipes[FallbackIndex % SortedRecipes.Num()];
                ++FallbackIndex;
            }

            AssignedRecipes.Add(ChosenRecipe);
            Slot.DesiredRecipe = ChosenRecipe;
        }
    }

    return EStateTreeRunStatus::Succeeded;
}
