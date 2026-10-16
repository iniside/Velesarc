// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcEcotopeSiteFields.h"

#include "Climate/ArcClimateSettings.h"
#include "Hydrology/ArcTerrainHydrology.h"
#include "Simulation/ArcEcotopeSiteBaker.h"
#include "Simulation/ArcEcotopeSiteFieldSettings.h"
#include "Simulation/ArcEcotopeSiteMetadata.h"

namespace ArcEcotopeSiteFieldsPrivate
{
	/** Floor of tan(beta) in the wetness index, so a pit floor does not divide by zero. */
	constexpr double MinTangentBeta = 0.005;

	/** Weight of the wetness term relative to the riparian term, per the plan's moisture formula. */
	constexpr double TwiTermScale = 0.85;

	bool SetError(FText& OutError, const FString& Message)
	{
		OutError = FText::FromString(Message);
		return false;
	}

	/** Validity band of McCune & Keon 2002 Eq. 3; the polynomial goes negative beyond it. */
	constexpr double MaxSunLoadSlopeDegrees = 60.0;

	/** At this slope the aspect term spans under one percent of the normalized range, so the cell carries no aspect signal. */
	constexpr double FlatSlopeDegrees = 0.5;

	constexpr double ContrastWindowHalfWidthDegrees = 22.5;

	constexpr double HottestAzimuthDegrees = 225.0;
	constexpr double ColdestAzimuthDegrees = 45.0;

	constexpr double ReferenceLatitudeDegrees = 45.0;
	constexpr double ReferenceRadiationMinimum = 0.0873;
	constexpr double ReferenceRadiationMaximum = 1.0056;

	double EvaluateMcCuneKeonRadiation(const double CosLatitude, const double SinLatitude,
		const double CosSlope, const double SinSlope, const double CosFoldedAspect)
	{
		return 0.339
			+ 0.808 * CosLatitude * CosSlope
			- 0.196 * SinLatitude * SinSlope
			- 0.482 * CosFoldedAspect * SinSlope;
	}

	/** The stored turn runs from local +X toward +Y in the same sense as compass north to east, so north at NorthYawDegrees subtracts off. */
	double GetAzimuthEastOfNorthDegrees(const double AspectNormalized, const double NorthYawDegrees)
	{
		double Azimuth = FMath::Fmod(AspectNormalized * 360.0 - NorthYawDegrees, 360.0);
		if (Azimuth < 0.0)
		{
			Azimuth += 360.0;
		}
		return Azimuth;
	}

	double MirrorAzimuthEastWest(const double AzimuthEastOfNorthDegrees)
	{
		return FMath::Fmod(360.0 - AzimuthEastOfNorthDegrees, 360.0);
	}

	double FoldAzimuthToHeatLoadDegrees(const double AzimuthEastOfNorthDegrees)
	{
		return FMath::Abs(180.0 - FMath::Abs(AzimuthEastOfNorthDegrees - HottestAzimuthDegrees));
	}

	bool IsAzimuthInContrastWindow(const double AzimuthEastOfNorthDegrees, const double WindowCentreDegrees)
	{
		return FMath::Abs(FMath::UnwindDegrees(AzimuthEastOfNorthDegrees - WindowCentreDegrees))
			<= ContrastWindowHalfWidthDegrees;
	}

	void SweepRadiationBounds(const double LatitudeDegrees, double& OutMinimum, double& OutMaximum)
	{
		constexpr int32 SlopeSampleCount = 2401;
		constexpr int32 AspectSampleCount = 721;

		const double LatitudeRadians = FMath::DegreesToRadians(LatitudeDegrees);
		const double CosLatitude = FMath::Cos(LatitudeRadians);
		const double SinLatitude = FMath::Sin(LatitudeRadians);

		TArray<double> AspectCosines;
		AspectCosines.Reserve(AspectSampleCount);
		for (int32 AspectSample = 0; AspectSample < AspectSampleCount; ++AspectSample)
		{
			const double AspectDegrees = 180.0 * static_cast<double>(AspectSample)
				/ static_cast<double>(AspectSampleCount - 1);
			AspectCosines.Add(FMath::Cos(FMath::DegreesToRadians(AspectDegrees)));
		}

		OutMinimum = TNumericLimits<double>::Max();
		OutMaximum = -TNumericLimits<double>::Max();
		for (int32 SlopeSample = 0; SlopeSample < SlopeSampleCount; ++SlopeSample)
		{
			const double SlopeDegrees = MaxSunLoadSlopeDegrees * static_cast<double>(SlopeSample)
				/ static_cast<double>(SlopeSampleCount - 1);
			const double SlopeRadians = FMath::DegreesToRadians(SlopeDegrees);
			const double CosSlope = FMath::Cos(SlopeRadians);
			const double SinSlope = FMath::Sin(SlopeRadians);

			for (const double CosFoldedAspect : AspectCosines)
			{
				const double Radiation = EvaluateMcCuneKeonRadiation(
					CosLatitude, SinLatitude, CosSlope, SinSlope, CosFoldedAspect);
				OutMinimum = FMath::Min(OutMinimum, Radiation);
				OutMaximum = FMath::Max(OutMaximum, Radiation);
			}
		}
	}

	/** Half width of the Winstral Sx window around the prevailing wind azimuth. */
	constexpr double WindWindowHalfWidthDegrees = 30.0;

	constexpr double MaxSlopeDegrees = 90.0;

	/** Field values the horizon pass stores when it is skipped: an unobstructed sky and neutral shelter. */
	constexpr float SkippedSkyViewFactor = 1.0f;
	constexpr float SkippedWindExposure = 0.5f;

	/** Horizon angle below which a blocker moves neither reduction by a measurable amount. */
	constexpr double NegligibleHorizonAngleDegrees = 1.0;

	/**
	 * The nearest blocker a ray can hit is one coarse marching cell away, so the largest angle the
	 * volume can register is atan(relief / that distance); below this relief even that is negligible.
	 */
	double CalculateAutoMinReliefM(const double CoarseCellSizeM, const double OffsetM)
	{
		return FMath::Tan(FMath::DegreesToRadians(NegligibleHorizonAngleDegrees)) * CoarseCellSizeM + OffsetM;
	}

	/** Slope at which the shed term reaches its full strength, 40 degrees. */
	constexpr double SoilShedSlopeRadians = 0.7;

	uint64 GetHydrologySolutionAllocatedBytes(const FArcHydrologySolution& Solution)
	{
		return Solution.FilledHeights.GetAllocatedSize()
			+ Solution.TraversalOrder.GetAllocatedSize()
			+ Solution.TangentBeta.GetAllocatedSize()
			+ Solution.D8Receiver.GetAllocatedSize()
			+ Solution.HandBase.GetAllocatedSize();
	}
}

