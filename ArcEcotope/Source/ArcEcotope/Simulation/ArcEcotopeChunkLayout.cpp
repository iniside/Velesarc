// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcEcotopeChunkLayout.h"

#include "Simulation/ArcEcotopeSiteMetadata.h"

namespace ArcEcotopeChunkLayoutPrivate
{
	bool IsPositiveInt32Product(const int32 A, const int32 B, int32& OutProduct)
	{
		const int64 Product = static_cast<int64>(A) * static_cast<int64>(B);
		if (Product <= 0 || Product > MAX_int32)
		{
			return false;
		}

		OutProduct = static_cast<int32>(Product);
		return true;
	}

	bool IsInBounds(const FIntPoint& Cell, const FIntPoint& Dimensions)
	{
		return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < Dimensions.X && Cell.Y < Dimensions.Y;
	}

	FIntRect MakeExpandedRect(const FIntRect& CoreRect, const FIntPoint& Dimensions, const int32 Halo)
	{
		const auto ClampCoordinate = [](const int64 Coordinate, const int32 Dimension)
		{
			return static_cast<int32>(FMath::Clamp<int64>(Coordinate, 0, static_cast<int64>(Dimension)));
		};
		return FIntRect(
			ClampCoordinate(static_cast<int64>(CoreRect.Min.X) - Halo, Dimensions.X),
			ClampCoordinate(static_cast<int64>(CoreRect.Min.Y) - Halo, Dimensions.Y),
			ClampCoordinate(static_cast<int64>(CoreRect.Max.X) + Halo, Dimensions.X),
			ClampCoordinate(static_cast<int64>(CoreRect.Max.Y) + Halo, Dimensions.Y));
	}
}

