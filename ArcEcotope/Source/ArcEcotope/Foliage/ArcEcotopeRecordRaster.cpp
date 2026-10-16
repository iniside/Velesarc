// Copyright Lukasz Baran. All Rights Reserved.

#include "Foliage/ArcEcotopeRecordRaster.h"

#if WITH_EDITOR

#include "Ecotope/ArcEcotopeModel.h"
#include "Simulation/ArcEcotopeSiteMetadata.h"
#include "Simulation/ArcForestWorldSnapshot.h"
#include "Species/ArcEcotopeSpeciesModel.h"

namespace ArcEcotopeRecordRasterPrivate
{
	using UE::ArcEcotope::FFieldRaster;
	using UE::ArcEcotope::FSpeciesCrown;
	using UE::ArcEcotope::TexelCentreLocal;

	int32 TexelIndexAt(const FFieldRaster& Raster, const FVector2D& LocalPosition)
	{
		const int32 X = FMath::FloorToInt32((LocalPosition.X - Raster.OriginLocal.X) / Raster.TexelSizeCm);
		const int32 Y = FMath::FloorToInt32((LocalPosition.Y - Raster.OriginLocal.Y) / Raster.TexelSizeCm);
		if (X < 0 || Y < 0 || X >= Raster.Dimensions.X || Y >= Raster.Dimensions.Y)
		{
			return INDEX_NONE;
		}

		return X + Y * Raster.Dimensions.X;
	}

	/**
	 * Spreads CrownAreaCm2 over CoveredTexelCount texels and deposits into TexelIndices, the in-bounds
	 * subset of them, so a clipped crown deposits about 1.0 per texel rather than concentrating its whole
	 * area on the surviving fraction. A crown containing no texel centre deposits its full area fraction
	 * into the texel holding FallbackLocal instead of vanishing.
	 */
	void DepositArea(FFieldRaster& Raster, const TArray<int32>& TexelIndices, int32 CoveredTexelCount,
		double CrownAreaCm2, const FVector2D& FallbackLocal)
	{
		const double TexelAreaCm2 = Raster.TexelSizeCm * Raster.TexelSizeCm;
		if (!(CrownAreaCm2 > 0.0) || !(TexelAreaCm2 > 0.0))
		{
			return;
		}

		if (CoveredTexelCount <= 0)
		{
			const int32 FallbackIndex = TexelIndexAt(Raster, FallbackLocal);
			if (FallbackIndex != INDEX_NONE)
			{
				Raster.Occupancy[FallbackIndex] += static_cast<float>(CrownAreaCm2 / TexelAreaCm2);
			}
			return;
		}

		const float PerTexel = static_cast<float>(
			CrownAreaCm2 / (TexelAreaCm2 * static_cast<double>(CoveredTexelCount)));
		for (const int32 TexelIndex : TexelIndices)
		{
			Raster.Occupancy[TexelIndex] += PerTexel;
		}
	}

