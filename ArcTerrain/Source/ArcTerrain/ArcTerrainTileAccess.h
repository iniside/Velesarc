// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTerrainTileFormat.h"
#include "Render/ArcTerrainDensityTiers.h"
#include "IO/IoHash.h"

class AArcTerrain;

/**
 * One tile file, loaded. Plain data: the parsed header plus its body decoded per rail. Held by
 * TSharedPtr and never by the GC — no UObject exists for a tile.
 */
struct FArcTerrainTilePayload
{
	FArcTerrainTileHeader Header;

	/** Height rail: Resolution^2 floats. Empty on the other two rails. */
	TArray<float> Floats;

	TArray<TArray<uint8>> Planes;

	FIntPoint TileKey = FIntPoint::ZeroValue;

	EArcTerrainRail Rail = EArcTerrainRail::Height;

	/** Monotonic across the process, assigned at every load. Pointer identity does NOT answer "is this
	 *  still the tile I read": a released payload's address is handed straight back to the next one, so a
	 *  consumer keyed on the address would miss the swap. */
	uint32 PayloadId = 0;

	int32 Resolution() const { return Header.Resolution; }

	uint64 RetainedBytes() const
	{
		uint64 Bytes = static_cast<uint64>(Floats.Num()) * sizeof(float);
		for (const TArray<uint8>& Plane : Planes)
		{
			Bytes += static_cast<uint64>(Plane.Num());
		}
		return Bytes;
	}

	/** Body index of the weight plane carrying ChannelNameHash, INDEX_NONE when this tile carries none. */
	ARCTERRAIN_API int32 FindWeightPlane(uint32 ChannelNameHash) const;

	/** Body index of the data plane carrying DataMapId, INDEX_NONE when this tile carries none. */
	ARCTERRAIN_API int32 FindDataPlane(uint8 DataMapId) const;
};

using FArcTerrainTilePayloadPtr = TSharedPtr<const FArcTerrainTilePayload, ESPMode::ThreadSafe>;

struct FArcTerrainTileStoreMutationCounters
{
	uint64 SyncCacheHits = 0;
	uint64 SyncPhysicalAttempts = 0;
	uint64 SyncPublications = 0;
	uint64 SyncFailures = 0;
	uint64 AsyncPublications = 0;
	uint64 AsyncFailures = 0;
	uint64 Releases = 0;
};

/** What the store holds for one terrain right now, per rail. Diagnostics only. */
struct FArcTerrainTileStoreStats
{
	int32 Loaded[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers] = {};
	int32 InFlight[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers] = {};
	int32 Unusable[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers] = {};
	int32 ManifestEntries[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers] = {};
	uint64 LoadedMutationSerial = 0;
	uint32 ProcessLoadedCoverageEpoch = 0;
	FArcTerrainTileStoreMutationCounters Interval;
	FArcTerrainTileStoreMutationCounters Lifetime;

	/** False means ManifestEntries is not an answer: the caller refused the disk read and nothing else had
	 *  loaded the manifest yet. */
	bool bManifestLoaded = false;
};

/** What the store says about ONE tile key of one rail, in the order the store is consulted. Diagnostics
 *  only — the spatial overlay's per-tile colour. */
enum class EArcTerrainTileResidency : uint8
{
	/** The payload is in memory. */
	Resident = 0,
	/** A read is in flight for it. */
	InFlight = 1,
	/** A read decoded it against a lattice it does not match; nothing will publish it until it is rewritten. */
	Unusable = 2,
	/** The manifest names it and nothing has brought it in. */
	InManifest = 3,
	/** The manifest is loaded and does not name it: there is no baked tile here. */
	Absent = 4,
	/** The manifest is NOT in memory, so InManifest and Absent cannot be told apart. */
	Unknown = 5,
};

/** One manifest entry of one rail: ground that HAS a baked tile, whatever the camera is near. */
struct FArcTerrainManifestTile
{
	FIntPoint TileKey = FIntPoint::ZeroValue;

	/** A read decoded this file against a lattice it does not match, so nothing will publish it until it is
	 *  rewritten: the entry names bytes on disk that this configuration cannot use. */
	bool bUnusable = false;
};

