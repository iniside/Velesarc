// Copyright Lukasz Baran. All Rights Reserved.

#include "Climate/ArcClimateBaker.h"

#include "ArcEcotopeModule.h"
#include "ArcTerrainActor.h"
#include "ArcTerrainSubsystem.h"
#include "ArcTerrainTileAccess.h"
#include "Climate/ArcClimateField.h"
#include "Climate/ArcClimateProduct.h"
#include "Climate/ArcClimateVolume.h"
#include "Components/BoxComponent.h"
#include "DataMaps/ArcTerrainDataMapTypes.h"
#include "Engine/World.h"
#include "Fields/ArcTerrainMaskDistance.h"
#include "HAL/IConsoleManager.h"
#include "Simulation/ArcEcotopeSiteBaker.h"
#include "Simulation/ArcEcotopeSiteFields.h"
#include "Simulation/ArcEcotopeSiteMetadata.h"
#include "Simulation/ArcEcotopeTerrainSelection.h"
#include "Surface/ArcTerrainSurfaceSample.h"

namespace ArcClimateBakerPrivate
{
	constexpr int32 MarginCells = 3;

	TAutoConsoleVariable<int32> CVarMaxProjectedClimateBakePeakMiB(
		TEXT("arcecotope.ClimateBake.MaxProjectedPeakMiB"),
		4096,
		TEXT("Reject a climate bake before large allocations when its conservative projected peak exceeds this MiB budget."),
		ECVF_Default);

	bool CheckedMultiply(const uint64 Left, const uint64 Right, uint64& OutValue)
	{
		if (Left != 0 && Right > MAX_uint64 / Left)
		{
			OutValue = 0;
			return false;
		}
		OutValue = Left * Right;
		return true;
	}

	bool CheckedAdd(const uint64 Left, const uint64 Right, uint64& OutValue)
	{
		if (Right > MAX_uint64 - Left)
		{
			OutValue = 0;
			return false;
		}
		OutValue = Left + Right;
		return true;
	}

	struct FClimateBakeMemoryProjection
	{
		uint64 SurfacePhaseBytes = 0;
		uint64 GlobalPhaseBytes = 0;
		uint64 WriterPhaseBytes = 0;
		uint64 StripePhaseBytes = 0;
		uint64 PublicationPhaseBytes = 0;

		bool GetPeakBytes(uint64& OutPeakBytes) const
		{
			OutPeakBytes = FMath::Max(
				FMath::Max(SurfacePhaseBytes, GlobalPhaseBytes),
				FMath::Max(FMath::Max(WriterPhaseBytes, StripePhaseBytes), PublicationPhaseBytes));
			return true;
		}
	};

	bool CheckedArrayBytes(const uint64 ElementCount, const uint64 ElementSize, uint64& OutBytes)
	{
		return CheckedMultiply(ElementCount, ElementSize, OutBytes);
	}

	bool GetLevelCellCount(
		const FIntPoint& GridDimensions,
		const uint8 Level,
		uint64& OutCellCount)
	{
		if (GridDimensions.X <= 0 || GridDimensions.Y <= 0
			|| Level > FArcClimateProductMetadata::MaxGridLevel)
		{
			return false;
		}
		const uint64 Divisor = static_cast<uint64>(1) << Level;
		uint64 WidthNumerator = 0;
		uint64 HeightNumerator = 0;
		if (!CheckedAdd(static_cast<uint64>(GridDimensions.X), Divisor - 1, WidthNumerator)
			|| !CheckedAdd(static_cast<uint64>(GridDimensions.Y), Divisor - 1, HeightNumerator))
		{
			return false;
		}
		const uint64 Width = WidthNumerator / Divisor;
		const uint64 Height = HeightNumerator / Divisor;
		return CheckedMultiply(Width, Height, OutCellCount);
	}

	bool GetLargestNativeFieldBlockBytes(
		const FIntPoint& GridDimensions,
		const int32 ChunkBaseCellSpan,
		const uint8 Level,
		uint64& OutBytes)
	{
		OutBytes = 0;
		if (GridDimensions.X <= 0 || GridDimensions.Y <= 0 || ChunkBaseCellSpan <= 0
			|| Level > FArcClimateProductMetadata::MaxGridLevel)
		{
			return false;
		}
		const uint64 Divisor = static_cast<uint64>(1) << Level;
		uint64 WidthNumerator = 0;
		uint64 HeightNumerator = 0;
		if (!CheckedAdd(static_cast<uint64>(GridDimensions.X), Divisor - 1, WidthNumerator)
			|| !CheckedAdd(static_cast<uint64>(GridDimensions.Y), Divisor - 1, HeightNumerator))
		{
			return false;
		}
		const uint64 LevelWidth = WidthNumerator / Divisor;
		const uint64 LevelHeight = HeightNumerator / Divisor;
		if (static_cast<uint64>(ChunkBaseCellSpan) % Divisor != 0)
		{
			return false;
		}
		const uint64 ChunkLevelSpan = static_cast<uint64>(ChunkBaseCellSpan) / Divisor;
		const uint64 BlockWidth = FMath::Min(LevelWidth, ChunkLevelSpan);
		const uint64 BlockHeight = FMath::Min(LevelHeight, ChunkLevelSpan);
		uint64 BlockCellCount = 0;
		return CheckedMultiply(BlockWidth, BlockHeight, BlockCellCount)
			&& CheckedArrayBytes(BlockCellCount, sizeof(float), OutBytes);
	}