	void DepositDisc(FFieldRaster& Raster, TArray<int32>& Scratch, const FVector2D& CentreLocal,
		double CrownRadiusCm, double StemCount)
	{
		if (!(CrownRadiusCm > 0.0) || !(StemCount > 0.0) || !FMath::IsFinite(CrownRadiusCm))
		{
			return;
		}

		Scratch.Reset();

		// Scanned unclamped: the crown's whole texel count is the normalizer, so a crown clipped by the
		// raster edge keeps its density instead of piling its full area onto the in-bounds survivors.
		const int32 MinX =
			FMath::FloorToInt32((CentreLocal.X - CrownRadiusCm - Raster.OriginLocal.X) / Raster.TexelSizeCm);
		const int32 MaxX =
			FMath::CeilToInt32((CentreLocal.X + CrownRadiusCm - Raster.OriginLocal.X) / Raster.TexelSizeCm);
		const int32 MinY =
			FMath::FloorToInt32((CentreLocal.Y - CrownRadiusCm - Raster.OriginLocal.Y) / Raster.TexelSizeCm);
		const int32 MaxY =
			FMath::CeilToInt32((CentreLocal.Y + CrownRadiusCm - Raster.OriginLocal.Y) / Raster.TexelSizeCm);

		// A crown whose box misses the raster deposits nothing either way, and CrownRadiusHeightRatio has no
		// ClampMax, so without this the scan is unbounded on an authored outlier.
		if (MaxX < 0 || MaxY < 0 || MinX >= Raster.Dimensions.X || MinY >= Raster.Dimensions.Y)
		{
			return;
		}

		const double RadiusSquared = CrownRadiusCm * CrownRadiusCm;
		int32 CoveredTexelCount = 0;
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				if (FVector2D::DistSquared(TexelCentreLocal(Raster, X, Y), CentreLocal) > RadiusSquared)
				{
					continue;
				}

				++CoveredTexelCount;
				if (X >= 0 && Y >= 0 && X < Raster.Dimensions.X && Y < Raster.Dimensions.Y)
				{
					Scratch.Add(X + Y * Raster.Dimensions.X);
				}
			}
		}

		DepositArea(Raster, Scratch, CoveredTexelCount, UE_DOUBLE_PI * RadiusSquared * StemCount, CentreLocal);
	}

	/** A cohort and a patch carry no position, so their stems' crown area spreads over the record cell. */
	void DepositCellRect(FFieldRaster& Raster, TArray<int32>& Scratch, const FVector2D& MinLocal,
		const FVector2D& MaxLocal, double CrownRadiusCm, double StemCount)
	{
		if (!(CrownRadiusCm > 0.0) || !(StemCount > 0.0) || !FMath::IsFinite(CrownRadiusCm)
			|| MaxLocal.X <= MinLocal.X || MaxLocal.Y <= MinLocal.Y)
		{
			return;
		}

		Scratch.Reset();

		const int32 MinX = FMath::Max(0,
			FMath::FloorToInt32((MinLocal.X - Raster.OriginLocal.X) / Raster.TexelSizeCm));
		const int32 MaxX = FMath::Min(Raster.Dimensions.X - 1,
			FMath::CeilToInt32((MaxLocal.X - Raster.OriginLocal.X) / Raster.TexelSizeCm));
		const int32 MinY = FMath::Max(0,
			FMath::FloorToInt32((MinLocal.Y - Raster.OriginLocal.Y) / Raster.TexelSizeCm));
		const int32 MaxY = FMath::Min(Raster.Dimensions.Y - 1,
			FMath::CeilToInt32((MaxLocal.Y - Raster.OriginLocal.Y) / Raster.TexelSizeCm));

		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				const FVector2D Centre = TexelCentreLocal(Raster, X, Y);
				if (Centre.X >= MinLocal.X && Centre.X < MaxLocal.X
					&& Centre.Y >= MinLocal.Y && Centre.Y < MaxLocal.Y)
				{
					Scratch.Add(X + Y * Raster.Dimensions.X);
				}
			}
		}

		DepositArea(Raster, Scratch, Scratch.Num(), UE_DOUBLE_PI * CrownRadiusCm * CrownRadiusCm * StemCount,
			(MinLocal + MaxLocal) * 0.5);
	}

	/** ArcForestGrowthEngine.cpp:812 CalculateCrownRadius, against the ratio FreezeBootstrapInput copies
	 *  straight off the species model (ArcEcotopeVolume.cpp:994). Never the serialized
	 *  FArcForestIndividualRecord::CrownRadiusCm, which is an independent value. */
	double CalculateCrownRadiusCm(double HeightCm, const FSpeciesCrown& Species)
	{
		return FMath::Max(0.0, HeightCm) * Species.CrownRadiusHeightRatio;
	}
}

