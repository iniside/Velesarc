// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcWeatherSubsystem.h"

#include "ArcWeatherLevelSettings.h"

UE_DEFINE_MASS_SUBSYSTEM_WRAPPER(UArcWeatherSubsystem, FMassSystem_ArcWeatherSubsystem);

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

void UArcWeatherSubsystem::SetBaseClimate(const FIntVector& CellCoords, const FArcClimateParams& Climate,
	float HumidityThreshold, float FreezeThreshold)
{
	FArcWeatherCell& Cell = Grid.FindOrAdd(CellCoords);
	Cell.BaseClimate = Climate;
	Cell.HumidityThreshold = HumidityThreshold;
	Cell.FreezeThreshold = FreezeThreshold;
}

void UArcWeatherSubsystem::SetBaseClimateInBounds(const FBox& Bounds, const FArcClimateParams& Climate,
	float HumidityThreshold, float FreezeThreshold)
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
				Cell.HumidityThreshold = HumidityThreshold;
				Cell.FreezeThreshold = FreezeThreshold;
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

int32 UArcWeatherSubsystem::GetRainLevelAtLocation(const FVector& Location) const
{
	const FArcWeatherCell* Cell = Grid.Find(WorldToCell(Location));
	const FArcClimateParams Weather = GetWeatherAtLocation(Location);
	const float HumThreshold = Cell ? Cell->HumidityThreshold : DefaultHumidityThreshold;
	const float FrzThreshold = Cell ? Cell->FreezeThreshold : DefaultFreezeThreshold;

	if (Weather.Temperature <= FrzThreshold)
	{
		return 0;
	}

	const UArcWeatherLevelSettings* Settings = GetDefault<UArcWeatherLevelSettings>();
	if (Weather.Humidity >= HumThreshold + Settings->RainLevel3)
	{
		return 3;
	}
	if (Weather.Humidity >= HumThreshold + Settings->RainLevel2)
	{
		return 2;
	}
	if (Weather.Humidity >= HumThreshold + Settings->RainLevel1)
	{
		return 1;
	}
	return 0;
}

int32 UArcWeatherSubsystem::GetColdLevelAtLocation(const FVector& Location) const
{
	const float Temperature = GetWeatherAtLocation(Location).Temperature;
	const UArcWeatherLevelSettings* Settings = GetDefault<UArcWeatherLevelSettings>();

	if (Temperature <= Settings->ColdLevel3)
	{
		return 3;
	}
	if (Temperature <= Settings->ColdLevel2)
	{
		return 2;
	}
	if (Temperature <= Settings->ColdLevel1)
	{
		return 1;
	}
	return 0;
}

int32 UArcWeatherSubsystem::GetHeatLevelAtLocation(const FVector& Location) const
{
	const float Temperature = GetWeatherAtLocation(Location).Temperature;
	const UArcWeatherLevelSettings* Settings = GetDefault<UArcWeatherLevelSettings>();

	if (Temperature >= Settings->HeatLevel3)
	{
		return 3;
	}
	if (Temperature >= Settings->HeatLevel2)
	{
		return 2;
	}
	if (Temperature >= Settings->HeatLevel1)
	{
		return 1;
	}
	return 0;
}