	bool ProjectClimateBakePeak(
		const FIntPoint& GridDimensions,
		const uint64 MarginedCellCount,
		const int64 TerrainSpannedCellCount,
		const int32 TerrainQuadsPerCell,
		const int32 ChunkBaseCellSpan,
		const uint8 TemperatureLevel,
		const uint8 HumidityLevel,
		const uint8 SunLoadLevel,
		const uint8 HorizonLevel,
		const uint8 MoistureLevel,
		FClimateBakeMemoryProjection& OutProjection)
	{
		OutProjection = FClimateBakeMemoryProjection();
		if (GridDimensions.X <= 0 || GridDimensions.Y <= 0 || MarginedCellCount == 0
			|| TerrainSpannedCellCount < 0 || TerrainQuadsPerCell <= 0 || ChunkBaseCellSpan <= 0)
		{
			return false;
		}
		uint64 BaseCellCount = 0;
		if (!CheckedMultiply(
			static_cast<uint64>(GridDimensions.X), static_cast<uint64>(GridDimensions.Y), BaseCellCount))
		{
			return false;
		}
		if (BaseCellCount == 0 || BaseCellCount > static_cast<uint64>(MAX_int32)
			|| MarginedCellCount > static_cast<uint64>(MAX_int32))
		{
			return false;
		}
		uint64 TileEdge = 0;
		uint64 TileTexels = 0;
		uint64 TexelBytes = 0;
		uint64 PerTerrainCellBytes = 0;
		uint64 TerrainCacheBytes = 0;
		if (!CheckedAdd(static_cast<uint64>(TerrainQuadsPerCell), 3ull, TileEdge)
			|| !CheckedMultiply(TileEdge, TileEdge, TileTexels)
			|| !CheckedMultiply(TileTexels, 24ull, TexelBytes)
			|| !CheckedAdd(TexelBytes, 512ull, PerTerrainCellBytes)
			|| !CheckedMultiply(
				static_cast<uint64>(TerrainSpannedCellCount),
				PerTerrainCellBytes,
				TerrainCacheBytes))
		{
			return false;
		}
		const uint64 FixedAllowanceBytes = 256ull * 1024ull * 1024ull;
		uint64 EightMarginedBytes = 0;
		uint64 ThirteenBaseBytes = 0;
		uint64 SeventeenBaseBytes = 0;
		uint64 GlobalMarginedBytes = 0;
		if (!CheckedArrayBytes(MarginedCellCount, 8ull, EightMarginedBytes)
			|| !CheckedArrayBytes(BaseCellCount, 13ull, ThirteenBaseBytes)
			|| !CheckedArrayBytes(BaseCellCount, 17ull, SeventeenBaseBytes)
			|| !CheckedAdd(TerrainCacheBytes, EightMarginedBytes, OutProjection.SurfacePhaseBytes)
			|| !CheckedAdd(OutProjection.SurfacePhaseBytes, ThirteenBaseBytes, OutProjection.SurfacePhaseBytes)
			|| !CheckedAdd(OutProjection.SurfacePhaseBytes, FixedAllowanceBytes, OutProjection.SurfacePhaseBytes)
			|| !CheckedArrayBytes(MarginedCellCount, 160ull, GlobalMarginedBytes)
			|| !CheckedAdd(GlobalMarginedBytes, SeventeenBaseBytes, OutProjection.GlobalPhaseBytes))
		{
			return false;
		}
		uint64 SunLevelCellCount = 0;
		uint64 SunLevelBytes = 0;
		if (!GetLevelCellCount(GridDimensions, SunLoadLevel, SunLevelCellCount)
			|| !CheckedArrayBytes(SunLevelCellCount, sizeof(float), SunLevelBytes)
			|| !CheckedAdd(OutProjection.GlobalPhaseBytes, SunLevelBytes, OutProjection.GlobalPhaseBytes)
			|| !CheckedAdd(OutProjection.GlobalPhaseBytes, FixedAllowanceBytes, OutProjection.GlobalPhaseBytes))
		{
			return false;
		}

		uint64 TemperatureCells = 0;
		uint64 HumidityCells = 0;
		uint64 HorizonCells = 0;
		uint64 MoistureCells = 0;
		if (!GetLevelCellCount(GridDimensions, TemperatureLevel, TemperatureCells)
			|| !GetLevelCellCount(GridDimensions, HumidityLevel, HumidityCells)
			|| !GetLevelCellCount(GridDimensions, HorizonLevel, HorizonCells)
			|| !GetLevelCellCount(GridDimensions, MoistureLevel, MoistureCells))
		{
			return false;
		}
		uint64 PayloadCells = 0;
		if (!CheckedAdd(TemperatureCells, HumidityCells, PayloadCells)
			|| !CheckedAdd(PayloadCells, SunLevelCellCount, PayloadCells)
			|| !CheckedAdd(PayloadCells, HorizonCells, PayloadCells)
			|| !CheckedAdd(PayloadCells, HorizonCells, PayloadCells)
			|| !CheckedAdd(PayloadCells, MoistureCells, PayloadCells))
		{
			return false;
		}
		uint64 PayloadBytes = 0;
		if (!CheckedArrayBytes(PayloadCells, 4ull, PayloadBytes))
		{
			return false;
		}
		uint64 HorizonBytes = 0;
		uint64 RetainedBaseBytes = 0;
		uint64 EarlierRetainedMarginedBytes = 0;
		uint64 EarlierRetainedBaseBytes = 0;
		uint64 ChannelBytes = 0;
		uint64 StripeBytes = 0;
		uint64 SunLoadBlockBytes = 0;
		uint64 WindExposureBlockBytes = 0;
		uint64 SkyViewFactorBlockBytes = 0;
		uint64 TemperatureBlockBytes = 0;
		uint64 HumidityBlockBytes = 0;
		uint64 MoistureBlockBytes = 0;
		uint64 ReducedBlockBytes = 0;
		const uint64 StripeRows = FMath::Min(
			static_cast<uint64>(ChunkBaseCellSpan), static_cast<uint64>(GridDimensions.Y));
		if (!GetLargestNativeFieldBlockBytes(
				GridDimensions, ChunkBaseCellSpan, SunLoadLevel, SunLoadBlockBytes)
			|| !GetLargestNativeFieldBlockBytes(
				GridDimensions, ChunkBaseCellSpan, HorizonLevel, WindExposureBlockBytes)
			|| !GetLargestNativeFieldBlockBytes(
				GridDimensions, ChunkBaseCellSpan, HorizonLevel, SkyViewFactorBlockBytes)
			|| !GetLargestNativeFieldBlockBytes(
				GridDimensions, ChunkBaseCellSpan, TemperatureLevel, TemperatureBlockBytes)
			|| !GetLargestNativeFieldBlockBytes(
				GridDimensions, ChunkBaseCellSpan, HumidityLevel, HumidityBlockBytes)
			|| !GetLargestNativeFieldBlockBytes(
				GridDimensions, ChunkBaseCellSpan, MoistureLevel, MoistureBlockBytes)
			|| !CheckedAdd(TemperatureBlockBytes, HumidityBlockBytes, ReducedBlockBytes)
			|| !CheckedArrayBytes(HorizonCells, sizeof(float) * 2ull, HorizonBytes)
			|| !CheckedArrayBytes(BaseCellCount, 12ull, RetainedBaseBytes)
			|| !CheckedArrayBytes(MarginedCellCount, 17ull, EarlierRetainedMarginedBytes)
			|| !CheckedArrayBytes(BaseCellCount, 17ull, EarlierRetainedBaseBytes)
			|| !CheckedArrayBytes(BaseCellCount, sizeof(double), ChannelBytes)
			|| !CheckedArrayBytes(
				static_cast<uint64>(GridDimensions.X), StripeRows, StripeBytes)
			|| !CheckedArrayBytes(StripeBytes, sizeof(float) * 2ull, StripeBytes)
			|| !CheckedAdd(PayloadBytes, SunLevelBytes, OutProjection.WriterPhaseBytes)
			|| !CheckedAdd(OutProjection.WriterPhaseBytes, HorizonBytes, OutProjection.WriterPhaseBytes)
			|| !CheckedAdd(OutProjection.WriterPhaseBytes, RetainedBaseBytes, OutProjection.WriterPhaseBytes)
			|| !CheckedAdd(OutProjection.WriterPhaseBytes, ChannelBytes, OutProjection.WriterPhaseBytes)
			|| !CheckedAdd(OutProjection.WriterPhaseBytes, FMath::Max(
				FMath::Max(SunLoadBlockBytes, WindExposureBlockBytes),
				FMath::Max(SkyViewFactorBlockBytes, MoistureBlockBytes)),
				OutProjection.WriterPhaseBytes)
			|| !CheckedAdd(OutProjection.WriterPhaseBytes, EarlierRetainedMarginedBytes,
				OutProjection.WriterPhaseBytes)
			|| !CheckedAdd(OutProjection.WriterPhaseBytes, EarlierRetainedBaseBytes,
				OutProjection.WriterPhaseBytes)
			|| !CheckedAdd(OutProjection.WriterPhaseBytes, FixedAllowanceBytes, OutProjection.WriterPhaseBytes)
			|| !CheckedAdd(PayloadBytes, RetainedBaseBytes, OutProjection.StripePhaseBytes)
			|| !CheckedAdd(OutProjection.StripePhaseBytes, SunLevelBytes, OutProjection.StripePhaseBytes)
			|| !CheckedAdd(OutProjection.StripePhaseBytes, HorizonBytes, OutProjection.StripePhaseBytes)
			|| !CheckedAdd(OutProjection.StripePhaseBytes, ChannelBytes, OutProjection.StripePhaseBytes)
			|| !CheckedAdd(OutProjection.StripePhaseBytes, StripeBytes, OutProjection.StripePhaseBytes)
			|| !CheckedAdd(OutProjection.StripePhaseBytes, ReducedBlockBytes, OutProjection.StripePhaseBytes)
			|| !CheckedAdd(OutProjection.StripePhaseBytes, EarlierRetainedMarginedBytes,
				OutProjection.StripePhaseBytes)
			|| !CheckedAdd(OutProjection.StripePhaseBytes, EarlierRetainedBaseBytes,
				OutProjection.StripePhaseBytes)
			|| !CheckedAdd(OutProjection.StripePhaseBytes, FixedAllowanceBytes, OutProjection.StripePhaseBytes)
			|| !CheckedMultiply(PayloadBytes, 2ull, OutProjection.PublicationPhaseBytes)
			|| !CheckedAdd(OutProjection.PublicationPhaseBytes, SunLevelBytes,
				OutProjection.PublicationPhaseBytes)
			|| !CheckedAdd(OutProjection.PublicationPhaseBytes, HorizonBytes,
				OutProjection.PublicationPhaseBytes)
			|| !CheckedAdd(OutProjection.PublicationPhaseBytes, RetainedBaseBytes,
				OutProjection.PublicationPhaseBytes)
			|| !CheckedAdd(OutProjection.PublicationPhaseBytes, ChannelBytes,
				OutProjection.PublicationPhaseBytes)
			|| !CheckedAdd(OutProjection.PublicationPhaseBytes, EarlierRetainedMarginedBytes,
				OutProjection.PublicationPhaseBytes)
			|| !CheckedAdd(OutProjection.PublicationPhaseBytes, EarlierRetainedBaseBytes,
				OutProjection.PublicationPhaseBytes)
			|| !CheckedAdd(OutProjection.PublicationPhaseBytes, FixedAllowanceBytes, OutProjection.PublicationPhaseBytes))
		{
			return false;
		}
		return true;
	}

	bool SetError(FText& OutError, const TCHAR* Message)
	{
		OutError = FText::FromString(Message);
		return false;
	}

	bool GetClimateOwnedRect(
		const FArcClimateProductMetadata& Metadata,
		const int32 ChunkBaseCellSpan,
		const FIntPoint ChunkId,
		const EArcClimateField Field,
		FIntRect& OutOwnedRect,
		FString& OutError)
	{
		OutError.Reset();
		if (ChunkBaseCellSpan <= 0 || ChunkId.X < 0 || ChunkId.Y < 0
			|| Metadata.GridDimensions.X <= 0 || Metadata.GridDimensions.Y <= 0)
		{
			OutError = TEXT("Climate producer could not map a field ownership rectangle.");
			return false;
		}
		const uint8 Level = Metadata.GetFieldLevel(Field);
		if (Level > FArcClimateProductMetadata::MaxGridLevel)
		{
			OutError = TEXT("Climate producer could not map a field ownership rectangle.");
			return false;
		}
		const uint64 Divisor = static_cast<uint64>(1) << Level;
		uint64 BaseMinX = 0;
		uint64 BaseMinY = 0;
		uint64 BaseMaxXUnclamped = 0;
		uint64 BaseMaxYUnclamped = 0;
		if (!CheckedMultiply(
				static_cast<uint64>(ChunkId.X), static_cast<uint64>(ChunkBaseCellSpan), BaseMinX)
			|| !CheckedMultiply(
				static_cast<uint64>(ChunkId.Y), static_cast<uint64>(ChunkBaseCellSpan), BaseMinY)
			|| !CheckedAdd(BaseMinX, static_cast<uint64>(ChunkBaseCellSpan), BaseMaxXUnclamped)
			|| !CheckedAdd(BaseMinY, static_cast<uint64>(ChunkBaseCellSpan), BaseMaxYUnclamped)
			|| BaseMinX >= static_cast<uint64>(Metadata.GridDimensions.X)
			|| BaseMinY >= static_cast<uint64>(Metadata.GridDimensions.Y))
		{
			OutError = TEXT("Climate producer could not map a field ownership rectangle.");
			return false;
		}
		const uint64 BaseMaxX = FMath::Min(
			BaseMaxXUnclamped, static_cast<uint64>(Metadata.GridDimensions.X));
		const uint64 BaseMaxY = FMath::Min(
			BaseMaxYUnclamped, static_cast<uint64>(Metadata.GridDimensions.Y));
		const FIntPoint LevelDimensions = Metadata.GetLevelDimensions(Level);
		if (BaseMaxX <= BaseMinX || BaseMaxY <= BaseMinY
			|| LevelDimensions.X <= 0 || LevelDimensions.Y <= 0)
		{
			OutError = TEXT("Climate producer could not map a field ownership rectangle.");
			return false;
		}
		uint64 MaxNumeratorX = 0;
		uint64 MaxNumeratorY = 0;
		const uint64 LevelMinX = BaseMinX / Divisor;
		const uint64 LevelMinY = BaseMinY / Divisor;
		if (!CheckedAdd(BaseMaxX, Divisor - 1, MaxNumeratorX)
			|| !CheckedAdd(BaseMaxY, Divisor - 1, MaxNumeratorY))
		{
			OutError = TEXT("Climate producer could not map a field ownership rectangle.");
			return false;
		}
		const uint64 LevelMaxX = FMath::Min(
			MaxNumeratorX / Divisor, static_cast<uint64>(LevelDimensions.X));
		const uint64 LevelMaxY = FMath::Min(
			MaxNumeratorY / Divisor, static_cast<uint64>(LevelDimensions.Y));
		if (LevelMinX > static_cast<uint64>(MAX_int32) || LevelMinY > static_cast<uint64>(MAX_int32)
			|| LevelMaxX > static_cast<uint64>(MAX_int32) || LevelMaxY > static_cast<uint64>(MAX_int32)
			|| LevelMaxX <= LevelMinX || LevelMaxY <= LevelMinY)
		{
			OutError = TEXT("Climate producer could not map a field ownership rectangle.");
			return false;
		}
		OutOwnedRect = FIntRect(
			FIntPoint(
				static_cast<int32>(LevelMinX),
				static_cast<int32>(LevelMinY)),
			FIntPoint(
				static_cast<int32>(LevelMaxX),
				static_cast<int32>(LevelMaxY)));
		OutError.Reset();
		return true;
	}