/** Everything one tile key's file and bookkeeping say — the per-point diagnostic's answer for one rail.
 *  READS THE FILE, so it is a console-command path and never a per-tick one. */
struct FArcTerrainTileProbe
{
	FArcTerrainTileGrid Grid;

	FArcTerrainTileHeader Header;

	FString FilePath;

	/** Filled only when the file parsed but does not match Grid: which header fields differ, and how. */
	FString Mismatch;

	int64 FileBytes = 0;

	bool bInManifest = false;
	bool bFileExists = false;
	bool bParsed = false;
	bool bCompatible = false;
	bool bLoaded = false;
	bool bSuppressedUnusable = false;
};

/**
 * THE read side of the tile rails: bytes in, bytes out. The rail is a parameter, never three copies of
 * one API, and nothing here hands out a GPU texture — a consumer that needs one builds it from these
 * bytes itself.
 *
 * Two states, and both are legal: a tile the manifest does not know is ground the modifier stack still
 * composites, and a tile it knows but that is not loaded yet is ground that arrives later. Neither is an
 * error and neither logs.
 */
namespace ArcTerrainTileAccess
{
	/** Whether Tier can index anything at all — a bound, not a coordinate that exists. */
	inline bool IsValidTier(int32 Tier) { return Tier >= 0 && Tier < ArcTerrain::MaxTiers; }

	/** S(Tier) out of the terrain's RESOLVED ladder, INDEX_NONE for a tier the ladder does not carry —
	 *  authored past MaxTiers, or dropped because the atlas funds no slice for it. GAME THREAD ONLY above
	 *  tier 0. Tier 0 is the terrain's own lattice and needs no ladder, so it answers 0 even for a terrain
	 *  the subsystem never saw. */
	ARCTERRAIN_API int32 ResolveHeightTierShift(const AArcTerrain& Terrain, int32 Tier);

	/** The weight ladder's twin of ResolveHeightTierShift, with the same INDEX_NONE contract. */
	ARCTERRAIN_API int32 ResolveWeightTierShift(const AArcTerrain& Terrain, int32 Tier);

	/** The rail's on-disk lattice for this terrain at one tier. False (OutGrid untouched) when the
	 *  configuration cannot produce one — a tile narrower than a cell resolves CellsPerTileEdge to zero,
	 *  and that is a refusal, never a divisor. The data rail has only tier 0. */
	ARCTERRAIN_API bool ResolveGrid(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		FArcTerrainTileGrid& OutGrid);

	ARCTERRAIN_API bool TileKeyFromLocal(const FArcTerrainTileGrid& Grid, FVector2D Local, FIntPoint& OutKey);

	ARCTERRAIN_API bool TileKeyFromCell(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		FArcTerrainCellKey CellKey, FIntPoint& OutKey);

	/** The same map on a lattice the caller already holds. THE ONE TO USE ON A PER-CELL PATH: the overload
	 *  above resolves the grid per call, and the weight rail's resolve scans every stamp of the terrain. */
	ARCTERRAIN_API FIntPoint TileKeyFromCell(const FArcTerrainTileGrid& Grid, FArcTerrainCellKey CellKey);

	/** Whether the terrain's directory holds a tile this configuration can read: the manifest names it and
	 *  no read has already found the file undecodable under the live lattice. GAME THREAD — the manifest is
	 *  loaded from the package-derived directory on first ask.
	 *
	 *  KnownGrid is the caller's already-resolved lattice for the rail, and it is worth passing on the
	 *  weight rail, whose resolve scans every stamp. */
	ARCTERRAIN_API bool HasTile(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey,
		const FArcTerrainTileGrid* KnownGrid = nullptr);

	/** The loaded payload, or null. Never reads the disk: a miss means "that area has not arrived", which
	 *  is what a per-tick game-thread consumer waits one tick on. */
	ARCTERRAIN_API FArcTerrainTilePayloadPtr FindLoadedTile(const AArcTerrain& Terrain, EArcTerrainRail Rail,
		int32 Tier, FIntPoint TileKey);

