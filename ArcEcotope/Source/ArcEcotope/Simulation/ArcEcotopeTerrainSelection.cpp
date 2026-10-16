// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcEcotopeTerrainSelection.h"

#include "ArcTerrainActor.h"
#include "ArcTerrainTileAccess.h"
#include "ArcTerrainTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace ArcEcotopeTerrainSelectionPrivate
{
	/** Per-axis cell span above which the covered cell count is treated as unbounded. */
	constexpr int64 MaxTerrainCellAxisSpan = 1000000000;

	/** Registry lookups a built-cell scan may spend per axis; a wider rectangle is probed on a stride. */
	constexpr int64 MaxTerrainScanSamplesPerAxis = 256;

	/** Cosine tolerance for "the volume's local +Z is the terrain's local +Z". */
	constexpr double VerticalAlignmentTolerance = 1.0e-4;

	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}

	/** Product of two non-negative counts, clamped instead of overflowing. */
	int64 SaturatingProduct(const int64 A, const int64 B)
	{
		if (A == 0 || B == 0)
		{
			return 0;
		}
		return A > TNumericLimits<int64>::Max() / B ? TNumericLimits<int64>::Max() : A * B;
	}
}

namespace UE::ArcEcotope
{
	bool IsVerticalAxisAligned(const FTransform& ActorTransform, const FTransform& TerrainTransform)
	{
		using namespace ArcEcotopeTerrainSelectionPrivate;

		const FVector VolumeUpWorld = ActorTransform.TransformVectorNoScale(FVector::UpVector);
		const FVector VolumeUpLocal = TerrainTransform.InverseTransformVectorNoScale(VolumeUpWorld).GetSafeNormal();
		return VolumeUpLocal.Z >= 1.0 - VerticalAlignmentTolerance;
	}

	bool TryComputeTerrainCellRect(
		const FBox& WorldBounds,
		const FTransform& TerrainTransform,
		const double CellSize,
		FIntPoint& OutMinCell,
		FIntPoint& OutMaxCell,
		int64& OutCellCount,
		FBox* OutLocalBounds)
	{
		using namespace ArcEcotopeTerrainSelectionPrivate;

		const FBox LocalBounds = WorldBounds.InverseTransformBy(TerrainTransform);
		if (!LocalBounds.IsValid || !IsFiniteVector(LocalBounds.Min) || !IsFiniteVector(LocalBounds.Max))
		{
			return false;
		}
		if (OutLocalBounds != nullptr)
		{
			*OutLocalBounds = LocalBounds;
		}

		const double MinRatioX = FMath::Floor(LocalBounds.Min.X / CellSize);
		const double MaxRatioX = FMath::Floor(LocalBounds.Max.X / CellSize);
		const double MinRatioY = FMath::Floor(LocalBounds.Min.Y / CellSize);
		const double MaxRatioY = FMath::Floor(LocalBounds.Max.Y / CellSize);
		if (!FMath::IsFinite(MinRatioX) || !FMath::IsFinite(MaxRatioX)
			|| !FMath::IsFinite(MinRatioY) || !FMath::IsFinite(MaxRatioY)
			|| FMath::Abs(MinRatioX) > static_cast<double>(MAX_int32) || FMath::Abs(MaxRatioX) > static_cast<double>(MAX_int32)
			|| FMath::Abs(MinRatioY) > static_cast<double>(MAX_int32) || FMath::Abs(MaxRatioY) > static_cast<double>(MAX_int32))
		{
			return false;
		}

		OutMinCell = FIntPoint(static_cast<int32>(MinRatioX), static_cast<int32>(MinRatioY));
		OutMaxCell = FIntPoint(static_cast<int32>(MaxRatioX), static_cast<int32>(MaxRatioY));

		const int64 SpanX = static_cast<int64>(OutMaxCell.X) - static_cast<int64>(OutMinCell.X) + 1;
		const int64 SpanY = static_cast<int64>(OutMaxCell.Y) - static_cast<int64>(OutMinCell.Y) + 1;
		if (SpanX <= 0 || SpanY <= 0)
		{
			return false;
		}

		OutCellCount = (SpanX > MaxTerrainCellAxisSpan || SpanY > MaxTerrainCellAxisSpan)
			? TNumericLimits<int64>::Max()
			: SpanX * SpanY;
		return true;
	}