bool ArcEcotopeSiteFields::GenerateMoisture(
	const FArcEcotopeMarginedHeightGrid& HeightGrid,
	const FArcClimateFieldSettings& Settings,
	const TConstArrayView<double> Accumulation,
	const TConstArrayView<uint8> ChannelMask,
	TArray<float>& OutBaseMoisture,
	TArray<float>& OutBaseHandM,
	FMoistureDiagnostics& OutDiagnostics,
	FText& OutError)
{
	using namespace ArcEcotopeSiteFieldsPrivate;

	OutDiagnostics = FMoistureDiagnostics();

	FArcHydrologyGrid HydrologyGrid;
	HydrologyGrid.Dimensions = HeightGrid.Dimensions;
	HydrologyGrid.SiteDimensions = HeightGrid.SiteDimensions;
	HydrologyGrid.MarginCells = HeightGrid.MarginCells;
	HydrologyGrid.CellSizeM = HeightGrid.CellSizeM;
	HydrologyGrid.HeightMeters = HeightGrid.HeightMeters;

	int32 CellCount = 0;
	if (!ArcTerrainHydrology::ValidateGrid(HydrologyGrid, CellCount, OutError))
	{
		return false;
	}

	const int32 SiteWidth = HeightGrid.SiteDimensions.X;
	const int32 SiteHeight = HeightGrid.SiteDimensions.Y;
	const double CellSizeM = HeightGrid.CellSizeM;
	const double CellAreaSquareM = CellSizeM * CellSizeM;

	const double QuantumM = static_cast<double>(Settings.ElevationQuantumM);
	const double TwiMidpoint = static_cast<double>(Settings.WetnessIndexMidpoint);
	const double TwiWidth = static_cast<double>(Settings.WetnessIndexWidth);
	const double HandDecayM = static_cast<double>(Settings.HeightAboveDrainageDecayM);
	if (!FMath::IsFinite(QuantumM) || QuantumM <= 0.0
		|| !FMath::IsFinite(TwiWidth) || TwiWidth <= 0.0
		|| !FMath::IsFinite(HandDecayM) || HandDecayM <= 0.0
		|| !FMath::IsFinite(TwiMidpoint))
	{
		return SetError(OutError, TEXT("ArcEcotope moisture requires positive ElevationQuantumM, WetnessIndexWidth and HeightAboveDrainageDecayM."));
	}

	FArcHydrologySettings HydrologySettings;
	HydrologySettings.ElevationQuantumM = QuantumM;

	FArcHydrologySolution Solution;
	if (!ArcTerrainHydrology::Solve(HydrologyGrid, HydrologySettings, Accumulation, ChannelMask, Solution, OutError))
	{
		return false;
	}

	const TArray<double>& FilledHeights = Solution.FilledHeights;
	const TArray<float>& TangentBeta = Solution.TangentBeta;
	const TArray<int32>& HandBase = Solution.HandBase;

	OutBaseMoisture.Reset();
	OutBaseMoisture.AddUninitialized(SiteWidth * SiteHeight);
	OutBaseHandM.Reset();
	OutBaseHandM.AddUninitialized(SiteWidth * SiteHeight);
	const uint64 SolutionBytes = GetHydrologySolutionAllocatedBytes(Solution);
	const uint64 HandTransientUpperBound = static_cast<uint64>(CellCount) * (sizeof(uint8) + sizeof(int32)) * 2;
	double MinHeightM = HeightGrid.HeightMeters[0];
	double MaxHeightM = HeightGrid.HeightMeters[0];
	for (const double HeightM : HeightGrid.HeightMeters)
	{
		MinHeightM = FMath::Min(MinHeightM, HeightM);
		MaxHeightM = FMath::Max(MaxHeightM, HeightM);
	}
	const uint64 BucketCount = static_cast<uint64>(FMath::FloorToInt64((MaxHeightM - MinHeightM) / QuantumM)) + 2;
	const uint64 PriorityFloodUpperBound = Solution.FilledHeights.GetAllocatedSize()
		+ Solution.TraversalOrder.GetAllocatedSize()
		+ static_cast<uint64>(CellCount) * (sizeof(double) + sizeof(uint8) + sizeof(int32)) * 2
		+ BucketCount * sizeof(int32) * 2;
	OutDiagnostics.PeakWorkingBytes = FMath::Max(
		SolutionBytes + OutBaseMoisture.GetAllocatedSize() + HandTransientUpperBound,
		PriorityFloodUpperBound);

	double TwiMin = 0.0;
	double TwiMax = 0.0;
	double TwiSum = 0.0;
	double HandMin = 0.0;
	double HandMax = 0.0;
	double HandSum = 0.0;
	int32 HandSampleCount = 0;
	int32 HandUnresolvedCellCount = 0;
	double MoistureMin = 0.0;
	double MoistureMax = 0.0;
	double MoistureSum = 0.0;

	for (int32 SiteY = 0; SiteY < SiteHeight; ++SiteY)
	{
		for (int32 SiteX = 0; SiteX < SiteWidth; ++SiteX)
		{
			const int32 CellIndex = HeightGrid.GetIndexFromSiteCell(SiteX, SiteY);
			// A supplied map reads zero across most of its extent, and Loge(0) is -inf; one cell area is
			// the area the cell contributes to itself, so the index degenerates to a slope index instead.
			const double SpecificAreaM = FMath::Max(Accumulation[CellIndex], CellAreaSquareM) / CellSizeM;
			const double Twi = FMath::Loge(SpecificAreaM
				/ FMath::Max(static_cast<double>(TangentBeta[CellIndex]), MinTangentBeta));

			const double Logistic = 1.0 / (1.0 + FMath::Exp(-(Twi - TwiMidpoint) / TwiWidth));

			double HandTerm = 0.0;
			double HandM = 0.0;
			const int32 BaseIndex = HandBase[CellIndex];
			if (BaseIndex != INDEX_NONE)
			{
				HandM = FMath::Max(0.0, FilledHeights[CellIndex] - FilledHeights[BaseIndex]);
				HandTerm = FMath::Exp(-HandM / HandDecayM);
			}
			else
			{
				++HandUnresolvedCellCount;
			}

			const double Moisture = FMath::Clamp(FMath::Max(HandTerm, TwiTermScale * Logistic), 0.0, 1.0);
			const int32 SiteIndex = SiteY * SiteWidth + SiteX;
			OutBaseMoisture[SiteIndex] = static_cast<float>(Moisture);
			// Zero metres above drainage is the drainage bottom itself, so a cell that reached no channel
			// carries a negative sentinel instead.
			OutBaseHandM[SiteIndex] = BaseIndex != INDEX_NONE ? static_cast<float>(HandM) : -1.0f;

			if (SiteIndex == 0)
			{
				TwiMin = Twi;
				TwiMax = Twi;
				MoistureMin = Moisture;
				MoistureMax = Moisture;
			}
			else
			{
				TwiMin = FMath::Min(TwiMin, Twi);
				TwiMax = FMath::Max(TwiMax, Twi);
				MoistureMin = FMath::Min(MoistureMin, Moisture);
				MoistureMax = FMath::Max(MoistureMax, Moisture);
			}
			TwiSum += Twi;
			MoistureSum += Moisture;

			if (BaseIndex != INDEX_NONE)
			{
				if (HandSampleCount == 0)
				{
					HandMin = HandM;
					HandMax = HandM;
				}
				else
				{
					HandMin = FMath::Min(HandMin, HandM);
					HandMax = FMath::Max(HandMax, HandM);
				}
				HandSum += HandM;
				++HandSampleCount;
			}
		}
	}

	const double SiteCellCount = static_cast<double>(SiteWidth) * static_cast<double>(SiteHeight);
	OutDiagnostics.TwiMin = TwiMin;
	OutDiagnostics.TwiMean = TwiSum / SiteCellCount;
	OutDiagnostics.TwiMax = TwiMax;
	OutDiagnostics.HandMinM = HandMin;
	OutDiagnostics.HandMeanM = HandSampleCount > 0 ? HandSum / static_cast<double>(HandSampleCount) : 0.0;
	OutDiagnostics.HandMaxM = HandMax;
	OutDiagnostics.MoistureMin = MoistureMin;
	OutDiagnostics.MoistureMean = MoistureSum / SiteCellCount;
	OutDiagnostics.MoistureMax = MoistureMax;
	OutDiagnostics.FilledCellCount = Solution.FilledCellCount;
	OutDiagnostics.SaturatedFlatCellCount = Solution.SaturatedFlatCellCount;
	OutDiagnostics.ChannelCellCount = Solution.ChannelCellCount;
	OutDiagnostics.HandUnresolvedCellCount = HandUnresolvedCellCount;

	OutError = FText::GetEmpty();
	return true;
}