	/**
	 * Blocking load, publishing into the same cache FindLoadedTile reads. GAME THREAD.
	 *
	 * The stream reads on a task and never here — this is the EXPLICIT exception for the editor bakers
	 * and the collision bootstrap, which ask about areas no invoker radius will ever pull in and are not
	 * on a frame path.
	 */
	ARCTERRAIN_API FArcTerrainTilePayloadPtr LoadTileSync(const AArcTerrain& Terrain, EArcTerrainRail Rail,
		int32 Tier, FIntPoint TileKey);

	/** Keys this rail currently holds in memory. The stream's eviction domain: a tile that has left the
	 *  invoker window is no longer named by it, so only the loaded set can name it. */
	ARCTERRAIN_API void GatherLoadedTileKeys(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		TArray<FIntPoint>& OutKeys);

	/** Every key this rail's manifest names, loaded or not. GAME THREAD — the manifest is read from disk on
	 *  first ask. For the rare terrain-wide re-bake, never for a per-tick test: the manifest holds tens of
	 *  thousands of entries over a 10 km terrain, which is why the stream walks its window instead. */
	ARCTERRAIN_API void GatherManifestTileKeys(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		TArray<FIntPoint>& OutKeys);

	/** Reads on a task and calls OnLoaded ON THE GAME THREAD with the payload, or with null on a miss.
	 *  A second request for a tile already in flight is folded into the first.
	 *
	 *  A read whose tile is rewritten or released before it completes is DISCARDED (OnLoaded gets null):
	 *  its bytes predate the write, and publishing them would cache a tile that no longer exists. */
	ARCTERRAIN_API void RequestTileAsync(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		FIntPoint TileKey, TFunction<void(EArcTerrainRail, int32, FIntPoint, FArcTerrainTilePayloadPtr)> OnLoaded);

	/** Drops the cached payload unless something outside the cache still holds it, and says which happened.
	 *  A tile a baker or a sampler is working through is not the stream's to free: it lies where no invoker
	 *  radius reaches, so the very next tick would read it off disk again. */
	ARCTERRAIN_API bool ReleaseTileIfUnreferenced(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		FIntPoint TileKey);

	/** GAME THREAD. Editor only: writes the manifest if a read found one of its entries has no file. */
	ARCTERRAIN_API void FlushManifest(const AArcTerrain& Terrain);

	ARCTERRAIN_API void ReleaseTerrainTiles(const AArcTerrain& Terrain);

	ARCTERRAIN_API void ReleaseAllTiles();

	/** The recipe key the tile's bytes on disk were written under, zero when the manifest does not know one.
	 *  GAME THREAD — the manifest is read from disk on first ask. */
	ARCTERRAIN_API FIoHash FindTileWrittenKey(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		FIntPoint TileKey);

#if WITH_EDITOR
	/** The writer's half of the manifest: an entry and its recipe key are published only after the bytes are
	 *  in place. The payload the entry replaces is dropped and any read of it still in flight is voided, so
	 *  the tile reads as not-loaded and the stream requests it again on its next pass. */
	ARCTERRAIN_API void NotifyTileWritten(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		FIntPoint TileKey, const FIoHash& RecipeKey);

	ARCTERRAIN_API void NotifyTileDropped(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		FIntPoint TileKey);
#endif

	/** One cell's apron-inclusive floats out of a height payload: PlaneIndex 0 is the heights, the rail's
	 *  only plane. False when the cell lies in another tile or the lattices disagree.
	 *
	 *  TAKES THE GRID, not the terrain: resolving the weight lattice scans every stamp on the terrain,
	 *  and a cutter called per plane per cell must not pay that. The caller resolves it once. */
	ARCTERRAIN_API bool ExtractCellFloats(const FArcTerrainTileGrid& Grid,
		const FArcTerrainTilePayload& Payload, int32 PlaneIndex, FArcTerrainCellKey CellKey,
		TArray<float>& OutFloats, int32& OutResolution);

	/** One cell's apron-inclusive bytes out of one body plane of a weight or data payload. */
	ARCTERRAIN_API bool ExtractCellPlane(const FArcTerrainTileGrid& Grid,
		const FArcTerrainTilePayload& Payload, int32 PlaneIndex, FArcTerrainCellKey CellKey,
		TArray<uint8>& OutBytes, int32& OutResolution);

