// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassEnvQueryGenerator_ProjectedPoints.h"

#include "NavigationSystem.h"
#include "Algo/RemoveIf.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "NavFilters/NavigationQueryFilter.h"

UArcMassEnvQueryGenerator_ProjectedPoints::UArcMassEnvQueryGenerator_ProjectedPoints(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProjectionData.TraceMode = EEnvQueryTrace::Navigation;
	ProjectionData.bCanProjectDown = true;
	ProjectionData.bCanDisableTrace = true;
	ProjectionData.ExtentX = 0.0f;
}

void UArcMassEnvQueryGenerator_ProjectedPoints::ProjectAndFilterNavPoints(TArray<FNavLocation>& Points, FEnvQueryInstance& QueryInstance) const
{
	if (Points.IsEmpty())
	{
		return;
	}
	
	const ANavigationData* NavData = nullptr;
	if (ProjectionData.TraceMode != EEnvQueryTrace::None)
	{
		const FNavAgentProperties& NavAgentProperties = FNavAgentProperties(32.f);

		UNavigationSystemV1* NavMeshSubsystem = Cast<UNavigationSystemV1>(QueryInstance.World->GetNavigationSystem());
		NavData = NavMeshSubsystem->GetNavDataForProps(NavAgentProperties, Points[0].Location);
		
		if (!NavData)
		{
			NavData = FEQSHelpers::FindNavigationDataForQuery(QueryInstance);
		}

		if (!NavData)
		{
			return;
		}

		{
			FSharedConstNavQueryFilter LocalNavigationFilter = UNavigationQueryFilter::GetQueryFilter(*NavData, ProjectionData.NavigationFilter);
			TArray<FNavigationProjectionWork> Workload;
			Workload.Reserve(Points.Num());

			if (ProjectionData.ProjectDown == ProjectionData.ProjectUp)
			{
				for (const auto& Point : Points)
				{
					Workload.Add(FNavigationProjectionWork(Point.Location));
				}
			}
			else
			{
				const FVector VerticalOffset = FVector(0, 0, (ProjectionData.ProjectUp - ProjectionData.ProjectDown) / 2);
				for (const auto& Point : Points)
				{
					Workload.Add(FNavigationProjectionWork(Point.Location + VerticalOffset));
				}
			}

			const FVector ProjectionExtent(ProjectionData.ExtentX, ProjectionData.ExtentX, (ProjectionData.ProjectDown + ProjectionData.ProjectUp) / 2);
			NavData->BatchProjectPoints(Workload, ProjectionExtent, LocalNavigationFilter);

			for (int32 Idx = Workload.Num() - 1; Idx >= 0; Idx--)
			{
				if (Workload[Idx].bResult)
				{
					Points[Idx] = Workload[Idx].OutLocation;
					Points[Idx].Location.Z += ProjectionData.PostProjectionVerticalOffset;
				}
			}

			//if (ProjectionData.TraceMode == ETraceMode::Discard)
			{
				const FNavLocation* PointsBegin = Points.GetData();
				int32 NewNum = Algo::StableRemoveIf(Points, [&Workload, PointsBegin](FNavLocation& Point)
				{
					return !Workload[IntCastChecked<int32>(&Point - PointsBegin)].bResult;
				});
				Points.SetNum(NewNum, EAllowShrinking::No);
			}
		}

		const int32 InitialElementsCount = QueryInstance.Items.Num();
		QueryInstance.ReserveItemData(InitialElementsCount + Points.Num());
		for (int32 Idx = 0; Idx < Points.Num(); Idx++)
		{
			// store using default function to handle creating new data entry 
			QueryInstance.AddItemData<UEnvQueryItemType_Point>(Points[Idx]);
		}

		FEnvQueryOptionInstance& OptionInstance = QueryInstance.Options[QueryInstance.OptionIndex];
		OptionInstance.bHasNavLocations = (ProjectionData.TraceMode == EEnvQueryTrace::Navigation);
	}
}
