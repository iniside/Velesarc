// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "ArcTerrainTypes.h"
#include "IO/IoHash.h"
#include "Templates/Function.h"

struct FArcTerrainDataMapCellBake;
class UArcTerrainChannelSet;

namespace ArcTerrainWeights
{
	struct FArcTerrainWeightPlane;
}

namespace ArcTerrainTileCache
{
	// Forward-declared rather than included: ArcTerrainTileCache.h pulls DDC headers, and DerivedDataCache
	// is a PRIVATE dependency of this module — modules that include this one through ArcTerrainSubsystem.h
	// have no include path to it.
	struct FAsyncGet;
	using FAsyncGetPtr = TSharedPtr<FAsyncGet, ESPMode::ThreadSafe>;
	using FArcTerrainAsyncGetCompletion = TUniqueFunction<void()>;
}

/** One tile's finished CPU height payload: the raster and its extrema. bFinite false means a non-finite
 *  texel was found and nothing else here is meaningful. */
struct FArcTerrainHeightBakeResult
{
	TArray<float> Texels;
	float LocalMin = 0.0f;
	float LocalMax = 0.0f;
	bool bFinite = false;
};

/**
 * The DDC blobs behind the three tile rails: an evaluated payload is stored under the rail's tile cache
 * key so a second machine — or a second session — writes the file without re-running the CPU evaluation.
 * The blob is the EVALUATION's output, never the file: the file's header is re-made on every write.
 */
namespace ArcTerrainTileBlobs
{
	/** Touches no UObject and no RHI: the finiteness scan and the extrema run off the game thread. */
	ARCTERRAIN_API void BuildHeightBakeResult(TArray<float>&& Heights,
		FArcTerrainHeightBakeResult& OutResult);

	/** Issue the non-blocking probe for one tile's cached heights. Null handle when Resolution cannot
	 *  address the raster. */
	ARCTERRAIN_API ArcTerrainTileCache::FAsyncGetPtr BeginGetCachedHeightTexels(const FIoHash& CacheKey,
		int32 Resolution, ArcTerrainTileCache::FArcTerrainAsyncGetCompletion&& Completion);

	/** Takes a completed probe's payload. False on a miss, on a still-in-flight probe and on any blob that
	 *  does not raster to Resolution^2. */
	ARCTERRAIN_API bool ConsumeCachedHeightTexels(const ArcTerrainTileCache::FAsyncGetPtr& Probe,
		int32 Resolution, TArray<float>& OutTexels);

	/** Store an evaluated payload. THE CALLER GATES ON FCellHeightEval::bComplete: an incomplete
	 *  evaluation still matches its key, so caching one freezes the missing stamp in for every machine
	 *  sharing the cache. */
	ARCTERRAIN_API void PutCachedHeightTexels(const FIoHash& CacheKey, TConstArrayView<float> Texels);

	ARCTERRAIN_API ArcTerrainTileCache::FAsyncGetPtr BeginGetCachedWeightPlanes(const FIoHash& CacheKey,
		int32 Resolution, int32 NumChannels, ArcTerrainTileCache::FArcTerrainAsyncGetCompletion&& Completion);

	/** Take a completed probe's plane set. False on a miss, on a still-in-flight probe, and on any blob
	 *  breaking what the tile header stands on: every plane Resolution^2 bytes and every plane naming a
	 *  DISTINCT channel of ChannelSet.
	 *
	 *  The blob labels its planes BY CHANNEL-NAME HASH, like the tile file: the cache key is invariant
	 *  under a pure channel reorder, so an index-labelled blob would hit under a reordered set and hand
	 *  the tile writer another channel's bytes. A label naming no current channel rejects the whole
	 *  blob — a partial plane set would claim the missing layers are gone. */
	ARCTERRAIN_API bool ConsumeCachedWeightPlanes(const ArcTerrainTileCache::FAsyncGetPtr& Probe,
		int32 Resolution, const UArcTerrainChannelSet& ChannelSet,
		TArray<ArcTerrainWeights::FArcTerrainWeightPlane>& OutPlanes);

	/** Stores nothing when a plane's ChannelIndex is outside ChannelSet: the label the blob is read back
	 *  by would not exist. */
	ARCTERRAIN_API void PutCachedWeightPlanes(const FIoHash& CacheKey,
		const UArcTerrainChannelSet& ChannelSet,
		TConstArrayView<ArcTerrainWeights::FArcTerrainWeightPlane> Planes);

	/** Non-blocking fetch of one tile's immutable data-mask payload. Decoding source textures still stays
	 * on the game thread, but cache I/O must never monopolise that thread. */
	ARCTERRAIN_API ArcTerrainTileCache::FAsyncGetPtr BeginGetCachedDataMasks(const FIoHash& CacheKey,
		ArcTerrainTileCache::FArcTerrainAsyncGetCompletion&& Completion);

	/** Takes one completed data-mask probe. False on miss, in-flight request, or malformed payload. */
	ARCTERRAIN_API bool ConsumeCachedDataMasks(const ArcTerrainTileCache::FAsyncGetPtr& Probe,
		FIntPoint TileKey, FArcTerrainDataMapCellBake& OutBake);

	ARCTERRAIN_API void PutCachedDataMasks(const FIoHash& CacheKey, const FArcTerrainDataMapCellBake& Bake);
}

#endif // WITH_EDITOR
