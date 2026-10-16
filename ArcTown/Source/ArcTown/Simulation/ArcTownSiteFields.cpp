// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcTownSiteFields.h"

#include "Fields/ArcTerrainMaskDistance.h"
#include "Hydrology/ArcTerrainHydrology.h"
#include "Simulation/ArcTownFieldSettings.h"
#include "Town/ArcTownSiteSnapshot.h"

namespace ArcTownSiteFieldsPrivate
{
namespace
{
	bool SetError(FText& OutError, const FString& Message)
	{
		OutError = FText::FromString(Message);
		return false;
	}
}
}

bool ArcTownSiteFields::BuildSiteFields(
	const FArcTownSiteSnapshot& Site,
	const FArcTownFieldSettings& Settings,
	FArcTownSiteFields& OutFields,
	FText& OutError)
{
	using namespace ArcTownSiteFieldsPrivate;

	OutFields = FArcTownSiteFields();

	if (!Site.IsValid())
	{
		return SetError(OutError, TEXT("ArcTown site fields need a valid sampled site snapshot; sample the site before building them."));
	}

	if (Site.Normals.Num() != Site.Heights.Num())
	{
		return SetError(OutError, FString::Printf(
			TEXT("ArcTown site fields read slope from the snapshot's surface normals, and this snapshot carries %d of them for %d heights. Re-sample the site."),
			Site.Normals.Num(), Site.Heights.Num()));
	}

	if (!FMath::IsFinite(Settings.ElevationQuantumM) || Settings.ElevationQuantumM <= 0.0
		|| !FMath::IsFinite(Settings.MaxBuildSlopeDeg) || Settings.MaxBuildSlopeDeg < 0.0 || Settings.MaxBuildSlopeDeg >= 90.0
		|| !FMath::IsFinite(Settings.MinWaterClearanceCm) || Settings.MinWaterClearanceCm < 0.0
		|| !FMath::IsFinite(Settings.MinHeightAboveChannelM) || Settings.MinHeightAboveChannelM < 0.0)
	{
		return SetError(OutError, TEXT("ArcTown site fields need a positive ElevationQuantumM, a MaxBuildSlopeDeg in [0, 90), and non-negative MinWaterClearanceCm and MinHeightAboveChannelM."));
	}

	const int32 Width = Site.GridDims.X;
	const int32 Height = Site.GridDims.Y;
	const int32 CellCount = Site.Heights.Num();

	// The snapshot stores centimetres; FArcHydrologyGrid is metres throughout.
	TArray<double> HeightMeters;
	HeightMeters.SetNumUninitialized(CellCount);
	for (int32 CellIndex = 0; CellIndex < CellCount; ++CellIndex)
	{
		HeightMeters[CellIndex] = static_cast<double>(Site.Heights[CellIndex]) * 0.01;
	}

	FArcHydrologyGrid Grid;
	Grid.Dimensions = Site.GridDims;
	Grid.SiteDimensions = Site.GetSiteDims();
	Grid.MarginCells = Site.MarginCells;
	Grid.CellSizeM = Site.SampleSpacing * 0.01;
	Grid.HeightMeters = HeightMeters;

	int32 ValidatedCellCount = 0;
	if (!ArcTerrainHydrology::ValidateGrid(Grid, ValidatedCellCount, OutError))
	{
		return false;
	}

	FArcHydrologySettings HydrologySettings;
	HydrologySettings.ElevationQuantumM = Settings.ElevationQuantumM;

	TArray<double> FilledHeights;
	TArray<int32> TraversalOrder;
	int32 FilledCellCount = 0;
	int32 SaturatedFlatCellCount = 0;
	if (!ArcTerrainHydrology::PriorityFloodEpsilon(Grid, HydrologySettings, FilledHeights, TraversalOrder,
		FilledCellCount, SaturatedFlatCellCount, OutError))
	{
		return false;
	}

	TArray<int32> D8Receiver;
	ArcTerrainHydrology::ComputeD8Receivers(Grid, FilledHeights, TraversalOrder, D8Receiver);

	TArray<int32> HandBase;
	ArcTerrainHydrology::ComputeHAND(Site.ChannelMask, D8Receiver, HandBase);

	TArray<double> SquaredCellDistances;
	ArcTerrainMaskDistance::BuildSquaredCellDistanceToMask(
		Site.ChannelMask, Width, Height, SquaredCellDistances);

	OutFields.Dims = Site.GridDims;
	OutFields.MarginCells = Site.MarginCells;
	OutFields.SpacingCm = Site.SampleSpacing;
	OutFields.Slope.SetNumUninitialized(CellCount);
	OutFields.DistanceToWaterCm.SetNumUninitialized(CellCount);
	OutFields.HeightAboveChannel.SetNumUninitialized(CellCount);
	OutFields.bBuildable.SetNumUninitialized(CellCount);

	const double MaxBuildSlope = FMath::Tan(FMath::DegreesToRadians(Settings.MaxBuildSlopeDeg));

	for (int32 CellIndex = 0; CellIndex < CellCount; ++CellIndex)
	{
		const double NormalZ = FMath::Clamp(
			static_cast<double>(Site.Normals[CellIndex].Z), UE_DOUBLE_KINDA_SMALL_NUMBER, 1.0);
		const double Slope = FMath::Tan(FMath::Acos(NormalZ));
		OutFields.Slope[CellIndex] = static_cast<float>(Slope);

		const double DistanceToWaterCm = FMath::Sqrt(SquaredCellDistances[CellIndex]) * Site.SampleSpacing;
		OutFields.DistanceToWaterCm[CellIndex] = static_cast<float>(DistanceToWaterCm);

		// No channel on this cell's receiver chain means no known water level, not water underfoot:
		const int32 BaseIndex = HandBase[CellIndex];
		const double HeightAboveChannel = BaseIndex != INDEX_NONE
			? FMath::Max(0.0, FilledHeights[CellIndex] - FilledHeights[BaseIndex])
			: static_cast<double>(TNumericLimits<float>::Max());
		OutFields.HeightAboveChannel[CellIndex] = static_cast<float>(HeightAboveChannel);

		OutFields.bBuildable[CellIndex] = Slope <= MaxBuildSlope
			&& DistanceToWaterCm >= Settings.MinWaterClearanceCm
			&& HeightAboveChannel >= Settings.MinHeightAboveChannelM
			? 1
			: 0;
	}

	OutError = FText::GetEmpty();
	return true;
}
