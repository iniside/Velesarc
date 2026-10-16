// Copyright Lukasz Baran. All Rights Reserved.

/**
 * Test: MarkInstanceRemoved_MissingTraitLogs
 *
 * Verifies that calling AArcPlacedEntityPartitionActor::MarkInstanceRemoved on a slot
 * whose UMassEntityConfigAsset has NO UArcPlacedEntityRespawnTrait:
 *   a) emits a LogMass Warning containing "no UArcPlacedEntityRespawnTrait"
 *   b) adds the instance to ClassRemovals.RemovedInstances (permanent removal)
 *   c) does NOT queue a FArcPlacedEntityPendingRespawn in PendingRespawns
 *
 * TODO (not yet implemented):
 * Same blocker as RespawnProcessor_FiresOnExpiry: AArcPlacedEntityPartitionActor
 * depends on UWorldPartition infrastructure that FActorTestSpawner does not provide.
 *
 * To implement this test legitimately:
 * 1. Boot a test world with WorldPartition (or mock the partition actor's BeginPlay).
 * 2. Author a UMassEntityConfigAsset WITHOUT UArcPlacedEntityRespawnTrait.
 * 3. Add one FArcPlacedEntityEntry with one InstanceTransform.
 * 4. Spawn a Mass entity for that slot (via SpawnEntities) so there is a live entity handle.
 * 5. Call MarkInstanceRemoved(0, 0).
 * 6. Verify:
 *    - ClassRemovals[0].RemovedInstances.Contains(0) == true
 *    - PendingRespawns.Num() == 0
 *    - The expected "no UArcPlacedEntityRespawnTrait" Warning was captured.
 *
 * Log-capture mechanism in other ArcX tests: AddExpectedError(TEXT("..."), EAutomationExpectedErrorFlags::Contains, 1)
 * plus bSuppressLogWarnings = true in BEFORE_EACH so the warning doesn't fail the run.
 *
 * This stub fails loudly so CI catches "test not yet runnable" rather than silently passing.
 */

#include "CQTest.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityTypes.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityRespawnTrait.h"

TEST_CLASS(MarkInstanceRemoved_MissingTraitLogs, "ArcMassPlacedEntity.MarkInstanceRemoved")
{
	TEST_METHOD(MissingTrait_LogsWarning_AndPermanentlyRemoves)
	{
		AddError(
			TEXT("MarkInstanceRemoved_MissingTraitLogs: not yet implemented — requires a world with "
			     "UWorldPartition active so AArcPlacedEntityPartitionActor can be spawned and "
			     "SpawnEntities can populate entity handles. "
			     "See the TODO comment in MarkInstanceRemoved_MissingTraitLogs.cpp for the full "
			     "implementation checklist."));
	}
};
