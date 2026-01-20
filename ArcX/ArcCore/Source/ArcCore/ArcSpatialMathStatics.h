// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ArcSpatialMathStatics.generated.h"

/**
 * 
 */
UCLASS()
class ARCCORE_API UArcSpatialMathStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Arc Core|Spatial")
    static TArray<FVector> GeneratePoissonDiskInCircle(
    			const FVector& Origin,
				float CircleRadius,
				float MinDistance,
				int32 MaxAttempts = 30);

private:
	static FVector2D GetRandomPointInCircle2D(float Radius)
	{
		float Angle = FMath::FRandRange(0.f, 2.f * PI);
		float R = Radius * FMath::Sqrt(FMath::FRand());
		return FVector2D(R * FMath::Cos(Angle), R * FMath::Sin(Angle));
	}
	
public:
	/**
 	 * Selects unique random locations from array. Call repeatedly until all consumed.
 	 * @param AllLocations      - Source array of all possible locations
 	 * @param SelectedIndices   - Tracks which indices already picked (pass empty array first call, reuse for subsequent)
 	 * @param Count             - How many to select this call
 	 * @param bResetIfExhausted - Auto-reset when all locations used
 	 * @return Selected locations (may be fewer than Count if exhausted)
 	 */
	UFUNCTION(BlueprintCallable, Category = "Location", meta = (AutoCreateRefTerm = "SelectedIndices"))
	static TArray<FVector> SelectUniqueLocations(
		const TArray<FVector>& AllLocations,
		UPARAM(ref) TSet<int32>& SelectedIndices,
		int32 Count,
		bool bResetIfExhausted = false);

	UFUNCTION(BlueprintPure, Category = "Location")
	static int32 GetRemainingLocationCount(const TArray<FVector>& AllLocations, const TSet<int32>& SelectedIndices)
	{
		return AllLocations.Num() - SelectedIndices.Num();
	}

	UFUNCTION(BlueprintCallable, Category = "Location")
	static void ResetLocationSelection(UPARAM(ref) TSet<int32>& SelectedIndices)
	{
		SelectedIndices.Empty();
	}
};
