// Copyright Lukasz Baran. All Rights Reserved.

/**
 * Tests for FArcSkinnedMeshHolderRegistry dense swap-remove + key-map consistency.
 *
 * This is the highest-risk part of the componentless Nanite skinned RT-churn fix (Step 4
 * of the incremental-skinned-instance-update plan): when a middle instance is removed the
 * tail swaps into its slot, and both HolderInstanceKeys (dense parallel array) and
 * HolderEntityToDenseIndex (authoritative reverse map) must be repointed atomically.
 *
 * Fixture:
 *   FActorTestSpawner + InitializeGameSubsystems → UMassEntitySubsystem (real EntityManager).
 *   FArcSkinnedMeshHolderRegistry::FindOrCreateHolder with null SkinnedAsset → holder entity
 *   created with all required fragments + render-state helper. With null asset,
 *   FArcMassInstancedSkinnedMeshRenderStateHelper::CreateRenderState early-returns (guards
 *   on Asset==nullptr at line 199), so no GPU proxy is ever built. AddInstance and
 *   RemoveInstance still run their full bookkeeping paths headlessly.
 *
 * What is asserted:
 *   - After 3 adds: PerInstanceData.Num()==3, HolderInstanceKeys has the correct 3 keys in
 *     order, HolderEntityToDenseIndex maps each key to index 0/1/2.
 *   - After removing the middle key ({E1,0}): PerInstanceData.Num()==2; tail ({E2,0}) moved
 *     to slot 1; HolderInstanceKeys[1]=={E2,0}; HolderEntityToDenseIndex[{E0,0}]==0,
 *     [{E2,0}]==1; {E1,0} absent.
 *   - After a subsequent add ({E3,0}): lands at dense index 2; map agrees.
 *   - Custom data stride: PerInstanceCustomData.Num() == PerInstanceData.Num() * Stride;
 *     surviving instance slab content is correct after the swap (E2's data at slot 1).
 *   - Remove-all (last-instance path): both maps wiped for the holder.
 *
 * NOT tested here (requires a live RT scene): GPU proxy stability, RT shadows, NaniteId.
 */

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/CommandBuffer.h"

#include "ArcMass/SkinnedMeshVisualization/ArcSkinnedMeshHolderRegistry.h"
#include "ArcMass/SkinnedMeshVisualization/ArcMassSkinnedMeshFragments.h"

// FArcHolderInstanceKey + FArcVisualizationMeshFragment
#include "Mesh/ArcISMHolderRegistry.h"
#include "Mesh/ArcRenderMeshFragments.h"

DEFINE_LOG_CATEGORY_STATIC(LogSkinnedHolderRegistryTest, Display, All);

// ---------------------------------------------------------------------------
// Helpers (file-scoped)
// ---------------------------------------------------------------------------
namespace ArcSkinnedHolderRegistryTests_Private
{
	/** Build a dummy source FMassEntityHandle with the given SerialNumber. */
	static FMassEntityHandle MakeDummyHandle(int32 Index, int32 SerialNumber)
	{
		FMassEntityHandle Handle;
		Handle.Index = Index;
		Handle.SerialNumber = SerialNumber;
		return Handle;
	}

	/**
	 * Helper to query the dense index of a source key from the registry.
	 * Returns INDEX_NONE when the key is not present.
	 */
	static int32 GetDenseIndex(
		const FArcSkinnedMeshHolderRegistry& Registry,
		FMassEntityHandle Holder,
		const FArcHolderInstanceKey& Key)
	{
		const TMap<FArcHolderInstanceKey, int32>* EntityToDense =
			Registry.HolderEntityToDenseIndex.Find(Holder);
		if (!EntityToDense)
		{
			return INDEX_NONE;
		}
		const int32* Found = EntityToDense->Find(Key);
		return Found ? *Found : INDEX_NONE;
	}

	/**
	 * Helper to get the key stored at a given dense slot in HolderInstanceKeys.
	 * Returns a default-constructed key (SourceEntity invalid) when out of range.
	 */
	static FArcHolderInstanceKey GetKeyAtSlot(
		const FArcSkinnedMeshHolderRegistry& Registry,
		FMassEntityHandle Holder,
		int32 Slot)
	{
		const TArray<FArcHolderInstanceKey>* Keys = Registry.HolderInstanceKeys.Find(Holder);
		if (!Keys || !Keys->IsValidIndex(Slot))
		{
			return FArcHolderInstanceKey{};
		}
		return (*Keys)[Slot];
	}
} // namespace ArcSkinnedHolderRegistryTests_Private

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

