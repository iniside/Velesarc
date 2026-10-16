// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/IntRect.h"

struct FArcEcotopeSiteMetadata;

/** Row-major coordinate of a simulation chunk. */
struct ARCECOTOPE_API FArcEcotopeChunkId
{
	int32 X = INDEX_NONE;
	int32 Y = INDEX_NONE;

	bool operator==(const FArcEcotopeChunkId& Other) const
	{
		return X == Other.X && Y == Other.Y;
	}

	bool IsValid() const
	{
		return X >= 0 && Y >= 0;
	}
};

/**
 * The immutable spatial extent owned by one chunk.
 *
 * All rectangles use FIntRect's half-open [Min, Max) convention. Core rectangles
 * partition their corresponding grids exactly: a core cell has one and only one
 * writer. Read rectangles are core rectangles expanded by the configured halo and
 * clamped only at the site boundary; they never define write ownership.
 */
struct ARCECOTOPE_API FArcEcotopeChunkRect
{
	FArcEcotopeChunkId Id;
	FIntRect BaseCoreRect;
	FIntRect BaseReadRect;
	FIntRect RecordCoreRect;
	FIntRect RecordReadRect;
};

/**
 * Read-only partition of a frozen site into aligned base-grid chunk cores.
 *
 * Build requires internal chunk boundaries to align to the record-grid divisor.
 * The final edge may contain the site's existing partial trailing record cell;
 * that cell belongs wholly to the final chunk, matching Site.GetLevelDimensions().
 */
class ARCECOTOPE_API FArcEcotopeChunkLayout
{
public:
	/**
	 * Builds a row-major chunk layout without allocating simulation working state.
	 * CoreBaseCellSpan must be positive and divisible by 1 << RecordGridLevel.
	 * RecordHaloCells is in record-grid cells and may be zero.
	 */
	static bool Build(
		const FArcEcotopeSiteMetadata& Site,
		int32 CoreBaseCellSpan,
		int32 RecordHaloCells,
		FArcEcotopeChunkLayout& OutLayout,
		FString& OutError);

	int32 GetChunkCount() const { return Chunks.Num(); }
	FIntPoint GetChunkGridDimensions() const { return ChunkGridDimensions; }
	FIntPoint GetBaseDimensions() const { return BaseDimensions; }
	FIntPoint GetRecordDimensions() const { return RecordDimensions; }
	int32 GetCoreBaseCellSpan() const { return CoreBaseCellSpan; }
	int32 GetCoreRecordCellSpan() const { return CoreRecordCellSpan; }
	int32 GetRecordHaloCells() const { return RecordHaloCells; }
	/** Row-major immutable descriptors. Indices are Id.X + Id.Y * GetChunkGridDimensions().X. */
	TConstArrayView<FArcEcotopeChunkRect> GetChunks() const { return Chunks; }

	/** Returns the descriptor for a row-major index; resets OutChunk and returns false for an invalid index. */
	bool GetChunk(int32 ChunkIndex, FArcEcotopeChunkRect& OutChunk) const;

	/** Returns the descriptor for an in-bounds chunk coordinate; resets OutChunk and returns false otherwise. */
	bool GetChunk(const FArcEcotopeChunkId& Id, FArcEcotopeChunkRect& OutChunk) const;

	/** Resolves the single core owner of an in-bounds base-grid coordinate; resets OutId on failure. */
	bool FindChunkForBaseCell(const FIntPoint& BaseCell, FArcEcotopeChunkId& OutId) const;

	/** Resolves the single core owner of an in-bounds record-grid coordinate; resets OutId on failure. */
	bool FindChunkForRecordCell(const FIntPoint& RecordCell, FArcEcotopeChunkId& OutId) const;

private:
	bool TryGetChunkIndex(const FArcEcotopeChunkId& Id, int32& OutChunkIndex) const;

	FIntPoint BaseDimensions = FIntPoint::ZeroValue;
	FIntPoint RecordDimensions = FIntPoint::ZeroValue;
	FIntPoint ChunkGridDimensions = FIntPoint::ZeroValue;
	int32 CoreBaseCellSpan = 0;
	int32 CoreRecordCellSpan = 0;
	int32 RecordHaloCells = 0;
	TArray<FArcEcotopeChunkRect> Chunks;
};
