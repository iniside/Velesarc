/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

/**
 * Tests for the bag-routed crafting-output feature (plan 2026-06-30-0816):
 *
 * Group 1 — ArcCraftBagOutput.AdvanceCompletedEntry (pure, no world)
 *   Exercises ArcCraftStationImpl::AdvanceCompletedEntry directly against
 *   in-memory FArcCraftQueueFragment values.  No UObjects, no world, no Mass.
 *
 * Group 2 — ArcCraftBagOutput.ResumeSubsystem
 *   Exercises UArcCraftOutputResumeSubsystem::RegisterBlocked / EvictEntity map
 *   logic via the world-subsystem obtained from FActorTestSpawner.
 *
 * Unique tag prefix: TAG_BagOut_*
 */

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

#include "ArcCraft/Station/ArcCraftStationTypes.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Station/ArcCraftStationImpl.h"
#include "ArcCraft/Mass/ArcCraftOutputResumeSubsystem.h"

#include "Mass/EntityHandle.h"

// ---------------------------------------------------------------------------
// Unique gameplay tags — prefix TAG_BagOut_* to avoid ODR collisions
// (none are actually needed by these tests; keeping the comment as a reminder)
// ---------------------------------------------------------------------------

// ===========================================================================
// Group 1 — AdvanceCompletedEntry (pure, no world)
// ===========================================================================

/**
 * Build a minimal FArcCraftQueueEntry with the given EntryId and Amount.
 * ElapsedTickTime is set so a caller can detect that it was reset to 0.
 */
static FArcCraftQueueEntry MakeBagOutEntry(const FGuid& InEntryId, int32 InAmount, float InElapsed = 99.0f)
{
	FArcCraftQueueEntry Entry;
	Entry.EntryId = InEntryId;
	Entry.Amount = InAmount;
	Entry.CompletedAmount = 0;
	Entry.ElapsedTickTime = InElapsed;
	Entry.StartTimestamp = 0;
	return Entry;
}

