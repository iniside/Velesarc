// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassEnvQueryGenerator_SimpleGrid.h"

#include "ArcMassEQSTypes.h"
#include "NavigationSystem.h"
#include "Algo/RemoveIf.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "NavFilters/NavigationQueryFilter.h"

UArcMassEnvQueryGenerator_SimpleGrid::UArcMassEnvQueryGenerator_SimpleGrid(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GenerateAround = UEnvQueryContext_MassEntityQuerier::StaticClass();
	ItemType = UEnvQueryItemType_Point::StaticClass();
	
	GridSize.DefaultValue = 500.0f;
	SpaceBetween.DefaultValue = 100.0f;
}

void UArcMassEnvQueryGenerator_SimpleGrid::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	UObject* BindOwner = QueryInstance.Owner.Get();
	GridSize.BindData(BindOwner, QueryInstance.QueryID);
	SpaceBetween.BindData(BindOwner, QueryInstance.QueryID);

	float RadiusValue = GridSize.GetValue();
	float DensityValue = SpaceBetween.GetValue();

	const int32 ItemCount = FPlatformMath::TruncToInt((RadiusValue * 2.0f / DensityValue) + 1);
	const int32 ItemCountHalf = ItemCount / 2;

	TArray<FVector> ContextLocations;
	QueryInstance.PrepareContext(GenerateAround, ContextLocations);

	TArray<FNavLocation> GridPoints;
	GridPoints.Reserve(ItemCount * ItemCount * ContextLocations.Num());

	for (int32 ContextIndex = 0; ContextIndex < ContextLocations.Num(); ContextIndex++)
	{
		for (int32 IndexX = 0; IndexX < ItemCount; ++IndexX)
		{
			for (int32 IndexY = 0; IndexY < ItemCount; ++IndexY)
			{
				const FNavLocation TestPoint = FNavLocation(ContextLocations[ContextIndex] - FVector(DensityValue * (IndexX - ItemCountHalf), DensityValue * (IndexY - ItemCountHalf), 0));
				GridPoints.Add(TestPoint);
			}
		}
	}

	ProjectAndFilterNavPoints(GridPoints, QueryInstance);
}