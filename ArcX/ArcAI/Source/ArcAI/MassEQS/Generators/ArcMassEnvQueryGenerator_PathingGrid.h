// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassEnvQueryGenerator_SimpleGrid.h"
#include "ArcMassEnvQueryGenerator_PathingGrid.generated.h"

class UNavigationQueryFilter;
/**
 * 
 */
UCLASS(meta = (DisplayName = "Arc Mass: Pathing Grid", Category = "Arc Mass Generators"))
class ARCAI_API UArcMassEnvQueryGenerator_PathingGrid : public UArcMassEnvQueryGenerator_SimpleGrid
{
	GENERATED_BODY()

	/** pathfinding direction */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	FAIDataProviderBoolValue PathToItem;

	/** navigation filter to use in pathfinding */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	TSubclassOf<UNavigationQueryFilter> NavigationFilter;

	/** multiplier for max distance between point and context */
	UPROPERTY(EditDefaultsOnly, Category = Pathfinding, AdvancedDisplay)
	FAIDataProviderFloatValue ScanRangeMultiplier;

	UArcMassEnvQueryGenerator_PathingGrid(const FObjectInitializer& ObjectInitializer);
	
	virtual void ProjectAndFilterNavPoints(TArray<FNavLocation>& Points, FEnvQueryInstance& QueryInstance) const override;
};