	/** One WEIGHT GROUP's apron-inclusive bytes out of one body plane, on the group lattice
	 *  (ArcTerrainTileUpload::GroupSubRectInTile). A coarse tier's group covers many cells of the tile it
	 *  is cut from, which is the whole difference from the cell form. */
	ARCTERRAIN_API bool ExtractGroupPlane(const FArcTerrainTileGrid& Grid,
		const FArcTerrainTilePayload& Payload, int32 PlaneIndex, FIntPoint GroupKey, int32 GroupQuads,
		int32 GroupsPerTileEdge, TArray<uint8>& OutBytes, int32& OutResolution);

	/** GAME THREAD. bLoadManifest reads the manifest from disk on first ask, like HasTile; without it the
	 *  entries are reported only when they are already in memory (OutStats.bManifestLoaded says which). */
	ARCTERRAIN_API void GatherStoreStats(const AArcTerrain& Terrain, bool bLoadManifest,
		FArcTerrainTileStoreStats& OutStats);
	ARCTERRAIN_API uint64 GetLoadedMutationSerial(const AArcTerrain& Terrain);
	ARCTERRAIN_API void ResetStoreIntervalStats(const AArcTerrain& Terrain);

	inline constexpr int32 MaxTileResidencyKeys = 4096;

	/** The per-key form of GatherStoreStats over the INCLUSIVE key rectangle [MinKey, MaxKey], row-major
	 *  and (MaxKey.X - MinKey.X + 1) wide. Reads the same four store maps, and NEVER loads the manifest —
	 *  a rail whose manifest is not already in memory reports Unknown rather than paying a disk read behind
	 *  a debug draw. OutStates is emptied when the rectangle is inverted or spans more than
	 *  MaxTileResidencyKeys. GAME THREAD. */
	ARCTERRAIN_API void GatherTileResidency(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		FIntPoint MinKey, FIntPoint MaxKey, TArray<EArcTerrainTileResidency>& OutStates);

	inline constexpr int32 MaxManifestCoverageKeys = 8192;

	/** Every key one rail's manifest names, with the store's unusable mark on each — what is BAKED, with no
	 *  reference to any invoker. GAME THREAD, and it LOADS THE MANIFEST from disk on first ask, so it belongs
	 *  behind an explicit operator action and never on a passive per-tick gate.
	 *
	 *  Returns the rail's full entry count, which exceeds OutTiles.Num() when the manifest holds more than
	 *  MaxManifestCoverageKeys entries: the caller sees the truncation instead of a silently short set. */
	ARCTERRAIN_API int32 GatherManifestCoverage(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		TArray<FArcTerrainManifestTile>& OutTiles);

	/** Bumped by every change GatherManifestCoverage can report — the manifest arriving, an entry written or
	 *  dropped, a key marked unusable or cleared of it. PROCESS-WIDE and lock-free, so a cache holding
	 *  coverage geometry can test it every tick without touching the store: a bump on ANOTHER terrain costs
	 *  that cache one rebuild, which is the safe direction. */
	ARCTERRAIN_API uint32 GetManifestCoverageEpoch();

	ARCTERRAIN_API uint32 GetLoadedCoverageEpoch();

	/** One tile key, end to end: manifest entry, file on disk, header parse, lattice match (with the
	 *  differing fields named), cached payload, unusable suppression. False when the rail has no lattice at
	 *  all. GAME THREAD; reads the file. */
	ARCTERRAIN_API bool ProbeTile(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		FIntPoint TileKey, FArcTerrainTileProbe& OutProbe);

	/** HasTile + LoadTileSync + the cell cut, for the editor bakers that walk cells. False means the cell
	 *  has no readable baked height — the caller decides whether that is flat ground or a hole. */
	ARCTERRAIN_API bool LoadCellHeightsSync(const AArcTerrain& Terrain, FArcTerrainCellKey CellKey,
		TArray<float>& OutHeights, int32& OutResolution);
}
