// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/RefCounting.h"
#include "ArcTerrainTypes.h"

#if WITH_EDITOR
#include "Containers/MpscQueue.h"
#include "IO/IoHash.h"
#include <atomic>
#endif

class FArcTerrainHeightfieldRef;
class UArcTerrainStampComponent;

namespace ArcTerrainCollision
{
	/** Pure-CPU FHeightField construction — thread-safe off the game thread. Heights are row-major
	 *  Y * Resolution + X, exactly the FHeightField sample layout (X = column -> local X). */
	ARCTERRAIN_API TRefCountPtr<FArcTerrainHeightfieldRef> BuildHeightfield(
		const TArray<float>& Heights, int32 Resolution, const FVector& BaseScale);

#if WITH_EDITOR
	ARCTERRAIN_API FIoHash ComputeCellCollisionKey(const FArcTerrainConfig& Config, float BaseHeightCm,
		FArcTerrainCellKey Key, const FTransform& TerrainTransform,
		TConstArrayView<UArcTerrainStampComponent*> SortedStamps);

	ARCTERRAIN_API FIoHash ComputeCollisionPatchCacheKey(const FIoHash& RecipeKey, int32 CollisionQuads,
		int32 PatchExtentM);

	TRefCountPtr<FArcTerrainHeightfieldRef> TryGetFromDDC(const FIoHash& Key, const FVector& BaseScale);

	struct FAsyncHeightfieldGet;

	struct FArcTerrainCollisionCompletionToken
	{
		uint32 TerrainId = 0;
		FArcTerrainCollisionKey PatchKey = FArcTerrainCollisionKey::ZeroValue;
		uint64 PatchGeneration = 0;
		uint64 DirtySerial = 0;
		FIoHash CollisionKey;
	};

	struct FArcTerrainCollisionCompletionMailbox
	{
		static constexpr int32 Capacity = 4096;

		TMpscQueue<FArcTerrainCollisionCompletionToken> Queue;
		std::atomic<int32> Pending { 0 };
		std::atomic<bool> bOverflowWake { false }; // Overflow keeps the handle; this revisits outstanding probes.
	};

	using FAsyncHeightfieldGetPtr = TSharedPtr<FAsyncHeightfieldGet, ESPMode::ThreadSafe>;

	FAsyncHeightfieldGetPtr BeginGetFromDDC(const FIoHash& Key,
		const TSharedPtr<FArcTerrainCollisionCompletionMailbox, ESPMode::ThreadSafe>& Mailbox,
		const FArcTerrainCollisionCompletionToken& Token);

	bool IsGetComplete(const FAsyncHeightfieldGetPtr& Request);

	TRefCountPtr<FArcTerrainHeightfieldRef> ConsumeFromDDC(const FAsyncHeightfieldGetPtr& Request,
		const FVector& BaseScale);

	void PutToDDC(const FIoHash& Key, const FArcTerrainHeightfieldRef& Ref);
#endif
}
