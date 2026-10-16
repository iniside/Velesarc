// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "ArcTerrainTileFormat.h"
#include "DerivedDataValueId.h"
#include "IO/IoHash.h"
#include "Memory/SharedBuffer.h"
#include "Templates/Function.h"

/**
 * Shared DDC blob rail for the height, weight and data payloads (bucket "ArcTerrainCell"), whatever
 * lattice they were evaluated on. Payload-agnostic: it moves opaque bytes under a caller-computed key,
 * and each payload owns its own key derivation and its own blob layout.
 *
 * ONE KEY CARRIES ONE PAYLOAD. A cache record can hold several values, but only when a single
 * FCacheRecordBuilder adds them all before one Put — a later Put on the same key builds a different
 * whole record, and under ECachePolicy::Default the cache store keeps the record that landed first and
 * drops the new one (FileSystemCacheStore::PutCacheRecord, FMemoryCacheStore::Put); the values never
 * merge, so the second value id stays unreachable forever. The value id is a LABEL on the payload, not
 * a namespace that separates payloads sharing a key. The three payloads below are disjoint only because
 * each folds a distinct format GUID into the Blake3 that produces its key.
 *
 * Two versioning levers per payload: a constexpr blob version written as the blob's first int32 and
 * checked after fetch (catches a reader that disagrees with the writer), and the format GUID in the key
 * (old records simply stop being looked up). Bumping the GUID is the ONLY lever that invalidates a
 * poisoned blob — deleting the section asset does not heal a cell, because the next probe restores the
 * same bytes from the cache.
 *
 * ArcTerrainCollisionBuilder deliberately keeps its own get/put in this same bucket rather than calling
 * here: it serialises a Chaos::FHeightFieldPtr through FChaosArchive, while these payloads are POD.
 */
namespace ArcTerrainTileCache
{
	struct FAsyncGet;

	/** Handle on one in-flight non-blocking fetch. Shared, never held by value: the completion callback
	 *  keeps a reference of its own, so a caller that drops the handle mid-flight abandons the request
	 *  instead of leaving it to write into freed state — and abandoning costs the dropper nothing. */
	using FAsyncGetPtr = TSharedPtr<FAsyncGet, ESPMode::ThreadSafe>;

	/** Blocking fetch. Returns a null buffer on a miss or when the record carries no such value. */
	ARCTERRAIN_API FSharedBuffer TryGet(
		const FIoHash& Key, const UE::DerivedData::FValueId& ValueId, const TCHAR* DebugName);

	using FArcTerrainAsyncGetCompletion = TUniqueFunction<void()>;

	/** Same request as TryGet at EPriority::Normal, issued without waiting. Poll with IsComplete. */
	ARCTERRAIN_API FAsyncGetPtr GetAsync(
		const FIoHash& Key,
		const UE::DerivedData::FValueId& ValueId,
		const TCHAR* Context,
		FArcTerrainAsyncGetCompletion&& Completion);

	/** False for a null handle and while the request is in flight. */
	ARCTERRAIN_API bool IsComplete(const FAsyncGetPtr& Request);

	/** Moves the fetched bytes out of a completed request. Null buffer on a miss, on a record without the
	 *  value, on a cancelled request and while the request is still in flight. */
	ARCTERRAIN_API FSharedBuffer TakeBuffer(const FAsyncGetPtr& Request);

	/** Fire-and-forget store; the request outlives this call. No-op for a null blob. */
	ARCTERRAIN_API void Put(
		const FIoHash& Key, const UE::DerivedData::FValueId& ValueId, FSharedBuffer&& Blob, const TCHAR* DebugName);

	ARCTERRAIN_API const UE::DerivedData::FValueId& HeightTexelsValueId();
	ARCTERRAIN_API const UE::DerivedData::FValueId& WeightPlanesValueId();
	ARCTERRAIN_API const UE::DerivedData::FValueId& DataMasksValueId();

	/** The value id of the rail's payload — the same three labels, selected by rail. */
	ARCTERRAIN_API const UE::DerivedData::FValueId& RailValueId(EArcTerrainRail Rail);
}

#endif // WITH_EDITOR
