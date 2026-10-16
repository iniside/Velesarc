// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHIResources.h"
#include "Engine/TextureDefines.h" // ETextureSourceFormat
#include "Assets/ArcTerrainPatchAsset.h" // EArcTerrainSourceChannel

class UTexture2D;

/**
 * One cached GPU stamp source: an immutable PF_R32_FLOAT texture holding the decoded-channel float
 * raster. Dims/SizeBytes are set on the game thread at creation; TextureRHI is set on the render
 * thread by the upload command, which is enqueued BEFORE any composite command that consumes the
 * entry — render-command FIFO ordering guarantees the texture exists by then.
 *
 * NOTE (MVP): uploads are full-precision R32 floats (R16F packing deferred); the encoding decode
 * (scale / zero) happens in the composite shader, NOT at upload — the cached texel is the RAW
 * channel value.
 */
struct FArcTerrainSourceTexture
{
	FTextureRHIRef TextureRHI;
	FIntPoint Dims = FIntPoint::ZeroValue;
	int64 SizeBytes = 0;
	float BorderMean = 0.0f;
};

using FArcTerrainSourceTexturePtr = TSharedPtr<FArcTerrainSourceTexture, ESPMode::ThreadSafe>;

/**
 * One cached CPU stamp source: the decoded-channel float raster kept RESIDENT on the CPU, shared
 * out by shared-ptr so a consumer gather is O(covering stamps), never a per-cell
 * FTextureSource::GetMipData re-decompress. Same raw-channel-value convention as the GPU entries:
 * the encoding decode ((texel - Zero) * Scale) is the CONSUMER's job.
 *
 * The production consumer is ArcTerrainCpuComposite::GatherCellHeightOps — every in-ring cell's
 * collision heightfield is evaluated off these rasters, so the LRU budget below is on the path that
 * keeps pawns off the floor, not on a spare surface.
 */
struct FArcTerrainSourceCpuFloats
{
	TArray<float> Values;
	FIntPoint Dims = FIntPoint::ZeroValue;
	int64 SizeBytes = 0;
	float BorderMean = 0.0f;
};

using FArcTerrainSourceCpuFloatsPtr = TSharedPtr<const FArcTerrainSourceCpuFloats, ESPMode::ThreadSafe>;

/**
 * Game-thread cache of stamp raster sources on the GPU, keyed by (ContentId, source channel).
 * Two upload paths:
 *  (1) UTexture2D -> FTextureSource::GetMipData (EDITOR-ONLY source data; lossless, pre-BCn) with
 *      TSF-format conversion + channel extraction to float;
 *  (2) baked float field provider (TArray<float> + dims).
 * Both create the RHI texture and RHICmdList.UpdateTexture2D inside an ENQUEUE_RENDER_COMMAND.
 *
 * Refcounting: in-flight composite jobs hold FArcTerrainSourceTexturePtr — the LRU never evicts an
 * entry whose shared refcount shows an outside holder. Budget cvar: arc.Terrain.SourceCacheMB.
 */
class ARCTERRAIN_API FArcTerrainSourceCache
{
public:
	/** The ETextureSourceFormat set the channel decode can read. Single authority: the decode
	 *  switch rejects anything this returns false for and asserts on anything it admits but does
	 *  not handle, so a caller can pre-flight a source without the two lists drifting. */
	static bool IsDecodableSourceFormat(ETextureSourceFormat Format);

	/** True for formats ConvertSourceToFloats decodes as grayscale: the requested channel is never
	 *  read, so any EArcTerrainSourceChannel produces the same values. Selecting a non-default
	 *  channel on one of these formats is harmless but caches the identical raster a second time,
	 *  since the channel is part of the cache key. */
	static bool IsSingleChannelSourceFormat(ETextureSourceFormat Format);

	/** True for 8-bit-per-channel formats. ConvertSourceToFloats performs no sRGB conversion —
	 *  the decoded value is the raw normalized byte regardless of the texture's SRGB flag — but an
	 *  8-bit texture authored with SRGB enabled was intended for gamma-corrected display, so its
	 *  raw bytes may not be the linear values the caller wants. */
	static bool IsEightBitSourceFormat(ETextureSourceFormat Format);

#if WITH_EDITORONLY_DATA
	/** Editor capability, not a runtime one: the raster lives in FTextureSource, so a UTexture2D-backed
	 *  stamp source does not exist outside the editor and the baked section is the cooked height
	 *  authority. Returns null on unsupported source formats. Channel is part of the cache key — the
	 *  same texture sampled through different channels caches separate rasters. */
	FArcTerrainSourceTexturePtr GetOrCreateFromTexture(
		UTexture2D& Texture, EArcTerrainSourceChannel SourceChannel, const FGuid& ContentId);
#endif

