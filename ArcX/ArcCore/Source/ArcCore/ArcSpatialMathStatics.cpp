// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSpatialMathStatics.h"

TArray<FVector> UArcSpatialMathStatics::GeneratePoissonDiskInCircle(
        const FVector& Origin,
        float CircleRadius,
        float MinDistance,
        int32 MaxAttempts)
{
    TArray<FVector> Points;
    TArray<FVector2D> LocalPoints; // Work in 2D locally
    TArray<int32> ActiveList;
    
    float CellSize = MinDistance / FMath::Sqrt(2.f);
    int32 GridSize = FMath::CeilToInt((CircleRadius * 2.f) / CellSize);
    
    TArray<int32> Grid;
    Grid.Init(-1, GridSize * GridSize);
    
    auto ToGridIndex = [&](const FVector2D& P) -> int32
    {
        int32 X = FMath::FloorToInt((P.X + CircleRadius) / CellSize);
        int32 Y = FMath::FloorToInt((P.Y + CircleRadius) / CellSize);
        X = FMath::Clamp(X, 0, GridSize - 1);
        Y = FMath::Clamp(Y, 0, GridSize - 1);
        return Y * GridSize + X;
    };
    
    auto IsValidPoint = [&](const FVector2D& P) -> bool
    {
        if (P.Size() > CircleRadius) return false;
        
        int32 CellX = FMath::FloorToInt((P.X + CircleRadius) / CellSize);
        int32 CellY = FMath::FloorToInt((P.Y + CircleRadius) / CellSize);
        
        for (int32 DY = -2; DY <= 2; DY++)
        {
            for (int32 DX = -2; DX <= 2; DX++)
            {
                int32 NX = CellX + DX;
                int32 NY = CellY + DY;
                
                if (NX < 0 || NX >= GridSize || NY < 0 || NY >= GridSize)
                    continue;
                
                int32 NeighborIdx = Grid[NY * GridSize + NX];
                if (NeighborIdx != -1)
                {
                    if (FVector2D::Distance(P, LocalPoints[NeighborIdx]) < MinDistance)
                        return false;
                }
            }
        }
        return true;
    };
    
    auto AddPoint = [&](const FVector2D& P)
    {
        LocalPoints.Add(P);
        ActiveList.Add(LocalPoints.Num() - 1);
        Grid[ToGridIndex(P)] = LocalPoints.Num() - 1;
    };
    
    // Start with random point near center
    FVector2D FirstPoint = GetRandomPointInCircle2D(CircleRadius * 0.5f);
    AddPoint(FirstPoint);
    
    while (ActiveList.Num() > 0)
    {
        int32 ActiveIdx = FMath::RandRange(0, ActiveList.Num() - 1);
        FVector2D Center = LocalPoints[ActiveList[ActiveIdx]];
        bool bFoundValid = false;
        
        for (int32 Attempt = 0; Attempt < MaxAttempts; Attempt++)
        {
            float Angle = FMath::FRandRange(0.f, 2.f * PI);
            float Dist = FMath::FRandRange(MinDistance, MinDistance * 2.f);
            
            FVector2D Candidate = Center + FVector2D(
                Dist * FMath::Cos(Angle),
                Dist * FMath::Sin(Angle)
            );
            
            if (IsValidPoint(Candidate))
            {
                AddPoint(Candidate);
                bFoundValid = true;
                break;
            }
        }
        
        if (!bFoundValid)
        {
            ActiveList.RemoveAtSwap(ActiveIdx);
        }
    }
    
    // Convert to world space FVector
    Points.Reserve(LocalPoints.Num());
    for (const FVector2D& P : LocalPoints)
    {
        Points.Add(FVector(Origin.X + P.X, Origin.Y + P.Y, Origin.Z));
    }
    
    return Points;
}

TArray<FVector> UArcSpatialMathStatics::SelectUniqueLocations(
	const TArray<FVector>& AllLocations,
	TSet<int32>& SelectedIndices,
	int32 Count,
	bool bResetIfExhausted)
{
	TArray<FVector> Result;
    
	if (AllLocations.Num() == 0 || Count <= 0)
	{
		return Result;
	}

	// Reset if exhausted and flag set
	if (bResetIfExhausted && SelectedIndices.Num() >= AllLocations.Num())
	{
		SelectedIndices.Empty();
	}

	// Build available indices on the fly
	TArray<int32> Available;
	Available.Reserve(AllLocations.Num() - SelectedIndices.Num());
    
	for (int32 i = 0; i < AllLocations.Num(); ++i)
	{
		if (!SelectedIndices.Contains(i))
		{
			Available.Add(i);
		}
	}

	const int32 ActualCount = FMath::Min(Count, Available.Num());
	Result.Reserve(ActualCount);

	// Fisher-Yates selection
	for (int32 i = 0; i < ActualCount; ++i)
	{
		const int32 RandomIdx = FMath::RandRange(0, Available.Num() - 1);
		const int32 PickedIndex = Available[RandomIdx];
        
		Result.Add(AllLocations[PickedIndex]);
		SelectedIndices.Add(PickedIndex);
        
		Available.RemoveAtSwap(RandomIdx);
	}

	return Result;
}