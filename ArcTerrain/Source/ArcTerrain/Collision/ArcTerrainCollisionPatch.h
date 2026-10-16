// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IO/IoHash.h"

#if WITH_EDITOR
#include "Collision/ArcTerrainCollisionBuilder.h" // FAsyncHeightfieldGetPtr
#endif

class UArcTerrainCollisionComponent;
class UArcTerrainMeshModifier;
class UStaticMeshComponent;

/**
 * One collision patch: a heightfield on the CollisionPatchExtent lattice and everything the build rail
 * keeps about it. THE COLLISION UNIT IS THIS, NOT A CELL — a patch is keyed, built, cached and torn down
 * without any FArcTerrainCellState existing under it, which is what lets a patch be a fraction of a cell
 * (or span several) and what keeps a pawn on the ground where no cell was ever materialised.
 */
struct FArcTerrainCollisionPatch
{
	/** Physics component of this patch (transient, owned by the AArcTerrain — weak here). */
	TWeakObjectPtr<UArcTerrainCollisionComponent> CollisionComponent;

	/** One invisible collision-only UStaticMeshComponent per mesh modifier overlapping this patch, in
	 *  GatherSortedMeshModifiers order (transient, owned by the AArcTerrain — weak here, so entries can
	 *  read back null).
	 *
	 *  ADDITIVE TO CollisionComponent, NEVER FOLDED INTO IT: the heightfield is single-valued in Z and
	 *  cannot represent an overhang. Lifetime is the COLLISION RING AND NOTHING ELSE — synced at
	 *  UpdateCollisionPatch's ring gate BEFORE the height-source verdict (a modifier's UStaticMesh needs
	 *  no heightfield, so a missing/failed/awaited height source must not take its body with it),
	 *  destroyed by DestroyPatchCollision. */
	TArray<TWeakObjectPtr<UStaticMeshComponent>> MeshModifierCollision;
	uint64 SyncedMeshModifierRevision = 0;
	TArray<TWeakObjectPtr<UArcTerrainMeshModifier>> SyncedMeshModifiers;

	/** THE PATCH'S OWN DIRTY COUNTER, and the reason the collision rail needs no cell Version: it is
	 *  bumped by MarkBoundsDirty for every patch an edit's bounds cut, exactly as the tile rails bump
	 *  theirs. Starts at 1 so 0 stays the never-computed sentinel of the two serials below. */
	uint64 DirtySerial = 1;

	// Identity of this registry slot; DirtySerial restarts at 1 if the same key is recreated.
	uint64 PatchGeneration = 0;

	/** DirtySerial whose heightfield is currently applied to CollisionComponent (0 = none). */
	uint64 AppliedDirtySerial = 0;

	bool bCollisionHeightSourceMissing = false;

#if WITH_EDITOR
	bool bCollisionBuiltIncomplete = false;

	double CollisionRetryAfter = 0.0;

	uint8 CollisionRetryCount = 0;

	FIoHash AppliedCollisionKeyHash;

	FIoHash CachedCollisionKey;
	uint64 CachedCollisionKeySerial = 0; // DirtySerial the cached key was computed for (0 = none)

	ArcTerrainCollision::FAsyncHeightfieldGetPtr CollisionProbe;

	FIoHash CollisionProbeKey;
#endif
};