bool ArcEcotopeSiteFields::GenerateSunLoadRect(
	const FArcEcotopeSiteMetadata& Metadata,
	const TConstArrayView<float> BaseSlopeDegrees,
	const TConstArrayView<float> BaseAspectNormalized,
	const TConstArrayView<uint8> BaseValidSurface,
	const FArcClimateFieldSettings& Settings,
	const FIntRect& BaseRect,
	TArray<float>& OutSunLoad,
	FSunLoadDiagnostics& OutDiagnostics,
	FText& OutError)
{
	using namespace ArcEcotopeSiteFieldsPrivate;

	OutDiagnostics = FSunLoadDiagnostics();
	const int32 RectCellCount = BaseRect.Width() * BaseRect.Height();
	if (Metadata.GetCellCount() <= 0 || BaseSlopeDegrees.Num() != RectCellCount
		|| BaseAspectNormalized.Num() != RectCellCount || BaseValidSurface.Num() != RectCellCount
		|| BaseRect.Min.X < 0 || BaseRect.Min.Y < 0
		|| BaseRect.Max.X > Metadata.GridDimensions.X || BaseRect.Max.Y > Metadata.GridDimensions.Y
		|| BaseRect.Width() <= 0 || BaseRect.Height() <= 0)
	{
		return SetError(OutError, TEXT("ArcEcotope sun load rectangle needs in-bounds slope, aspect and validity sources."));
	}

	const double LatitudeDegrees = static_cast<double>(Settings.LatitudeDeg);
	const double NorthYawDegrees = static_cast<double>(Settings.NorthYawDeg);
	if (!FMath::IsFinite(LatitudeDegrees) || FMath::Abs(LatitudeDegrees) > 90.0 || !FMath::IsFinite(NorthYawDegrees))
	{
		return SetError(OutError, TEXT("ArcEcotope sun load requires a finite LatitudeDeg within 90 degrees and a finite NorthYawDeg."));
	}

	double RadiationMinimum = 0.0;
	double RadiationMaximum = 0.0;
	SweepRadiationBounds(LatitudeDegrees, RadiationMinimum, RadiationMaximum);
	const double RadiationSpan = RadiationMaximum - RadiationMinimum;
	if (!FMath::IsFinite(RadiationSpan) || RadiationSpan <= UE_DOUBLE_KINDA_SMALL_NUMBER)
	{
		return SetError(OutError, TEXT("ArcEcotope sun load found no radiation range to normalize."));
	}

	const double LatitudeRadians = FMath::DegreesToRadians(LatitudeDegrees);
	const double CosLatitude = FMath::Cos(LatitudeRadians);
	const double SinLatitude = FMath::Sin(LatitudeRadians);
	const bool bMirrorEastWest = Settings.bMirrorEastWest;
	OutSunLoad.Reset();
	OutSunLoad.Reserve(BaseRect.Width() * BaseRect.Height());

	double SunLoadMin = 0.0;
	double SunLoadMax = 0.0;
	double SunLoadSum = 0.0;
	int32 SampledCellCount = 0;
	int32 FlatCellCount = 0;
	int32 SlopeClampedCellCount = 0;
	double SouthWestSum = 0.0;
	int32 SouthWestCellCount = 0;
	double NorthEastSum = 0.0;
	int32 NorthEastCellCount = 0;
	for (int32 CellY = BaseRect.Min.Y; CellY < BaseRect.Max.Y; ++CellY)
	{
		for (int32 CellX = BaseRect.Min.X; CellX < BaseRect.Max.X; ++CellX)
		{
			const int32 RectIndex = (CellX - BaseRect.Min.X) + (CellY - BaseRect.Min.Y) * BaseRect.Width();
			const int32 CellIndex = CellX + CellY * Metadata.GridDimensions.X;
			const double SampledSlopeDegrees = static_cast<double>(BaseSlopeDegrees[RectIndex]);
			const double SampledAspectNormalized = static_cast<double>(BaseAspectNormalized[RectIndex]);
			if (!FMath::IsFinite(SampledSlopeDegrees) || !FMath::IsFinite(SampledAspectNormalized))
			{
				return SetError(OutError, FString::Printf(TEXT("ArcEcotope sun load found a non-finite slope or aspect at cell %d."), CellIndex));
			}

			const double SlopeDegrees = FMath::Clamp(SampledSlopeDegrees, 0.0, MaxSunLoadSlopeDegrees);
			const double AzimuthEastOfNorthDegrees = GetAzimuthEastOfNorthDegrees(SampledAspectNormalized, NorthYawDegrees);
			const double EvaluatedAzimuthDegrees = bMirrorEastWest ? MirrorAzimuthEastWest(AzimuthEastOfNorthDegrees) : AzimuthEastOfNorthDegrees;
			const double Radiation = EvaluateMcCuneKeonRadiation(CosLatitude, SinLatitude,
				FMath::Cos(FMath::DegreesToRadians(SlopeDegrees)),
				FMath::Sin(FMath::DegreesToRadians(SlopeDegrees)),
				FMath::Cos(FMath::DegreesToRadians(FoldAzimuthToHeatLoadDegrees(EvaluatedAzimuthDegrees))));
			const double SunLoad = FMath::Clamp((Radiation - RadiationMinimum) / RadiationSpan, 0.0, 1.0);
			OutSunLoad.Add(static_cast<float>(SunLoad));

			if (BaseValidSurface[RectIndex] == 0)
			{
				continue;
			}
			if (SampledCellCount == 0)
			{
				SunLoadMin = SunLoad;
				SunLoadMax = SunLoad;
			}
			else
			{
				SunLoadMin = FMath::Min(SunLoadMin, SunLoad);
				SunLoadMax = FMath::Max(SunLoadMax, SunLoad);
			}
			SunLoadSum += SunLoad;
			++SampledCellCount;
			SlopeClampedCellCount += SampledSlopeDegrees > MaxSunLoadSlopeDegrees ? 1 : 0;
			if (SlopeDegrees < FlatSlopeDegrees)
			{
				++FlatCellCount;
			}
			else if (IsAzimuthInContrastWindow(AzimuthEastOfNorthDegrees, HottestAzimuthDegrees))
			{
				SouthWestSum += SunLoad;
				++SouthWestCellCount;
			}
			else if (IsAzimuthInContrastWindow(AzimuthEastOfNorthDegrees, ColdestAzimuthDegrees))
			{
				NorthEastSum += SunLoad;
				++NorthEastCellCount;
			}
		}
	}

	OutDiagnostics.RadiationMinimum = RadiationMinimum;
	OutDiagnostics.RadiationMaximum = RadiationMaximum;
	OutDiagnostics.SunLoadMin = SunLoadMin;
	OutDiagnostics.SunLoadMean = SampledCellCount > 0 ? SunLoadSum / SampledCellCount : 0.0;
	OutDiagnostics.SunLoadMax = SunLoadMax;
	OutDiagnostics.SouthWestMean = SouthWestCellCount > 0 ? SouthWestSum / SouthWestCellCount : 0.0;
	OutDiagnostics.NorthEastMean = NorthEastCellCount > 0 ? NorthEastSum / NorthEastCellCount : 0.0;
	OutDiagnostics.SouthWestCellCount = SouthWestCellCount;
	OutDiagnostics.NorthEastCellCount = NorthEastCellCount;
	OutDiagnostics.SlopeClampedCellCount = SlopeClampedCellCount;
	OutDiagnostics.FlatCellFraction = SampledCellCount > 0 ? static_cast<double>(FlatCellCount) / SampledCellCount : 0.0;
	OutDiagnostics.SampledCellCount = SampledCellCount;
	OutError = FText::GetEmpty();
	return true;
}