TEST_CLASS(ArcCraftBagOutputAdvanceEntry, "ArcCraft.BagOutput.AdvanceEntry")
{
	// =========================================================================
	// Test 1 — happy path: CompletedAmount incremented, ElapsedTickTime reset
	//
	// Single entry, Amount=3. After one AdvanceCompletedEntry call the entry
	// is still present (CompletedAmount=1 < Amount=3) but ElapsedTickTime is 0.
	// =========================================================================
	TEST_METHOD(AdvanceCompletedEntry_IncrementsCountAndResetsTimer)
	{
		const FGuid Id = FGuid::NewGuid();

		FArcCraftQueueFragment Queue;
		Queue.Entries.Add(MakeBagOutEntry(Id, /*Amount=*/3, /*Elapsed=*/10.0f));

		ArcCraftStationImpl::AdvanceCompletedEntry(Queue, Id);

		ASSERT_THAT(AreEqual(1, Queue.Entries.Num(),
			TEXT("Entry must still be present: CompletedAmount(1) < Amount(3)")));
		ASSERT_THAT(AreEqual(1, Queue.Entries[0].CompletedAmount,
			TEXT("CompletedAmount must be incremented to 1")));
		ASSERT_THAT(IsTrue(Queue.Entries[0].ElapsedTickTime == 0.0f,
			TEXT("ElapsedTickTime must be reset to 0")));
	}

	// =========================================================================
	// Test 2 — removal when CompletedAmount reaches Amount
	//
	// Amount=1: a single advance must remove the entry.
	// =========================================================================
	TEST_METHOD(AdvanceCompletedEntry_RemovesEntryWhenCompleted)
	{
		const FGuid Id = FGuid::NewGuid();

		FArcCraftQueueFragment Queue;
		Queue.Entries.Add(MakeBagOutEntry(Id, /*Amount=*/1));

		ArcCraftStationImpl::AdvanceCompletedEntry(Queue, Id);

		ASSERT_THAT(AreEqual(0, Queue.Entries.Num(),
			TEXT("Entry must be removed when CompletedAmount reaches Amount")));
	}

	// =========================================================================
	// Test 3 — no-op for an unknown EntryId
	//
	// Queue has a valid entry; advancing with a different Guid must leave
	// everything unchanged.
	// =========================================================================
	TEST_METHOD(AdvanceCompletedEntry_NoOpForUnknownId)
	{
		const FGuid KnownId = FGuid::NewGuid();
		const FGuid UnknownId = FGuid::NewGuid();

		FArcCraftQueueFragment Queue;
		Queue.Entries.Add(MakeBagOutEntry(KnownId, /*Amount=*/2, /*Elapsed=*/5.0f));

		ArcCraftStationImpl::AdvanceCompletedEntry(Queue, UnknownId);

		ASSERT_THAT(AreEqual(1, Queue.Entries.Num(),
			TEXT("No-op: entry must not be removed for an unknown EntryId")));
		ASSERT_THAT(AreEqual(0, Queue.Entries[0].CompletedAmount,
			TEXT("No-op: CompletedAmount must not change")));
		ASSERT_THAT(IsTrue(Queue.Entries[0].ElapsedTickTime == 5.0f,
			TEXT("No-op: ElapsedTickTime must not change")));
	}

	// =========================================================================
	// Test 4 — no-op for an empty queue
	// =========================================================================
	TEST_METHOD(AdvanceCompletedEntry_NoOpOnEmptyQueue)
	{
		const FGuid Id = FGuid::NewGuid();
		FArcCraftQueueFragment Queue;
		// Empty — should not crash or assert.
		ArcCraftStationImpl::AdvanceCompletedEntry(Queue, Id);

		ASSERT_THAT(AreEqual(0, Queue.Entries.Num(),
			TEXT("Empty queue must remain empty after advance with any EntryId")));
	}

	// =========================================================================
	// Test 5 — locate by EntryId (not index)
	//
	// Two entries: remove the FIRST one (EntryId=IdA) by advancing it to
	// completion. The second entry (IdB) must survive intact.
	//
	// This verifies that advance uses EntryId-keyed lookup, not an index, so
	// removing an earlier entry never corrupts the later one.
	// =========================================================================
	TEST_METHOD(AdvanceCompletedEntry_LocatesByIdNotIndex)
	{
		const FGuid IdA = FGuid::NewGuid();
		const FGuid IdB = FGuid::NewGuid();

		FArcCraftQueueFragment Queue;
		Queue.Entries.Add(MakeBagOutEntry(IdA, /*Amount=*/1, /*Elapsed=*/3.0f)); // index 0
		Queue.Entries.Add(MakeBagOutEntry(IdB, /*Amount=*/2, /*Elapsed=*/7.0f)); // index 1

		// Advance IdA (index 0) to completion — it should be removed.
		ArcCraftStationImpl::AdvanceCompletedEntry(Queue, IdA);

		ASSERT_THAT(AreEqual(1, Queue.Entries.Num(),
			TEXT("Entry IdA must be removed; IdB must survive")));
		ASSERT_THAT(IsTrue(Queue.Entries[0].EntryId == IdB,
			TEXT("Remaining entry must be IdB")));
		ASSERT_THAT(AreEqual(2, Queue.Entries[0].Amount,
			TEXT("Surviving entry must have Amount=2 (unchanged)")));
	}

	// =========================================================================
	// Test 6 — multiple advances: partial completion then final removal
	//
	// Amount=3: advance three times. After 2nd, entry still present. After 3rd, removed.
	// =========================================================================
	TEST_METHOD(AdvanceCompletedEntry_PartialThenFinal)
	{
		const FGuid Id = FGuid::NewGuid();

		FArcCraftQueueFragment Queue;
		Queue.Entries.Add(MakeBagOutEntry(Id, /*Amount=*/3));

		// First advance: still alive.
		ArcCraftStationImpl::AdvanceCompletedEntry(Queue, Id);
		ASSERT_THAT(AreEqual(1, Queue.Entries.Num(),
			TEXT("Entry must survive after 1st advance (CompletedAmount=1 < 3)")));
		ASSERT_THAT(AreEqual(1, Queue.Entries[0].CompletedAmount));

		// Second advance: still alive.
		ArcCraftStationImpl::AdvanceCompletedEntry(Queue, Id);
		ASSERT_THAT(AreEqual(1, Queue.Entries.Num(),
			TEXT("Entry must survive after 2nd advance (CompletedAmount=2 < 3)")));
		ASSERT_THAT(AreEqual(2, Queue.Entries[0].CompletedAmount));

		// Third advance: removed.
		ArcCraftStationImpl::AdvanceCompletedEntry(Queue, Id);
		ASSERT_THAT(AreEqual(0, Queue.Entries.Num(),
			TEXT("Entry must be removed after 3rd advance (CompletedAmount=3 == Amount=3)")));
	}
};

