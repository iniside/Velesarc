// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AArcTownVolume;
class UWorld;

/**
 * Removal of a settlement's baked output: the partition actors carrying its instances, and the pad
 * stamps flattening the terrain under them. Partition actors are keyed on
 * AArcTownPartitionActor::OwnerGuid — every one belongs to exactly one town, so no per-client
 * bookkeeping has to be unpicked; pads are instance components of the volume itself.
 *
 * Neither entry point opens a transaction: the instance components they destroy are NonTransactional,
 * so an undo would restore empty partition actors and a ledger naming them. Rebuild is the recovery
 * path, and the Details buttons say so.
 */
namespace ArcTownTeardown
{
	/**
	 * Destroys the partition actors carrying OwnerGuid == TownId, both the ones the town's ledger names
	 * and any inside SearchBounds it lost, empties the ledger, and destroys the volume's pad stamps —
	 * a clear that left the pads would leave the terrain flattened under buildings that no longer
	 * exist. Town may be null, in which case the loaded volume carrying TownId is searched for; pass it
	 * whenever the caller holds it, because TownId is not unique among loaded actors (two placements of
	 * one Level Instance share it). Pads are only reachable through the volume, so a null Town that
	 * resolves to nothing leaves them in place. Returns the number of partition actors destroyed.
	 */
	ARCTOWNEDITOR_API int32 ClearTown(
		UWorld& World,
		AArcTownVolume* Town,
		const FGuid& TownId,
		const FBox& SearchBounds);

	/**
	 * Destroys every ArcTown partition actor whose owning volume no longer exists, for the cases the
	 * teardown hook could not cover: a crash, a force delete, a ledger that drifted. Returns the count,
	 * and 0 without destroying anything when the set of live towns cannot be enumerated in full.
	 */
	ARCTOWNEDITOR_API int32 SweepOrphans(UWorld& World);
}