bool ArcEcotopeSiteFields::GenerateHorizonFields(
	const FArcEcotopeMarginedHeightGrid& HeightGrid,
	const FArcEcotopeSiteMetadata& Metadata,
	const TConstArrayView<float> BaseSlopeDegrees,
	const TConstArrayView<uint8> BaseValidSurface,
	const FArcClimateFieldSettings& Settings,
	TArray<float>& OutLevelSkyViewFactor,
	TArray<float>& OutLevelWindExposure,
	FHorizonDiagnostics& OutDiagnostics,
	FText& OutError)
{
	using namespace ArcEcotopeSiteFieldsPrivate;

	OutDiagnostics = FHorizonDiagnostics();

	const uint8 Level = Settings.HorizonLevel;
	const int32 BaseCellCount = Metadata.GetCellCount();
	const int32 LevelCellCount = Metadata.GetLevelCellCount(Level);
	const FIntPoint LevelDimensions = Metadata.GetLevelDimensions(Level);
	if (BaseCellCount <= 0 || LevelCellCount <= 0
		|| BaseSlopeDegrees.Num() != BaseCellCount
		|| BaseValidSurface.Num() != BaseCellCount)
	{
		return SetError(OutError, FString::Printf(
			TEXT("ArcEcotope horizon needs slope and validity arrays of %d cells and a usable level %d; got %d and %d over %d level cells."),
			BaseCellCount,
			static_cast<int32>(Level),
			BaseSlopeDegrees.Num(),
			BaseValidSurface.Num(),
			LevelCellCount));
	}

	const int32 Width = HeightGrid.Dimensions.X;
	const int32 Height = HeightGrid.Dimensions.Y;
	const int32 SiteWidth = HeightGrid.SiteDimensions.X;
	const int32 SiteHeight = HeightGrid.SiteDimensions.Y;
	const int32 MarginCells = HeightGrid.MarginCells;
	const double CellSizeM = HeightGrid.CellSizeM;
	if (SiteWidth != Metadata.GridDimensions.X || SiteHeight != Metadata.GridDimensions.Y
		|| MarginCells < 0
		|| SiteWidth + 2 * MarginCells != Width
		|| SiteHeight + 2 * MarginCells != Height
		|| static_cast<int64>(Width) * static_cast<int64>(Height) != HeightGrid.HeightMeters.Num())
	{
		return SetError(OutError, FString::Printf(
			TEXT("ArcEcotope horizon needs a margined height grid of %dx%d cells matching the snapshot; got %dx%d over a %dx%d site with a %d cell margin."),
			Metadata.GridDimensions.X, Metadata.GridDimensions.Y, Width, Height, SiteWidth, SiteHeight, MarginCells));
	}

	if (!FMath::IsFinite(CellSizeM) || CellSizeM <= 0.0)
	{
		return SetError(OutError, TEXT("ArcEcotope horizon requires a finite positive cell size in metres."));
	}

	const double HorizonRadiusM = static_cast<double>(Settings.HorizonRadiusM);
	const double AuthoredMinReliefM = static_cast<double>(Settings.MinReliefM);
	const double MinReliefOffsetM = static_cast<double>(Settings.MinReliefOffsetM);
	const double RefShelterAngleDegrees = static_cast<double>(Settings.RefShelterAngleDeg);
	const double PrevailingWindAzimuthDegrees = static_cast<double>(Settings.PrevailingWindAzimuthDeg);
	const double WindAnisotropy = static_cast<double>(Settings.WindAnisotropy);
	const double NorthYawDegrees = static_cast<double>(Settings.NorthYawDeg);
	const int32 AzimuthCount = Settings.HorizonAzimuths;
	if (AzimuthCount < 1
		|| !FMath::IsFinite(HorizonRadiusM) || HorizonRadiusM < 0.0
		|| !FMath::IsFinite(AuthoredMinReliefM) || AuthoredMinReliefM < 0.0
		|| !FMath::IsFinite(MinReliefOffsetM) || MinReliefOffsetM < 0.0
		|| !FMath::IsFinite(RefShelterAngleDegrees) || RefShelterAngleDegrees <= 0.0
		|| !FMath::IsFinite(PrevailingWindAzimuthDegrees)
		|| !FMath::IsFinite(WindAnisotropy) || WindAnisotropy < 0.0 || WindAnisotropy > 1.0
		|| !FMath::IsFinite(NorthYawDegrees))
	{
		return SetError(OutError, TEXT("ArcEcotope horizon requires at least one azimuth, a non-negative HorizonRadiusM, MinReliefM and MinReliefOffsetM, a positive RefShelterAngleDeg and a WindAnisotropy within [0, 1]."));
	}

	double MinHeightM = HeightGrid.HeightMeters[0];
	double MaxHeightM = HeightGrid.HeightMeters[0];
	for (const double HeightM : HeightGrid.HeightMeters)
	{
		if (!FMath::IsFinite(HeightM))
		{
			return SetError(OutError, TEXT("ArcEcotope horizon height grid contains a non-finite height."));
		}
		MinHeightM = FMath::Min(MinHeightM, HeightM);
		MaxHeightM = FMath::Max(MaxHeightM, HeightM);
	}

	const double ReliefM = MaxHeightM - MinHeightM;
	const double CoarseCellSizeM = CellSizeM * static_cast<double>(static_cast<int32>(1) << Level);
	const double MinReliefM = Settings.bAutoMinRelief
		? CalculateAutoMinReliefM(CoarseCellSizeM, MinReliefOffsetM)
		: AuthoredMinReliefM;
	OutDiagnostics.ReliefM = ReliefM;
	OutDiagnostics.EffectiveMinReliefM = MinReliefM;
	OutDiagnostics.bMinReliefAutoDerived = Settings.bAutoMinRelief;
	OutDiagnostics.LevelDimensions = LevelDimensions;
	OutDiagnostics.AzimuthCount = AzimuthCount;

	OutLevelSkyViewFactor.Reset();
	OutLevelWindExposure.Reset();

	TArray<uint8> LevelAnyChildValid;
	LevelAnyChildValid.Init(0, LevelCellCount);
	for (int32 BaseCellIndex = 0; BaseCellIndex < BaseCellCount; ++BaseCellIndex)
	{
		if (BaseValidSurface[BaseCellIndex] != 0)
		{
			const int32 LevelCellIndex = Metadata.MapCellIndexBetweenLevels(BaseCellIndex, 0, Level);
			if (LevelCellIndex == INDEX_NONE)
			{
				return SetError(OutError, FString::Printf(
					TEXT("ArcEcotope horizon could not map base cell %d to level %d."),
					BaseCellIndex, static_cast<int32>(Level)));
			}
			LevelAnyChildValid[LevelCellIndex] = 1;
		}
	}
	int32 LevelAnyChildValidCount = 0;
	for (const uint8 AnyValid : LevelAnyChildValid)
	{
		LevelAnyChildValidCount += AnyValid != 0 ? 1 : 0;
	}

	const EHorizonSkipReason SkipReason = HorizonRadiusM <= 0.0
		? EHorizonSkipReason::ZeroRadius
		: (ReliefM < MinReliefM ? EHorizonSkipReason::ReliefBelowMinimum : EHorizonSkipReason::None);
	if (SkipReason != EHorizonSkipReason::None)
	{
		OutLevelSkyViewFactor.Init(SkippedSkyViewFactor, LevelCellCount);
		OutLevelWindExposure.Init(SkippedWindExposure, LevelCellCount);

		OutDiagnostics.SkipReason = SkipReason;
		OutDiagnostics.SkyViewFactorMin = SkippedSkyViewFactor;
		OutDiagnostics.SkyViewFactorMean = SkippedSkyViewFactor;
		OutDiagnostics.SkyViewFactorMax = SkippedSkyViewFactor;
		OutDiagnostics.WindExposureMin = SkippedWindExposure;
		OutDiagnostics.WindExposureMean = SkippedWindExposure;
		OutDiagnostics.WindExposureMax = SkippedWindExposure;
		OutDiagnostics.SampledCellCount = LevelAnyChildValidCount;
		OutDiagnostics.PeakWorkingBytes = LevelAnyChildValid.GetAllocatedSize()
			+ OutLevelSkyViewFactor.GetAllocatedSize() + OutLevelWindExposure.GetAllocatedSize();

		OutError = FText::GetEmpty();
		return true;
	}

	TArray<float> BaseCosineSlope;
	BaseCosineSlope.AddUninitialized(BaseCellCount);
	for (int32 BaseCellIndex = 0; BaseCellIndex < BaseCellCount; ++BaseCellIndex)
	{
		const double SlopeDegrees = static_cast<double>(BaseSlopeDegrees[BaseCellIndex]);
		if (!FMath::IsFinite(SlopeDegrees))
		{
			return SetError(OutError, FString::Printf(
				TEXT("ArcEcotope horizon found a non-finite slope at cell %d."), BaseCellIndex));
		}

		BaseCosineSlope[BaseCellIndex] = static_cast<float>(FMath::Cos(FMath::DegreesToRadians(
			FMath::Clamp(SlopeDegrees, 0.0, MaxSlopeDegrees))));
	}

	TArray<float> LevelCosineSlope;
	if (!ArcEcotopeSiteFields::ReduceFieldToLevel(Metadata, BaseCosineSlope, Level, LevelCosineSlope))
	{
		return SetError(OutError, FString::Printf(
			TEXT("ArcEcotope horizon could not reduce the slope cosine to level %d."), static_cast<int32>(Level)));
	}

	// Blocker heights are the arithmetic mean of the 2^Level block of base cells the level cell sits in; at
	// the site edge the block completes from the margin. The operator is frozen: a different pooling gives
	// a different horizon for the same terrain in a neighbouring volume.
	const int32 BaseCellsPerLevelCell = static_cast<int32>(1) << Level;
	const int32 CoarseMinX = (-MarginCells) >> Level;
	const int32 CoarseMinY = (-MarginCells) >> Level;
	const int32 CoarseWidth = ((SiteWidth + MarginCells - 1) >> Level) - CoarseMinX + 1;
	const int32 CoarseHeight = ((SiteHeight + MarginCells - 1) >> Level) - CoarseMinY + 1;
	const int64 CoarseCellCount64 = static_cast<int64>(CoarseWidth) * static_cast<int64>(CoarseHeight);
	if (CoarseWidth <= 0 || CoarseHeight <= 0 || CoarseCellCount64 > MAX_int32)
	{
		return SetError(OutError, TEXT("ArcEcotope horizon could not build a coarse height grid for the margined lattice."));
	}
	const int32 CoarseCellCount = static_cast<int32>(CoarseCellCount64);
	OutDiagnostics.CoarseDimensions = FIntPoint(CoarseWidth, CoarseHeight);

	TArray<double> CoarseHeightSums;
	CoarseHeightSums.Init(0.0, CoarseCellCount);
	TArray<int32> CoarseHeightCounts;
	CoarseHeightCounts.Init(0, CoarseCellCount);
	for (int32 MarginedY = 0; MarginedY < Height; ++MarginedY)
	{
		const int32 CoarseY = ((MarginedY - MarginCells) >> Level) - CoarseMinY;
		for (int32 MarginedX = 0; MarginedX < Width; ++MarginedX)
		{
			const int32 CoarseX = ((MarginedX - MarginCells) >> Level) - CoarseMinX;
			const int32 CoarseIndex = CoarseY * CoarseWidth + CoarseX;
			CoarseHeightSums[CoarseIndex] += HeightGrid.HeightMeters[MarginedY * Width + MarginedX];
			++CoarseHeightCounts[CoarseIndex];
		}
	}

	TArray<double> CoarseHeights;
	CoarseHeights.AddUninitialized(CoarseCellCount);
	for (int32 CoarseIndex = 0; CoarseIndex < CoarseCellCount; ++CoarseIndex)
	{
		if (CoarseHeightCounts[CoarseIndex] <= 0)
		{
			return SetError(OutError, TEXT("ArcEcotope horizon coarse height grid has a cell with no base samples."));
		}
		CoarseHeights[CoarseIndex] = CoarseHeightSums[CoarseIndex]
			/ static_cast<double>(CoarseHeightCounts[CoarseIndex]);
	}

	TArray<double> AzimuthDirectionX;
	TArray<double> AzimuthDirectionY;
	TArray<double> AzimuthWeight;
	AzimuthDirectionX.AddUninitialized(AzimuthCount);
	AzimuthDirectionY.AddUninitialized(AzimuthCount);
	AzimuthWeight.AddUninitialized(AzimuthCount);
	int32 NearestWindAzimuth = 0;
	double NearestWindOffsetDegrees = TNumericLimits<double>::Max();
	TArray<uint8> IsInWindWindow;
	IsInWindWindow.Init(0, AzimuthCount);
	for (int32 Azimuth = 0; Azimuth < AzimuthCount; ++Azimuth)
	{
		const double AzimuthEastOfNorthDegrees = 360.0 * static_cast<double>(Azimuth)
			/ static_cast<double>(AzimuthCount);
		const double LocalTurnRadians = FMath::DegreesToRadians(AzimuthEastOfNorthDegrees + NorthYawDegrees);
		AzimuthDirectionX[Azimuth] = FMath::Cos(LocalTurnRadians);
		AzimuthDirectionY[Azimuth] = FMath::Sin(LocalTurnRadians);

		const double OffsetDegrees = FMath::Abs(FMath::UnwindDegrees(
			AzimuthEastOfNorthDegrees - PrevailingWindAzimuthDegrees));
		IsInWindWindow[Azimuth] = OffsetDegrees <= WindWindowHalfWidthDegrees ? 1 : 0;
		if (OffsetDegrees < NearestWindOffsetDegrees)
		{
			NearestWindOffsetDegrees = OffsetDegrees;
			NearestWindAzimuth = Azimuth;
		}
	}
	IsInWindWindow[NearestWindAzimuth] = 1;

	double AzimuthWeightTotal = 0.0;
	for (int32 Azimuth = 0; Azimuth < AzimuthCount; ++Azimuth)
	{
		AzimuthWeight[Azimuth] = (1.0 - WindAnisotropy)
			+ WindAnisotropy * static_cast<double>(IsInWindWindow[Azimuth]);
		AzimuthWeightTotal += AzimuthWeight[Azimuth];
	}

	const int32 MaxStepCount = FMath::Max(1, FMath::FloorToInt32(HorizonRadiusM / CoarseCellSizeM));

	OutLevelSkyViewFactor.AddUninitialized(LevelCellCount);
	OutLevelWindExposure.AddUninitialized(LevelCellCount);

	double SkyViewMin = 0.0;
	double SkyViewMax = 0.0;
	double SkyViewSum = 0.0;
	double WindMin = 0.0;
	double WindMax = 0.0;
	double WindSum = 0.0;
	double HorizonAngleSum = 0.0;
	int32 SampledCellCount = 0;
	int32 NoBlockerCellCount = 0;
	int32 SkyViewFactorAtZeroCellCount = 0;
	int32 WindExposureAtOneCellCount = 0;

	for (int32 LevelY = 0; LevelY < LevelDimensions.Y; ++LevelY)
	{
		for (int32 LevelX = 0; LevelX < LevelDimensions.X; ++LevelX)
		{
			const int32 LevelCellIndex = LevelY * LevelDimensions.X + LevelX;

			const int32 ObserverSiteX = FMath::Min(
				LevelX * BaseCellsPerLevelCell + BaseCellsPerLevelCell / 2, SiteWidth - 1);
			const int32 ObserverSiteY = FMath::Min(
				LevelY * BaseCellsPerLevelCell + BaseCellsPerLevelCell / 2, SiteHeight - 1);
			const double ObserverHeightM =
				HeightGrid.HeightMeters[HeightGrid.GetIndexFromSiteCell(ObserverSiteX, ObserverSiteY)];

			const int32 ObserverCoarseX = LevelX - CoarseMinX;
			const int32 ObserverCoarseY = LevelY - CoarseMinY;

			double SineSum = 0.0;
			double WeightedAngleDegreesSum = 0.0;
			bool bAnyBlockerFound = false;

			for (int32 Azimuth = 0; Azimuth < AzimuthCount; ++Azimuth)
			{
				// The horizon is the steepest tangent along the ray; atan is monotone, so one conversion
				// per azimuth is exact and the march itself stays a divide and a compare.
				double MaxTangent = 0.0;
				bool bAnySampleTaken = false;
				for (int32 Step = 1; Step <= MaxStepCount; ++Step)
				{
					const int32 SampleX = ObserverCoarseX
						+ FMath::RoundToInt32(static_cast<double>(Step) * AzimuthDirectionX[Azimuth]);
					const int32 SampleY = ObserverCoarseY
						+ FMath::RoundToInt32(static_cast<double>(Step) * AzimuthDirectionY[Azimuth]);
					if (SampleX < 0 || SampleX >= CoarseWidth || SampleY < 0 || SampleY >= CoarseHeight)
					{
						break;
					}

					const double Tangent = (CoarseHeights[SampleY * CoarseWidth + SampleX] - ObserverHeightM)
						/ (static_cast<double>(Step) * CoarseCellSizeM);
					if (!bAnySampleTaken || Tangent > MaxTangent)
					{
						MaxTangent = Tangent;
					}
					bAnySampleTaken = true;
				}

				const double MaxAngleRadians = FMath::Atan(MaxTangent);
				SineSum += FMath::Sin(FMath::Max(MaxAngleRadians, 0.0));
				WeightedAngleDegreesSum += AzimuthWeight[Azimuth] * FMath::RadiansToDegrees(MaxAngleRadians);
				bAnyBlockerFound |= MaxAngleRadians > 0.0;
			}

			// The (1 + cos S) / 2 factor is the sky view factor of an unobstructed inclined plane: without
			// it a tilted cell is credited with the sky its own ground plane occludes.
			const double MeanSine = SineSum / static_cast<double>(AzimuthCount);
			const double InclinedPlaneFactor =
				(1.0 + static_cast<double>(LevelCosineSlope[LevelCellIndex])) * 0.5;
			const double SkyViewFactor = FMath::Clamp((1.0 - MeanSine) * InclinedPlaneFactor, 0.0, 1.0);

			const double MeanHorizonAngleDegrees = WeightedAngleDegreesSum / AzimuthWeightTotal;
			const double WindExposure = FMath::Clamp(
				0.5 - MeanHorizonAngleDegrees / (2.0 * RefShelterAngleDegrees), 0.0, 1.0);

			OutLevelSkyViewFactor[LevelCellIndex] = static_cast<float>(SkyViewFactor);
			OutLevelWindExposure[LevelCellIndex] = static_cast<float>(WindExposure);

			if (LevelAnyChildValid[LevelCellIndex] == 0)
			{
				continue;
			}

			if (SampledCellCount == 0)
			{
				SkyViewMin = SkyViewFactor;
				SkyViewMax = SkyViewFactor;
				WindMin = WindExposure;
				WindMax = WindExposure;
			}
			else
			{
				SkyViewMin = FMath::Min(SkyViewMin, SkyViewFactor);
				SkyViewMax = FMath::Max(SkyViewMax, SkyViewFactor);
				WindMin = FMath::Min(WindMin, WindExposure);
				WindMax = FMath::Max(WindMax, WindExposure);
			}
			SkyViewSum += SkyViewFactor;
			WindSum += WindExposure;
			HorizonAngleSum += MeanHorizonAngleDegrees;
			++SampledCellCount;

			if (!bAnyBlockerFound)
			{
				++NoBlockerCellCount;
			}
			if (OutLevelSkyViewFactor[LevelCellIndex] == 0.0f)
			{
				++SkyViewFactorAtZeroCellCount;
			}
			if (OutLevelWindExposure[LevelCellIndex] == 1.0f)
			{
				++WindExposureAtOneCellCount;
			}
		}
	}

	const double SampledCells = static_cast<double>(FMath::Max(SampledCellCount, 1));
	OutDiagnostics.SkyViewFactorMin = SkyViewMin;
	OutDiagnostics.SkyViewFactorMean = SkyViewSum / SampledCells;
	OutDiagnostics.SkyViewFactorMax = SkyViewMax;
	OutDiagnostics.WindExposureMin = WindMin;
	OutDiagnostics.WindExposureMean = WindSum / SampledCells;
	OutDiagnostics.WindExposureMax = WindMax;
	OutDiagnostics.MeanHorizonAngleDegrees = HorizonAngleSum / SampledCells;
	OutDiagnostics.NoBlockerWithinRadiusFraction = static_cast<double>(NoBlockerCellCount) / SampledCells;
	OutDiagnostics.SkyViewFactorAtZeroCellCount = SkyViewFactorAtZeroCellCount;
	OutDiagnostics.WindExposureAtOneCellCount = WindExposureAtOneCellCount;
	OutDiagnostics.SampledCellCount = SampledCellCount;
	OutDiagnostics.PeakWorkingBytes = LevelAnyChildValid.GetAllocatedSize()
		+ BaseCosineSlope.GetAllocatedSize() + LevelCosineSlope.GetAllocatedSize()
		+ CoarseHeightSums.GetAllocatedSize() + CoarseHeightCounts.GetAllocatedSize() + CoarseHeights.GetAllocatedSize()
		+ AzimuthDirectionX.GetAllocatedSize() + AzimuthDirectionY.GetAllocatedSize() + AzimuthWeight.GetAllocatedSize()
		+ IsInWindWindow.GetAllocatedSize()
		+ OutLevelSkyViewFactor.GetAllocatedSize() + OutLevelWindExposure.GetAllocatedSize();

	OutError = FText::GetEmpty();
	return true;
}

