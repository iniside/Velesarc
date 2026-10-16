// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTerrainTypes.h"

/** One of the three independent on-disk tile rails. Carried IN the header as well as in the file
 *  extension, so a file renamed or copied into another rail is rejected by its content. */
enum class EArcTerrainRail : uint8
{
	Height = 0,
	Weight = 1,
	Data = 2,
};

namespace ArcTerrain
{
	inline constexpr int32 NumTileRails = 3;

	/**
	 * Integer division that actually floors. FMath::DivideAndRoundDown is `return Dividend / Divisor;`
	 * (Engine/Source/Runtime/Core/Public/Math/UnrealMathUtility.h:699-704, read 2026-08-07), which
	 * truncates toward zero and therefore rounds down only for a non-negative dividend. Every lattice key
	 * on this plugin's rails is signed and the terrain straddles the origin, so the engine helper maps the
	 * whole negative side one unit too high.
	 */
	inline constexpr int64 FloorDiv(int64 Dividend, int64 Divisor)
	{
		const int64 Quotient = Dividend / Divisor;
		return (Dividend % Divisor != 0 && (Dividend < 0) != (Divisor < 0)) ? Quotient - 1 : Quotient;
	}
}

/**
 * Lattice one tile file is written on. Height and data resolve from FArcTerrainConfig alone; the weight
 * rail also needs the runtime-resolved cell weight quads, which the config cannot produce (the native
 * source dimension and the slice ceiling only ever lower it).
 */
struct FArcTerrainTileGrid
{
	int32 TileQuads = 0;

	/** Zero means the tile is narrower than a cell, i.e. the tile >= cell constraint is violated. It is
	 *  never used as a divisor and never floored to 1 — a grid carrying it is invalid, so the rail
	 *  refuses the tile instead of resolving to a plausible substitute. */
	int32 CellsPerTileEdge = 0;

	int32 TileExtentM = 0;
	int32 TexelSizeCm = 0;

	/** Texels per tile edge: the quads plus the 1-before / 2-after apron. */
	int32 Resolution() const { return TileQuads > 0 ? TileQuads + 3 : 0; }

	bool IsValid() const
	{
		return TileQuads > 0 && TileQuads <= ArcTerrain::MaxTileQuads && CellsPerTileEdge > 0
			&& TileExtentM > 0 && TexelSizeCm > 0;
	}
};

struct FArcTerrainTileHeader
{
	uint32 Version = 0;

	EArcTerrainRail Rail = EArcTerrainRail::Height;

	uint32 TileExtentM = 0;

	uint32 TexelSizeCm = 0;

	/** Texels per edge AS WRITTEN — the authority on whether the file still matches the configuration.
	 *  Never re-derived at read time; a mismatch is a miss, not an error. */
	int32 Resolution = 0;

	uint8 CompressionMethod = 0;

	uint32 CompressedBodyBytes = 0;

	/** Derivable from Resolution and the plane count, carried so the decode buffer is sized and
	 *  cross-checked from the file itself. */
	uint32 UncompressedBodyBytes = 0;

	/** 0 IS A LEGAL HASH — absence is the array's length, never a value. */
	TArray<uint32> PlaneIds;

	/** Data rail ONLY: EArcTerrainDataMap per plane, unique, in body order. A separate array from
	 *  PlaneIds because this rail's identity is the mask enum, not a channel name. */
	TArray<uint8> DataMapIds;

	TArray<float> PlaneQuantMin;
	TArray<float> PlaneQuantMax;

	ARCTERRAIN_API bool IsValid() const;
};

namespace ArcTerrainTileFormat
{
	/** 'A','T','I','L' in file order. */
	inline constexpr uint32 TileMagic = uint32('A') | (uint32('T') << 8) | (uint32('I') << 16) | (uint32('L') << 24);

	inline constexpr uint32 TileVersion = 8;

	inline constexpr int32 FixedHeaderBytes = 32;

	/** Leading bytes PeekTileVersion needs: the magic and the version that follows it. */
	inline constexpr int32 VersionPeekBytes = 8;

	inline constexpr uint8 CompressionNone = 0;
	inline constexpr uint8 CompressionOodleKraken = 1;

	inline constexpr int32 HeightPlaneCount = 1;

	inline constexpr int32 MaxPlaneCount = 255;

	ARCTERRAIN_API int32 BytesPerTexel(EArcTerrainRail Rail);