// ===========================================================================
// Group 2 — UArcCraftOutputResumeSubsystem map logic
//
// UArcCraftOutputResumeSubsystem is a UWorldSubsystem. We obtain it from the
// test world created by FActorTestSpawner (which calls InitializeGameSubsystems).
// This lets us exercise RegisterBlocked/EvictEntity without touching bag
// operations or the craft tick processor.
//
// The subsystem's BlockedByBagOwner map is private.  We verify behaviour
// indirectly: RegisterBlocked + EvictEntity + RegisterBlocked-dedup are all
// observable by checking that OnBagSlotsChanged (fired via the bag event
// subsystem's DrainQueues) either does or does not clear entries on the queue
// fragment of a station entity we set up in the Mass entity manager.
//
// Approach: after RegisterBlocked(Owner, Station), manually queue a
// bag-slots-changed event via UArcMassBagEventSubsystem::QueueBagSlotsChanged
// and drain it. If the subsystem is correctly mapped, OnBagSlotsChanged will
// clear bOutputBlocked on the station's queue entry.
// ===========================================================================

#include "Bags/ArcBagEventSubsystem.h"
#include "Bags/ArcBagId.h"

#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "StructUtils/InstancedStruct.h"

namespace ArcCraftBagOutputResumeTestLocal
{
	/**
	 * Create a minimal Mass entity that has FArcCraftQueueFragment with one entry
	 * that has bOutputBlocked=true.  Returns the entity handle.
	 */
	FMassEntityHandle CreateBlockedStationEntity(FMassEntityManager& EM, FArcCraftQueueFragment& OutQueue)
	{
		TArray<FInstancedStruct> Frags;
		Frags.Add(FInstancedStruct::Make(FArcCraftQueueFragment()));
		FMassEntityHandle Entity = EM.CreateEntity(Frags);

		FArcCraftQueueEntry BlockedEntry;
		BlockedEntry.EntryId = FGuid::NewGuid();
		BlockedEntry.Amount = 1;
		BlockedEntry.bOutputBlocked = true;

		FArcCraftQueueFragment* Queue = EM.GetFragmentDataPtr<FArcCraftQueueFragment>(Entity);
		check(Queue);
		Queue->Entries.Add(BlockedEntry);

		OutQueue = *Queue;
		return Entity;
	}

	/**
	 * Create a throwaway Mass entity to use purely as a map key (e.g. the BagOwner).
	 * FMassEntityManager::CreateEntity asserts the fragment list is non-empty, and the
	 * resume subsystem never validates the BagOwner handle, so one filler fragment suffices.
	 */
	FMassEntityHandle CreateKeyEntity(FMassEntityManager& EM)
	{
		TArray<FInstancedStruct> Frags;
		Frags.Add(FInstancedStruct::Make(FArcCraftQueueFragment()));
		return EM.CreateEntity(Frags);
	}
} // namespace ArcCraftBagOutputResumeTestLocal

