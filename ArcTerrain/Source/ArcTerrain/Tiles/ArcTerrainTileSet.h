// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"

class UArcTerrainSubsystem;

/**
 * Free-list pool of transient UTextureRenderTarget2D keyed by (resolution, format). Creation
 * follows the Landscape scratch-RT pattern (LandscapeEditResourcesSubsystem.cpp): transient
 * NewObject, bCanCreateUAV, RenderTargetFormat, InitAutoFormat, UpdateResourceImmediate(false).
 *
 * GC: every created RT is rooted in the owning subsystem's UPROPERTY OwnedRenderTargets array
 * (via AddOwnedRenderTarget); the pool itself holds raw pointers backed by that root. RTs are
 * un-rooted only in UArcTerrainSubsystem::Deinitialize.
 */
class ARCTERRAIN_API FArcTerrainRenderTargetPool
{
public:
	void Initialize(UArcTerrainSubsystem& InOwner);

	/** Pop a compatible free RT or create a new one (rooted in the owner). */
	UTextureRenderTarget2D* Acquire(int32 Resolution, ETextureRenderTargetFormat Format);

	/** Return an RT to the free list. Contents are stale — every composite clears first. */
	void Release(UTextureRenderTarget2D* RenderTarget);

	/** Drop all free-list entries (the owner un-roots the objects separately). */
	void Reset();

private:
	struct FPoolKey
	{
		int32 Resolution = 0;
		ETextureRenderTargetFormat Format = RTF_R32f;

		bool operator==(const FPoolKey& Other) const
		{
			return Resolution == Other.Resolution && Format == Other.Format;
		}

		friend uint32 GetTypeHash(const FPoolKey& Key)
		{
			return HashCombine(GetTypeHash(Key.Resolution), GetTypeHash(static_cast<uint8>(Key.Format)));
		}
	};

	UArcTerrainSubsystem* Owner = nullptr;
	TMap<FPoolKey, TArray<UTextureRenderTarget2D*>> FreeLists;
};

/**
 * Per-cell WEIGHT render-target set: ceil(NumChannels / 4) RTF_RGBA8 tiles at (WeightResolution)^2
 * texels. The weight composite runs at the source-weightmap's texel density (WeightQuads + 3),
 * decoupled from the height/mesh quad count, so a high-res source weightmap keeps its full resolution
 * regardless of vertex density. Each tile carries a 1-texel apron per side; adjacent cells share
 * border AND apron texels by global-index construction.
 *
 * Height has NO per-cell render target: the composite publishes it straight into the terrain's tile
 * atlas slice, which is the single height source every consumer reads.
 *
 * Raw pointers — GC rooting lives in the subsystem (see FArcTerrainRenderTargetPool).
 */
struct FArcTerrainTileSet
{
	TArray<UTextureRenderTarget2D*> WeightRTs;

	bool IsAllocatedFor(int32 NumWeightRTs, int32 WeightResolution) const;

	/** Acquire/replace RTs so each weight tile matches WeightResolution (count NumWeightRTs); no-op
	 *  when it already does. */
	void Acquire(FArcTerrainRenderTargetPool& Pool, int32 NumWeightRTs, int32 WeightResolution);

	/** Return all RTs to the pool and clear the set. */
	void Release(FArcTerrainRenderTargetPool& Pool);
};