	/** Entries in the rail's OWN id array: PlaneIds on the weight rail, DataMapIds on the data rail, 0 on
	 *  the height rail. */
	ARCTERRAIN_API int32 NumPlaneIds(const FArcTerrainTileHeader& Header);

	ARCTERRAIN_API int32 NumBodyPlanes(const FArcTerrainTileHeader& Header);

	/** Bytes of one body plane, or 0 when the header is unusable. */
	ARCTERRAIN_API int64 PlaneBytes(const FArcTerrainTileHeader& Header);

	/** Byte offset of a body plane from the start of the DECOMPRESSED BODY, or INDEX_NONE when out of
	 *  range. */
	ARCTERRAIN_API int64 PlaneBodyOffset(const FArcTerrainTileHeader& Header, int32 PlaneIndex);

	ARCTERRAIN_API int64 HeaderBytes(const FArcTerrainTileHeader& Header);

	ARCTERRAIN_API int64 ExpectedFileBytes(const FArcTerrainTileHeader& Header);

	ARCTERRAIN_API void SerializeHeader(const FArcTerrainTileHeader& Header, TArray<uint8>& OutBytes);

	ARCTERRAIN_API bool ParseHeader(TConstArrayView<uint8> FileBytes, FArcTerrainTileHeader& OutHeader);

	/** Version out of a tile's leading bytes WITHOUT validating anything else, so a file written under a
	 *  version this build cannot parse still identifies itself. False — OutVersion untouched — when the
	 *  bytes are short or the magic is not ours. */
	ARCTERRAIN_API bool PeekTileVersion(TConstArrayView<uint8> LeadingBytes, uint32& OutVersion);

	ARCTERRAIN_API bool IsCompatible(const FArcTerrainTileHeader& Header, EArcTerrainRail ExpectedRail,
		const FArcTerrainTileGrid& ExpectedGrid);

	ARCTERRAIN_API FArcTerrainTileHeader MakeWeightHeader(const FArcTerrainTileGrid& Grid,
		TConstArrayView<uint32> PlaneIds);

	/** The height rail's header, carrying the tile's own quantization window. A window that is not finite
	 *  or runs backwards yields an invalid header rather than a file whose decode is a guess. */
	ARCTERRAIN_API FArcTerrainTileHeader MakeHeightHeader(const FArcTerrainTileGrid& Grid, float QuantMin,
		float QuantMax);

	/** The one convention the height body has, written here and read by DecodeHeightTexels: a
	 *  little-endian uint16 per texel, decoding as QuantMin + (Texel / 65535) * (QuantMax - QuantMin). */
	ARCTERRAIN_API void EncodeHeightTexels(TConstArrayView<float> Heights, float QuantMin, float QuantMax,
		TArray<uint8>& OutBody);

	ARCTERRAIN_API void DecodeHeightTexels(TConstArrayView<uint8> Body, float QuantMin, float QuantMax,
		TArrayView<float> OutHeights);

	/** The data rail's header, carrying one quantization window per plane. Sizes that disagree with
	 *  DataMapIds yield an invalid header rather than a file whose decode is a guess. */
	ARCTERRAIN_API FArcTerrainTileHeader MakeDataHeader(const FArcTerrainTileGrid& Grid,
		TConstArrayView<uint8> DataMapIds, TConstArrayView<float> QuantMin, TConstArrayView<float> QuantMax);

	ARCTERRAIN_API EArcTerrainExtent RailTileExtent(const FArcTerrainConfig& Config, EArcTerrainRail Rail);

	ARCTERRAIN_API FArcTerrainTileGrid HeightTileGrid(const FArcTerrainConfig& Config, int32 CumulativeShift);

	ARCTERRAIN_API FArcTerrainTileGrid DataTileGrid(const FArcTerrainConfig& Config);

	/** ResolvedCellWeightQuads is the TIER'S texels per cell edge (FResolvedWeightTier::CellWeightQuads):
	 *  TileQuads is that times the shifted CellsPerTileEdge, so the base count here scales the tile raster. */
	ARCTERRAIN_API FArcTerrainTileGrid WeightTileGrid(const FArcTerrainConfig& Config,
		int32 ResolvedCellWeightQuads, int32 CumulativeShift);

	/** False, OutKey untouched, when the grid is unusable — the caller has no tile, rather than a tile at a
	 *  fabricated key. The pitch is Grid.TileExtentM, which already carries the tier's shift. */
	ARCTERRAIN_API bool TileKeyFromLocal(const FArcTerrainTileGrid& Grid, FVector2D Local, FIntPoint& OutKey);
}
