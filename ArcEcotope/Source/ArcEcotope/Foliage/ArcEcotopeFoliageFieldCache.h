// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "DerivedDataValueId.h"
#include "IO/IoHash.h"
#include "Memory/SharedBuffer.h"

/**
 * DDC blob rail for the derived forest field (bucket "ArcEcotopeFoliageField"). Payload-agnostic: it
 * moves opaque bytes under a caller-computed key, exactly as ArcTerrainTileCache does for terrain
 * tiles, and the caller owns both the key derivation and the blob layout.
 *
 * One key carries one payload: a second Put on the same key is dropped rather than merged, so a blob
 * is invalidated only by moving the key — which is what the format GUID folded into it is for.
 */
namespace ArcEcotopeFoliageFieldCache
{
	/** Blocking fetch. Null buffer on a miss or when the record carries no such value. */
	ARCECOTOPE_API FSharedBuffer TryGet(const FIoHash& Key, const TCHAR* DebugName);

	/** Fire-and-forget store; the request outlives this call. No-op for a null blob. */
	ARCECOTOPE_API void Put(const FIoHash& Key, FSharedBuffer&& Blob, const TCHAR* DebugName);
}

#endif // WITH_EDITOR
