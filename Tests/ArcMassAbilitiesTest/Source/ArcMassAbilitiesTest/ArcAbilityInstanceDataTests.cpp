// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Abilities/ArcAbilityStateTreeSubsystem.h"
#include "MassEntitySubsystem.h"
#include "StateTree.h"

// ============================================================================
// ArcAbilityInstanceData — subsystem generation / handle validity
// ============================================================================

TEST_CLASS(ArcAbilityInstanceData, "ArcMassAbilities.Abilities.InstanceData")
{
	FActorTestSpawner Spawner;
	UArcAbilityStateTreeSubsystem* Subsystem = nullptr;
	UStateTree* DummyStateTree = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();

		Subsystem = Spawner.GetWorld().GetSubsystem<UArcAbilityStateTreeSubsystem>();
		check(Subsystem);

		// A minimal non-null UStateTree is all AllocateInstanceData requires (it check()s non-null
		// then calls Item.InstanceData.Reset() — no schema/compile needed for allocation alone).
		DummyStateTree = NewObject<UStateTree>();
	}

	// After a free, the handle that was returned at allocation must no longer be valid and
	// GetInstanceData must return nullptr.
	TEST_METHOD(StaleHandle_InvalidAfterFree)
	{
		FMassStateTreeInstanceHandle Handle = Subsystem->AllocateInstanceData(DummyStateTree);
		ASSERT_THAT(IsTrue(Subsystem->IsValidHandle(Handle)));
		ASSERT_THAT(IsNotNull(Subsystem->GetInstanceData(Handle)));

		Subsystem->FreeInstanceData(Handle);

		ASSERT_THAT(IsFalse(Subsystem->IsValidHandle(Handle)));
		ASSERT_THAT(IsNull(Subsystem->GetInstanceData(Handle)));
	}

	// Freeing the same handle twice must be a no-op.  If the second free inserted the index
	// into the freelist again, the two subsequent allocations would share the same slot.
	// Verify that two allocations after a double-free return handles with DISTINCT indices.
	TEST_METHOD(DoubleFree_NoOp)
	{
		FMassStateTreeInstanceHandle Handle = Subsystem->AllocateInstanceData(DummyStateTree);
		const int32 Index = Handle.GetIndex();

		Subsystem->FreeInstanceData(Handle);  // first free — index enters freelist
		Subsystem->FreeInstanceData(Handle);  // second free — must be a no-op (stale handle)

		FMassStateTreeInstanceHandle H1 = Subsystem->AllocateInstanceData(DummyStateTree);
		FMassStateTreeInstanceHandle H2 = Subsystem->AllocateInstanceData(DummyStateTree);

		// The two new handles must refer to different slots — no aliasing.
		ASSERT_THAT(AreNotEqual(H1.GetIndex(), H2.GetIndex()));
	}

	// Alloc → free → alloc: the new handle must reuse the same slot (freelist recycle) but
	// carry a different generation so the old handle stays invalid while the new one is valid.
	TEST_METHOD(Realloc_ReusesIndexWithNewGeneration)
	{
		FMassStateTreeInstanceHandle H1 = Subsystem->AllocateInstanceData(DummyStateTree);
		const int32 Index = H1.GetIndex();
		const int32 Gen1 = H1.GetGeneration();

		Subsystem->FreeInstanceData(H1);

		FMassStateTreeInstanceHandle H2 = Subsystem->AllocateInstanceData(DummyStateTree);

		// Slot reused.
		ASSERT_THAT(AreEqual(Index, H2.GetIndex()));
		// Generation advanced — old handle is stale, new one is valid.
		ASSERT_THAT(AreNotEqual(Gen1, H2.GetGeneration()));
		ASSERT_THAT(IsNull(Subsystem->GetInstanceData(H1)));
		ASSERT_THAT(IsNotNull(Subsystem->GetInstanceData(H2)));
	}
};