TEST_CLASS(SkinnedHolderRegistry_DenseSwapRemove, "ArcMassPlacedEntity.SkinnedHolderRegistry")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FArcSkinnedMeshHolderRegistry Registry;

	// Bucket that FindOrCreateHolder writes into (simulates the actor-local bucket).
	TMap<FArcSkinnedISMHolderKey, FMassEntityHandle> Bucket;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::bSuppressLogErrors = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique(TEXT("LogUObjectGlobals"));
		FAutomationTestBase::SuppressedLogCategories.AddUnique(TEXT("LogRHI"));
		FAutomationTestBase::SuppressedLogCategories.AddUnique(TEXT("LogMass"));
		FAutomationTestBase::SuppressedLogCategories.AddUnique(TEXT("LogRendering"));

		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		ASSERT_THAT(IsNotNull(MES));
		EntityManager = &MES->GetMutableEntityManager();
	}

	// -------------------------------------------------------------------------
	// Core scenario: add×3, remove-middle, add, assert full bookkeeping integrity.
	// -------------------------------------------------------------------------
	TEST_METHOD(AddRemoveMiddleAdd_BookkeepingConsistent)
	{
		// --- Build the holder via registry (null asset → no GPU proxy, fully headless) ---
		const FArcSkinnedISMHolderKey HolderKey;   // null-asset key
		const FArcMassSkinnedMeshFragment SkinnedMeshFrag{};  // null SkinnedAsset
		const FArcVisualizationMeshFragment VisMeshFrag{};

		const FMassEntityHandle Holder = Registry.FindOrCreateHolder(
			Bucket, HolderKey, SkinnedMeshFrag, VisMeshFrag, nullptr, *EntityManager);

		ASSERT_THAT(IsTrue(Holder.IsValid()));
		ASSERT_THAT(IsTrue(EntityManager->IsEntityValid(Holder)));

		// --- Three source entity handles (dummy, never created in EntityManager) ---
		const FMassEntityHandle E0 = ArcSkinnedHolderRegistryTests_Private::MakeDummyHandle(100, 1);
		const FMassEntityHandle E1 = ArcSkinnedHolderRegistryTests_Private::MakeDummyHandle(101, 1);
		const FMassEntityHandle E2 = ArcSkinnedHolderRegistryTests_Private::MakeDummyHandle(102, 1);

		const FArcHolderInstanceKey Key0{E0, 0};
		const FArcHolderInstanceKey Key1{E1, 0};
		const FArcHolderInstanceKey Key2{E2, 0};

		// bDeferBoundsAndCullingRefresh=true skips the GetConstSharedFragmentDataChecked
		// bounds path in AddInstance (null asset is fine here but avoids any incidental asserts).
		const bool bDefer = true;

		// --- Add E0, E1, E2 ---
		const int32 Idx0 = Registry.AddInstance(Holder, E0, 0, FTransform::Identity, 0, *EntityManager, bDefer);
		const int32 Idx1 = Registry.AddInstance(Holder, E1, 0, FTransform::Identity, 0, *EntityManager, bDefer);
		const int32 Idx2 = Registry.AddInstance(Holder, E2, 0, FTransform::Identity, 0, *EntityManager, bDefer);

		// Dense indices must be 0,1,2 in order.
		ASSERT_THAT(AreEqual(0, Idx0));
		ASSERT_THAT(AreEqual(1, Idx1));
		ASSERT_THAT(AreEqual(2, Idx2));

		// PerInstanceData length.
		const FArcMassRenderISMSkinnedFragment* SkinnedFrag =
			EntityManager->GetFragmentDataPtr<FArcMassRenderISMSkinnedFragment>(Holder);
		ASSERT_THAT(IsNotNull(SkinnedFrag));
		ASSERT_THAT(AreEqual(3, SkinnedFrag->PerInstanceData.Num()));

		// HolderInstanceKeys parallel array: slots 0,1,2 hold Key0,Key1,Key2.
		ASSERT_THAT(IsTrue(ArcSkinnedHolderRegistryTests_Private::GetKeyAtSlot(Registry, Holder, 0) == Key0));
		ASSERT_THAT(IsTrue(ArcSkinnedHolderRegistryTests_Private::GetKeyAtSlot(Registry, Holder, 1) == Key1));
		ASSERT_THAT(IsTrue(ArcSkinnedHolderRegistryTests_Private::GetKeyAtSlot(Registry, Holder, 2) == Key2));

		// HolderEntityToDenseIndex reverse map.
		ASSERT_THAT(AreEqual(0, ArcSkinnedHolderRegistryTests_Private::GetDenseIndex(Registry, Holder, Key0)));
		ASSERT_THAT(AreEqual(1, ArcSkinnedHolderRegistryTests_Private::GetDenseIndex(Registry, Holder, Key1)));
		ASSERT_THAT(AreEqual(2, ArcSkinnedHolderRegistryTests_Private::GetDenseIndex(Registry, Holder, Key2)));

		// --- Remove the MIDDLE instance (E1, dense index 1) ---
		// Track which entity and slot the OnInstanceMoved callback reported. The callback is
		// dispatched inside a FMassDeferredSetCommand and only fires when the moved entity is
		// valid in the EntityManager. Our source entities are dummy handles (not created in
		// the manager) so the deferred guard skips the lambda body — that is correct behaviour
		// for the cache-patch path. The bookkeeping maps (HolderInstanceKeys + HolderEntityToDenseIndex)
		// are updated SYNCHRONOUSLY before the deferred command is enqueued, so we assert those
		// directly without needing to flush.
		FMassEntityHandle MovedEntityFromCallback;
		int32 MovedToSlotFromCallback = INDEX_NONE;

		const bool bHolderEmptied = Registry.RemoveInstance(
			Holder, E1, 0, *EntityManager,
			[&MovedEntityFromCallback, &MovedToSlotFromCallback]
			(FMassEntityManager& /*EM*/, FMassEntityHandle InMoved, int32 InSlot)
			{
				MovedEntityFromCallback = InMoved;
				MovedToSlotFromCallback = InSlot;
			});

		// Holder must NOT be emptied (2 remain).
		ASSERT_THAT(IsFalse(bHolderEmptied));

		// PerInstanceData shrinks to 2.
		ASSERT_THAT(AreEqual(2, SkinnedFrag->PerInstanceData.Num()));

		// Tail (E2) swapped into slot 1 — synchronous map update.
		ASSERT_THAT(IsTrue(ArcSkinnedHolderRegistryTests_Private::GetKeyAtSlot(Registry, Holder, 0) == Key0));
		ASSERT_THAT(IsTrue(ArcSkinnedHolderRegistryTests_Private::GetKeyAtSlot(Registry, Holder, 1) == Key2));

		// Reverse map: E0→0, E2→1, E1 gone — all synchronous, no flush needed.
		ASSERT_THAT(AreEqual(0, ArcSkinnedHolderRegistryTests_Private::GetDenseIndex(Registry, Holder, Key0)));
		ASSERT_THAT(AreEqual(1, ArcSkinnedHolderRegistryTests_Private::GetDenseIndex(Registry, Holder, Key2)));
		ASSERT_THAT(AreEqual(INDEX_NONE, ArcSkinnedHolderRegistryTests_Private::GetDenseIndex(Registry, Holder, Key1)));

		// --- Add E3 after the removal ---
		const FMassEntityHandle E3 = ArcSkinnedHolderRegistryTests_Private::MakeDummyHandle(103, 1);
		const FArcHolderInstanceKey Key3{E3, 0};

		const int32 Idx3 = Registry.AddInstance(Holder, E3, 0, FTransform::Identity, 0, *EntityManager, bDefer);

		// Lands at dense index 2.
		ASSERT_THAT(AreEqual(2, Idx3));
		ASSERT_THAT(AreEqual(3, SkinnedFrag->PerInstanceData.Num()));
		ASSERT_THAT(AreEqual(2, ArcSkinnedHolderRegistryTests_Private::GetDenseIndex(Registry, Holder, Key3)));
		ASSERT_THAT(IsTrue(ArcSkinnedHolderRegistryTests_Private::GetKeyAtSlot(Registry, Holder, 2) == Key3));
	}

	// -------------------------------------------------------------------------
	// Custom-data stride: PerInstanceCustomData stays parallel to PerInstanceData.
	// After a middle removal the surviving instances' custom-data slabs must be correct.
	// -------------------------------------------------------------------------
	TEST_METHOD(CustomData_StayParallelAfterSwapRemove)
	{
		const FArcSkinnedISMHolderKey HolderKey;
		const FArcMassSkinnedMeshFragment SkinnedMeshFrag{};
		const FArcVisualizationMeshFragment VisMeshFrag{};

		const FMassEntityHandle Holder = Registry.FindOrCreateHolder(
			Bucket, HolderKey, SkinnedMeshFrag, VisMeshFrag, nullptr, *EntityManager);
		ASSERT_THAT(IsTrue(Holder.IsValid()));

		// Set a non-zero stride so the custom-data slab logic runs.
		FArcMassRenderISMSkinnedFragment* SkinnedFrag =
			EntityManager->GetFragmentDataPtr<FArcMassRenderISMSkinnedFragment>(Holder);
		ASSERT_THAT(IsNotNull(SkinnedFrag));
		const int32 Stride = 3;
		SkinnedFrag->NumCustomDataFloats = Stride;

		const FMassEntityHandle E0 = ArcSkinnedHolderRegistryTests_Private::MakeDummyHandle(200, 2);
		const FMassEntityHandle E1 = ArcSkinnedHolderRegistryTests_Private::MakeDummyHandle(201, 2);
		const FMassEntityHandle E2 = ArcSkinnedHolderRegistryTests_Private::MakeDummyHandle(202, 2);

		const FArcHolderInstanceKey Key0{E0, 0};
		const FArcHolderInstanceKey Key1{E1, 0};
		const FArcHolderInstanceKey Key2{E2, 0};

		Registry.AddInstance(Holder, E0, 0, FTransform::Identity, 0, *EntityManager, /*bDefer=*/true);
		Registry.AddInstance(Holder, E1, 0, FTransform::Identity, 0, *EntityManager, /*bDefer=*/true);
		Registry.AddInstance(Holder, E2, 0, FTransform::Identity, 0, *EntityManager, /*bDefer=*/true);

		ASSERT_THAT(AreEqual(3 * Stride, SkinnedFrag->PerInstanceCustomData.Num()));

		// Write distinct custom data per instance so we can verify slab identity after the swap.
		// Instance 0 (E0): 1.0, 1.1, 1.2
		// Instance 1 (E1): 2.0, 2.1, 2.2
		// Instance 2 (E2): 3.0, 3.1, 3.2
		SkinnedFrag->PerInstanceCustomData[0 * Stride + 0] = 1.0f;
		SkinnedFrag->PerInstanceCustomData[0 * Stride + 1] = 1.1f;
		SkinnedFrag->PerInstanceCustomData[0 * Stride + 2] = 1.2f;

		SkinnedFrag->PerInstanceCustomData[1 * Stride + 0] = 2.0f;
		SkinnedFrag->PerInstanceCustomData[1 * Stride + 1] = 2.1f;
		SkinnedFrag->PerInstanceCustomData[1 * Stride + 2] = 2.2f;

		SkinnedFrag->PerInstanceCustomData[2 * Stride + 0] = 3.0f;
		SkinnedFrag->PerInstanceCustomData[2 * Stride + 1] = 3.1f;
		SkinnedFrag->PerInstanceCustomData[2 * Stride + 2] = 3.2f;

		// Remove middle (E1, slot 1). E2's slab must be copied into slot 1.
		Registry.RemoveInstance(Holder, E1, 0, *EntityManager, nullptr);

		// After removal: 2 instances, 6 floats.
		ASSERT_THAT(AreEqual(2, SkinnedFrag->PerInstanceData.Num()));
		ASSERT_THAT(AreEqual(2 * Stride, SkinnedFrag->PerInstanceCustomData.Num()));

		// Slot 0 (E0): still 1.0, 1.1, 1.2.
		ASSERT_THAT(IsTrue(SkinnedFrag->PerInstanceCustomData[0 * Stride + 0] == 1.0f));
		ASSERT_THAT(IsTrue(SkinnedFrag->PerInstanceCustomData[0 * Stride + 1] == 1.1f));
		ASSERT_THAT(IsTrue(SkinnedFrag->PerInstanceCustomData[0 * Stride + 2] == 1.2f));

		// Slot 1 now holds E2's slab: 3.0, 3.1, 3.2.
		ASSERT_THAT(IsTrue(SkinnedFrag->PerInstanceCustomData[1 * Stride + 0] == 3.0f));
		ASSERT_THAT(IsTrue(SkinnedFrag->PerInstanceCustomData[1 * Stride + 1] == 3.1f));
		ASSERT_THAT(IsTrue(SkinnedFrag->PerInstanceCustomData[1 * Stride + 2] == 3.2f));
	}

	// -------------------------------------------------------------------------
	// Last-instance path: RemoveInstance returns true and wipes the holder maps.
	// -------------------------------------------------------------------------
	TEST_METHOD(RemoveLastInstance_WipesHolderMaps)
	{
		// Use a distinct bucket key so this holder doesn't collide with other tests.
		FArcSkinnedISMHolderKey HolderKey;
		HolderKey.ConfigHash = 0xDEAD;

		const FArcMassSkinnedMeshFragment SkinnedMeshFrag{};
		const FArcVisualizationMeshFragment VisMeshFrag{};

		TMap<FArcSkinnedISMHolderKey, FMassEntityHandle> LocalBucket;
		const FMassEntityHandle Holder = Registry.FindOrCreateHolder(
			LocalBucket, HolderKey, SkinnedMeshFrag, VisMeshFrag, nullptr, *EntityManager);
		ASSERT_THAT(IsTrue(Holder.IsValid()));

		const FMassEntityHandle E0 = ArcSkinnedHolderRegistryTests_Private::MakeDummyHandle(300, 3);

		Registry.AddInstance(Holder, E0, 0, FTransform::Identity, 0, *EntityManager, /*bDefer=*/true);

		// Single-instance remove → last-instance path → returns true.
		const bool bEmptied = Registry.RemoveInstance(Holder, E0, 0, *EntityManager, nullptr);
		ASSERT_THAT(IsTrue(bEmptied));

		// Both maps must have been wiped for this holder.
		const TArray<FArcHolderInstanceKey>* Keys = Registry.HolderInstanceKeys.Find(Holder);
		const TMap<FArcHolderInstanceKey, int32>* EntityToDense = Registry.HolderEntityToDenseIndex.Find(Holder);
		ASSERT_THAT(IsNull(Keys));
		ASSERT_THAT(IsNull(EntityToDense));
	}

	// -------------------------------------------------------------------------
	// Removing the LAST element (tail, not middle): no swap occurs, map still consistent.
	// -------------------------------------------------------------------------
	TEST_METHOD(RemoveTail_NoSwap_MapsConsistent)
	{
		FArcSkinnedISMHolderKey HolderKey;
		HolderKey.ConfigHash = 0xBEEF;

		const FArcMassSkinnedMeshFragment SkinnedMeshFrag{};
		const FArcVisualizationMeshFragment VisMeshFrag{};

		TMap<FArcSkinnedISMHolderKey, FMassEntityHandle> LocalBucket;
		const FMassEntityHandle Holder = Registry.FindOrCreateHolder(
			LocalBucket, HolderKey, SkinnedMeshFrag, VisMeshFrag, nullptr, *EntityManager);
		ASSERT_THAT(IsTrue(Holder.IsValid()));

		const FMassEntityHandle E0 = ArcSkinnedHolderRegistryTests_Private::MakeDummyHandle(400, 4);
		const FMassEntityHandle E1 = ArcSkinnedHolderRegistryTests_Private::MakeDummyHandle(401, 4);

		const FArcHolderInstanceKey Key0{E0, 0};
		const FArcHolderInstanceKey Key1{E1, 0};

		Registry.AddInstance(Holder, E0, 0, FTransform::Identity, 0, *EntityManager, /*bDefer=*/true);
		Registry.AddInstance(Holder, E1, 0, FTransform::Identity, 0, *EntityManager, /*bDefer=*/true);

		// Remove the TAIL (E1, index 1): Index==LastIndex → no swap, just pop.
		const bool bEmptied = Registry.RemoveInstance(Holder, E1, 0, *EntityManager, nullptr);
		ASSERT_THAT(IsFalse(bEmptied));

		const FArcMassRenderISMSkinnedFragment* SkinnedFrag =
			EntityManager->GetFragmentDataPtr<FArcMassRenderISMSkinnedFragment>(Holder);
		ASSERT_THAT(IsNotNull(SkinnedFrag));
		ASSERT_THAT(AreEqual(1, SkinnedFrag->PerInstanceData.Num()));

		// E0 still at slot 0, E1 gone.
		ASSERT_THAT(IsTrue(ArcSkinnedHolderRegistryTests_Private::GetKeyAtSlot(Registry, Holder, 0) == Key0));
		ASSERT_THAT(AreEqual(0, ArcSkinnedHolderRegistryTests_Private::GetDenseIndex(Registry, Holder, Key0)));
		ASSERT_THAT(AreEqual(INDEX_NONE, ArcSkinnedHolderRegistryTests_Private::GetDenseIndex(Registry, Holder, Key1)));
	}
};
