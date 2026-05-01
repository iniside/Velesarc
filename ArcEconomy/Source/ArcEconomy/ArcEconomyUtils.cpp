// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEconomyUtils.h"
#include "Mass/ArcEconomyFragments.h"
#include "ArcCraft/Recipe/ArcRecipeDefinition.h"
#include "Engine/AssetManager.h"

const TArray<UArcRecipeDefinition*>& ArcEconomy::ResolveAllowedRecipes(FArcBuildingEconomyConfig& EconomyConfig)
{
    if (EconomyConfig.bRecipesCached)
    {
        return EconomyConfig.CachedRecipes;
    }

    // Manual list takes priority
    if (EconomyConfig.AllowedRecipes.Num() > 0)
    {
        EconomyConfig.CachedRecipes.Reserve(EconomyConfig.AllowedRecipes.Num());
        for (const TObjectPtr<UArcRecipeDefinition>& RecipePtr : EconomyConfig.AllowedRecipes)
        {
            if (RecipePtr)
            {
                EconomyConfig.CachedRecipes.Add(RecipePtr);
            }
        }
    }
    // Dynamic discovery via station tags
    else if (!EconomyConfig.ProductionStationTags.IsEmpty())
    {
        IAssetRegistry& AR = UAssetManager::Get().GetAssetRegistry();

        FARFilter AssetFilter;
        AssetFilter.ClassPaths.Add(UArcRecipeDefinition::StaticClass()->GetClassPathName());

        TSet<FTopLevelAssetPath> DerivedClassNames;
        AR.GetDerivedClassNames(AssetFilter.ClassPaths, TSet<FTopLevelAssetPath>(), DerivedClassNames);
        AssetFilter.ClassPaths.Append(DerivedClassNames.Array());
        AssetFilter.bRecursiveClasses = true;

        TArray<FAssetData> AllRecipes;
        AR.GetAssets(AssetFilter, AllRecipes);

        TArray<FAssetData> FilteredAssets = UArcRecipeDefinition::FilterByStationTags(AllRecipes, EconomyConfig.ProductionStationTags);

        EconomyConfig.CachedRecipes.Reserve(FilteredAssets.Num());
        for (const FAssetData& AssetData : FilteredAssets)
        {
            UArcRecipeDefinition* Recipe = Cast<UArcRecipeDefinition>(AssetData.GetAsset());
            if (Recipe)
            {
                EconomyConfig.CachedRecipes.Add(Recipe);
            }
        }
    }

    EconomyConfig.bRecipesCached = true;
    return EconomyConfig.CachedRecipes;
}