	int64 CountBuiltTerrainCells(
		const AArcTerrain& Terrain,
		const FIntPoint& MinCell,
		const FIntPoint& MaxCell,
		bool& bOutSampled)
	{
		using namespace ArcEcotopeTerrainSelectionPrivate;

		bOutSampled = false;

		const int64 SpanX = static_cast<int64>(MaxCell.X) - static_cast<int64>(MinCell.X) + 1;
		const int64 SpanY = static_cast<int64>(MaxCell.Y) - static_cast<int64>(MinCell.Y) + 1;
		if (SpanX <= 0 || SpanY <= 0)
		{
			return 0;
		}

		const int64 StrideX = FMath::DivideAndRoundUp(SpanX, MaxTerrainScanSamplesPerAxis);
		const int64 StrideY = FMath::DivideAndRoundUp(SpanY, MaxTerrainScanSamplesPerAxis);
		bOutSampled = StrideX > 1 || StrideY > 1;

		int64 HitCount = 0;
		for (int64 CellY = MinCell.Y; CellY <= static_cast<int64>(MaxCell.Y); CellY += StrideY)
		{
			for (int64 CellX = MinCell.X; CellX <= static_cast<int64>(MaxCell.X); CellX += StrideX)
			{
				const FArcTerrainCellKey Key(static_cast<int32>(CellX), static_cast<int32>(CellY));
				FIntPoint TileKey = FIntPoint::ZeroValue;
				if (ArcTerrainTileAccess::TileKeyFromCell(Terrain, EArcTerrainRail::Height, 0, Key, TileKey)
					&& ArcTerrainTileAccess::HasTile(Terrain, EArcTerrainRail::Height, 0, TileKey))
				{
					++HitCount;
				}
			}
		}

		return SaturatingProduct(HitCount, SaturatingProduct(StrideX, StrideY));
	}

	FArcEcotopeTerrainChoice SelectTerrainForVolume(
		UWorld& World,
		const FBox& WorldVolumeBounds,
		const FBox& WorldSampleBounds,
		const FTransform& ActorTransform)
	{
		FArcEcotopeTerrainChoice Choice;
		FString SelectedPathName;

		for (TActorIterator<AArcTerrain> TerrainIt(&World); TerrainIt; ++TerrainIt)
		{
			AArcTerrain* Terrain = *TerrainIt;
			if (!IsValid(Terrain))
			{
				continue;
			}

			const FArcTerrainConfig& Config = Terrain->Config;
			if (!FMath::IsFinite(Config.CellSize()) || Config.CellSize() <= 0.0)
			{
				continue;
			}

			const FTransform TerrainTransform = Terrain->GetTerrainTransform();
			if (!IsVerticalAxisAligned(ActorTransform, TerrainTransform))
			{
				continue;
			}

			FIntPoint MarginedMinCell = FIntPoint::ZeroValue;
			FIntPoint MarginedMaxCell = FIntPoint::ZeroValue;
			int64 MarginedCellCount = 0;
			if (!TryComputeTerrainCellRect(WorldSampleBounds, TerrainTransform, Config.CellSize(),
				MarginedMinCell, MarginedMaxCell, MarginedCellCount))
			{
				continue;
			}

			FIntPoint VolumeMinCell = FIntPoint::ZeroValue;
			FIntPoint VolumeMaxCell = FIntPoint::ZeroValue;
			int64 VolumeCellCount = 0;
			FBox LocalVolumeBounds(ForceInit);
			if (!TryComputeTerrainCellRect(WorldVolumeBounds, TerrainTransform, Config.CellSize(),
				VolumeMinCell, VolumeMaxCell, VolumeCellCount, &LocalVolumeBounds))
			{
				continue;
			}

			const double VolumeCenterHeightCm = FMath::Abs(LocalVolumeBounds.GetCenter().Z);

			// Coverage under the volume is the only content-aware term, so it breaks the tie: a terrain
			// built only in the margin must not outrank the one the site cells actually sample.
			bool bVolumeCountSampled = false;
			const int64 VolumeBuiltCellCount =
				CountBuiltTerrainCells(*Terrain, VolumeMinCell, VolumeMaxCell, bVolumeCountSampled);

			const FString PathName = Terrain->GetPathName();
			bool bBetter = Choice.Terrain == nullptr;
			if (!bBetter && VolumeCenterHeightCm <= Choice.VolumeCenterHeightCm)
			{
				bBetter = VolumeCenterHeightCm < Choice.VolumeCenterHeightCm
					|| VolumeBuiltCellCount > Choice.VolumeBuiltCellCount
					|| (VolumeBuiltCellCount == Choice.VolumeBuiltCellCount
						&& PathName.Compare(SelectedPathName, ESearchCase::CaseSensitive) < 0);
			}
			if (bBetter)
			{
				Choice.Terrain = Terrain;
				Choice.VolumeCenterHeightCm = VolumeCenterHeightCm;
				Choice.VolumeBuiltCellCount = VolumeBuiltCellCount;
				Choice.bVolumeCountSampled = bVolumeCountSampled;
				SelectedPathName = PathName;
			}
		}

		return Choice;
	}
}
