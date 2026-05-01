// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FArcBuildingEconomyConfig;
class UArcRecipeDefinition;

namespace ArcEconomy
{
    /** Resolve recipe list from economy config: manual list if non-empty, else dynamic discovery via station tags. Cached after first call. */
    const TArray<UArcRecipeDefinition*>& ResolveAllowedRecipes(FArcBuildingEconomyConfig& EconomyConfig);
}