namespace UE::ArcEcotope
{
	TMap<FGuid, FSpeciesCrown> BuildSpeciesCrowns(const UArcEcotopeModel& Ecotope)
	{
		TMap<FGuid, FSpeciesCrown> SpeciesById;
		SpeciesById.Reserve(Ecotope.SpeciesSetups.Num());
		for (const FArcEcotopeSpeciesSetup& Setup : Ecotope.SpeciesSetups)
		{
			if (Setup.Species == nullptr)
			{
				continue;
			}

			ArcForestSpeciesModel::FArcForestDerivedEcology Derived;
			FText DerivationError;
			if (!ArcForestSpeciesModel::DeriveEcology(*Setup.Species, Derived, DerivationError))
			{
				continue;
			}

			FSpeciesCrown& Crown = SpeciesById.FindOrAdd(Setup.Species->SpeciesId);
			Crown.CrownRadiusHeightRatio = static_cast<double>(Derived.CrownRadiusHeightRatio);
			Crown.CrownOccupancyWeight = static_cast<double>(Derived.CrownOccupancyWeight);
		}

		return SpeciesById;
	}

	void RasterizeRecordOccupancy(
		const FArcForestWorldSnapshot& Snapshot,
		const FArcEcotopeSiteMetadata& Site,
		const TMap<FGuid, FSpeciesCrown>& SpeciesById,
		TOptional<FGuid> SpeciesFilter,
		FFieldRaster& Raster)
	{
		using namespace ArcEcotopeRecordRasterPrivate;

		TArray<int32> Scratch;

		for (const FArcForestIndividualRecord& Record : Snapshot.Individuals)
		{
			if (Record.LifeStatus != EArcForestLifeStatus::Alive)
			{
				continue;
			}

			if (SpeciesFilter.IsSet() && Record.SpeciesId != SpeciesFilter.GetValue())
			{
				continue;
			}

			const FSpeciesCrown* Species = SpeciesById.Find(Record.SpeciesId);
			if (Species == nullptr)
			{
				continue;
			}

			DepositDisc(Raster, Scratch, FVector2D(Record.LocalPosition.X, Record.LocalPosition.Y),
				CalculateCrownRadiusCm(Record.HeightCm, *Species), 1.0);
		}

		for (const FArcForestCohortRecord& Record : Snapshot.Cohorts)
		{
			if (SpeciesFilter.IsSet() && Record.SpeciesId != SpeciesFilter.GetValue())
			{
				continue;
			}

			const FSpeciesCrown* Species = SpeciesById.Find(Record.SpeciesId);
			FVector2D CellMinLocal = FVector2D::ZeroVector;
			FVector2D CellMaxLocal = FVector2D::ZeroVector;
			if (Species == nullptr
				|| !Site.TryGetLevelCellRect(Record.CellIndex, Site.RecordGridLevel, CellMinLocal, CellMaxLocal))
			{
				continue;
			}

			DepositCellRect(Raster, Scratch, CellMinLocal, CellMaxLocal,
				CalculateCrownRadiusCm(Record.MeanHeightCm, *Species), static_cast<double>(Record.StemCount));
		}

		for (const FArcForestPatchRecord& Record : Snapshot.Patches)
		{
			if (SpeciesFilter.IsSet() && Record.SpeciesId != SpeciesFilter.GetValue())
			{
				continue;
			}

			const FSpeciesCrown* Species = SpeciesById.Find(Record.SpeciesId);
			FVector2D CellMinLocal = FVector2D::ZeroVector;
			FVector2D CellMaxLocal = FVector2D::ZeroVector;
			if (Species == nullptr
				|| !Site.TryGetLevelCellRect(Record.CellIndex, Site.RecordGridLevel, CellMinLocal, CellMaxLocal))
			{
				continue;
			}

			// AccumulateCanopyOccupancy weights a patch's cover by CrownOccupancyWeight; a cohort's stem count is unweighted.
			DepositCellRect(Raster, Scratch, CellMinLocal, CellMaxLocal,
				CalculateCrownRadiusCm(Record.MeanHeightCm, *Species),
				static_cast<double>(Record.StemOrCoverEquivalent) * Species->CrownOccupancyWeight);
		}
	}
}

#endif // WITH_EDITOR