bool FArcEcotopeChunkLayout::Build(
	const FArcEcotopeSiteMetadata& Site,
	const int32 InCoreBaseCellSpan,
	const int32 InRecordHaloCells,
	FArcEcotopeChunkLayout& OutLayout,
	FString& OutError)
{
	OutLayout = FArcEcotopeChunkLayout();
	OutError.Reset();

	if (InCoreBaseCellSpan <= 0)
	{
		OutError = TEXT("ArcEcotope chunk core base-cell span must be greater than zero.");
		return false;
	}
	if (InRecordHaloCells < 0)
	{
		OutError = TEXT("ArcEcotope chunk record-cell halo must not be negative.");
		return false;
	}
	if (Site.RecordGridLevel > FArcEcotopeSiteMetadata::MaxGridLevel)
	{
		OutError = TEXT("ArcEcotope chunk layout received an unsupported record grid level.");
		return false;
	}

	const int64 RecordDivisor64 = 1ll << Site.RecordGridLevel;
	if (RecordDivisor64 <= 0 || RecordDivisor64 > MAX_int32)
	{
		OutError = TEXT("ArcEcotope chunk layout record-grid divisor is not representable.");
		return false;
	}
	const int32 RecordDivisor = static_cast<int32>(RecordDivisor64);
	if (InCoreBaseCellSpan % RecordDivisor != 0)
	{
		OutError = FString::Printf(
			TEXT("ArcEcotope chunk core span %d must be divisible by record-grid divisor %d."),
			InCoreBaseCellSpan,
			RecordDivisor);
		return false;
	}

	const FIntPoint InBaseDimensions = Site.GridDimensions;
	if (InBaseDimensions.X <= 0 || InBaseDimensions.Y <= 0)
	{
		OutError = TEXT("ArcEcotope chunk layout requires positive base dimensions.");
		return false;
	}

	int32 BaseCellCount = 0;
	if (!ArcEcotopeChunkLayoutPrivate::IsPositiveInt32Product(InBaseDimensions.X, InBaseDimensions.Y, BaseCellCount))
	{
		OutError = TEXT("ArcEcotope chunk layout base-cell count is not representable.");
		return false;
	}

	const FIntPoint InRecordDimensions = Site.GetLevelDimensions(Site.RecordGridLevel);
	const FIntPoint ExpectedRecordDimensions(
		static_cast<int32>((static_cast<int64>(InBaseDimensions.X) + RecordDivisor - 1) / RecordDivisor),
		static_cast<int32>((static_cast<int64>(InBaseDimensions.Y) + RecordDivisor - 1) / RecordDivisor));
	if (InRecordDimensions != ExpectedRecordDimensions)
	{
		OutError = TEXT("ArcEcotope chunk layout base and record-grid dimensions are not aligned.");
		return false;
	}

	int32 RecordCellCount = 0;
	if (!ArcEcotopeChunkLayoutPrivate::IsPositiveInt32Product(InRecordDimensions.X, InRecordDimensions.Y, RecordCellCount))
	{
		OutError = TEXT("ArcEcotope chunk layout record-cell count is not representable.");
		return false;
	}

	const int64 ChunkGridX64 = (static_cast<int64>(InBaseDimensions.X) + InCoreBaseCellSpan - 1) / InCoreBaseCellSpan;
	const int64 ChunkGridY64 = (static_cast<int64>(InBaseDimensions.Y) + InCoreBaseCellSpan - 1) / InCoreBaseCellSpan;
	if (ChunkGridX64 <= 0 || ChunkGridY64 <= 0 || ChunkGridX64 > MAX_int32 || ChunkGridY64 > MAX_int32)
	{
		OutError = TEXT("ArcEcotope chunk layout grid dimensions are not representable.");
		return false;
	}

	const FIntPoint InChunkGridDimensions(static_cast<int32>(ChunkGridX64), static_cast<int32>(ChunkGridY64));
	int32 ChunkCount = 0;
	if (!ArcEcotopeChunkLayoutPrivate::IsPositiveInt32Product(InChunkGridDimensions.X, InChunkGridDimensions.Y, ChunkCount))
	{
		OutError = TEXT("ArcEcotope chunk layout chunk count is not representable.");
		return false;
	}

	const int32 InCoreRecordCellSpan = InCoreBaseCellSpan / RecordDivisor;
	const int64 BaseHaloCells64 = static_cast<int64>(InRecordHaloCells) * RecordDivisor;
	if (BaseHaloCells64 > MAX_int32)
	{
		OutError = TEXT("ArcEcotope chunk layout base-grid read halo is not representable.");
		return false;
	}
	const int32 BaseHaloCells = static_cast<int32>(BaseHaloCells64);

	FArcEcotopeChunkLayout NewLayout;
	NewLayout.BaseDimensions = InBaseDimensions;
	NewLayout.RecordDimensions = InRecordDimensions;
	NewLayout.ChunkGridDimensions = InChunkGridDimensions;
	NewLayout.CoreBaseCellSpan = InCoreBaseCellSpan;
	NewLayout.CoreRecordCellSpan = InCoreRecordCellSpan;
	NewLayout.RecordHaloCells = InRecordHaloCells;
	NewLayout.Chunks.Reserve(ChunkCount);

	for (int32 ChunkY = 0; ChunkY < InChunkGridDimensions.Y; ++ChunkY)
	{
		for (int32 ChunkX = 0; ChunkX < InChunkGridDimensions.X; ++ChunkX)
		{
			const int64 BaseMinX64 = static_cast<int64>(ChunkX) * InCoreBaseCellSpan;
			const int64 BaseMinY64 = static_cast<int64>(ChunkY) * InCoreBaseCellSpan;
			const int64 BaseMaxX64 = FMath::Min(BaseMinX64 + InCoreBaseCellSpan, static_cast<int64>(InBaseDimensions.X));
			const int64 BaseMaxY64 = FMath::Min(BaseMinY64 + InCoreBaseCellSpan, static_cast<int64>(InBaseDimensions.Y));
			if (BaseMinX64 < 0 || BaseMinY64 < 0 || BaseMaxX64 <= BaseMinX64 || BaseMaxY64 <= BaseMinY64
				|| BaseMaxX64 > MAX_int32 || BaseMaxY64 > MAX_int32
				|| BaseMinX64 % RecordDivisor != 0 || BaseMinY64 % RecordDivisor != 0
				|| (BaseMaxX64 != InBaseDimensions.X && BaseMaxX64 % RecordDivisor != 0)
				|| (BaseMaxY64 != InBaseDimensions.Y && BaseMaxY64 % RecordDivisor != 0))
			{
				OutError = TEXT("ArcEcotope chunk layout produced a core that does not align to the record grid.");
				return false;
			}

			FArcEcotopeChunkRect& Chunk = NewLayout.Chunks.AddDefaulted_GetRef();
			Chunk.Id.X = ChunkX;
			Chunk.Id.Y = ChunkY;
			Chunk.BaseCoreRect = FIntRect(
				static_cast<int32>(BaseMinX64), static_cast<int32>(BaseMinY64),
				static_cast<int32>(BaseMaxX64), static_cast<int32>(BaseMaxY64));
			Chunk.BaseReadRect = ArcEcotopeChunkLayoutPrivate::MakeExpandedRect(
				Chunk.BaseCoreRect, InBaseDimensions, BaseHaloCells);
			Chunk.RecordCoreRect = FIntRect(
				Chunk.BaseCoreRect.Min.X / RecordDivisor, Chunk.BaseCoreRect.Min.Y / RecordDivisor,
				static_cast<int32>((BaseMaxX64 + RecordDivisor - 1) / RecordDivisor),
				static_cast<int32>((BaseMaxY64 + RecordDivisor - 1) / RecordDivisor));
			Chunk.RecordReadRect = ArcEcotopeChunkLayoutPrivate::MakeExpandedRect(
				Chunk.RecordCoreRect, InRecordDimensions, InRecordHaloCells);
		}
	}

	if (NewLayout.Chunks.Num() != ChunkCount)
	{
		OutError = TEXT("ArcEcotope chunk layout did not create every planned chunk.");
		return false;
	}

	OutLayout = MoveTemp(NewLayout);
	return true;
}

