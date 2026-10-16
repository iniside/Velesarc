// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassEquipmentAvailabilityTests.h"
#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Mass/EntityHandle.h"
#include "Mass/EntityFragments.h"
#include "StructUtils/InstancedStruct.h"
#include "NativeGameplayTags.h"
#include "GameplayTagContainer.h"
#include "Equipment/ArcMassEquipmentTypes.h"
#include "Equipment/ArcMassEquipmentFragment.h"
#include "Equipment/ArcMassEquipmentUtils.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Tags/ArcGameplayTagRequirements.h"
#include "Items/ArcItemId.h"

// ---------------------------------------------------------------------------
// Native test tags
// ---------------------------------------------------------------------------

// Owner tag used to gate availability by owned tags
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Equipment_OwnerTag_A, "Test.Equipment.OwnerTag.A");
// SlotId tag used in RequireSlotOccupied tests (represents the "belt" slot)
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Equipment_Slot_Belt, "Test.Equipment.Slot.Belt");
// A second slot used for the multi-condition AND test
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Equipment_Slot_Chest, "Test.Equipment.Slot.Chest");

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

TEST_CLASS(ArcMassEquipmentAvailability, "ArcMassItems.Equipment.Availability")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;

	/** Creates a minimal entity and adds the two sparse fragments the conditions read. */
	FMassEntityHandle CreateEquipmentEntity()
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment()));
		FMassEntityHandle NewEntity = EntityManager->CreateEntity(Fragments);
		EntityManager->AddSparseElementToEntity<FArcOwnedTagsFragment>(NewEntity);
		EntityManager->AddSparseElementToEntity<FArcMassEquipmentFragment>(NewEntity);
		return NewEntity;
	}

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();
		Entity = CreateEquipmentEntity();
	}

	// -----------------------------------------------------------------------
	// Test 1 — empty AvailabilityConditions list always returns true
	// -----------------------------------------------------------------------
	TEST_METHOD(NoConditions_IsAvailable)
	{
		FArcMassEquipmentSlot Slot;
		Slot.SlotId = TAG_Test_Equipment_Slot_Belt;
		// AvailabilityConditions is empty by default

		bool bAvailable = Arc::MassEquipment::IsSlotAvailable(*EntityManager, Entity, Slot);
		ASSERT_THAT(IsTrue(bAvailable));
	}

	// -----------------------------------------------------------------------
	// Test 2 — _OwnerTags requires a tag the entity does NOT have → false
	// -----------------------------------------------------------------------
	TEST_METHOD(OwnerTags_MissingTag_IsUnavailable)
	{
		FArcMassEquipmentSlotAvailability_OwnerTags Cond;
		Cond.OwnerTags.RequireTags.AddTag(TAG_Test_Equipment_OwnerTag_A);

		FArcMassEquipmentSlot Slot;
		Slot.SlotId = TAG_Test_Equipment_Slot_Belt;
		Slot.AvailabilityConditions.Add(FInstancedStruct::Make(Cond));

		// Entity has FArcOwnedTagsFragment but no tags added yet
		bool bAvailable = Arc::MassEquipment::IsSlotAvailable(*EntityManager, Entity, Slot);
		ASSERT_THAT(IsFalse(bAvailable));
	}

	// -----------------------------------------------------------------------
	// Test 3 — same slot, after AddTag the requirement is met → true
	// -----------------------------------------------------------------------
	TEST_METHOD(OwnerTags_TagPresent_IsAvailable)
	{
		FArcMassEquipmentSlotAvailability_OwnerTags Cond;
		Cond.OwnerTags.RequireTags.AddTag(TAG_Test_Equipment_OwnerTag_A);

		FArcMassEquipmentSlot Slot;
		Slot.SlotId = TAG_Test_Equipment_Slot_Belt;
		Slot.AvailabilityConditions.Add(FInstancedStruct::Make(Cond));

		// Add the required tag to the entity
		FArcOwnedTagsFragment* OwnedFrag = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		ASSERT_THAT(IsNotNull(OwnedFrag));
		ArcAbilities::AddOwnedTags(*EntityManager, Entity, FGameplayTagContainer(TAG_Test_Equipment_OwnerTag_A));

		bool bAvailable = Arc::MassEquipment::IsSlotAvailable(*EntityManager, Entity, Slot);
		ASSERT_THAT(IsTrue(bAvailable));
	}

	// -----------------------------------------------------------------------
	// Test 4 — _RequireSlotOccupied: empty EquippedItems → false;
	//          after Add(Belt) → true
	// -----------------------------------------------------------------------
	TEST_METHOD(RequireSlotOccupied_EmptyEquippedItems_IsUnavailable)
	{
		FArcMassEquipmentSlotAvailability_RequireSlotOccupied Cond;
		Cond.RequiredSlotId = TAG_Test_Equipment_Slot_Belt;

		FArcMassEquipmentSlot Slot;
		Slot.SlotId = TAG_Test_Equipment_Slot_Chest;
		Slot.AvailabilityConditions.Add(FInstancedStruct::Make(Cond));

		// EquippedItems map is empty
		bool bAvailable = Arc::MassEquipment::IsSlotAvailable(*EntityManager, Entity, Slot);
		ASSERT_THAT(IsFalse(bAvailable));
	}

	TEST_METHOD(RequireSlotOccupied_SlotOccupied_IsAvailable)
	{
		FArcMassEquipmentSlotAvailability_RequireSlotOccupied Cond;
		Cond.RequiredSlotId = TAG_Test_Equipment_Slot_Belt;

		FArcMassEquipmentSlot Slot;
		Slot.SlotId = TAG_Test_Equipment_Slot_Chest;
		Slot.AvailabilityConditions.Add(FInstancedStruct::Make(Cond));

		// Directly populate EquippedItems (single-threaded test body, not inside a processor)
		FArcMassEquipmentFragment* EquipFrag = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassEquipmentFragment>();
		ASSERT_THAT(IsNotNull(EquipFrag));
		EquipFrag->EquippedItems.Add(TAG_Test_Equipment_Slot_Belt, FArcItemId::Generate());

		bool bAvailable = Arc::MassEquipment::IsSlotAvailable(*EntityManager, Entity, Slot);
		ASSERT_THAT(IsTrue(bAvailable));
	}

	// -----------------------------------------------------------------------
	// Test 5 — multiple conditions AND: one failing → false; both passing → true
	// -----------------------------------------------------------------------
	TEST_METHOD(MultipleConditions_OneFailing_IsUnavailable)
	{
		// Condition A: owner must have OwnerTag.A (will fail — not added)
		FArcMassEquipmentSlotAvailability_OwnerTags CondA;
		CondA.OwnerTags.RequireTags.AddTag(TAG_Test_Equipment_OwnerTag_A);

		// Condition B: belt slot must be occupied (will pass — we add it)
		FArcMassEquipmentSlotAvailability_RequireSlotOccupied CondB;
		CondB.RequiredSlotId = TAG_Test_Equipment_Slot_Belt;

		FArcMassEquipmentFragment* EquipFrag = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassEquipmentFragment>();
		ASSERT_THAT(IsNotNull(EquipFrag));
		EquipFrag->EquippedItems.Add(TAG_Test_Equipment_Slot_Belt, FArcItemId::Generate());

		FArcMassEquipmentSlot Slot;
		Slot.SlotId = TAG_Test_Equipment_Slot_Chest;
		Slot.AvailabilityConditions.Add(FInstancedStruct::Make(CondA)); // fails
		Slot.AvailabilityConditions.Add(FInstancedStruct::Make(CondB)); // passes

		bool bAvailable = Arc::MassEquipment::IsSlotAvailable(*EntityManager, Entity, Slot);
		ASSERT_THAT(IsFalse(bAvailable));
	}

	TEST_METHOD(MultipleConditions_BothPassing_IsAvailable)
	{
		// Condition A: owner must have OwnerTag.A
		FArcMassEquipmentSlotAvailability_OwnerTags CondA;
		CondA.OwnerTags.RequireTags.AddTag(TAG_Test_Equipment_OwnerTag_A);

		// Condition B: belt slot must be occupied
		FArcMassEquipmentSlotAvailability_RequireSlotOccupied CondB;
		CondB.RequiredSlotId = TAG_Test_Equipment_Slot_Belt;

		// Satisfy both
		FArcOwnedTagsFragment* OwnedFrag = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		ASSERT_THAT(IsNotNull(OwnedFrag));
		ArcAbilities::AddOwnedTags(*EntityManager, Entity, FGameplayTagContainer(TAG_Test_Equipment_OwnerTag_A));

		FArcMassEquipmentFragment* EquipFrag = FMassEntityView(*EntityManager, Entity).GetSparseFragmentDataPtr<FArcMassEquipmentFragment>();
		ASSERT_THAT(IsNotNull(EquipFrag));
		EquipFrag->EquippedItems.Add(TAG_Test_Equipment_Slot_Belt, FArcItemId::Generate());

		FArcMassEquipmentSlot Slot;
		Slot.SlotId = TAG_Test_Equipment_Slot_Chest;
		Slot.AvailabilityConditions.Add(FInstancedStruct::Make(CondA));
		Slot.AvailabilityConditions.Add(FInstancedStruct::Make(CondB));

		bool bAvailable = Arc::MassEquipment::IsSlotAvailable(*EntityManager, Entity, Slot);
		ASSERT_THAT(IsTrue(bAvailable));
	}

	// -----------------------------------------------------------------------
	// Test 6 — virtual dispatch: FArcMassEquipmentSlotCondition::CanEquip
	//          through FInstancedStruct; test-only subclass returns false
	// -----------------------------------------------------------------------
	TEST_METHOD(CustomCondition_VirtualDispatch_ReturnsFalse)
	{
		FArcMassEquipmentSlot Slot;
		Slot.SlotId = TAG_Test_Equipment_Slot_Belt;
		Slot.CustomCondition = FInstancedStruct::Make(FArcTestEquipmentSlotCondition_AlwaysFalse());

		// Retrieve via FInstancedStruct::GetPtr — proves virtual dispatch without
		// needing FArcMassEquipmentSharedFragment / FEntityBuilder preset plumbing
		const FArcMassEquipmentSlotCondition* Cond = Slot.CustomCondition.GetPtr<FArcMassEquipmentSlotCondition>();
		ASSERT_THAT(IsNotNull(Cond));

		bool bCanEquip = Cond->CanEquip(Entity, *EntityManager, nullptr, nullptr);
		ASSERT_THAT(IsFalse(bCanEquip));
	}
};