bool ArcEcotopeSiteFields::GenerateSoilDepthCapacityRect(
	const FArcEcotopeSiteMetadata& Metadata,
	const TConstArrayView<float> BaseSlopeDegrees,
	const TConstArrayView<uint8> BaseValidSurface,
	const TConstArrayView<float> BaseSuppliedDepthM,
	const FArcEcotopeSiteFieldSettings& Settings,
	const FIntRect& BaseRect,
	TArray<float>& OutSoilCapacity,
	FSoilDepthDiagnostics& OutDiagnostics,
	FText& OutError)
{
	using namespace ArcEcotopeSiteFieldsPrivate;

	OutDiagnostics = FSoilDepthDiagnostics();
	const int32 RectCellCount = BaseRect.Width() * BaseRect.Height();
	const int32 SiteWidth = Metadata.GridDimensions.X;
	const int32 SiteHeight = Metadata.GridDimensions.Y;
	if (Metadata.GetCellCount() <= 0 || BaseSlopeDegrees.Num() != RectCellCount || BaseValidSurface.Num() != RectCellCount
		|| BaseSuppliedDepthM.Num() != RectCellCount
		|| BaseRect.Min.X < 0 || BaseRect.Min.Y < 0 || BaseRect.Max.X > SiteWidth || BaseRect.Max.Y > SiteHeight
		|| BaseRect.Width() <= 0 || BaseRect.Height() <= 0)
	{
		return SetError(OutError, TEXT("ArcEcotope soil depth rectangle needs an in-bounds rectangle and matching source views."));
	}

	const double DepthMinM = static_cast<double>(Settings.SoilDepthMinM);
	const double DepthNormalizerM = static_cast<double>(Settings.SoilDepthNormalizerM);
	const double SlopeShedCoefficient = static_cast<double>(Settings.SoilSlopeShedCoefficient);
	if (!FMath::IsFinite(DepthMinM) || DepthMinM < 0.0
		|| !FMath::IsFinite(DepthNormalizerM) || DepthNormalizerM <= 0.0
		|| !FMath::IsFinite(SlopeShedCoefficient) || SlopeShedCoefficient < 0.0 || SlopeShedCoefficient > 1.0)
	{
		return SetError(OutError, TEXT("ArcEcotope soil depth requires finite authored constants in their supported ranges."));
	}

	OutSoilCapacity.Reset();
	OutSoilCapacity.Reserve(BaseRect.Width() * BaseRect.Height());
	double PreClampDepthMin = 0.0;
	double PreClampDepthMax = 0.0;
	double PreClampDepthSum = 0.0;
	double CapacityMin = 0.0;
	double CapacityMax = 0.0;
	double CapacitySum = 0.0;
	double SlopeTermSum = 0.0;
	int32 SampledCellCount = 0;
	int32 DepthAtMinClampCellCount = 0;
	int32 CapacityAtZeroCellCount = 0;
	int32 CapacityAtOneCellCount = 0;
	for (int32 SiteY = BaseRect.Min.Y; SiteY < BaseRect.Max.Y; ++SiteY)
	{
		for (int32 SiteX = BaseRect.Min.X; SiteX < BaseRect.Max.X; ++SiteX)
		{
			const int32 SiteIndex = SiteX + SiteY * SiteWidth;
			const int32 RectIndex = (SiteX - BaseRect.Min.X) + (SiteY - BaseRect.Min.Y) * BaseRect.Width();
			const double SlopeDegrees = static_cast<double>(BaseSlopeDegrees[RectIndex]);
			const double PreClampDepthM = static_cast<double>(BaseSuppliedDepthM[RectIndex]);
			if (!FMath::IsFinite(PreClampDepthM) || !FMath::IsFinite(SlopeDegrees))
			{
				return SetError(OutError, FString::Printf(TEXT("ArcEcotope soil depth found a non-finite supplied depth or slope at cell %d."), SiteIndex));
			}
			const double DepthM = FMath::Max(PreClampDepthM, DepthMinM);
			const double SlopeRadians = FMath::DegreesToRadians(FMath::Clamp(SlopeDegrees, 0.0, MaxSlopeDegrees));
			const double SlopeTerm = 1.0 - SlopeShedCoefficient * FMath::Clamp(SlopeRadians / SoilShedSlopeRadians, 0.0, 1.0);
			const double Capacity = FMath::Clamp(DepthM / DepthNormalizerM, 0.0, 1.0) * SlopeTerm;
			OutSoilCapacity.Add(static_cast<float>(Capacity));
			if (BaseValidSurface[RectIndex] == 0)
			{
				continue;
			}
			if (SampledCellCount == 0)
			{
				PreClampDepthMin = PreClampDepthM; PreClampDepthMax = PreClampDepthM;
				CapacityMin = Capacity; CapacityMax = Capacity;
			}
			else
			{
				PreClampDepthMin = FMath::Min(PreClampDepthMin, PreClampDepthM); PreClampDepthMax = FMath::Max(PreClampDepthMax, PreClampDepthM);
				CapacityMin = FMath::Min(CapacityMin, Capacity); CapacityMax = FMath::Max(CapacityMax, Capacity);
			}
			PreClampDepthSum += PreClampDepthM; CapacitySum += Capacity; SlopeTermSum += SlopeTerm;
			++SampledCellCount;
			DepthAtMinClampCellCount += PreClampDepthM <= DepthMinM ? 1 : 0;
			CapacityAtZeroCellCount += Capacity == 0.0 ? 1 : 0;
			CapacityAtOneCellCount += Capacity == 1.0 ? 1 : 0;
		}
	}
	const double SampledCells = static_cast<double>(FMath::Max(SampledCellCount, 1));
	OutDiagnostics.PreClampDepthMinM = PreClampDepthMin; OutDiagnostics.PreClampDepthMeanM = PreClampDepthSum / SampledCells; OutDiagnostics.PreClampDepthMaxM = PreClampDepthMax;
	OutDiagnostics.CapacityMin = CapacityMin; OutDiagnostics.CapacityMean = CapacitySum / SampledCells; OutDiagnostics.CapacityMax = CapacityMax;
	OutDiagnostics.DepthAtMinClampCellCount = DepthAtMinClampCellCount;
	OutDiagnostics.SlopeTermMean = SlopeTermSum / SampledCells;
	OutDiagnostics.CapacityAtZeroCellCount = CapacityAtZeroCellCount; OutDiagnostics.CapacityAtOneCellCount = CapacityAtOneCellCount; OutDiagnostics.SampledCellCount = SampledCellCount;
	OutError = FText::GetEmpty();
	return true;
}

