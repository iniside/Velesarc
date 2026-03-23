// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcWeatherSubsystem.generated.h"

// Minimal climate description: temperature and humidity
USTRUCT(BlueprintType)
struct ARCWEATHER_API FArcClimateParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climate")
	float Temperature = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climate")
	float Humidity = 50.f;
};

// A single cell in the global weather grid
USTRUCT(BlueprintType)
struct ARCWEATHER_API FArcWeatherCell
{
	GENERATED_BODY()

	// Base climate, set by climate region volumes
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	FArcClimateParams BaseClimate;

	// Mutable weather offset on top of base climate
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	float TemperatureOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	float HumidityOffset = 0.f;

	// Humidity percentage at which this cell counts as "raining"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	float HumidityThreshold = 70.f;

	// Temperature (C) below which this cell counts as "freezing"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	float FreezeThreshold = 0.f;

	FArcClimateParams GetEffective() const
	{
		FArcClimateParams Result;
		Result.Temperature = BaseClimate.Temperature + TemperatureOffset;
		Result.Humidity = FMath::Clamp(BaseClimate.Humidity + HumidityOffset, 0.f, 100.f);
		return Result;
	}
};

// Ice fragment: two physical properties, melt/freeze rate is derived
USTRUCT()
struct ARCWEATHER_API FArcIceFragment : public FMassFragment
{
	GENERATED_BODY()

	// Temperature at which ice begins melting (C). Higher = more resistant ice.
	UPROPERTY(EditAnywhere)
	float TemperatureResistance = 0.f;

	// Thermal inertia: higher values mean slower temperature response.
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.1"))
	float ThermalMass = 1.f;
};

UCLASS()
class ARCWEATHER_API UArcWeatherSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Grid configuration ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "100.0"))
	float CellSize = 10000.f;

	// Default climate for cells that have no data
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	FArcClimateParams DefaultClimate;

	// Default thresholds for cells outside any climate region
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	float DefaultHumidityThreshold = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	float DefaultFreezeThreshold = 0.f;

	// Degrees below freeze threshold at which cold intensity = 1.0 (unclamped beyond)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0.01"))
	float ColdNormalizationDelta = 15.f;

	// Humidity points above rain threshold at which rain intensity = 1.0 (unclamped beyond)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0.01"))
	float RainNormalizationDelta = 30.f;

	// --- Seasons ---

	// Each entry is a climate offset applied globally for that season
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	TArray<FArcClimateParams> Seasons;

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetCurrentSeason(int32 SeasonIndex);

	UFUNCTION(BlueprintCallable, Category = "Weather")
	int32 GetCurrentSeason() const { return CurrentSeasonIndex; }

	// --- Coordinate helpers ---

	UFUNCTION(BlueprintCallable, Category = "Weather")
	FIntVector WorldToCell(const FVector& WorldLocation) const;

	// --- Base climate (set by region volumes on BeginPlay) ---

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetBaseClimate(const FIntVector& CellCoords, const FArcClimateParams& Climate,
	    float HumidityThreshold, float FreezeThreshold);

	// Set base climate for all cells overlapping the given bounds
	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetBaseClimateInBounds(const FBox& Bounds, const FArcClimateParams& Climate,
	    float HumidityThreshold, float FreezeThreshold);

	// Remove cells within bounds (reverts them to DefaultClimate)
	UFUNCTION(BlueprintCallable, Category = "Weather")
	void ClearCellsInBounds(const FBox& Bounds);

	// --- Weather modification (mutable layer on top of base) ---

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetWeatherOffset(const FIntVector& CellCoords, float TemperatureOffset, float HumidityOffset);

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetWeatherOffsetAtLocation(const FVector& Location, float TemperatureOffset, float HumidityOffset);

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void ClearWeatherOffset(const FIntVector& CellCoords);

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void ClearWeatherOffsetAtLocation(const FVector& Location);

	// --- Queries ---

	// Returns effective weather (base + offset) at a world location
	UFUNCTION(BlueprintCallable, Category = "Weather")
	FArcClimateParams GetWeatherAtLocation(const FVector& Location) const;

	// Returns base climate at a world location (no weather offset)
	UFUNCTION(BlueprintCallable, Category = "Weather")
	FArcClimateParams GetBaseClimateAtLocation(const FVector& Location) const;

	// Returns true if it is raining at a world location (humidity >= threshold AND above freezing)
	UFUNCTION(BlueprintCallable, Category = "Weather")
	bool IsRainingAtLocation(const FVector& Location) const;

	// Returns true if it is freezing at a world location (temperature < freeze threshold)
	UFUNCTION(BlueprintCallable, Category = "Weather")
	bool IsFreezingAtLocation(const FVector& Location) const;

	// Returns cold intensity: 0 if not freezing, (FreezeThreshold - Temperature) / ColdNormalizationDelta otherwise
	UFUNCTION(BlueprintCallable, Category = "Weather")
	float GetColdIntensityAtLocation(const FVector& Location) const;

	// Returns rain intensity: 0 if not raining, (Humidity - HumidityThreshold) / RainNormalizationDelta otherwise
	UFUNCTION(BlueprintCallable, Category = "Weather")
	float GetRainIntensityAtLocation(const FVector& Location) const;

	// --- Debug accessors ---

	const TMap<FIntVector, FArcWeatherCell>& GetGrid() const { return Grid; }
	TMap<FIntVector, FArcWeatherCell>& GetMutableGrid() { return Grid; }

private:
	// Sparse global grid: only cells that have been written to exist
	TMap<FIntVector, FArcWeatherCell> Grid;

	int32 CurrentSeasonIndex = 0;
};