	/** Baked-field path: Provider fills the row-major float buffer + dims (called only on miss). */
	FArcTerrainSourceTexturePtr GetOrCreateFromProvider(
		const FGuid& ContentId, const TFunction<bool(TArray<float>&, FIntPoint&)>& Provider);

#if WITH_EDITORONLY_DATA
	/**
	 * CPU-FLOAT twin of GetOrCreateFromTexture (native-composite Stage 1 Step 1): the same
	 * ConvertSourceToFloats decode, kept CPU-resident and keyed by (ContentId, channel) in its own
	 * LRU (same MiB budget cvar, separate accounting — GPU entries hold VRAM, these hold RAM).
	 * Consumers hold the returned shared-ptr across threads; eviction only drops the cache's ref.
	 * NOTE: a source consumed by BOTH caches decodes at most twice per session (once per cache) —
	 * the point of this cache is per-CELL amortization of the gather, which it fully provides.
	 */
	FArcTerrainSourceCpuFloatsPtr GetOrCreateCpuFloatsFromTexture(
		UTexture2D& Texture, EArcTerrainSourceChannel SourceChannel, const FGuid& ContentId);
#endif

	/** CPU-float twin of GetOrCreateFromProvider. The provider may therefore run once per CACHE
	 *  (GPU + CPU) for the same ContentId — providers fill from stored bake data and are idempotent. */
	FArcTerrainSourceCpuFloatsPtr GetOrCreateCpuFloatsFromProvider(
		const FGuid& ContentId, const TFunction<bool(TArray<float>&, FIntPoint&)>& Provider);

	/** Unevictable while pinned; false when the entry is absent or the pinned bytes would pass
	 *  UArcTerrainSettings::SourceCachePinMB, and the caller then runs on the plain LRU. */
	bool PinCpuFloats(const FGuid& ContentId, EArcTerrainSourceChannel SourceChannel);

	void UnpinAllCpuFloats();

	void ShedPinnedCpuFloatsToBudget();

	/** Turning it on clears the counters. */
	void SetCpuStatsEnabled(bool bEnabled);
	bool IsCpuStatsEnabled() const { return bCpuStatsEnabled; }

	FString DescribeCpuStats() const;

	/** Drop everything. Call only after FlushRenderingCommands (no in-flight consumers). */
	void Empty();

	/** Force an immediate LRU trim of BOTH the GPU and CPU budgets to the current arc.Terrain.SourceCacheMB
	 *  (resolved via the settings/cvar override). The budgets otherwise only trim on insert, so lowering
	 *  the setting live wouldn't reclaim until the next source upload — the subsystem's editor-only
	 *  OnSettingChanged handler calls this to reclaim now. GAME THREAD ONLY. */
	void ReEnforceBudgets();

private:
	struct FCacheKey
	{
		FGuid ContentId;
		uint8 Channel = 0;

		bool operator==(const FCacheKey& Other) const
		{
			return ContentId == Other.ContentId && Channel == Other.Channel;
		}

		friend uint32 GetTypeHash(const FCacheKey& Key)
		{
			return HashCombine(GetTypeHash(Key.ContentId), GetTypeHash(Key.Channel));
		}
	};

	struct FCacheEntry
	{
		FArcTerrainSourceTexturePtr Texture;
		uint64 LastUsedFrame = 0;
	};

	struct FCpuCacheEntry
	{
		TSharedPtr<FArcTerrainSourceCpuFloats, ESPMode::ThreadSafe> Floats;
		uint64 LastUsedFrame = 0;
	};

	/** Create the entry + enqueue the GPU upload, insert, then enforce the LRU budget. */
	FArcTerrainSourceTexturePtr CreateAndUpload(
		const FCacheKey& Key, TArray<float>&& Values, FIntPoint Dims, float BorderMean);

	/** Insert a CPU-float entry, then enforce the CPU LRU budget. */
	FArcTerrainSourceCpuFloatsPtr InsertCpuFloats(
		const FCacheKey& Key, TArray<float>&& Values, FIntPoint Dims, float BorderMean);

	void EnforceBudget();
	void EnforceCpuBudget();

	TMap<FCacheKey, FCacheEntry> Entries;
	int64 TotalBytes = 0;

	TMap<FCacheKey, FCpuCacheEntry> CpuEntries;
	int64 TotalCpuBytes = 0;

	TSet<FCacheKey> PinnedCpuKeys;
	int64 PinnedCpuBytes = 0;
	bool bPinBudgetWarned = false;

	struct FCpuStatsDecode
	{
		FIntPoint Dims = FIntPoint::ZeroValue;
		int64 Bytes = 0;
		int32 Decodes = 0;
		double DecodeMs = 0.0;
	};

	bool bCpuStatsEnabled = false;
	int32 CpuStatHits = 0;
	int32 CpuStatMisses = 0;
	int32 CpuStatEvictions = 0;
	int32 CpuStatPinsRefused = 0;
	TMap<FCacheKey, FCpuStatsDecode> CpuStatDecodes;
};