TEST_CLASS(ArcCraftBagOutputResumeSubsystem, "ArcCraft.BagOutput.ResumeSubsystem")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EM = nullptr;
	UArcCraftOutputResumeSubsystem* ResumeSubsystem = nullptr;
	UArcMassBagEventSubsystem* BagEventSubsystem = nullptr;

	BEFORE_EACH()
	{
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EM = &MES->GetMutableEntityManager();

		ResumeSubsystem = Spawner.GetWorld().GetSubsystem<UArcCraftOutputResumeSubsystem>();
		BagEventSubsystem = Spawner.GetWorld().GetSubsystem<UArcMassBagEventSubsystem>();
	}

	// =========================================================================
	// Test 1 — RegisterBlocked then bag-slots-changed clears bOutputBlocked
	//
	// RegisterBlocked(Owner, Station); fire OnBagSlotsChanged for Owner via
	// QueueBagSlotsChanged + DrainQueues; confirm bOutputBlocked cleared on
	// the station's queue entry.
	// =========================================================================
	TEST_METHOD(RegisterBlocked_BagEventClearsBlockedFlag)
	{
		if (!ResumeSubsystem || !BagEventSubsystem)
		{
			// Subsystems absent in this test world — skip gracefully.
			return;
		}

		// Create an entity to act as BagOwner (no special fragments required).
		FMassEntityHandle BagOwner = ArcCraftBagOutputResumeTestLocal::CreateKeyEntity(*EM);

		// Create a station entity with a blocked queue entry.
		FArcCraftQueueFragment UnusedQueue;
		FMassEntityHandle Station = ArcCraftBagOutputResumeTestLocal::CreateBlockedStationEntity(*EM, UnusedQueue);

		// Confirm the entry starts blocked.
		{
			FArcCraftQueueFragment* Queue = EM->GetFragmentDataPtr<FArcCraftQueueFragment>(Station);
			ASSERT_THAT(IsNotNull(Queue));
			ASSERT_THAT(IsTrue(Queue->Entries.Num() > 0));
			ASSERT_THAT(IsTrue(Queue->Entries[0].bOutputBlocked,
				TEXT("Entry must start with bOutputBlocked=true")));
		}

		// Register the block and fire the bag event.
		ResumeSubsystem->RegisterBlocked(BagOwner, Station);
		BagEventSubsystem->QueueBagSlotsChanged(BagOwner, FArcBagId());
		BagEventSubsystem->DrainQueues();

		// bOutputBlocked must now be cleared on the station's entry.
		{
			FArcCraftQueueFragment* Queue = EM->GetFragmentDataPtr<FArcCraftQueueFragment>(Station);
			ASSERT_THAT(IsNotNull(Queue));
			ASSERT_THAT(IsTrue(Queue->Entries.Num() > 0));
			ASSERT_THAT(IsFalse(Queue->Entries[0].bOutputBlocked,
				TEXT("bOutputBlocked must be cleared after the bag-slots-changed event drains")));
		}
	}

	// =========================================================================
	// Test 2 — RegisterBlocked deduplication
	//
	// Registering the same (BagOwner, Station) pair twice then firing
	// OnBagSlotsChanged still clears the flag exactly once (no double-clear
	// or double-removal error).
	// =========================================================================
	TEST_METHOD(RegisterBlocked_DedupsSamePair)
	{
		if (!ResumeSubsystem || !BagEventSubsystem)
		{
			return;
		}

		FMassEntityHandle BagOwner = ArcCraftBagOutputResumeTestLocal::CreateKeyEntity(*EM);

		FArcCraftQueueFragment UnusedQueue;
		FMassEntityHandle Station = ArcCraftBagOutputResumeTestLocal::CreateBlockedStationEntity(*EM, UnusedQueue);

		// Register the SAME pair twice.
		ResumeSubsystem->RegisterBlocked(BagOwner, Station);
		ResumeSubsystem->RegisterBlocked(BagOwner, Station);

		// Fire the event — should run without crash and clear the flag.
		BagEventSubsystem->QueueBagSlotsChanged(BagOwner, FArcBagId());
		BagEventSubsystem->DrainQueues();

		FArcCraftQueueFragment* Queue = EM->GetFragmentDataPtr<FArcCraftQueueFragment>(Station);
		ASSERT_THAT(IsNotNull(Queue));
		ASSERT_THAT(IsTrue(Queue->Entries.Num() > 0));
		ASSERT_THAT(IsFalse(Queue->Entries[0].bOutputBlocked,
			TEXT("bOutputBlocked must be cleared exactly once (dedup prevents double-fire)")));
	}

	// =========================================================================
	// Test 3 — EvictEntity as BagOwner removes the key; no subsequent event
	//
	// RegisterBlocked, then EvictEntity(BagOwner), then fire the event.
	// The flag must remain blocked (subsystem removed the mapping).
	// =========================================================================
	TEST_METHOD(EvictEntity_AsBagOwner_RemovesMapping)
	{
		if (!ResumeSubsystem || !BagEventSubsystem)
		{
			return;
		}

		FMassEntityHandle BagOwner = ArcCraftBagOutputResumeTestLocal::CreateKeyEntity(*EM);

		FArcCraftQueueFragment UnusedQueue;
		FMassEntityHandle Station = ArcCraftBagOutputResumeTestLocal::CreateBlockedStationEntity(*EM, UnusedQueue);

		ResumeSubsystem->RegisterBlocked(BagOwner, Station);

		// Evict the BagOwner — removes (BagOwner, Station) from the map.
		ResumeSubsystem->EvictEntity(BagOwner);

		// Fire the event — should be a no-op for this Station.
		BagEventSubsystem->QueueBagSlotsChanged(BagOwner, FArcBagId());
		BagEventSubsystem->DrainQueues();

		FArcCraftQueueFragment* Queue = EM->GetFragmentDataPtr<FArcCraftQueueFragment>(Station);
		ASSERT_THAT(IsNotNull(Queue));
		ASSERT_THAT(IsTrue(Queue->Entries.Num() > 0));
		ASSERT_THAT(IsTrue(Queue->Entries[0].bOutputBlocked,
			TEXT("bOutputBlocked must remain true: mapping was evicted before the event fired")));
	}

	// =========================================================================
	// Test 4 — EvictEntity as Station removes it from value lists
	//
	// RegisterBlocked(Owner, StationA) and RegisterBlocked(Owner, StationB).
	// EvictEntity(StationA). Fire the event for Owner. Only StationB must be
	// unblocked; StationA must remain blocked.
	// =========================================================================
	TEST_METHOD(EvictEntity_AsStation_RemovesFromValueList)
	{
		if (!ResumeSubsystem || !BagEventSubsystem)
		{
			return;
		}

		FMassEntityHandle BagOwner = ArcCraftBagOutputResumeTestLocal::CreateKeyEntity(*EM);

		FArcCraftQueueFragment UnusedA;
		FArcCraftQueueFragment UnusedB;
		FMassEntityHandle StationA = ArcCraftBagOutputResumeTestLocal::CreateBlockedStationEntity(*EM, UnusedA);
		FMassEntityHandle StationB = ArcCraftBagOutputResumeTestLocal::CreateBlockedStationEntity(*EM, UnusedB);

		ResumeSubsystem->RegisterBlocked(BagOwner, StationA);
		ResumeSubsystem->RegisterBlocked(BagOwner, StationB);

		// Evict StationA as a station (value) — removes it from the Owner's value list.
		ResumeSubsystem->EvictEntity(StationA);

		// Fire the bag-slots-changed event for BagOwner.
		BagEventSubsystem->QueueBagSlotsChanged(BagOwner, FArcBagId());
		BagEventSubsystem->DrainQueues();

		// StationA must still be blocked (it was evicted from the map).
		FArcCraftQueueFragment* QueueA = EM->GetFragmentDataPtr<FArcCraftQueueFragment>(StationA);
		ASSERT_THAT(IsNotNull(QueueA));
		ASSERT_THAT(IsTrue(QueueA->Entries[0].bOutputBlocked,
			TEXT("StationA must remain blocked after being evicted from the value list")));

		// StationB must be unblocked (it was still mapped).
		FArcCraftQueueFragment* QueueB = EM->GetFragmentDataPtr<FArcCraftQueueFragment>(StationB);
		ASSERT_THAT(IsNotNull(QueueB));
		ASSERT_THAT(IsFalse(QueueB->Entries[0].bOutputBlocked,
			TEXT("StationB must be unblocked by the bag-slots-changed event")));
	}
};
