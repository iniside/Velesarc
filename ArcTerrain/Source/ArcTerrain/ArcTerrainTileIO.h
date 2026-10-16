// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTerrainTileFormat.h"

/**
 * Byte-level tile file access. No UObject is touched and nothing here assumes the game thread — the
 * caller resolves the directory (ArcTerrainTileNames) on the game thread and reads on a task.
 *
 * Writes are atomic: the bytes land under a temporary name in the destination directory and are moved
 * into place only once complete, so a reader never observes a half-written tile and the manifest entry
 * can be added strictly AFTER a successful move.
 */
/** Outcome of a tile read. The three failures are NOT interchangeable: only FileMissing says the manifest
 *  names a tile the disk does not have, and only Unusable is permanent for this lattice. */
enum class EArcTerrainTileLoad : uint8
{
	Loaded,
	FileMissing,
	/** The file is there and the bytes did not open. TRANSIENT — a lock, a scanner, a Move still landing —
	 *  so the key is left exactly as it was and the next pass asks again. */
	ReadFailed,
	/** The file is there and DID NOT DECODE as this tile: bad magic, another rail's, or written on a
	 *  lattice this configuration no longer asks for. The same bytes decode the same way every time, so
	 *  the key is suppressed until a rewrite. */
	Unusable,
};

namespace ArcTerrainTileIO
{
	ARCTERRAIN_API bool SaveFileAtomic(const FString& FilePath, TConstArrayView<uint8> FileBytes);

	/** Returns the file bytes actually written, or 0 when the tile was refused or the write failed. */
	ARCTERRAIN_API int64 SaveTile(const FString& FilePath, const FArcTerrainTileHeader& Header,
		TConstArrayView<uint8> Body);

	ARCTERRAIN_API bool LoadFileBytes(const FString& FilePath, TArray<uint8>& OutFileBytes);

	/**
	 * Whole-file read plus structural and configuration validation. Anything other than Loaded leaves
	 * OutHeader and OutFileBytes untouched and is not an error — the tile is absent and gets re-baked.
	 */
	ARCTERRAIN_API EArcTerrainTileLoad LoadTile(const FString& FilePath, EArcTerrainRail ExpectedRail,
		const FArcTerrainTileGrid& ExpectedGrid, FArcTerrainTileHeader& OutHeader, TArray<uint8>& OutFileBytes);
}
