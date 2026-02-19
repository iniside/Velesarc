// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcWeatherSubsystem.h"

void UArcWeatherSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UArcWeatherSubsystem::Deinitialize()
{
	Grid.Empty();
	Super::Deinitialize();
}

FIntVector UArcWeatherSubsystem::WorldToCell(const FVector& WorldLocation) const
{
	return FIntVector(
		FMath::FloorToInt32(WorldLocation.X / CellSize),
		FMath::FloorToInt32(WorldLocation.Y / CellSize),
		FMath::FloorToInt32(WorldLocation.Z / CellSize)
	);
}

void UArcWeatherSubsystem::SetBaseClimate(const FIntVector& CellCoords, const FArcClimateParams& Climate)
{
	FArcWeatherCell& Cell = Grid.FindOrAdd(CellCoords);
	Cell.BaseClimate = Climate;
}

void UArcWeatherSubsystem::SetBaseClimateInBounds(const FBox& Bounds, const FArcClimateParams& Climate)
{
	const FIntVector MinCell = WorldToCell(Bounds.Min);
	const FIntVector MaxCell = WorldToCell(Bounds.Max);

	for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z)
	{
		for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
		{
			for (int32 X = MinCell.X; X <= MaxCell.X; ++X)
			{
				FArcWeatherCell& Cell = Grid.FindOrAdd(FIntVector(X, Y, Z));
				Cell.BaseClimate = Climate;
			}
		}
	}
}

void UArcWeatherSubsystem::ClearCellsInBounds(const FBox& Bounds)
{
	const FIntVector MinCell = WorldToCell(Bounds.Min);
	const FIntVector MaxCell = WorldToCell(Bounds.Max);

	for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z)
	{
		for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
		{
			for (int32 X = MinCell.X; X <= MaxCell.X; ++X)
			{
				Grid.Remove(FIntVector(X, Y, Z));
			}
		}
	}
}

void UArcWeatherSubsystem::SetWeatherOffset(const FIntVector& CellCoords, float TemperatureOffset, float HumidityOffset)
{
	if (FArcWeatherCell* Cell = Grid.Find(CellCoords))
	{
		Cell->TemperatureOffset = TemperatureOffset;
		Cell->HumidityOffset = HumidityOffset;
	}
}

void UArcWeatherSubsystem::SetWeatherOffsetAtLocation(const FVector& Location, float TemperatureOffset, float HumidityOffset)
{
	SetWeatherOffset(WorldToCell(Location), TemperatureOffset, HumidityOffset);
}

void UArcWeatherSubsystem::ClearWeatherOffset(const FIntVector& CellCoords)
{
	if (FArcWeatherCell* Cell = Grid.Find(CellCoords))
	{
		Cell->TemperatureOffset = 0.f;
		Cell->HumidityOffset = 0.f;
	}
}

void UArcWeatherSubsystem::ClearWeatherOffsetAtLocation(const FVector& Location)
{
	ClearWeatherOffset(WorldToCell(Location));
}

void UArcWeatherSubsystem::SetCurrentSeason(int32 SeasonIndex)
{
	CurrentSeasonIndex = Seasons.IsValidIndex(SeasonIndex) ? SeasonIndex : 0;
}

FArcClimateParams UArcWeatherSubsystem::GetWeatherAtLocation(const FVector& Location) const
{
	const FArcWeatherCell* Cell = Grid.Find(WorldToCell(Location));
	FArcClimateParams Result = Cell ? Cell->GetEffective() : DefaultClimate;

	if (Seasons.IsValidIndex(CurrentSeasonIndex))
	{
		const FArcClimateParams& Season = Seasons[CurrentSeasonIndex];
		Result.Temperature += Season.Temperature;
		Result.Humidity = FMath::Clamp(Result.Humidity + Season.Humidity, 0.f, 100.f);
	}

	return Result;
}

FArcClimateParams UArcWeatherSubsystem::GetBaseClimateAtLocation(const FVector& Location) const
{
	const FArcWeatherCell* Cell = Grid.Find(WorldToCell(Location));
	return Cell ? Cell->BaseClimate : DefaultClimate;
}