bool FArcEcotopeChunkLayout::GetChunk(const int32 ChunkIndex, FArcEcotopeChunkRect& OutChunk) const
{
	OutChunk = FArcEcotopeChunkRect();
	if (!Chunks.IsValidIndex(ChunkIndex))
	{
		return false;
	}

	OutChunk = Chunks[ChunkIndex];
	return true;
}

bool FArcEcotopeChunkLayout::GetChunk(const FArcEcotopeChunkId& Id, FArcEcotopeChunkRect& OutChunk) const
{
	OutChunk = FArcEcotopeChunkRect();
	int32 ChunkIndex = INDEX_NONE;
	return TryGetChunkIndex(Id, ChunkIndex) && GetChunk(ChunkIndex, OutChunk);
}

bool FArcEcotopeChunkLayout::FindChunkForBaseCell(const FIntPoint& BaseCell, FArcEcotopeChunkId& OutId) const
{
	OutId = FArcEcotopeChunkId();
	if (!ArcEcotopeChunkLayoutPrivate::IsInBounds(BaseCell, BaseDimensions) || CoreBaseCellSpan <= 0)
	{
		return false;
	}

	const FArcEcotopeChunkId CandidateId{BaseCell.X / CoreBaseCellSpan, BaseCell.Y / CoreBaseCellSpan};
	int32 ChunkIndex = INDEX_NONE;
	if (!TryGetChunkIndex(CandidateId, ChunkIndex))
	{
		return false;
	}
	OutId = CandidateId;
	return true;
}

bool FArcEcotopeChunkLayout::FindChunkForRecordCell(const FIntPoint& RecordCell, FArcEcotopeChunkId& OutId) const
{
	OutId = FArcEcotopeChunkId();
	if (!ArcEcotopeChunkLayoutPrivate::IsInBounds(RecordCell, RecordDimensions) || CoreRecordCellSpan <= 0)
	{
		return false;
	}

	const FArcEcotopeChunkId CandidateId{RecordCell.X / CoreRecordCellSpan, RecordCell.Y / CoreRecordCellSpan};
	int32 ChunkIndex = INDEX_NONE;
	if (!TryGetChunkIndex(CandidateId, ChunkIndex))
	{
		return false;
	}
	OutId = CandidateId;
	return true;
}

bool FArcEcotopeChunkLayout::TryGetChunkIndex(const FArcEcotopeChunkId& Id, int32& OutChunkIndex) const
{
	if (Id.X < 0 || Id.Y < 0 || Id.X >= ChunkGridDimensions.X || Id.Y >= ChunkGridDimensions.Y)
	{
		return false;
	}

	const int64 ChunkIndex64 = static_cast<int64>(Id.X) + static_cast<int64>(Id.Y) * ChunkGridDimensions.X;
	if (ChunkIndex64 < 0 || ChunkIndex64 >= Chunks.Num() || ChunkIndex64 > MAX_int32)
	{
		return false;
	}

	OutChunkIndex = static_cast<int32>(ChunkIndex64);
	return true;
}
