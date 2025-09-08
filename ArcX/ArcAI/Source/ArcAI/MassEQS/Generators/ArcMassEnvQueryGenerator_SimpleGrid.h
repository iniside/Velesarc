// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataProviders/AIDataProvider.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "ArcMassEnvQueryGenerator_PathingGrid.generated.h"

/**
 * 
 */
UCLASS()
class ARCAI_API UArcMassEnvQueryGenerator_PathingGrid : public UEnvQueryGenerator
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

	AIMODULE_API virtual void ProjectAndFilterNavPoints(TArray<FNavLocation>& Points, FEnvQueryInstance& QueryInstance) const override;
};
