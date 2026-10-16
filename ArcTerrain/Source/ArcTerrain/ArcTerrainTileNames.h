// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTerrainTileFormat.h"

class AArcTerrain;

/**
 * Deterministic tile file naming and the two directories a tile rail addresses: where the editor WRITES
 * tiles and where any world — including a PIE duplicate, which never writes — READS them.
 */
namespace ArcTerrainTileNames
{
	/** "TileH_" / "TileW_" / "TileD_". */
	ARCTERRAIN_API const TCHAR* RailFilePrefix(EArcTerrainRail Rail);

	/** ".arch" / ".arcw" / ".arcd". The rail is also in the header, so the extension is a lookup key, not
	 *  a trust boundary. */
	ARCTERRAIN_API const TCHAR* RailFileExtension(EArcTerrainRail Rail);

	/** "TileH_<X>_<Y>.arch" at tier 0, "TileH_T<Tier>_<X>_<Y>.arch" above it; negatives encoded as 'N' +
	 *  magnitude ("TileH_N1_2.arch"). Tier 0 keeps the unadorned name, so the existing bake stays addressed. */
	ARCTERRAIN_API FString MakeTileFileName(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey);

	/** "TileH_*.arch" — the pattern RebuildFromDisk hands to IFileManager::FindFiles; it spans every tier. */
	ARCTERRAIN_API FString MakeTileFileWildcard(EArcTerrainRail Rail);

	/** Inverse of MakeTileFileName, strict: a name that is not exactly what this module writes is
	 *  rejected rather than coerced into a key. Both forms are accepted and OutTier says which. */
	ARCTERRAIN_API bool ParseTileFileName(EArcTerrainRail Rail, const FString& FileName, int32& OutTier,
		FIntPoint& OutTileKey);

	/** "<MapFileDir>/<MapName>_ArcTerrain/<TerrainObjectName>" — tiles live next to the map that owns the
	 *  terrain, one subfolder per terrain actor, keyed on the terrain's object name.
	 *
	 *  The map package is resolved via Terrain.GetLevel()->GetPackage(), NOT Terrain.GetPackage(): under
	 *  World Partition the actor's own package is its __ExternalActors__ package.
	 *
	 *  Empty — "this terrain must not have tiles written for it" — when the terrain has no level package,
	 *  when the map is UNSAVED (/Temp/Untitled_N is a real mount point and would otherwise convert to a
	 *  path under Saved/), or when the map package is a PIE duplicate. */
	ARCTERRAIN_API FString MakeTileDirForWrite(const AArcTerrain& Terrain);

	/** As MakeTileDirForWrite, but strips the UEDPIE_<N>_ prefix instead of refusing: a PIE world is a
	 *  throwaway duplicate that must still stream the tiles its source map owns. */
	ARCTERRAIN_API FString MakeTileDirForRead(const AArcTerrain& Terrain);
}