bool ArcEcotopeSiteFields::ReduceFieldToLevel(
	const FArcEcotopeSiteMetadata& Metadata,
	const TConstArrayView<float> BaseField,
	const uint8 Level,
	TArray<float>& OutLevelField,
	uint64* OutPeakWorkingBytes)
{
	if (OutPeakWorkingBytes != nullptr)
	{
		*OutPeakWorkingBytes = 0;
	}
	const int32 BaseCellCount = Metadata.GetCellCount();
	const int32 LevelCellCount = Metadata.GetLevelCellCount(Level);
	if (BaseCellCount <= 0 || LevelCellCount <= 0 || BaseField.Num() != BaseCellCount)
	{
		return false;
	}

	TArray<double> Sums;
	Sums.Init(0.0, LevelCellCount);
	TArray<int32> Counts;
	Counts.Init(0, LevelCellCount);

	for (int32 BaseCellIndex = 0; BaseCellIndex < BaseCellCount; ++BaseCellIndex)
	{
		const int32 LevelCellIndex = Metadata.MapCellIndexBetweenLevels(BaseCellIndex, 0, Level);
		if (LevelCellIndex == INDEX_NONE)
		{
			return false;
		}

		Sums[LevelCellIndex] += static_cast<double>(BaseField[BaseCellIndex]);
		++Counts[LevelCellIndex];
	}

	OutLevelField.Reset();
	OutLevelField.AddUninitialized(LevelCellCount);
	for (int32 LevelCellIndex = 0; LevelCellIndex < LevelCellCount; ++LevelCellIndex)
	{
		if (Counts[LevelCellIndex] <= 0)
		{
			return false;
		}

		OutLevelField[LevelCellIndex] = static_cast<float>(
			FMath::Clamp(Sums[LevelCellIndex] / static_cast<double>(Counts[LevelCellIndex]), 0.0, 1.0));
	}
	if (OutPeakWorkingBytes != nullptr)
	{
		*OutPeakWorkingBytes = Sums.GetAllocatedSize() + Counts.GetAllocatedSize() + OutLevelField.GetAllocatedSize();
	}

	return true;
}
