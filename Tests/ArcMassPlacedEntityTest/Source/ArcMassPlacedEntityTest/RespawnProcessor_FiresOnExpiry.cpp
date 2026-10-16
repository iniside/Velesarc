// Copyright Lukasz Baran. All Rights Reserved.

/**
 * Test: RespawnProcessor_FiresOnExpiry
 *
 * Verifies that UArcPlacedEntityRespawnProcessor correctly invokes
 * AArcPlacedEntityPartitionActor::RestoreExpiredRemovals when the 1Hz interval
 * elapses with a live tracker entity pointing at a real partition actor.
 *
 * TODO (not yet implemented):
 * AArcPlacedEntityPartitionActor inherits from APartitionActor and requires
 * UWorldPartition to be active in the test world. FActorTestSpawner creates a
 * minimal world WITHOUT WorldPartition, so SpawnActor<AArcPlacedEntityPartitionActor>
 * fails or produces a non-functional actor (BeginPlay panics on the partition subsystem).
 *
 * To implement this test legitimately:
 * 1. Create a test world with WorldPartition enabled (requires editor-only infrastructure
 *    or a custom CQTest fixture that boots a full PIE session).
 * 2. Alternatively, mock the partition actor via a lightweight subclass that overrides
 *    BeginPlay to create the tracker entity directly without calling APartitionActor::BeginPlay.
 * 3. Then: spawn actor, call BeginPlay mock, verify tracker entity, call MarkInstanceRemoved,
 *    advance world time via FDateTime mock or by writing RespawnAtUtc = NowUtc - 1,
 *    drive Processor via UE::Mass::Executor::Run with DeltaTime > 1.0f,
 *    verify SpawnedEntities[flat] is valid again.
 *
 * This stub fails loudly so CI catches "test not yet runnable" rather than silently passing.
 */

#include "CQTest.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityRespawnProcessor.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityRespawnTrait.h"

TEST_CLASS(RespawnProcessor_FiresOnExpiry, "ArcMassPlacedEntity.RespawnProcessor")
{
	TEST_METHOD(Processor_Fires_WhenRespawnAtUtcElapsed)
	{
		AddError(
			TEXT("RespawnProcessor_FiresOnExpiry: not yet implemented — requires a world with "
			     "UWorldPartition active so AArcPlacedEntityPartitionActor::BeginPlay can run. "
			     "See the TODO comment in RespawnProcessor_FiresOnExpiry.cpp for the full "
			     "implementation checklist."));
	}
};
