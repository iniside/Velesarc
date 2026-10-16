// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTerrainTileFormat.h"
#include "IO/IoHash.h"

/**
 * Which tile keys a terrain directory holds, per rail, and the recipe key each one's bytes were written
 * under. A DERIVATIVE OF THE DISK, never the truth: an entry whose file is gone resolves to a miss, and a
 * file with no entry means the manifest is stale and gets rebuilt. Neither divergence is an error.
 *
 * The three rails are independent — deleting every height tile leaves the weight and data sets intact.
 */
struct FArcTerrainTileManifest
{
	static FIntVector MakeKey(int32 Tier, FIntPoint TileKey) { return FIntVector(Tier, TileKey.X, TileKey.Y); }

	TMap<FIntVector, FIoHash> HeightTiles;
	TMap<FIntVector, FIoHash> WeightTiles;
	TMap<FIntVector, FIoHash> DataTiles;

	ARCTERRAIN_API TMap<FIntVector, FIoHash>& Rail(EArcTerrainRail InRail);
	ARCTERRAIN_API const TMap<FIntVector, FIoHash>& Rail(EArcTerrainRail InRail) const;

	bool Contains(EArcTerrainRail InRail, int32 Tier, FIntPoint TileKey) const
	{
		return Rail(InRail).Contains(MakeKey(Tier, TileKey));
	}

	void Add(EArcTerrainRail InRail, int32 Tier, FIntPoint TileKey, const FIoHash& RecipeKey)
	{
		Rail(InRail).Add(MakeKey(Tier, TileKey), RecipeKey);
	}

	/** Zero when the rail has no entry for the tile, and equally when the entry carries no key. */
	FIoHash FindRecipeKey(EArcTerrainRail InRail, int32 Tier, FIntPoint TileKey) const
	{
		const FIoHash* Found = Rail(InRail).Find(MakeKey(Tier, TileKey));
		return Found != nullptr ? *Found : FIoHash();
	}

	void Remove(EArcTerrainRail InRail, int32 Tier, FIntPoint TileKey)
	{
		Rail(InRail).Remove(MakeKey(Tier, TileKey));
	}

	int32 Num() const { return HeightTiles.Num() + WeightTiles.Num() + DataTiles.Num(); }

	ARCTERRAIN_API void Reset();
};

namespace ArcTerrainTileManifest
{
	inline constexpr const TCHAR* ManifestFileName = TEXT("Tiles.arcidx");

	ARCTERRAIN_API FString MakeManifestPath(const FString& TileDir);

#if WITH_EDITOR
	/** Atomic, like every tile write — a manifest entry and its recipe key are only ever published after
	 *  the file is in place. */
	ARCTERRAIN_API bool Save(const FString& TileDir, const FArcTerrainTileManifest& Manifest);
#endif

	/** False (Manifest reset) when the file is absent or unreadable, which asks for RebuildFromDisk. */
	ARCTERRAIN_API bool Load(const FString& TileDir, FArcTerrainTileManifest& OutManifest);

	/** Three IFileManager::FindFiles, one per rail: the manifest is regenerated from the file names
	 *  alone, without opening a tile — so every entry it produces carries a zero recipe key. */
	ARCTERRAIN_API void RebuildFromDisk(const FString& TileDir, FArcTerrainTileManifest& OutManifest);
}