	bool SliceAndWriteClimateField(
		FArcClimateProductBuildWriter& Writer,
		const FArcClimateProductMetadata& Metadata,
		const int32 ChunkBaseCellSpan,
		const EArcClimateField Field,
		const TConstArrayView<float> FieldValues,
		FString& OutError)
	{
		const uint8 Level = Metadata.GetFieldLevel(Field);
		const FIntPoint Dimensions = Metadata.GetLevelDimensions(Level);
		uint64 ExpectedValueCount = 0;
		if (ChunkBaseCellSpan <= 0 || Dimensions.X <= 0 || Dimensions.Y <= 0
			|| !CheckedMultiply(
				static_cast<uint64>(Dimensions.X), static_cast<uint64>(Dimensions.Y), ExpectedValueCount)
			|| ExpectedValueCount > static_cast<uint64>(MAX_int32)
			|| FieldValues.Num() != static_cast<int32>(ExpectedValueCount))
		{
			OutError = TEXT("Climate producer received a malformed global field.");
			return false;
		}
		uint64 ChunkGridWidthNumerator = 0;
		uint64 ChunkGridHeightNumerator = 0;
		if (!CheckedAdd(
				static_cast<uint64>(Metadata.GridDimensions.X),
				static_cast<uint64>(ChunkBaseCellSpan - 1), ChunkGridWidthNumerator)
			|| !CheckedAdd(
				static_cast<uint64>(Metadata.GridDimensions.Y),
				static_cast<uint64>(ChunkBaseCellSpan - 1), ChunkGridHeightNumerator))
		{
			OutError = TEXT("Climate producer could not size the climate chunk grid.");
			return false;
		}
		const uint64 ChunkGridWidthUnsigned = ChunkGridWidthNumerator
			/ static_cast<uint64>(ChunkBaseCellSpan);
		const uint64 ChunkGridHeightUnsigned = ChunkGridHeightNumerator
			/ static_cast<uint64>(ChunkBaseCellSpan);
		if (ChunkGridWidthUnsigned == 0 || ChunkGridHeightUnsigned == 0
			|| ChunkGridWidthUnsigned > static_cast<uint64>(MAX_int32)
			|| ChunkGridHeightUnsigned > static_cast<uint64>(MAX_int32))
		{
			OutError = TEXT("Climate producer could not size the climate chunk grid.");
			return false;
		}
		const int32 ChunkGridWidth = static_cast<int32>(ChunkGridWidthUnsigned);
		const int32 ChunkGridHeight = static_cast<int32>(ChunkGridHeightUnsigned);
		for (int32 ChunkY = 0; ChunkY < ChunkGridHeight; ++ChunkY)
		{
			for (int32 ChunkX = 0; ChunkX < ChunkGridWidth; ++ChunkX)
			{
				FIntRect OwnedRect;
				if (!GetClimateOwnedRect(
						Metadata, ChunkBaseCellSpan, FIntPoint(ChunkX, ChunkY), Field, OwnedRect, OutError))
				{
					return false;
				}
				const uint64 BlockWidth = static_cast<uint64>(OwnedRect.Max.X - OwnedRect.Min.X);
				const uint64 BlockHeight = static_cast<uint64>(OwnedRect.Max.Y - OwnedRect.Min.Y);
				uint64 BlockValueCount = 0;
				if (!CheckedMultiply(BlockWidth, BlockHeight, BlockValueCount)
					|| BlockValueCount == 0 || BlockValueCount > static_cast<uint64>(MAX_int32))
				{
					OutError = TEXT("Climate producer could not size a field ownership block.");
					return false;
				}
				TArray<float> BlockValues;
				BlockValues.Reserve(static_cast<int32>(BlockValueCount));
				for (int32 Y = OwnedRect.Min.Y; Y < OwnedRect.Max.Y; ++Y)
				{
					uint64 RowStart = 0;
					uint64 SourceIndex = 0;
					uint64 SourceEnd = 0;
					if (!CheckedMultiply(
							static_cast<uint64>(Y), static_cast<uint64>(Dimensions.X), RowStart)
						|| !CheckedAdd(RowStart, static_cast<uint64>(OwnedRect.Min.X), SourceIndex)
						|| !CheckedAdd(SourceIndex, BlockWidth, SourceEnd)
						|| SourceIndex > static_cast<uint64>(MAX_int32)
						|| SourceEnd > static_cast<uint64>(FieldValues.Num()))
					{
						OutError = TEXT("Climate producer could not address a field ownership block.");
						return false;
					}
					BlockValues.Append(
						FieldValues.GetData() + static_cast<int32>(SourceIndex),
						static_cast<int32>(BlockWidth));
				}
				if (!Writer.WriteFieldBlock(
						Field, FIntPoint(ChunkX, ChunkY), BlockValues, OutError))
				{
					return false;
				}
			}
		}
		return true;
	}

	bool ReduceClimateStripeToOwnedField(
		const FArcEcotopeSiteMetadata& Metadata,
		const uint8 Level,
		const int32 StripeStartY,
		const int32 StripeRowCount,
		const FIntRect& OwnedRect,
		const TConstArrayView<float> StripeValues,
		TArray<float>& OutValues,
		FString& OutError)
	{
		OutError.Reset();
		if (Level > FArcClimateProductMetadata::MaxGridLevel
			|| Metadata.GridDimensions.X <= 0 || Metadata.GridDimensions.Y <= 0
			|| StripeStartY < 0 || StripeRowCount <= 0
			|| OwnedRect.Min.X < 0 || OwnedRect.Min.Y < 0
			|| OwnedRect.Max.X <= OwnedRect.Min.X || OwnedRect.Max.Y <= OwnedRect.Min.Y)
		{
			OutError = TEXT("Climate producer could not reduce a climate stripe to a field ownership block.");
			return false;
		}
		const uint64 Divisor = static_cast<uint64>(1) << Level;
		const uint64 Width = static_cast<uint64>(Metadata.GridDimensions.X);
		const uint64 Height = static_cast<uint64>(Metadata.GridDimensions.Y);
		uint64 WidthNumerator = 0;
		uint64 HeightNumerator = 0;
		uint64 StripeEndY = 0;
		if (!CheckedAdd(Width, Divisor - 1, WidthNumerator)
			|| !CheckedAdd(Height, Divisor - 1, HeightNumerator)
			|| !CheckedAdd(static_cast<uint64>(StripeStartY), static_cast<uint64>(StripeRowCount), StripeEndY))
		{
			OutError = TEXT("Climate producer could not reduce a climate stripe to a field ownership block.");
			return false;
		}
		const uint64 LevelWidth = WidthNumerator / Divisor;
		const uint64 LevelHeight = HeightNumerator / Divisor;
		const uint64 OwnedMinX = static_cast<uint64>(OwnedRect.Min.X);
		const uint64 OwnedMinY = static_cast<uint64>(OwnedRect.Min.Y);
		const uint64 OwnedMaxX = static_cast<uint64>(OwnedRect.Max.X);
		const uint64 OwnedMaxY = static_cast<uint64>(OwnedRect.Max.Y);
		const uint64 OwnedWidth = OwnedMaxX - OwnedMinX;
		const uint64 OwnedHeight = OwnedMaxY - OwnedMinY;
		uint64 StripeValueCount = 0;
		uint64 OwnedValueCount = 0;
		if (StripeEndY > Height || StripeEndY > static_cast<uint64>(MAX_int32)
			|| OwnedMaxX > LevelWidth || OwnedMaxY > LevelHeight
			|| !CheckedMultiply(Width, static_cast<uint64>(StripeRowCount), StripeValueCount)
			|| StripeValueCount > static_cast<uint64>(MAX_int32)
			|| StripeValues.Num() != static_cast<int32>(StripeValueCount)
			|| !CheckedMultiply(OwnedWidth, OwnedHeight, OwnedValueCount)
			|| OwnedValueCount == 0 || OwnedValueCount > static_cast<uint64>(MAX_int32))
		{
			OutError = TEXT("Climate producer could not reduce a climate stripe to a field ownership block.");
			return false;
		}
		OutValues.Reset();
		OutValues.Reserve(static_cast<int32>(OwnedValueCount));
		for (int32 LevelY = OwnedRect.Min.Y; LevelY < OwnedRect.Max.Y; ++LevelY)
		{
			for (int32 LevelX = OwnedRect.Min.X; LevelX < OwnedRect.Max.X; ++LevelX)
			{
				uint64 MinBaseX = 0;
				uint64 MinBaseY = 0;
				uint64 MaxBaseXUnclamped = 0;
				uint64 MaxBaseYUnclamped = 0;
				if (!CheckedMultiply(static_cast<uint64>(LevelX), Divisor, MinBaseX)
					|| !CheckedMultiply(static_cast<uint64>(LevelY), Divisor, MinBaseY)
					|| !CheckedAdd(MinBaseX, Divisor, MaxBaseXUnclamped)
					|| !CheckedAdd(MinBaseY, Divisor, MaxBaseYUnclamped))
				{
					OutError = TEXT("Climate producer could not reduce a climate stripe to a field ownership block.");
					return false;
				}
				const uint64 MaxBaseX = FMath::Min(MaxBaseXUnclamped, Width);
				const uint64 MaxBaseY = FMath::Min(MaxBaseYUnclamped, Height);
				if (MinBaseY < static_cast<uint64>(StripeStartY) || MaxBaseY > StripeEndY
					|| MaxBaseX <= MinBaseX || MaxBaseY <= MinBaseY
					|| MinBaseX > static_cast<uint64>(MAX_int32)
					|| MinBaseY > static_cast<uint64>(MAX_int32)
					|| MaxBaseX > static_cast<uint64>(MAX_int32)
					|| MaxBaseY > static_cast<uint64>(MAX_int32))
				{
					OutError = TEXT("Climate producer could not reduce a climate stripe to a field ownership block.");
					return false;
				}
				double Sum = 0.0;
				uint64 Count = 0;
				for (uint64 BaseY = MinBaseY; BaseY < MaxBaseY; ++BaseY)
				{
					for (uint64 BaseX = MinBaseX; BaseX < MaxBaseX; ++BaseX)
					{
						uint64 StripeRowOffset = 0;
						uint64 StripeIndex = 0;
						if (!CheckedMultiply(
								BaseY - static_cast<uint64>(StripeStartY), Width, StripeRowOffset)
							|| !CheckedAdd(StripeRowOffset, BaseX, StripeIndex)
							|| StripeIndex >= static_cast<uint64>(StripeValues.Num()))
						{
							OutError = TEXT("Climate producer could not address a climate stripe value.");
							return false;
						}
						Sum += static_cast<double>(StripeValues[static_cast<int32>(StripeIndex)]);
						++Count;
					}
				}
				if (Count <= 0)
				{
					OutError = TEXT("Climate producer could not reduce a climate stripe to a field ownership block.");
					return false;
				}
				OutValues.Add(static_cast<float>(FMath::Clamp(
					Sum / static_cast<double>(Count), 0.0, 1.0)));
			}
		}
		OutError.Reset();
		return true;
	}

	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}

	struct FTerrainSurfaceSampler
	{
		const AArcTerrain* Terrain = nullptr;
		const FArcTerrainConfig* Config = nullptr;
		FTransform TerrainTransform = FTransform::Identity;
		TArray<FArcTerrainSurfaceCellCache> Cells;
		TMap<FArcTerrainCellKey, int32> KeyToIndex;
		int32 FlatCellIndex = INDEX_NONE;
	};

	int32 GetOrBuildCellCache(FTerrainSurfaceSampler& Sampler, const FArcTerrainCellKey Key)
	{
		if (const int32* ExistingIndex = Sampler.KeyToIndex.Find(Key))
		{
			return *ExistingIndex;
		}
		FIntPoint TileKey = FIntPoint::ZeroValue;
		const bool bHasTile =
			ArcTerrainTileAccess::TileKeyFromCell(*Sampler.Terrain, EArcTerrainRail::Height, 0, Key, TileKey)
			&& ArcTerrainTileAccess::HasTile(*Sampler.Terrain, EArcTerrainRail::Height, 0, TileKey);
		if (!bHasTile)
		{
			if (Sampler.FlatCellIndex == INDEX_NONE)
			{
				Sampler.FlatCellIndex = Sampler.Cells.Num();
				Sampler.Cells.Add(ArcTerrainSurface::BuildFlatCellCache(
					0.0f,
					Sampler.Config->QuadsPerCell(),
					Sampler.Config->CellSize()));
			}
			Sampler.KeyToIndex.Add(Key, Sampler.FlatCellIndex);
			return Sampler.FlatCellIndex;
		}

		TArray<float> CellTexels;
		int32 CellResolution = 0;
		if (!ArcTerrainTileAccess::LoadCellHeightsSync(
				*Sampler.Terrain, Key, CellTexels, CellResolution)
			|| CellResolution < 4)
		{
			Sampler.KeyToIndex.Add(Key, INDEX_NONE);
			return INDEX_NONE;
		}
		const int32 CellIndex = Sampler.Cells.Num();
		Sampler.Cells.Add(ArcTerrainSurface::BuildCellCache(
			Key, CellTexels, CellResolution - 3, Sampler.Config->CellSize()));
		Sampler.KeyToIndex.Add(Key, CellIndex);
		return CellIndex;
	}

	bool TrySampleTerrainSurface(
		FTerrainSurfaceSampler& Sampler,
		const FVector& WorldPosition,
		FVector& OutWorldPosition,
		FVector& OutWorldNormal)
	{
		const FVector LocalPosition = Sampler.TerrainTransform.InverseTransformPosition(WorldPosition);
		if (!IsFiniteVector(LocalPosition))
		{
			return false;
		}
		const FArcTerrainCellKey Key = ArcTerrain::CellKeyFromLocal(LocalPosition, *Sampler.Config);
		const int32 CellIndex = GetOrBuildCellCache(Sampler, Key);
		if (CellIndex == INDEX_NONE)
		{
			return false;
		}
		const FArcTerrainSurfaceCellCache& Cell = Sampler.Cells[CellIndex];
		const double CellSize = Sampler.Config->CellSize();
		const float FracX = static_cast<float>(FMath::Clamp(
			(LocalPosition.X - static_cast<double>(Key.X) * CellSize) / CellSize, 0.0, 1.0));
		const float FracY = static_cast<float>(FMath::Clamp(
			(LocalPosition.Y - static_cast<double>(Key.Y) * CellSize) / CellSize, 0.0, 1.0));
		const float HeightCm = ArcTerrainSurface::BilinearApron(
			Cell.HeightsWithApron, Cell.RenderQuads, FracX, FracY);
		if (!FMath::IsFinite(HeightCm))
		{
			return false;
		}
		const FVector3f LocalNormal = ArcTerrainSurface::BilinearApronVec(
			Cell.NormalsWithApron, Cell.RenderQuads, FracX, FracY);
		OutWorldPosition = Sampler.TerrainTransform.TransformPosition(
			FVector(LocalPosition.X, LocalPosition.Y, static_cast<double>(HeightCm)));
		OutWorldNormal = Sampler.TerrainTransform.TransformVectorNoScale(FVector(LocalNormal)).GetSafeNormal();
		return true;
	}

	FVector2D GetUniformCellCenterLocal(
		const FArcEcotopeSiteMetadata& Metadata,
		const int32 SiteX,
		const int32 SiteY)
	{
		const double CellSize = static_cast<double>(Metadata.CellSizeCm);
		return FVector2D(
			Metadata.LocalBounds.Min.X + (static_cast<double>(SiteX) + 0.5) * CellSize,
			Metadata.LocalBounds.Min.Y + (static_cast<double>(SiteY) + 0.5) * CellSize);
	}

	bool TryGetLocalSurfaceZCm(
		FTerrainSurfaceSampler& Sampler,
		const FTransform& ActorTransform,
		const FVector2D& LocalCenter,
		const double LocalQueryZCm,
		double& OutLocalSurfaceZCm)
	{
		if (Sampler.Terrain == nullptr)
		{
			return false;
		}
		const FVector WorldQuery = ActorTransform.TransformPosition(
			FVector(LocalCenter.X, LocalCenter.Y, LocalQueryZCm));
		FVector WorldSurface = FVector::ZeroVector;
		FVector WorldNormal = FVector::ZeroVector;
		if (!TrySampleTerrainSurface(Sampler, WorldQuery, WorldSurface, WorldNormal))
		{
			return false;
		}
		const FVector LocalSurface = ActorTransform.InverseTransformPosition(WorldSurface);
		if (!IsFiniteVector(LocalSurface))
		{
			return false;
		}
		OutLocalSurfaceZCm = LocalSurface.Z;
		return true;
	}

	void WriteSurface(
		TArray<uint8>& OutValid,
		TArray<float>& OutSlope,
		TArray<float>& OutAspect,
		const int32 CellIndex,
		const FVector& LocalNormal)
	{
		OutValid[CellIndex] = 1;
		OutSlope[CellIndex] = static_cast<float>(FMath::RadiansToDegrees(
			FMath::Acos(FMath::Clamp(LocalNormal.Z, -1.0, 1.0))));
		const FVector2D Downhill(LocalNormal.X, LocalNormal.Y);
		if (!Downhill.IsNearlyZero())
		{
			double AspectRadians = FMath::Atan2(Downhill.Y, Downhill.X);
			if (AspectRadians < 0.0)
			{
				AspectRadians += UE_TWO_PI;
			}
			const float NormalizedAspect = static_cast<float>(AspectRadians / UE_TWO_PI);
			OutAspect[CellIndex] = NormalizedAspect >= 0.0f && NormalizedAspect < 1.0f
				? NormalizedAspect : 0.0f;
		}
	}

	struct FDataMapCoverage
	{
		int32 CellCount = 0;
		int32 AvailableCount = 0;

		void Accumulate(const bool bAvailable)
		{
			++CellCount;
			AvailableCount += bAvailable ? 1 : 0;
		}
	};

	float SampleWeightedDataMap(
		const FArcEcotopeSiteMetadata& Metadata,
		const FTransform& ActorTransform,
		const EArcTerrainDataMap Map,
		const bool bEnabled,
		const double Weight,
		const double Gamma,
		const int32 X,
		const int32 Y,
		const UArcTerrainSubsystem& TerrainSubsystem,
		AArcTerrain& Terrain,
		FDataMapCoverage& InOutCoverage)
	{
		if (!bEnabled)
		{
			return 0.0f;
		}
		const FVector WorldPosition = ActorTransform.TransformPosition(Metadata.GetCellCenterLocal(X, Y));
		float Reading = 0.0f;
		const bool bHasReading = TerrainSubsystem.SampleDataMap(Terrain, Map, WorldPosition, Reading)
			&& FMath::IsFinite(Reading);
		InOutCoverage.Accumulate(bHasReading);
		if (!bHasReading)
		{
			return 0.0f;
		}
		const double Weighted = FMath::Clamp(Weight * static_cast<double>(Reading), 0.0, 1.0);
		return static_cast<float>(FMath::Clamp(FMath::Pow(Weighted, Gamma), 0.0, 1.0));
	}

	double SampleFlowStillness(
		const FArcEcotopeSiteMetadata& Metadata,
		const FTransform& ActorTransform,
		const bool bEnabled,
		const int32 X,
		const int32 Y,
		const UArcTerrainSubsystem& TerrainSubsystem,
		AArcTerrain& Terrain,
		FDataMapCoverage& InOutFlowXCoverage,
		FDataMapCoverage& InOutFlowYCoverage)
	{
		if (!bEnabled)
		{
			return 0.0;
		}
		const FVector WorldPosition = ActorTransform.TransformPosition(Metadata.GetCellCenterLocal(X, Y));
		float FlowX = 0.0f;
		const bool bHasFlowX = TerrainSubsystem.SampleDataMap(
			Terrain, EArcTerrainDataMap::FlowMapX, WorldPosition, FlowX) && FMath::IsFinite(FlowX);
		InOutFlowXCoverage.Accumulate(bHasFlowX);
		float FlowY = 0.0f;
		const bool bHasFlowY = TerrainSubsystem.SampleDataMap(
			Terrain, EArcTerrainDataMap::FlowMapY, WorldPosition, FlowY) && FMath::IsFinite(FlowY);
		InOutFlowYCoverage.Accumulate(bHasFlowY);
		if (!bHasFlowX || !bHasFlowY)
		{
			return 0.0;
		}
		const double Speed = FMath::Sqrt(
			static_cast<double>(FlowX) * FlowX + static_cast<double>(FlowY) * FlowY);
		return FMath::Clamp(1.0 - Speed, 0.0, 1.0);
	}

	void BuildSuppliedHydrologyInputs(
		const FArcClimateHydrologySettings& Settings,
		const FArcEcotopeSiteMetadata& Metadata,
		const FArcEcotopeMarginedHeightGrid& HeightGrid,
		const FTransform& ActorTransform,
		const UArcTerrainSubsystem& TerrainSubsystem,
		AArcTerrain& Terrain,
		TArray<double>& OutAccumulationM2,
		TArray<uint8>& OutChannelMask,
		FDataMapCoverage& OutFlowCoverage,
		FDataMapCoverage& OutRiversCoverage)
	{
		const bool bSampleRivers = Settings.bUseRivers;
		const double CatchmentScaleM2 = Settings.FlowToCatchmentAreaScale;
		const double MissingCatchmentM2 = Settings.MissingFlowCatchmentAreaM2;
		const int32 CellCount = HeightGrid.Dimensions.X * HeightGrid.Dimensions.Y;
		OutAccumulationM2.SetNumUninitialized(CellCount);
		OutChannelMask.Reset();
		if (bSampleRivers)
		{
			OutChannelMask.Init(0, CellCount);
		}
		int32 ChannelCellCount = 0;
		for (int32 SiteY = -HeightGrid.MarginCells;
			SiteY < HeightGrid.SiteDimensions.Y + HeightGrid.MarginCells; ++SiteY)
		{
			for (int32 SiteX = -HeightGrid.MarginCells;
				SiteX < HeightGrid.SiteDimensions.X + HeightGrid.MarginCells; ++SiteX)
			{
				const int32 CellIndex = HeightGrid.GetIndexFromSiteCell(SiteX, SiteY);
				OutAccumulationM2[CellIndex] = MissingCatchmentM2;
				const FVector2D LocalCenter = GetUniformCellCenterLocal(Metadata, SiteX, SiteY);
				const FVector WorldPosition = ActorTransform.TransformPosition(FVector(
					LocalCenter.X, LocalCenter.Y, HeightGrid.HeightMeters[CellIndex] * 100.0));
				float FlowReading = 0.0f;
				const bool bHasFlow =
					TerrainSubsystem.SampleDataMap(Terrain, EArcTerrainDataMap::Flow, WorldPosition, FlowReading)
					&& FMath::IsFinite(FlowReading);
				OutFlowCoverage.Accumulate(bHasFlow);
				if (bHasFlow)
				{
					OutAccumulationM2[CellIndex] = static_cast<double>(FlowReading) * CatchmentScaleM2;
				}
				if (!bSampleRivers)
				{
					continue;
				}
				float RiversReading = 0.0f;
				const bool bHasRivers =
					TerrainSubsystem.SampleDataMap(Terrain, EArcTerrainDataMap::Rivers, WorldPosition, RiversReading)
					&& FMath::IsFinite(RiversReading);
				OutRiversCoverage.Accumulate(bHasRivers);
				if (bHasRivers && RiversReading > 0.0f)
				{
					OutChannelMask[CellIndex] = 1;
					++ChannelCellCount;
				}
			}
		}
		if (bSampleRivers && ChannelCellCount == 0)
		{
			OutChannelMask.Empty();
		}
	}

	bool BuildChannelDistanceField(
		const FArcEcotopeSiteMetadata& Metadata,
		const FArcClimateHydrologySettings& TerrainData,
		const TConstArrayView<float> BaseChannelExclusion,
		TArray<double>& OutSquaredCellDistances,
		bool& OutHasChannel)
	{
		const int32 CellCount = Metadata.GetCellCount();
		if (CellCount <= 0 || BaseChannelExclusion.Num() != CellCount)
		{
			return false;
		}
		OutHasChannel = false;
		TArray<uint8> ChannelMask;
		ChannelMask.SetNumUninitialized(CellCount);
		for (int32 CellIndex = 0; CellIndex < CellCount; ++CellIndex)
		{
			ChannelMask[CellIndex] = BaseChannelExclusion[CellIndex] >= TerrainData.RiversChannelThreshold ? 1 : 0;
			OutHasChannel = OutHasChannel || ChannelMask[CellIndex] != 0;
		}
		ArcTerrainMaskDistance::BuildSquaredCellDistanceToMask(
			ChannelMask, Metadata.GridDimensions.X, Metadata.GridDimensions.Y, OutSquaredCellDistances);
		return true;
	}

	bool BuildAndPublish(AArcClimateVolume& Volume, FText& OutError)
	{
		if (!IsInGameThread())
		{
			return SetError(OutError, TEXT("Climate baking must run on the game thread."));
		}
		UWorld* World = Volume.GetWorld();
		UBoxComponent* BoundsComponent = Volume.VolumeBounds;
		if (World == nullptr || BoundsComponent == nullptr)
		{
			return SetError(OutError, TEXT("Climate baking requires a loaded world and VolumeBounds."));
		}
		if (World->GetWorldPartition() != nullptr && Volume.GetIsSpatiallyLoaded())
		{
			return SetError(OutError, TEXT("Climate baking requires an always-loaded ClimateVolume in World Partition."));
		}
		const FTransform ActorTransform = Volume.GetActorTransform();
		const FVector ActorScale = ActorTransform.GetScale3D();
		const FVector Extent = BoundsComponent->GetUnscaledBoxExtent();
		if (!IsFiniteVector(ActorTransform.GetLocation()) || !ActorTransform.GetRotation().IsNormalized()
			|| !IsFiniteVector(ActorScale) || ActorScale.X <= 0.0 || ActorScale.Y <= 0.0 || ActorScale.Z <= 0.0
			|| !FMath::IsNearlyEqual(ActorScale.X, ActorScale.Y)
			|| !FMath::IsNearlyEqual(ActorScale.X, ActorScale.Z)
			|| !IsFiniteVector(Extent) || Extent.X <= 0.0 || Extent.Y <= 0.0 || Extent.Z <= 0.0
			|| !FMath::IsFinite(Volume.CellSizeCm) || Volume.CellSizeCm <= 0.0f)
		{
			return SetError(OutError, TEXT("Climate baking requires valid bounds, grid settings, and a positive uniform transform."));
		}
		FText SettingsError;
		if (!Volume.FieldSettings.IsValid(SettingsError) || !Volume.HydrologySettings.IsValid(SettingsError))
		{
			return SetError(OutError, *FString::Printf(
				TEXT("Climate baking requires valid settings: %s"), *SettingsError.ToString()));
		}
		uint8 MaximumLevel = FMath::Max(
			FMath::Max(Volume.FieldSettings.TemperatureLevel, Volume.FieldSettings.HumidityLevel),
			FMath::Max(Volume.FieldSettings.SunLoadLevel, Volume.FieldSettings.HorizonLevel));
		if (Volume.ChunkBaseCellSpan <= 0 || Volume.ChunkBaseCellSpan % (1 << MaximumLevel) != 0)
		{
			return SetError(OutError, TEXT("Climate ChunkBaseCellSpan must align to every native climate field level."));
		}

		if (!FMath::IsFinite(Volume.GradientBandCm) || Volume.GradientBandCm < 0.0)
		{
			return SetError(OutError, TEXT("Climate GradientBandCm must be finite and non-negative."));
		}
		const double ContributionBandCm = Volume.GradientBandCm;
		const FVector GridExtent(Extent.X + ContributionBandCm, Extent.Y + ContributionBandCm, Extent.Z);
		const double Width = GridExtent.X * 2.0;
		const double Height = GridExtent.Y * 2.0;
		const double WidthRatio = Width / Volume.CellSizeCm;
		const double HeightRatio = Height / Volume.CellSizeCm;
		const double Int64UpperBound = static_cast<double>(MAX_int64);
		if (!FMath::IsFinite(WidthRatio) || !FMath::IsFinite(HeightRatio)
			|| WidthRatio <= 0.0 || HeightRatio <= 0.0
			|| WidthRatio >= Int64UpperBound || HeightRatio >= Int64UpperBound)
		{
			return SetError(OutError, TEXT("Climate grid has invalid dimensions."));
		}
		const int64 GridWidth = FMath::CeilToInt64(WidthRatio);
		const int64 GridHeight = FMath::CeilToInt64(HeightRatio);
		if (GridWidth <= 0 || GridHeight <= 0 || GridWidth > MAX_int32 || GridHeight > MAX_int32)
		{
			return SetError(OutError, TEXT("Climate grid exceeds the supported cell allocation limit."));
		}
		uint64 BaseCellCount = 0;
		if (!CheckedMultiply(
			static_cast<uint64>(GridWidth), static_cast<uint64>(GridHeight), BaseCellCount)
			|| BaseCellCount == 0 || BaseCellCount > static_cast<uint64>(MAX_int32))
		{
			return SetError(OutError, TEXT("Climate grid exceeds the supported cell allocation limit."));
		}
		uint64 MarginedWidthUnsigned = 0;
		uint64 MarginedHeightUnsigned = 0;
		uint64 MarginedCellCount = 0;
		if (!CheckedAdd(
			static_cast<uint64>(GridWidth), static_cast<uint64>(2 * MarginCells), MarginedWidthUnsigned)
			|| !CheckedAdd(
				static_cast<uint64>(GridHeight), static_cast<uint64>(2 * MarginCells), MarginedHeightUnsigned)
			|| MarginedWidthUnsigned == 0 || MarginedHeightUnsigned == 0
			|| MarginedWidthUnsigned > static_cast<uint64>(MAX_int32)
			|| MarginedHeightUnsigned > static_cast<uint64>(MAX_int32)
			|| !CheckedMultiply(MarginedWidthUnsigned, MarginedHeightUnsigned, MarginedCellCount)
			|| MarginedCellCount == 0 || MarginedCellCount > static_cast<uint64>(MAX_int32))
		{
			return SetError(OutError, TEXT("Climate margined grid exceeds the supported allocation limit."));
		}
		const int32 CellCount = static_cast<int32>(BaseCellCount);
		const int32 MarginedWidth = static_cast<int32>(MarginedWidthUnsigned);
		const int32 MarginedHeight = static_cast<int32>(MarginedHeightUnsigned);

		const FBox LocalBounds(-GridExtent, GridExtent);
		FArcEcotopeSiteMetadata GeneratorMetadata;
		GeneratorMetadata.LocalBounds = LocalBounds;
		GeneratorMetadata.CellSizeCm = Volume.CellSizeCm;
		GeneratorMetadata.GridDimensions = FIntPoint(static_cast<int32>(GridWidth), static_cast<int32>(GridHeight));
		GeneratorMetadata.MoistureLevel = 0;
		GeneratorMetadata.SunLoadLevel = Volume.FieldSettings.SunLoadLevel;
		GeneratorMetadata.WindExposureLevel = Volume.FieldSettings.HorizonLevel;
		GeneratorMetadata.SkyViewFactorLevel = Volume.FieldSettings.HorizonLevel;
		GeneratorMetadata.TemperatureLevel = Volume.FieldSettings.TemperatureLevel;
		const uint8 ClimateHumidityLevel = Volume.FieldSettings.HumidityLevel;
		const FArcClimateFieldSettings& SiteSettings = Volume.FieldSettings;
		const FArcClimateHydrologySettings& TerrainSettings = Volume.HydrologySettings;

		const double CellSize = Volume.CellSizeCm;
		const FVector MarginedMin(
			-GridExtent.X - MarginCells * CellSize,
			-GridExtent.Y - MarginCells * CellSize,
			-GridExtent.Z);
		const FVector MarginedMax(
			-GridExtent.X + (GridWidth + MarginCells) * CellSize,
			-GridExtent.Y + (GridHeight + MarginCells) * CellSize,
			GridExtent.Z);
		const FBox WorldSampleBounds = FBox(MarginedMin, MarginedMax).TransformBy(ActorTransform);
		const FBox WorldProductBounds = LocalBounds.TransformBy(ActorTransform);
		const UE::ArcEcotope::FArcEcotopeTerrainChoice TerrainChoice =
			UE::ArcEcotope::SelectTerrainForVolume(
				*World, WorldProductBounds, WorldSampleBounds, ActorTransform);
		AArcTerrain* Terrain = TerrainChoice.Terrain;
		if (Terrain == nullptr)
		{
			return SetError(OutError, TEXT("Climate bake requires a covering ArcTerrain height source."));
		}
		const UArcTerrainSubsystem* TerrainSubsystem = World->GetSubsystem<UArcTerrainSubsystem>();
		if (TerrainSubsystem == nullptr)
		{
			return SetError(OutError, TEXT("Climate bake requires the ArcTerrain subsystem."));
		}
		FIntPoint TerrainMinCell = FIntPoint::ZeroValue;
		FIntPoint TerrainMaxCell = FIntPoint::ZeroValue;
		int64 TerrainSpannedCellCount = 0;
		if (!UE::ArcEcotope::TryComputeTerrainCellRect(
				WorldSampleBounds,
				Terrain->GetTerrainTransform(),
				Terrain->Config.CellSize(),
				TerrainMinCell,
				TerrainMaxCell,
				TerrainSpannedCellCount))
		{
			return SetError(OutError, TEXT("Climate bake could not bound the selected ArcTerrain cache footprint."));
		}
		FClimateBakeMemoryProjection Projection;
		uint64 ProjectedPeakBytes = 0;
		if (!ProjectClimateBakePeak(
				GeneratorMetadata.GridDimensions,
				MarginedCellCount,
				TerrainSpannedCellCount,
				Terrain->Config.QuadsPerCell(),
				Volume.ChunkBaseCellSpan,
				GeneratorMetadata.TemperatureLevel,
				ClimateHumidityLevel,
				GeneratorMetadata.SunLoadLevel,
				GeneratorMetadata.WindExposureLevel,
				GeneratorMetadata.MoistureLevel,
				Projection)
			|| !Projection.GetPeakBytes(ProjectedPeakBytes))
		{
			return SetError(OutError, TEXT("Climate bake could not conservatively bound peak memory."));
		}
		uint64 PeakGateBytes = 0;
		if (!CheckedMultiply(
				static_cast<uint64>(FMath::Max(
					CVarMaxProjectedClimateBakePeakMiB.GetValueOnGameThread(), 1)),
				1024ull * 1024ull,
				PeakGateBytes))
		{
			return SetError(OutError, TEXT("Climate bake memory gate is unrepresentable."));
		}
		if (ProjectedPeakBytes > PeakGateBytes)
		{
			return SetError(OutError, *FString::Printf(
				TEXT("Climate bake projects %.2f MiB, above the configured %.2f MiB peak gate; reduce the volume/cell density or raise arcecotope.ClimateBake.MaxProjectedPeakMiB deliberately."),
				static_cast<double>(ProjectedPeakBytes) / (1024.0 * 1024.0),
				static_cast<double>(PeakGateBytes) / (1024.0 * 1024.0)));
		}
		FArcEcotopeMarginedHeightGrid HeightGrid;
		HeightGrid.Dimensions = FIntPoint(static_cast<int32>(MarginedWidth), static_cast<int32>(MarginedHeight));
		HeightGrid.SiteDimensions = GeneratorMetadata.GridDimensions;
		HeightGrid.MarginCells = MarginCells;
		HeightGrid.CellSizeM = CellSize * 0.01;
		HeightGrid.HeightMeters.Init(0.0, static_cast<int32>(MarginedCellCount));
		TArray<uint8> ValidSurface;
		ValidSurface.Init(0, CellCount);
		TArray<float> SlopeDegrees;
		SlopeDegrees.Init(0.0f, CellCount);
		TArray<float> AspectNormalized;
		AspectNormalized.Init(0.0f, CellCount);
		TArray<float> SurfaceWorldZMeters;
		SurfaceWorldZMeters.Init(0.0f, CellCount);
		const bool bTrailingColumnClamped = static_cast<double>(GridWidth) * CellSize > Width;
		const bool bTrailingRowClamped = static_cast<double>(GridHeight) * CellSize > Height;
		{
			FTerrainSurfaceSampler SurfaceSampler;
			SurfaceSampler.Terrain = Terrain;
			SurfaceSampler.Config = &Terrain->Config;
			SurfaceSampler.TerrainTransform = Terrain->GetTerrainTransform();
			for (int32 Y = 0; Y < GeneratorMetadata.GridDimensions.Y; ++Y)
			{
				for (int32 X = 0; X < GeneratorMetadata.GridDimensions.X; ++X)
				{
					const int32 CellIndex = X + Y * GeneratorMetadata.GridDimensions.X;
					const FVector LocalCenter = GeneratorMetadata.GetCellCenterLocal(X, Y);
					const FVector WorldStart = ActorTransform.TransformPosition(
						FVector(LocalCenter.X, LocalCenter.Y, Extent.Z));
					FVector WorldSurface = FVector::ZeroVector;
					FVector WorldNormal = FVector::ZeroVector;
					if (!TrySampleTerrainSurface(SurfaceSampler, WorldStart, WorldSurface, WorldNormal))
					{
						return SetError(OutError, *FString::Printf(
							TEXT("Climate bake requires readable ArcTerrain height data at climate cell (%d, %d)."),
							X, Y));
					}
					const FVector LocalSurface = ActorTransform.InverseTransformPosition(WorldSurface);
					const FVector LocalNormal = ActorTransform.InverseTransformVectorNoScale(WorldNormal).GetSafeNormal();
					if (!IsFiniteVector(LocalSurface) || !IsFiniteVector(LocalNormal) || LocalNormal.IsNearlyZero())
					{
						return SetError(OutError, *FString::Printf(
							TEXT("Climate bake requires a valid ArcTerrain surface sample at climate cell (%d, %d)."),
							X, Y));
					}
					WriteSurface(ValidSurface, SlopeDegrees, AspectNormalized, CellIndex, LocalNormal);
					SurfaceWorldZMeters[CellIndex] = static_cast<float>(WorldSurface.Z * 0.01);
					double LatticeSurfaceZCm = LocalSurface.Z;
					if ((bTrailingColumnClamped && X == GeneratorMetadata.GridDimensions.X - 1)
						|| (bTrailingRowClamped && Y == GeneratorMetadata.GridDimensions.Y - 1))
					{
						double LatticeZCm = 0.0;
						if (!TryGetLocalSurfaceZCm(
								SurfaceSampler, ActorTransform,
								GetUniformCellCenterLocal(GeneratorMetadata, X, Y), Extent.Z, LatticeZCm))
						{
							return SetError(OutError, *FString::Printf(
								TEXT("Climate bake requires ArcTerrain coverage at trailing climate cell (%d, %d)."),
								X, Y));
						}
						LatticeSurfaceZCm = LatticeZCm;
					}
					HeightGrid.HeightMeters[HeightGrid.GetIndexFromSiteCell(X, Y)] = LatticeSurfaceZCm * 0.01;
				}
			}
			for (int32 MarginedY = 0; MarginedY < HeightGrid.Dimensions.Y; ++MarginedY)
			{
				const int32 SiteY = MarginedY - HeightGrid.MarginCells;
				for (int32 MarginedX = 0; MarginedX < HeightGrid.Dimensions.X; ++MarginedX)
				{
					const int32 SiteX = MarginedX - HeightGrid.MarginCells;
					if (SiteX >= 0 && SiteX < GeneratorMetadata.GridDimensions.X
						&& SiteY >= 0 && SiteY < GeneratorMetadata.GridDimensions.Y)
					{
						continue;
					}
					double SurfaceZCm = 0.0;
					const int32 MarginedIndex = HeightGrid.GetIndexFromSiteCell(SiteX, SiteY);
					if (TryGetLocalSurfaceZCm(
							SurfaceSampler, ActorTransform,
							GetUniformCellCenterLocal(GeneratorMetadata, SiteX, SiteY), Extent.Z, SurfaceZCm))
					{
						HeightGrid.HeightMeters[MarginedIndex] = SurfaceZCm * 0.01;
					}
					else
					{
						return SetError(OutError, *FString::Printf(
							TEXT("Climate bake requires ArcTerrain coverage at margined climate cell (%d, %d)."),
							SiteX, SiteY));
					}
				}
			}
		}

		TArray<float> BaseSunLoad;
		ArcEcotopeSiteFields::FSunLoadDiagnostics SunDiagnostics;
		FText FieldError;
		if (!ArcEcotopeSiteFields::GenerateSunLoadRect(
				GeneratorMetadata, SlopeDegrees, AspectNormalized, ValidSurface, SiteSettings,
				FIntRect(FIntPoint::ZeroValue, GeneratorMetadata.GridDimensions),
				BaseSunLoad, SunDiagnostics, FieldError))
		{
			return SetError(OutError, *FieldError.ToString());
		}
		TArray<float> SunLoad;
		if (!ArcEcotopeSiteFields::ReduceFieldToLevel(
				GeneratorMetadata, BaseSunLoad, GeneratorMetadata.SunLoadLevel, SunLoad))
		{
			return SetError(OutError, TEXT("Climate sun reduction failed."));
		}
		BaseSunLoad.Reset();
		AspectNormalized.Reset();

		TArray<double> SuppliedAccumulationM2;
		TArray<uint8> SuppliedChannelMask;
		FDataMapCoverage HydrologyFlowCoverage;
		FDataMapCoverage HydrologyRiversCoverage;
		BuildSuppliedHydrologyInputs(
			TerrainSettings, GeneratorMetadata, HeightGrid,
			ActorTransform, *TerrainSubsystem, *Terrain, SuppliedAccumulationM2, SuppliedChannelMask,
			HydrologyFlowCoverage, HydrologyRiversCoverage);
		UE_CLOGF(HydrologyFlowCoverage.CellCount > 0
				&& HydrologyFlowCoverage.AvailableCount < HydrologyFlowCoverage.CellCount,
			LogArcEcotope, Warning,"ArcEcotope climate bake of %ls: Flow data map covered %d/%d hydrology cells; uncovered cells fall back to the missing-catchment-area default.",
			*Volume.GetName(), HydrologyFlowCoverage.AvailableCount, HydrologyFlowCoverage.CellCount);
		UE_CLOGF(TerrainSettings.bUseRivers && HydrologyRiversCoverage.CellCount > 0
				&& HydrologyRiversCoverage.AvailableCount < HydrologyRiversCoverage.CellCount,
			LogArcEcotope, Warning,"ArcEcotope climate bake of %ls: Rivers data map covered %d/%d hydrology cells; uncovered cells cannot become channel.",
			*Volume.GetName(), HydrologyRiversCoverage.AvailableCount, HydrologyRiversCoverage.CellCount);
		TArray<float> BaseMoisture;
		TArray<float> BaseHandWorldM;
		ArcEcotopeSiteFields::FMoistureDiagnostics MoistureDiagnostics;
		if (!ArcEcotopeSiteFields::GenerateMoisture(
				HeightGrid, SiteSettings, SuppliedAccumulationM2, SuppliedChannelMask,
				BaseMoisture, BaseHandWorldM, MoistureDiagnostics, FieldError))
		{
			return SetError(OutError, *FieldError.ToString());
		}
		{
			const int32 UnresolvedCellCount = MoistureDiagnostics.HandUnresolvedCellCount;
			const double UnresolvedFraction = CellCount > 0
				? static_cast<double>(UnresolvedCellCount) / static_cast<double>(CellCount) : 0.0;
			UE_CLOGF(CellCount > 0 && UnresolvedCellCount >= CellCount, LogArcEcotope, Error,"ArcEcotope climate bake of %ls: handUnresolved cells=%d/%d fraction=%.3f; no climate cell reached a channel at all, which is what an empty channel mask produces when bUseRivers is off or the Rivers map covers nothing, and every consumer of height above drainage falls back to its unresolved branch.",
				*Volume.GetName(), UnresolvedCellCount, CellCount, UnresolvedFraction);
			UE_CLOGF(UnresolvedCellCount > 0 && (CellCount <= 0 || UnresolvedCellCount < CellCount),
				LogArcEcotope, Warning,"ArcEcotope climate bake of %ls: handUnresolved cells=%d/%d fraction=%.3f; those cells reached no channel, so their moisture carries the TWI term alone and their height above drainage is unresolved.",
				*Volume.GetName(), UnresolvedCellCount, CellCount, UnresolvedFraction);
			UE_CLOGF(UnresolvedCellCount == 0, LogArcEcotope, Log,"ArcEcotope climate bake of %ls: handUnresolved cells=0/%d fraction=0.000",
				*Volume.GetName(), CellCount);
			UE_LOGF(LogArcEcotope, Log,"ArcEcotope climate bake of %ls: moisture diagnostics peakWorkingBytes=%llu twi min/mean/max=%.4f/%.4f/%.4f hand min/mean/max=%.2fm/%.2fm/%.2fm moisture min/mean/max=%.4f/%.4f/%.4f filled=%d saturatedFlat=%d channel=%d handUnresolved=%d",
				*Volume.GetName(),
				MoistureDiagnostics.PeakWorkingBytes,
				MoistureDiagnostics.TwiMin, MoistureDiagnostics.TwiMean, MoistureDiagnostics.TwiMax,
				MoistureDiagnostics.HandMinM, MoistureDiagnostics.HandMeanM, MoistureDiagnostics.HandMaxM,
				MoistureDiagnostics.MoistureMin, MoistureDiagnostics.MoistureMean, MoistureDiagnostics.MoistureMax,
				MoistureDiagnostics.FilledCellCount, MoistureDiagnostics.SaturatedFlatCellCount,
				MoistureDiagnostics.ChannelCellCount, MoistureDiagnostics.HandUnresolvedCellCount);
		}
		{
			TArray<float> SortedMoisture(BaseMoisture);
			SortedMoisture.Sort();
			const int32 SortedCount = SortedMoisture.Num();
			if (SortedCount > 0)
			{
				const TFunction<float(double)> Percentile = [&SortedMoisture, SortedCount](const double Fraction) -> float
				{
					const int32 Index = FMath::Clamp(
						FMath::RoundToInt(Fraction * static_cast<double>(SortedCount - 1)), 0, SortedCount - 1);
					return SortedMoisture[Index];
				};
				double MoistureSum = 0.0;
				int32 ZeroCount = 0;
				int32 OneCount = 0;
				for (const float Value : SortedMoisture)
				{
					MoistureSum += static_cast<double>(Value);
					ZeroCount += Value == 0.0f ? 1 : 0;
					OneCount += Value == 1.0f ? 1 : 0;
				}
				const double MoistureMean = MoistureSum / static_cast<double>(SortedCount);
				const double ZeroFraction = static_cast<double>(ZeroCount) / static_cast<double>(SortedCount);
				const double OneFraction = static_cast<double>(OneCount) / static_cast<double>(SortedCount);
				UE_LOGF(LogArcEcotope, Log,"ArcEcotope climate bake of %ls: moisture field cells=%d min=%.4f p05=%.4f p25=%.4f p50=%.4f p75=%.4f p95=%.4f max=%.4f mean=%.4f atZero=%d(%.3f) atOne=%d(%.3f)",
					*Volume.GetName(), SortedCount,
					SortedMoisture[0], Percentile(0.05), Percentile(0.25), Percentile(0.50), Percentile(0.75),
					Percentile(0.95), SortedMoisture[SortedCount - 1], MoistureMean,
					ZeroCount, ZeroFraction, OneCount, OneFraction);
			}
		}
		for (float& HandValue : BaseHandWorldM)
		{
			if (HandValue >= 0.0f)
			{
				HandValue *= static_cast<float>(ActorScale.X);
			}
		}
		SuppliedAccumulationM2.Reset();
		SuppliedChannelMask.Reset();

		TArray<float> SkyViewFactor;
		TArray<float> WindExposure;
		ArcEcotopeSiteFields::FHorizonDiagnostics HorizonDiagnostics;
		if (!ArcEcotopeSiteFields::GenerateHorizonFields(
				HeightGrid, GeneratorMetadata, SlopeDegrees, ValidSurface, SiteSettings,
				SkyViewFactor, WindExposure, HorizonDiagnostics, FieldError))
		{
			return SetError(OutError, *FieldError.ToString());
		}
		HeightGrid.HeightMeters.Reset();
		SlopeDegrees.Reset();
		ValidSurface.Reset();

		TArray<AArcClimateVolume*> SortedVolumes;
		UE::ArcEcotope::Climate::GatherSortedVolumes(*World, WorldProductBounds, SortedVolumes);
		const bool bBakingVolumeContributes = SortedVolumes.Contains(&Volume);
		FDataMapCoverage RiversCoverage;
		TArray<float> BaseChannelExclusion;
		BaseChannelExclusion.SetNumUninitialized(CellCount);
		for (int32 Y = 0; Y < GeneratorMetadata.GridDimensions.Y; ++Y)
		{
			for (int32 X = 0; X < GeneratorMetadata.GridDimensions.X; ++X)
			{
				BaseChannelExclusion[X + Y * GeneratorMetadata.GridDimensions.X] = SampleWeightedDataMap(
					GeneratorMetadata, ActorTransform,
					EArcTerrainDataMap::Rivers, TerrainSettings.bUseRivers,
					TerrainSettings.RiversWeight, TerrainSettings.RiversGamma,
					X, Y, *TerrainSubsystem, *Terrain, RiversCoverage);
			}
		}
		TArray<double> ChannelSquaredCellDistances;
		bool bHasChannel = false;
		if (!BuildChannelDistanceField(
				GeneratorMetadata, TerrainSettings, BaseChannelExclusion,
				ChannelSquaredCellDistances, bHasChannel))
		{
			return SetError(OutError, TEXT("Climate channel distance field generation failed."));
		}
		BaseChannelExclusion.Reset();
		const FIntPoint SkyDimensions = GeneratorMetadata.GetLevelDimensions(
			GeneratorMetadata.SkyViewFactorLevel);
		FDataMapCoverage FlowXCoverage;
		FDataMapCoverage FlowYCoverage;
		FArcClimateProductMetadata ProductMetadata;
		ProductMetadata.SchemaVersion = FArcClimateProductMetadata::GetCurrentSchemaVersion();
		ProductMetadata.ClimateVolumeId = Volume.ClimateVolumeId;
		ProductMetadata.BakeTransform = ActorTransform;
		ProductMetadata.LocalBounds = LocalBounds;
		ProductMetadata.CellSizeCm = Volume.CellSizeCm;
		ProductMetadata.GridDimensions = GeneratorMetadata.GridDimensions;
		ProductMetadata.ChunkBaseCellSpan = Volume.ChunkBaseCellSpan;
		ProductMetadata.TemperatureLevel = GeneratorMetadata.TemperatureLevel;
		ProductMetadata.HumidityLevel = ClimateHumidityLevel;
		ProductMetadata.SunLoadLevel = GeneratorMetadata.SunLoadLevel;
		ProductMetadata.WindExposureLevel = GeneratorMetadata.WindExposureLevel;
		ProductMetadata.SkyViewFactorLevel = GeneratorMetadata.SkyViewFactorLevel;
		ProductMetadata.MoistureLevel = GeneratorMetadata.MoistureLevel;
		ProductMetadata.FieldValidityMask = FArcClimateProductMetadata::AllFieldValidityMask;
		ProductMetadata.bUseRivers = TerrainSettings.bUseRivers;
		ProductMetadata.RiversWeight = TerrainSettings.RiversWeight;
		ProductMetadata.RiversGamma = TerrainSettings.RiversGamma;
		ProductMetadata.RiversChannelThreshold = TerrainSettings.RiversChannelThreshold;
		if (HorizonDiagnostics.SkipReason != ArcEcotopeSiteFields::EHorizonSkipReason::None)
		{
			ProductMetadata.FieldValidityMask &= ~(
				(1u << static_cast<uint8>(EArcClimateField::WindExposure))
				| (1u << static_cast<uint8>(EArcClimateField::SkyViewFactor)));
		}
		FArcClimateProductBuildWriter Writer;
		FString WriterError;
		if (!Writer.Begin(ProductMetadata, Volume.ChunkBaseCellSpan, WriterError)
			|| !SliceAndWriteClimateField(
				Writer, ProductMetadata, Volume.ChunkBaseCellSpan,
				EArcClimateField::SunLoad, SunLoad, WriterError)
			|| !SliceAndWriteClimateField(
				Writer, ProductMetadata, Volume.ChunkBaseCellSpan,
				EArcClimateField::WindExposure, WindExposure, WriterError)
			|| !SliceAndWriteClimateField(
				Writer, ProductMetadata, Volume.ChunkBaseCellSpan,
				EArcClimateField::SkyViewFactor, SkyViewFactor, WriterError)
			|| !SliceAndWriteClimateField(
				Writer, ProductMetadata, Volume.ChunkBaseCellSpan,
				EArcClimateField::Moisture, BaseMoisture, WriterError))
		{
			return SetError(OutError, *FString::Printf(
				TEXT("Climate staged writer failed: %s"), *WriterError));
		}
		SunLoad.Reset();
		WindExposure.Reset();
		for (int64 StripeStartY64 = 0;
			StripeStartY64 < static_cast<int64>(GeneratorMetadata.GridDimensions.Y);
			StripeStartY64 += static_cast<int64>(Volume.ChunkBaseCellSpan))
		{
			const int32 StripeStartY = static_cast<int32>(StripeStartY64);
			const int32 StripeRowCount = FMath::Min(
				Volume.ChunkBaseCellSpan,
				GeneratorMetadata.GridDimensions.Y - StripeStartY);
			uint64 StripeValueCount = 0;
			if (!CheckedMultiply(
				static_cast<uint64>(GeneratorMetadata.GridDimensions.X),
				static_cast<uint64>(StripeRowCount), StripeValueCount)
				|| StripeValueCount == 0 || StripeValueCount > static_cast<uint64>(MAX_int32))
			{
				return SetError(OutError, TEXT("Climate temperature stripe exceeds the supported allocation limit."));
			}
			TArray<float> TemperatureStripe;
			TArray<float> HumidityStripe;
			TemperatureStripe.SetNumUninitialized(static_cast<int32>(StripeValueCount));
			HumidityStripe.SetNumUninitialized(static_cast<int32>(StripeValueCount));
			for (int32 LocalY = 0; LocalY < StripeRowCount; ++LocalY)
			{
				const int32 Y = StripeStartY + LocalY;
				for (int32 X = 0; X < GeneratorMetadata.GridDimensions.X; ++X)
				{
					uint64 CellIndexUnsigned = 0;
					uint64 StripeIndexUnsigned = 0;
					if (!CheckedMultiply(
							static_cast<uint64>(Y),
							static_cast<uint64>(GeneratorMetadata.GridDimensions.X), CellIndexUnsigned)
						|| !CheckedAdd(CellIndexUnsigned, static_cast<uint64>(X), CellIndexUnsigned)
						|| !CheckedMultiply(
							static_cast<uint64>(LocalY),
							static_cast<uint64>(GeneratorMetadata.GridDimensions.X), StripeIndexUnsigned)
						|| !CheckedAdd(StripeIndexUnsigned, static_cast<uint64>(X), StripeIndexUnsigned)
						|| CellIndexUnsigned >= static_cast<uint64>(CellCount)
						|| StripeIndexUnsigned >= StripeValueCount)
					{
						return SetError(OutError, TEXT("Climate temperature stripe index is outside the supported allocation."));
					}
					const int32 CellIndex = static_cast<int32>(CellIndexUnsigned);
					const int32 StripeIndex = static_cast<int32>(StripeIndexUnsigned);
					const FVector WorldPosition = ActorTransform.TransformPosition(
						GeneratorMetadata.GetCellCenterLocal(X, Y));
					const UE::ArcEcotope::Climate::FArcClimateAnalyticSample ClimateSample =
						UE::ArcEcotope::Climate::EvaluateAt(
							SortedVolumes,
							bBakingVolumeContributes ? &Volume : nullptr,
							Volume.ReferenceElevationM,
							WorldPosition);
					double Temperature = ClimateSample.Temperature;
					Temperature -= TerrainSettings.LapseRatePer100M
						* (static_cast<double>(SurfaceWorldZMeters[CellIndex])
							- ClimateSample.ReferenceElevationM) / 100.0;
					Temperature = FMath::Clamp(Temperature, 0.0, 1.0);
					const double Stillness = SampleFlowStillness(
						GeneratorMetadata, ActorTransform,
						TerrainSettings.bUseFlowDirection, X, Y, *TerrainSubsystem, *Terrain,
						FlowXCoverage, FlowYCoverage);
					const double MoistureFactor = FMath::Clamp(
						static_cast<double>(TerrainSettings.MoistureCoolingStrength)
						* BaseMoisture[CellIndex]
						* FMath::Lerp(1.0, static_cast<double>(TerrainSettings.StillWaterCoolingBoost), Stillness),
						0.0, 1.0);
					Temperature -= MoistureFactor * Temperature;
					const float HandWorldM = BaseHandWorldM[CellIndex];
					const double Pooling = HandWorldM < 0.0f ? 0.0
						: FMath::Exp(-static_cast<double>(HandWorldM)
							/ static_cast<double>(TerrainSettings.ColdAirPoolingDepthM));
					const int32 SkyIndex = (X >> GeneratorMetadata.SkyViewFactorLevel)
						+ (Y >> GeneratorMetadata.SkyViewFactorLevel) * SkyDimensions.X;
					const double SkyWeight = TerrainSettings.bColdAirWeightedBySkyView
						? static_cast<double>(SkyViewFactor[SkyIndex]) : 1.0;
					const double ColdFactor = FMath::Clamp(
						static_cast<double>(TerrainSettings.ColdAirDrainageStrength) * Pooling * SkyWeight,
						0.0, 1.0);
					Temperature -= ColdFactor * Temperature;
					TemperatureStripe[StripeIndex] = static_cast<float>(FMath::Clamp(Temperature, 0.0, 1.0));

					double Humidity = FMath::Clamp(ClimateSample.Humidity, 0.0, 1.0);
					const double ReachM = TerrainSettings.WaterHumidityReachM;
					if (ReachM > 0.0 && bHasChannel)
					{
						const double DistanceM = FMath::Sqrt(ChannelSquaredCellDistances[CellIndex])
							* GeneratorMetadata.CellSizeCm / 100.0;
						const double Falloff = FMath::Clamp(1.0 - DistanceM / ReachM, 0.0, 1.0);
						Humidity = FMath::Lerp(Humidity, 1.0, Falloff);
					}
					HumidityStripe[StripeIndex] = static_cast<float>(FMath::Clamp(Humidity, 0.0, 1.0));
				}
			}
			const int32 ChunkY = StripeStartY / Volume.ChunkBaseCellSpan;
			const int32 ChunkGridWidth = static_cast<int32>(
				(static_cast<int64>(GeneratorMetadata.GridDimensions.X)
					+ Volume.ChunkBaseCellSpan - 1) / Volume.ChunkBaseCellSpan);
			for (int32 ChunkX = 0; ChunkX < ChunkGridWidth; ++ChunkX)
			{
				const FIntPoint ChunkId(ChunkX, ChunkY);
				FIntRect TemperatureRect;
				FIntRect HumidityRect;
				TArray<float> TemperatureValues;
				TArray<float> HumidityValues;
				FString LocalError;
				if (!GetClimateOwnedRect(
						ProductMetadata, Volume.ChunkBaseCellSpan, ChunkId,
						EArcClimateField::Temperature, TemperatureRect, LocalError))
				{
					return SetError(OutError, *LocalError);
				}
				if (!GetClimateOwnedRect(
						ProductMetadata, Volume.ChunkBaseCellSpan, ChunkId,
						EArcClimateField::Humidity, HumidityRect, LocalError))
				{
					return SetError(OutError, *LocalError);
				}
				if (!ReduceClimateStripeToOwnedField(
						GeneratorMetadata, GeneratorMetadata.TemperatureLevel,
						StripeStartY, StripeRowCount, TemperatureRect, TemperatureStripe, TemperatureValues, LocalError))
				{
					return SetError(OutError, *LocalError);
				}
				if (!ReduceClimateStripeToOwnedField(
						GeneratorMetadata, ClimateHumidityLevel,
						StripeStartY, StripeRowCount, HumidityRect, HumidityStripe, HumidityValues, LocalError))
				{
					return SetError(OutError, *LocalError);
				}
				if (!Writer.WriteFieldBlock(
						EArcClimateField::Temperature, ChunkId, TemperatureValues, WriterError))
				{
					return SetError(OutError, *FString::Printf(
						TEXT("Climate staged writer failed: %s"), *WriterError));
				}
				if (!Writer.WriteFieldBlock(
						EArcClimateField::Humidity, ChunkId, HumidityValues, WriterError))
				{
					return SetError(OutError, *FString::Printf(
						TEXT("Climate staged writer failed: %s"), *WriterError));
				}
			}
		}
		SurfaceWorldZMeters.Reset();
		BaseMoisture.Reset();
		BaseHandWorldM.Reset();
		SkyViewFactor.Reset();
		ChannelSquaredCellDistances.Reset();
		FArcClimateProductMetadata FinalMetadata;
		FArcClimateChunkManifest FinalManifest;
		TArray<FSharedBuffer> FinalPayloads;
		if (!Writer.Finalize(FinalMetadata, FinalManifest, FinalPayloads, WriterError))
		{
			return SetError(OutError, *FString::Printf(
				TEXT("Climate staged writer finalization failed: %s"), *WriterError));
		}
		TSharedRef<FArcClimateFrozenSource, ESPMode::ThreadSafe> SmokeSource =
			MakeShared<FArcClimateFrozenSource, ESPMode::ThreadSafe>();
		SmokeSource->Metadata = FinalMetadata;
		SmokeSource->Manifest = FinalManifest;
		SmokeSource->Payloads = FinalPayloads;
		FArcClimateProductReader SmokeReader;
		if (!SmokeReader.Open(SmokeSource, WriterError))
		{
			return SetError(OutError, *FString::Printf(
				TEXT("Climate product round-trip validation failed: %s"), *WriterError));
		}

		if (!Volume.ReplaceClimateWithValidatedChunks(
				MoveTemp(FinalMetadata), MoveTemp(FinalManifest), MoveTemp(FinalPayloads), OutError))
		{
			return false;
		}
		Volume.MarkPackageDirty();
		UE_LOGF(LogArcEcotope, Log,"Climate bake succeeded for %ls: cells=%d chunks=%d.",
			*Volume.GetName(), CellCount, Volume.ClimateChunkManifest.Chunks.Num());
		OutError = FText::GetEmpty();
		return true;
	}
}

bool FArcClimateBaker::Bake(AArcClimateVolume& Volume, FText& OutError)
{
	return ArcClimateBakerPrivate::BuildAndPublish(Volume, OutError);
}
