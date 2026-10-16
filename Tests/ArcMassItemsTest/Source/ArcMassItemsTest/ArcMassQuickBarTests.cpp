// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "Mass/Executor.h"
#include "Mass/EntityHandle.h"
#include "Mass/EntityFragments.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/SharedStruct.h"
#include "NativeGameplayTags.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "Operations/ArcMassItemOperations.h"
#include "QuickBar/ArcMassQuickBarConfig.h"
#include "QuickBar/ArcMassQuickBarSharedFragment.h"
#include "QuickBar/ArcMassQuickBarStateFragment.h"
#include "QuickBar/ArcMassQuickBarTypes.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Core/ArcCoreAssetManager.h"

#include "ArcMassItemTestsSettings.h"
#include "ArcMassQuickBarTestHelpers.h"
#include "QuickBar/ArcMassQuickBarOperations.h"
#include "QuickBar/ArcMassQuickBarAutoSelect.h"
#include "ArcMassQuickBarTestStructs.h"

#include "Items/ArcItemsHelpers.h"
#include "ItemFragments/ArcItemFragment_GrantedMassAbilities.h"
#include "QuickBar/ArcMassQuickBarInputBinding.h"
#include "QuickBar/ArcMassQuickBarInputBinding_Ability.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Abilities/ArcAbilityHandle.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_QBTest_QuickSlot, "Test.QuickBar.QuickSlot");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_QBTest_ItemSlot, "Test.QuickBar.ItemSlot");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_QBTest_NonMatchingSlot, "Test.QuickBar.NonMatching");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_QBTest_Bar, "Test.QuickBar.Bar");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_QBTest_BarA, "Test.QuickBar.BarA");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_QBTest_BarB, "Test.QuickBar.BarB");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_QBTest_BarC, "Test.QuickBar.BarC");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_QBTest_Group, "Test.QuickBar.Group");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_QBTest_QuickSlotA, "Test.QuickBar.SlotA");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_QBTest_QuickSlotB, "Test.QuickBar.SlotB");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_QBTest_QuickSlotC, "Test.QuickBar.SlotC");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_QBTest_ItemSlotA, "Test.QuickBar.ItemSlotA");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_QBTest_ItemSlotB, "Test.QuickBar.ItemSlotB");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_QBTest_Input_Fire, "Test.InputTag.Fire");

TEST_CLASS(ArcMassQuickBar, "ArcMassItems.QuickBar")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	const UArcMassItemTestsSettings* MassSettings = nullptr;

	UArcMassQuickBarConfig* MakeBasicConfig()
	{
		return ArcMassItems::QuickBar::TestHelpers::BuildQuickBarConfig([](UArcMassQuickBarConfig& Cfg)
		{
			FArcMassQuickBarEntry& BarEntry = Cfg.QuickBars.AddDefaulted_GetRef();
			BarEntry.BarId = TAG_QBTest_Bar;
			FArcMassQuickBarSlot& QSlot = BarEntry.Slots.AddDefaulted_GetRef();
			QSlot.QuickBarSlotId = TAG_QBTest_QuickSlot;
			QSlot.ItemSlotId = TAG_QBTest_ItemSlot;
			QSlot.bAutoSelect = true;
		});
	}

	UArcMassQuickBarConfig* MakeConfigWithHandler()
	{
		return ArcMassItems::QuickBar::TestHelpers::BuildQuickBarConfig([](UArcMassQuickBarConfig& Cfg)
		{
			FArcMassQuickBarEntry& BarEntry = Cfg.QuickBars.AddDefaulted_GetRef();
			BarEntry.BarId = TAG_QBTest_Bar;
			FArcMassQuickBarSlot& QSlot = BarEntry.Slots.AddDefaulted_GetRef();
			QSlot.QuickBarSlotId = TAG_QBTest_QuickSlot;
			QSlot.ItemSlotId = TAG_QBTest_ItemSlot;
			QSlot.SelectedHandlers.Add(FInstancedStruct::Make(FArcQuickBarTestSlotHandler()));
		});
	}

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();
		MassSettings = GetDefault<UArcMassItemTestsSettings>();

	}

	TEST_METHOD(AutoSelect_ItemSlottedToMatchingSlot_AddsQuickBarSlot)
	{
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, MakeBasicConfig());

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ASSERT_THAT(IsTrue(ItemId.IsValid()));

		FGameplayTag ItemSlotTag = TAG_QBTest_ItemSlot.GetTag();
		ASSERT_THAT(IsTrue(ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, ItemSlotTag)));

		ArcMassItems::QuickBar::TestHelpers::RunAutoSelect(*EntityManager, Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
		ASSERT_THAT(IsNotNull(State));
		ASSERT_THAT(AreEqual(1, State->ActiveSlots.Num()));
		ASSERT_THAT(AreEqual(TAG_QBTest_QuickSlot.GetTag(), State->ActiveSlots[0].QuickSlotId));
		ASSERT_THAT(AreEqual(TAG_QBTest_Bar.GetTag(), State->ActiveSlots[0].BarId));
		ASSERT_THAT(IsTrue(State->ActiveSlots[0].AssignedItemId == ItemId));
	}

	TEST_METHOD(AutoSelect_ItemSlottedToNonMatchingSlot_DoesNotAddQuickBarSlot)
	{
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, MakeBasicConfig());

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		FGameplayTag NonMatching = TAG_QBTest_NonMatchingSlot.GetTag();
		ASSERT_THAT(IsTrue(ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, NonMatching)));

		ArcMassItems::QuickBar::TestHelpers::RunAutoSelect(*EntityManager, Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
		ASSERT_THAT(IsNotNull(State));
		ASSERT_THAT(AreEqual(0, State->ActiveSlots.Num()));
	}

	TEST_METHOD(AutoSelect_ItemRemovedFromSlot_RemovesQuickBarSlot)
	{
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, MakeBasicConfig());

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		FGameplayTag SlotTag = TAG_QBTest_ItemSlot.GetTag();
		ASSERT_THAT(IsTrue(ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, SlotTag)));

		{
			ArcMassItems::QuickBar::TestHelpers::RunAutoSelect(*EntityManager, Entity);
		}

		{
			FMassEntityView View(*EntityManager, Entity);
			const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
			ASSERT_THAT(IsNotNull(State));
			ASSERT_THAT(AreEqual(1, State->ActiveSlots.Num()));
		}

		ASSERT_THAT(IsTrue(ArcMassItems::RemoveItemFromSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId)));

		{
			ArcMassItems::QuickBar::TestHelpers::RunAutoSelect(*EntityManager, Entity);
		}

		{
			FMassEntityView View(*EntityManager, Entity);
			const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
			ASSERT_THAT(IsNotNull(State));
			ASSERT_THAT(AreEqual(0, State->ActiveSlots.Num()));
		}
	}

	TEST_METHOD(AutoSelect_ManualActivationOnly_DoesNotAutoActivate_EvenWhenSlotAutoSelectTrue)
	{
		// Regression: ManualActivationOnly must be authoritative over per-slot bAutoSelect.
		// FArcMassQuickBarSlot::bAutoSelect defaults to true, so prior to the fix at
		// AddAndActivateSlot's mode switch, ManualActivationOnly bars still auto-activated
		// any slot that received an item.
		UArcMassQuickBarConfig* Cfg = ArcMassItems::QuickBar::TestHelpers::BuildQuickBarConfig(
			[](UArcMassQuickBarConfig& C)
		{
			FArcMassQuickBarEntry& BarEntry = C.QuickBars.AddDefaulted_GetRef();
			BarEntry.BarId = TAG_QBTest_Bar;
			BarEntry.QuickSlotsMode = EArcMassQuickSlotsMode::ManualActivationOnly;
			FArcMassQuickBarSlot& QSlot = BarEntry.Slots.AddDefaulted_GetRef();
			QSlot.QuickBarSlotId = TAG_QBTest_QuickSlot;
			QSlot.ItemSlotId = TAG_QBTest_ItemSlot;
			// bAutoSelect intentionally left at its true default.
		});

		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, Cfg);
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ASSERT_THAT(IsTrue(ItemId.IsValid()));
		ASSERT_THAT(IsTrue(ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, TAG_QBTest_ItemSlot.GetTag())));

		ArcMassItems::QuickBar::TestHelpers::RunAutoSelect(*EntityManager, Entity);

		// Slot was added (entry exists) but is NOT active.
		FMassEntityView View(*EntityManager, Entity);
		const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
		ASSERT_THAT(IsNotNull(State));
		ASSERT_THAT(AreEqual(1, State->ActiveSlots.Num()));
		ASSERT_THAT(IsFalse(State->ActiveSlots[0].bIsSlotActive));
		ASSERT_THAT(IsFalse(ArcMassItems::QuickBar::IsSlotActive(*EntityManager, Entity, TAG_QBTest_Bar, TAG_QBTest_QuickSlot)));

		// Manual activation still works.
		ASSERT_THAT(IsTrue(ArcMassItems::QuickBar::ActivateSlot(*EntityManager, Entity, TAG_QBTest_Bar, TAG_QBTest_QuickSlot)));
		ASSERT_THAT(IsTrue(ArcMassItems::QuickBar::IsSlotActive(*EntityManager, Entity, TAG_QBTest_Bar, TAG_QBTest_QuickSlot)));
	}

	TEST_METHOD(Slot_ActivateSlot_TogglesActiveAndFiresHandler)
	{
		FArcQuickBarTestSlotHandler::Reset();
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, MakeConfigWithHandler());
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		ArcMassItems::QuickBar::AddAndActivateSlot(*EntityManager, Entity,
			TAG_QBTest_Bar, TAG_QBTest_QuickSlot, ItemId);

		ASSERT_THAT(AreEqual(1, FArcQuickBarTestSlotHandler::SelectedCount));
		ASSERT_THAT(IsTrue(ArcMassItems::QuickBar::IsSlotActive(*EntityManager, Entity, TAG_QBTest_Bar, TAG_QBTest_QuickSlot)));
	}

	TEST_METHOD(Slot_ActivateSlot_OnAlreadyActive_IsIdempotent)
	{
		FArcQuickBarTestSlotHandler::Reset();
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, MakeConfigWithHandler());
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		// First add+activate (auto-activate via bAutoSelect=true default).
		ArcMassItems::QuickBar::AddAndActivateSlot(*EntityManager, Entity,
			TAG_QBTest_Bar, TAG_QBTest_QuickSlot, ItemId);
		ASSERT_THAT(AreEqual(1, FArcQuickBarTestSlotHandler::SelectedCount));

		// Second activate should be a no-op for the handler counter.
		bool bResult = ArcMassItems::QuickBar::ActivateSlot(*EntityManager, Entity,
			TAG_QBTest_Bar, TAG_QBTest_QuickSlot);

		ASSERT_THAT(IsTrue(bResult));
		ASSERT_THAT(AreEqual(1, FArcQuickBarTestSlotHandler::SelectedCount));
	}

	TEST_METHOD(Slot_DeactivateSlot_TogglesAndFiresDeselected)
	{
		FArcQuickBarTestSlotHandler::Reset();
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, MakeConfigWithHandler());
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::QuickBar::AddAndActivateSlot(*EntityManager, Entity,
			TAG_QBTest_Bar, TAG_QBTest_QuickSlot, ItemId);

		ASSERT_THAT(IsTrue(ArcMassItems::QuickBar::DeactivateSlot(*EntityManager, Entity,
			TAG_QBTest_Bar, TAG_QBTest_QuickSlot)));

		ASSERT_THAT(AreEqual(1, FArcQuickBarTestSlotHandler::DeselectedCount));
		ASSERT_THAT(IsFalse(ArcMassItems::QuickBar::IsSlotActive(*EntityManager, Entity, TAG_QBTest_Bar, TAG_QBTest_QuickSlot)));
	}

	TEST_METHOD(Slot_DeactivateSlot_OnInactive_IsIdempotent)
	{
		FArcQuickBarTestSlotHandler::Reset();
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, MakeConfigWithHandler());
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);

		ArcMassItems::QuickBar::AddAndActivateSlot(*EntityManager, Entity,
			TAG_QBTest_Bar, TAG_QBTest_QuickSlot, ItemId);
		ArcMassItems::QuickBar::DeactivateSlot(*EntityManager, Entity,
			TAG_QBTest_Bar, TAG_QBTest_QuickSlot);
		FArcQuickBarTestSlotHandler::Reset();

		bool bResult = ArcMassItems::QuickBar::DeactivateSlot(*EntityManager, Entity,
			TAG_QBTest_Bar, TAG_QBTest_QuickSlot);

		ASSERT_THAT(IsTrue(bResult));
		ASSERT_THAT(AreEqual(0, FArcQuickBarTestSlotHandler::DeselectedCount));
	}

	TEST_METHOD(Bar_ActivateBar_ActivatesAllSlotsAndFiresAction)
	{
		FArcQuickBarTestBarAction::Reset();
		UArcMassQuickBarConfig* Cfg = ArcMassItems::QuickBar::TestHelpers::BuildQuickBarConfig([](UArcMassQuickBarConfig& C)
		{
			FArcMassQuickBarEntry& Bar = C.QuickBars.AddDefaulted_GetRef();
			Bar.BarId = TAG_QBTest_Bar;
			Bar.QuickBarSelectedActions.Add(FInstancedStruct::Make(FArcQuickBarTestBarAction()));
			FArcMassQuickBarSlot& S1 = Bar.Slots.AddDefaulted_GetRef();
			S1.QuickBarSlotId = TAG_QBTest_QuickSlot;
			S1.ItemSlotId = TAG_QBTest_ItemSlot;
		});
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, Cfg);
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::QuickBar::AddAndActivateSlot(*EntityManager, Entity,
			TAG_QBTest_Bar, TAG_QBTest_QuickSlot, ItemId);

		ArcMassItems::QuickBar::ActivateBar(*EntityManager, Entity, TAG_QBTest_Bar);

		ASSERT_THAT(AreEqual(1, FArcQuickBarTestBarAction::ActivatedCount));
		ASSERT_THAT(IsTrue(ArcMassItems::QuickBar::IsSlotActive(*EntityManager, Entity, TAG_QBTest_Bar, TAG_QBTest_QuickSlot)));
	}

	TEST_METHOD(Bar_DeactivateBar_DeactivatesAllSlotsAndFiresAction)
	{
		FArcQuickBarTestBarAction::Reset();
		UArcMassQuickBarConfig* Cfg = ArcMassItems::QuickBar::TestHelpers::BuildQuickBarConfig([](UArcMassQuickBarConfig& C)
		{
			FArcMassQuickBarEntry& Bar = C.QuickBars.AddDefaulted_GetRef();
			Bar.BarId = TAG_QBTest_Bar;
			Bar.QuickBarSelectedActions.Add(FInstancedStruct::Make(FArcQuickBarTestBarAction()));
			FArcMassQuickBarSlot& S1 = Bar.Slots.AddDefaulted_GetRef();
			S1.QuickBarSlotId = TAG_QBTest_QuickSlot;
			S1.ItemSlotId = TAG_QBTest_ItemSlot;
		});
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, Cfg);
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::QuickBar::AddAndActivateSlot(*EntityManager, Entity,
			TAG_QBTest_Bar, TAG_QBTest_QuickSlot, ItemId);

		ArcMassItems::QuickBar::DeactivateBar(*EntityManager, Entity, TAG_QBTest_Bar);

		ASSERT_THAT(AreEqual(1, FArcQuickBarTestBarAction::DeactivatedCount));
		ASSERT_THAT(IsFalse(ArcMassItems::QuickBar::IsSlotActive(*EntityManager, Entity, TAG_QBTest_Bar, TAG_QBTest_QuickSlot)));
	}

	UArcMassQuickBarConfig* MakeThreeSlotConfig(EArcMassQuickSlotsMode Mode = EArcMassQuickSlotsMode::Cyclable, bool bWithBlockingValidator = false)
	{
		return ArcMassItems::QuickBar::TestHelpers::BuildQuickBarConfig([Mode, bWithBlockingValidator](UArcMassQuickBarConfig& C)
		{
			FArcMassQuickBarEntry& Bar = C.QuickBars.AddDefaulted_GetRef();
			Bar.BarId = TAG_QBTest_Bar;
			Bar.QuickSlotsMode = Mode;

			const FGameplayTag SlotIds[] = { TAG_QBTest_QuickSlotA.GetTag(), TAG_QBTest_QuickSlotB.GetTag(), TAG_QBTest_QuickSlotC.GetTag() };
			for (const FGameplayTag& SlotId : SlotIds)
			{
				FArcMassQuickBarSlot& S = Bar.Slots.AddDefaulted_GetRef();
				S.QuickBarSlotId = SlotId;
				if (bWithBlockingValidator)
				{
					S.Validators.Add(FInstancedStruct::Make(FArcQuickBarTestValidator()));
				}
			}
		});
	}

	FMassEntityHandle MakeThreeSlotEntityWithItems(UArcMassQuickBarConfig* Cfg)
	{
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, Cfg);
		const FGameplayTag SlotIds[] = { TAG_QBTest_QuickSlotA.GetTag(), TAG_QBTest_QuickSlotB.GetTag(), TAG_QBTest_QuickSlotC.GetTag() };
		for (const FGameplayTag& Slot : SlotIds)
		{
			FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
			FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
			ArcMassItems::QuickBar::AddAndActivateSlot(*EntityManager, Entity,
				TAG_QBTest_Bar, Slot, ItemId);
		}
		return Entity;
	}

	TEST_METHOD(Cycle_Slot_WrapsAround)
	{
		FMassEntityHandle Entity = MakeThreeSlotEntityWithItems(MakeThreeSlotConfig());
		FGameplayTag Next = ArcMassItems::QuickBar::CycleSlot(*EntityManager, Entity,
			TAG_QBTest_Bar, TAG_QBTest_QuickSlotC.GetTag(), +1, TFunction<bool(const FArcItemData*)>{});
		ASSERT_THAT(AreEqual(TAG_QBTest_QuickSlotA.GetTag(), Next));
	}

	TEST_METHOD(Cycle_Slot_NegativeDirectionGoesBackward)
	{
		FMassEntityHandle Entity = MakeThreeSlotEntityWithItems(MakeThreeSlotConfig());
		FGameplayTag Next = ArcMassItems::QuickBar::CycleSlot(*EntityManager, Entity,
			TAG_QBTest_Bar, TAG_QBTest_QuickSlotA.GetTag(), -1, TFunction<bool(const FArcItemData*)>{});
		ASSERT_THAT(AreEqual(TAG_QBTest_QuickSlotC.GetTag(), Next));
	}

	TEST_METHOD(Cycle_Slot_SkipsValidatorFailedSlots)
	{
		FArcQuickBarTestValidator::BlockedSlotId = TAG_QBTest_QuickSlotB.GetTag();
		FMassEntityHandle Entity = MakeThreeSlotEntityWithItems(MakeThreeSlotConfig(EArcMassQuickSlotsMode::Cyclable, true));
		FGameplayTag Next = ArcMassItems::QuickBar::CycleSlot(*EntityManager, Entity,
			TAG_QBTest_Bar, TAG_QBTest_QuickSlotA.GetTag(), +1, TFunction<bool(const FArcItemData*)>{});
		ASSERT_THAT(AreEqual(TAG_QBTest_QuickSlotC.GetTag(), Next));
		FArcQuickBarTestValidator::BlockedSlotId = FGameplayTag::EmptyTag;
	}

	TEST_METHOD(Cycle_Slot_NonCyclableMode_ReturnsEmptyTag)
	{
		FMassEntityHandle Entity = MakeThreeSlotEntityWithItems(MakeThreeSlotConfig(EArcMassQuickSlotsMode::AutoActivateOnly));
		FGameplayTag Next = ArcMassItems::QuickBar::CycleSlot(*EntityManager, Entity,
			TAG_QBTest_Bar, TAG_QBTest_QuickSlotA.GetTag(), +1, TFunction<bool(const FArcItemData*)>{});
		ASSERT_THAT(IsFalse(Next.IsValid()));
	}

	// Two bars in a single cyclable group. Used by the cycle tests.
	UArcMassQuickBarConfig* MakeTwoBarGroupConfig()
	{
		return ArcMassItems::QuickBar::TestHelpers::BuildQuickBarConfig([](UArcMassQuickBarConfig& C)
		{
			FArcMassQuickBarGroup& Group = C.Groups.AddDefaulted_GetRef();
			Group.GroupId = TAG_QBTest_Group;
			Group.bCyclable = true;

			const FGameplayTag BarTags[] = { TAG_QBTest_BarA.GetTag(), TAG_QBTest_BarB.GetTag() };
			for (const FGameplayTag& BarTag : BarTags)
			{
				FArcMassQuickBarEntry& Bar = Group.Bars.AddDefaulted_GetRef();
				Bar.BarId = BarTag;
				FArcMassQuickBarSlot& S = Bar.Slots.AddDefaulted_GetRef();
				S.QuickBarSlotId = TAG_QBTest_QuickSlot;
				S.ItemSlotId = TAG_QBTest_ItemSlot;
			}
		});
	}

	TEST_METHOD(Cycle_Bar_SwitchesWhenAllSlotsValid)
	{
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, MakeTwoBarGroupConfig());
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::QuickBar::AddAndActivateSlot(*EntityManager, Entity,
			TAG_QBTest_BarA, TAG_QBTest_QuickSlot, ItemId);
		ArcMassItems::QuickBar::AddAndActivateSlot(*EntityManager, Entity,
			TAG_QBTest_BarB, TAG_QBTest_QuickSlot, ItemId);
		// Establish BarA as the group's currently-active bar.
		ArcMassItems::QuickBar::ActivateBar(*EntityManager, Entity, TAG_QBTest_BarA);

		FGameplayTag NewBar = ArcMassItems::QuickBar::CycleActiveBar(*EntityManager, Entity,
			TAG_QBTest_Group);
		ASSERT_THAT(AreEqual(TAG_QBTest_BarB.GetTag(), NewBar));
		ASSERT_THAT(AreEqual(TAG_QBTest_BarB.GetTag(),
			ArcMassItems::QuickBar::GetActiveBarInGroup(*EntityManager, Entity, TAG_QBTest_Group)));
	}

	TEST_METHOD(Cycle_Bar_StaysWhenTargetMissingItems)
	{
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, MakeTwoBarGroupConfig());
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::QuickBar::AddAndActivateSlot(*EntityManager, Entity,
			TAG_QBTest_BarA, TAG_QBTest_QuickSlot, ItemId);
		// BarB has no AddAndActivateSlot — its slot has no assigned item.
		ArcMassItems::QuickBar::ActivateBar(*EntityManager, Entity, TAG_QBTest_BarA);

		FGameplayTag NewBar = ArcMassItems::QuickBar::CycleActiveBar(*EntityManager, Entity,
			TAG_QBTest_Group);
		ASSERT_THAT(IsFalse(NewBar.IsValid()));
	}

	TEST_METHOD(Cycle_Bar_NegativeDirection)
	{
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, MakeTwoBarGroupConfig());
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::QuickBar::AddAndActivateSlot(*EntityManager, Entity,
			TAG_QBTest_BarA, TAG_QBTest_QuickSlot, ItemId);
		ArcMassItems::QuickBar::AddAndActivateSlot(*EntityManager, Entity,
			TAG_QBTest_BarB, TAG_QBTest_QuickSlot, ItemId);
		ArcMassItems::QuickBar::ActivateBar(*EntityManager, Entity, TAG_QBTest_BarA);

		FGameplayTag NewBar = ArcMassItems::QuickBar::CycleActiveBar(*EntityManager, Entity,
			TAG_QBTest_Group, /*Direction=*/-1);
		ASSERT_THAT(AreEqual(TAG_QBTest_BarB.GetTag(), NewBar));
	}

	// Three bars in a single group with a configurable cyclable flag.
	// The third bar lets us exercise round-robin direction independent of just A<->B.
	UArcMassQuickBarConfig* MakeThreeBarGroupConfig(bool bCyclable = true)
	{
		return ArcMassItems::QuickBar::TestHelpers::BuildQuickBarConfig(
			[bCyclable](UArcMassQuickBarConfig& C)
		{
			FArcMassQuickBarGroup& Group = C.Groups.AddDefaulted_GetRef();
			Group.GroupId = TAG_QBTest_Group;
			Group.bCyclable = bCyclable;

			const FGameplayTag BarTags[] = {
				TAG_QBTest_BarA.GetTag(), TAG_QBTest_BarB.GetTag(), TAG_QBTest_BarC.GetTag()
			};
			for (const FGameplayTag& BarTag : BarTags)
			{
				FArcMassQuickBarEntry& Bar = Group.Bars.AddDefaulted_GetRef();
				Bar.BarId = BarTag;
				FArcMassQuickBarSlot& S = Bar.Slots.AddDefaulted_GetRef();
				S.QuickBarSlotId = TAG_QBTest_QuickSlot;
				S.ItemSlotId = TAG_QBTest_ItemSlot;
			}
		});
	}

	void AssignItemsToAllBars(FMassEntityHandle Entity, FArcItemId ItemId)
	{
		const FGameplayTag BarTags[] = {
			TAG_QBTest_BarA.GetTag(), TAG_QBTest_BarB.GetTag(), TAG_QBTest_BarC.GetTag()
		};
		for (const FGameplayTag& BarTag : BarTags)
		{
			ArcMassItems::QuickBar::AddAndActivateSlot(*EntityManager, Entity,
				BarTag, TAG_QBTest_QuickSlot, ItemId);
		}
	}

	TEST_METHOD(Group_ActivateBar_DeactivatesSiblings)
	{
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager,
			MakeThreeBarGroupConfig(/*bCyclable=*/true));
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		AssignItemsToAllBars(Entity, ItemId);

		ArcMassItems::QuickBar::ActivateBar(*EntityManager, Entity, TAG_QBTest_BarA);
		ASSERT_THAT(IsTrue(ArcMassItems::QuickBar::IsSlotActive(*EntityManager, Entity, TAG_QBTest_BarA, TAG_QBTest_QuickSlot)));

		ArcMassItems::QuickBar::ActivateBar(*EntityManager, Entity, TAG_QBTest_BarB);

		ASSERT_THAT(IsFalse(ArcMassItems::QuickBar::IsSlotActive(*EntityManager, Entity, TAG_QBTest_BarA, TAG_QBTest_QuickSlot)));
		ASSERT_THAT(IsTrue(ArcMassItems::QuickBar::IsSlotActive(*EntityManager, Entity, TAG_QBTest_BarB, TAG_QBTest_QuickSlot)));
		ASSERT_THAT(AreEqual(TAG_QBTest_BarB.GetTag(),
			ArcMassItems::QuickBar::GetActiveBarInGroup(*EntityManager, Entity, TAG_QBTest_Group)));
	}

	TEST_METHOD(Group_CycleActiveBar_RoundRobinForward)
	{
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager,
			MakeThreeBarGroupConfig(/*bCyclable=*/true));
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		AssignItemsToAllBars(Entity, ItemId);
		ArcMassItems::QuickBar::ActivateBar(*EntityManager, Entity, TAG_QBTest_BarA);

		FGameplayTag Step1 = ArcMassItems::QuickBar::CycleActiveBar(*EntityManager, Entity, TAG_QBTest_Group, +1);
		ASSERT_THAT(AreEqual(TAG_QBTest_BarB.GetTag(), Step1));

		FGameplayTag Step2 = ArcMassItems::QuickBar::CycleActiveBar(*EntityManager, Entity, TAG_QBTest_Group, +1);
		ASSERT_THAT(AreEqual(TAG_QBTest_BarC.GetTag(), Step2));

		FGameplayTag Step3 = ArcMassItems::QuickBar::CycleActiveBar(*EntityManager, Entity, TAG_QBTest_Group, +1);
		ASSERT_THAT(AreEqual(TAG_QBTest_BarA.GetTag(), Step3));
	}

	TEST_METHOD(Group_CycleActiveBar_NotCyclable_ReturnsEmptyTag)
	{
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager,
			MakeThreeBarGroupConfig(/*bCyclable=*/false));
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		AssignItemsToAllBars(Entity, ItemId);
		ArcMassItems::QuickBar::ActivateBar(*EntityManager, Entity, TAG_QBTest_BarA);

		FGameplayTag Result = ArcMassItems::QuickBar::CycleActiveBar(*EntityManager, Entity, TAG_QBTest_Group, +1);
		ASSERT_THAT(IsFalse(Result.IsValid()));
		// BarA stays active.
		ASSERT_THAT(AreEqual(TAG_QBTest_BarA.GetTag(),
			ArcMassItems::QuickBar::GetActiveBarInGroup(*EntityManager, Entity, TAG_QBTest_Group)));
	}

	TEST_METHOD(Group_CycleActiveBar_SkipsBarWithMissingItems)
	{
		// BarB has no item assigned — cycle from A should skip it and land on C.
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager,
			MakeThreeBarGroupConfig(/*bCyclable=*/true));
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::QuickBar::AddAndActivateSlot(*EntityManager, Entity,
			TAG_QBTest_BarA, TAG_QBTest_QuickSlot, ItemId);
		// BarB intentionally not populated.
		ArcMassItems::QuickBar::AddAndActivateSlot(*EntityManager, Entity,
			TAG_QBTest_BarC, TAG_QBTest_QuickSlot, ItemId);
		ArcMassItems::QuickBar::ActivateBar(*EntityManager, Entity, TAG_QBTest_BarA);

		FGameplayTag Next = ArcMassItems::QuickBar::CycleActiveBar(*EntityManager, Entity, TAG_QBTest_Group, +1);
		ASSERT_THAT(AreEqual(TAG_QBTest_BarC.GetTag(), Next));
	}

	TEST_METHOD(Group_UngroupedBarsUnaffectedByGroupActivation)
	{
		// One bar inside a group, one bar outside any group.
		// Activating the grouped bar must not deactivate the ungrouped bar.
		UArcMassQuickBarConfig* Cfg = ArcMassItems::QuickBar::TestHelpers::BuildQuickBarConfig(
			[](UArcMassQuickBarConfig& C)
		{
			FArcMassQuickBarEntry& UngroupedBar = C.QuickBars.AddDefaulted_GetRef();
			UngroupedBar.BarId = TAG_QBTest_Bar;
			FArcMassQuickBarSlot& S0 = UngroupedBar.Slots.AddDefaulted_GetRef();
			S0.QuickBarSlotId = TAG_QBTest_QuickSlot;
			S0.ItemSlotId = TAG_QBTest_ItemSlot;

			FArcMassQuickBarGroup& Group = C.Groups.AddDefaulted_GetRef();
			Group.GroupId = TAG_QBTest_Group;
			FArcMassQuickBarEntry& GroupedBar = Group.Bars.AddDefaulted_GetRef();
			GroupedBar.BarId = TAG_QBTest_BarA;
			FArcMassQuickBarSlot& S1 = GroupedBar.Slots.AddDefaulted_GetRef();
			S1.QuickBarSlotId = TAG_QBTest_QuickSlot;
			S1.ItemSlotId = TAG_QBTest_ItemSlot;
		});

		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, Cfg);
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::QuickBar::AddAndActivateSlot(*EntityManager, Entity,
			TAG_QBTest_Bar, TAG_QBTest_QuickSlot, ItemId);
		ArcMassItems::QuickBar::AddAndActivateSlot(*EntityManager, Entity,
			TAG_QBTest_BarA, TAG_QBTest_QuickSlot, ItemId);

		ArcMassItems::QuickBar::ActivateBar(*EntityManager, Entity, TAG_QBTest_BarA);

		ASSERT_THAT(IsTrue(ArcMassItems::QuickBar::IsSlotActive(*EntityManager, Entity, TAG_QBTest_Bar, TAG_QBTest_QuickSlot)));
		ASSERT_THAT(IsTrue(ArcMassItems::QuickBar::IsSlotActive(*EntityManager, Entity, TAG_QBTest_BarA, TAG_QBTest_QuickSlot)));
	}

	TEST_METHOD(Slot_RemoveSlot_WhileActive_DeactivatesFirst)
	{
		FArcQuickBarTestSlotHandler::Reset();
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, MakeConfigWithHandler());
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::QuickBar::AddAndActivateSlot(*EntityManager, Entity,
			TAG_QBTest_Bar, TAG_QBTest_QuickSlot, ItemId);
		ASSERT_THAT(AreEqual(1, FArcQuickBarTestSlotHandler::SelectedCount));

		ArcMassItems::QuickBar::RemoveSlot(*EntityManager, Entity,
			TAG_QBTest_Bar, TAG_QBTest_QuickSlot);

		ASSERT_THAT(AreEqual(1, FArcQuickBarTestSlotHandler::DeselectedCount));
		ASSERT_THAT(IsFalse(ArcMassItems::QuickBar::IsSlotActive(*EntityManager, Entity, TAG_QBTest_Bar, TAG_QBTest_QuickSlot)));
		ASSERT_THAT(IsFalse(ArcMassItems::QuickBar::GetAssignedItem(*EntityManager, Entity, TAG_QBTest_Bar, TAG_QBTest_QuickSlot).IsValid()));
	}

	TEST_METHOD(MultiBar_OnlyMatchingBarSlotIsAdded)
	{
		UArcMassQuickBarConfig* Cfg = ArcMassItems::QuickBar::TestHelpers::BuildQuickBarConfig([](UArcMassQuickBarConfig& C)
		{
			FArcMassQuickBarEntry& BarA = C.QuickBars.AddDefaulted_GetRef();
			BarA.BarId = TAG_QBTest_BarA;
			FArcMassQuickBarSlot& SA = BarA.Slots.AddDefaulted_GetRef();
			SA.QuickBarSlotId = TAG_QBTest_QuickSlotA;
			SA.ItemSlotId = TAG_QBTest_ItemSlotA;

			FArcMassQuickBarEntry& BarB = C.QuickBars.AddDefaulted_GetRef();
			BarB.BarId = TAG_QBTest_BarB;
			FArcMassQuickBarSlot& SB = BarB.Slots.AddDefaulted_GetRef();
			SB.QuickBarSlotId = TAG_QBTest_QuickSlotB;
			SB.ItemSlotId = TAG_QBTest_ItemSlotB;
		});
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, Cfg);
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ASSERT_THAT(IsTrue(ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(),
			ItemId, TAG_QBTest_ItemSlotA.GetTag())));

		ArcMassItems::QuickBar::TestHelpers::RunAutoSelect(*EntityManager, Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
		ASSERT_THAT(IsNotNull(State));
		ASSERT_THAT(AreEqual(1, State->ActiveSlots.Num()));
		ASSERT_THAT(AreEqual(TAG_QBTest_BarA.GetTag(), State->ActiveSlots[0].BarId));
	}

	TEST_METHOD(AutoSelect_RemoveThenAddDifferentItem_TransitionsCleanly)
	{
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, MakeBasicConfig());
		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId Item1 = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Item1, TAG_QBTest_ItemSlot.GetTag());

		{
			ArcMassItems::QuickBar::TestHelpers::RunAutoSelect(*EntityManager, Entity);
		}

		ArcMassItems::RemoveItemFromSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Item1);
		{
			ArcMassItems::QuickBar::TestHelpers::RunAutoSelect(*EntityManager, Entity);
		}

		FArcItemId Item2 = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Item2, TAG_QBTest_ItemSlot.GetTag());
		{
			ArcMassItems::QuickBar::TestHelpers::RunAutoSelect(*EntityManager, Entity);
		}

		FMassEntityView View(*EntityManager, Entity);
		const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
		ASSERT_THAT(IsNotNull(State));
		ASSERT_THAT(AreEqual(1, State->ActiveSlots.Num()));
		ASSERT_THAT(IsTrue(State->ActiveSlots[0].AssignedItemId == Item2));
		ASSERT_THAT(IsTrue(State->ActiveSlots[0].bIsSlotActive));
	}

	TEST_METHOD(RequiredTags_SlotRequiredTags_SkipsItemMissingTags)
	{
		const FArcItemDefinitionData* Def = FArcItemDefinitionData::Find(MassSettings->ItemWithDefinitionTags.AssetId);
		ASSERT_THAT(IsNotNull(Def));
		const FArcItemFragment_Tags* TagFrag = Def->FindFragment<FArcItemFragment_Tags>();
		ASSERT_THAT(IsNotNull(TagFrag));
		const FGameplayTagContainer RequiredTags = TagFrag->ItemTags;
		ASSERT_THAT(IsFalse(RequiredTags.IsEmpty()));

		UArcMassQuickBarConfig* Cfg = ArcMassItems::QuickBar::TestHelpers::BuildQuickBarConfig([&RequiredTags](UArcMassQuickBarConfig& C)
		{
			FArcMassQuickBarEntry& Bar = C.QuickBars.AddDefaulted_GetRef();
			Bar.BarId = TAG_QBTest_Bar;
			FArcMassQuickBarSlot& S = Bar.Slots.AddDefaulted_GetRef();
			S.QuickBarSlotId = TAG_QBTest_QuickSlot;
			S.ItemSlotId = TAG_QBTest_ItemSlot;
			S.ItemRequiredTags = RequiredTags;
		});
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, Cfg);

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, TAG_QBTest_ItemSlot.GetTag());

		ArcMassItems::QuickBar::TestHelpers::RunAutoSelect(*EntityManager, Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
		ASSERT_THAT(IsNotNull(State));
		ASSERT_THAT(AreEqual(0, State->ActiveSlots.Num()));
	}

	TEST_METHOD(RequiredTags_SlotRequiredTags_AcceptsItemWithTags)
	{
		const FArcItemDefinitionData* Def = FArcItemDefinitionData::Find(MassSettings->ItemWithDefinitionTags.AssetId);
		ASSERT_THAT(IsNotNull(Def));
		const FArcItemFragment_Tags* TagFrag = Def->FindFragment<FArcItemFragment_Tags>();
		ASSERT_THAT(IsNotNull(TagFrag));
		const FGameplayTagContainer RequiredTags = TagFrag->ItemTags;
		ASSERT_THAT(IsFalse(RequiredTags.IsEmpty()));

		UArcMassQuickBarConfig* Cfg = ArcMassItems::QuickBar::TestHelpers::BuildQuickBarConfig([&RequiredTags](UArcMassQuickBarConfig& C)
		{
			FArcMassQuickBarEntry& Bar = C.QuickBars.AddDefaulted_GetRef();
			Bar.BarId = TAG_QBTest_Bar;
			FArcMassQuickBarSlot& S = Bar.Slots.AddDefaulted_GetRef();
			S.QuickBarSlotId = TAG_QBTest_QuickSlot;
			S.ItemSlotId = TAG_QBTest_ItemSlot;
			S.ItemRequiredTags = RequiredTags;
		});
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, Cfg);

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->ItemWithDefinitionTags, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, TAG_QBTest_ItemSlot.GetTag());

		ArcMassItems::QuickBar::TestHelpers::RunAutoSelect(*EntityManager, Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
		ASSERT_THAT(IsNotNull(State));
		ASSERT_THAT(AreEqual(1, State->ActiveSlots.Num()));
	}

	TEST_METHOD(RequiredTags_BarRequiredTags_GatesAllSlots)
	{
		const FArcItemDefinitionData* Def = FArcItemDefinitionData::Find(MassSettings->ItemWithDefinitionTags.AssetId);
		ASSERT_THAT(IsNotNull(Def));
		const FArcItemFragment_Tags* TagFrag = Def->FindFragment<FArcItemFragment_Tags>();
		ASSERT_THAT(IsNotNull(TagFrag));
		const FGameplayTagContainer RequiredTags = TagFrag->ItemTags;
		ASSERT_THAT(IsFalse(RequiredTags.IsEmpty()));

		UArcMassQuickBarConfig* Cfg = ArcMassItems::QuickBar::TestHelpers::BuildQuickBarConfig([&RequiredTags](UArcMassQuickBarConfig& C)
		{
			FArcMassQuickBarEntry& Bar = C.QuickBars.AddDefaulted_GetRef();
			Bar.BarId = TAG_QBTest_Bar;
			Bar.ItemRequiredTags = RequiredTags;
			FArcMassQuickBarSlot& S = Bar.Slots.AddDefaulted_GetRef();
			S.QuickBarSlotId = TAG_QBTest_QuickSlot;
			S.ItemSlotId = TAG_QBTest_ItemSlot;
		});
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, Cfg);

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, TAG_QBTest_ItemSlot.GetTag());

		ArcMassItems::QuickBar::TestHelpers::RunAutoSelect(*EntityManager, Entity);

		FMassEntityView View(*EntityManager, Entity);
		const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
		ASSERT_THAT(IsNotNull(State));
		ASSERT_THAT(AreEqual(0, State->ActiveSlots.Num()));
	}

	UArcMassQuickBarConfig* MakeConfigWithInputBind()
	{
		return ArcMassItems::QuickBar::TestHelpers::BuildQuickBarConfig([](UArcMassQuickBarConfig& C)
		{
			FArcMassQuickBarEntry& Bar = C.QuickBars.AddDefaulted_GetRef();
			Bar.BarId = TAG_QBTest_Bar;
			FArcMassQuickBarSlot& S = Bar.Slots.AddDefaulted_GetRef();
			S.QuickBarSlotId = TAG_QBTest_QuickSlot;
			S.ItemSlotId = TAG_QBTest_ItemSlot;

			FArcMassQuickBarInputBinding_Ability Binding;
			Binding.InputTag = TAG_QBTest_Input_Fire;
			S.InputBind = FInstancedStruct::Make(Binding);
		});
	}

	TEST_METHOD(InputBind_ActivateSlot_BindsInputTagToGrantedAbilities)
	{
		TArray<FInstancedStruct> Extras;
		Extras.Add(FInstancedStruct::Make(FArcAbilityCollectionFragment()));

		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(
			*EntityManager, MakeConfigWithInputBind(), Extras);

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->ItemWithGrantedMassAbility, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ASSERT_THAT(IsTrue(ItemId.IsValid()));

		ASSERT_THAT(IsTrue(ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, TAG_QBTest_ItemSlot.GetTag())));

		const FArcItemData* ItemData = nullptr;
		{
			FStructView StoreView = EntityManager->GetMutableSparseElementDataForEntity(FArcMassItemStoreFragment::StaticStruct(), Entity);
			FArcMassItemStoreFragment* Store = StoreView.GetPtr<FArcMassItemStoreFragment>();
			ASSERT_THAT(IsNotNull(Store));
			FArcMassReplicatedItem* Wrapper = Store->ReplicatedItems.FindById(ItemId);
			ASSERT_THAT(IsNotNull(Wrapper));
			ItemData = Wrapper->ToItem();
		}
		ASSERT_THAT(IsNotNull(ItemData));
		const FArcItemInstance_GrantedMassAbilities* Granted =
			ArcItemsHelper::FindInstance<FArcItemInstance_GrantedMassAbilities>(ItemData);
		ASSERT_THAT(IsNotNull(Granted));
		ASSERT_THAT(IsTrue(Granted->GrantedAbilities.Num() > 0));

		{
			ArcMassItems::QuickBar::TestHelpers::RunAutoSelect(*EntityManager, Entity);
		}

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment* Coll = View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsNotNull(Coll));
		ASSERT_THAT(AreEqual(1, Coll->Abilities.Num()));
		ASSERT_THAT(AreEqual(TAG_QBTest_Input_Fire.GetTag(), Coll->Abilities[0].InputTag));
	}

	TEST_METHOD(InputBind_DeactivateSlot_UnbindsInputTag)
	{
		TArray<FInstancedStruct> Extras;
		Extras.Add(FInstancedStruct::Make(FArcAbilityCollectionFragment()));
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(
			*EntityManager, MakeConfigWithInputBind(), Extras);

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->ItemWithGrantedMassAbility, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ASSERT_THAT(IsTrue(ItemId.IsValid()));

		ASSERT_THAT(IsTrue(ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, TAG_QBTest_ItemSlot.GetTag())));

		const FArcItemData* ItemData = nullptr;
		{
			FStructView StoreView = EntityManager->GetMutableSparseElementDataForEntity(FArcMassItemStoreFragment::StaticStruct(), Entity);
			FArcMassItemStoreFragment* Store = StoreView.GetPtr<FArcMassItemStoreFragment>();
			ASSERT_THAT(IsNotNull(Store));
			FArcMassReplicatedItem* Wrapper = Store->ReplicatedItems.FindById(ItemId);
			ASSERT_THAT(IsNotNull(Wrapper));
			ItemData = Wrapper->ToItem();
		}
		ASSERT_THAT(IsNotNull(ItemData));
		const FArcItemInstance_GrantedMassAbilities* Granted =
			ArcItemsHelper::FindInstance<FArcItemInstance_GrantedMassAbilities>(ItemData);
		ASSERT_THAT(IsNotNull(Granted));
		ASSERT_THAT(IsTrue(Granted->GrantedAbilities.Num() > 0));

		{
			ArcMassItems::QuickBar::TestHelpers::RunAutoSelect(*EntityManager, Entity);
		}

		ASSERT_THAT(IsTrue(ArcMassItems::QuickBar::DeactivateSlot(*EntityManager, Entity,
			TAG_QBTest_Bar, TAG_QBTest_QuickSlot)));

		FMassEntityView View(*EntityManager, Entity);
		const FArcAbilityCollectionFragment* Coll = View.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>();
		ASSERT_THAT(IsNotNull(Coll));
		ASSERT_THAT(IsFalse(Coll->Abilities[0].InputTag.IsValid()));
	}

	TEST_METHOD(InputBind_NoGrantedAbilities_BindIsNoOp)
	{
		TArray<FInstancedStruct> Extras;
		Extras.Add(FInstancedStruct::Make(FArcAbilityCollectionFragment()));
		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(
			*EntityManager, MakeConfigWithInputBind(), Extras);

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1); // no granted-abilities instance
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, TAG_QBTest_ItemSlot.GetTag());
		{
			ArcMassItems::QuickBar::TestHelpers::RunAutoSelect(*EntityManager, Entity);
		}

		ASSERT_THAT(IsTrue(ArcMassItems::QuickBar::IsSlotActive(*EntityManager, Entity, TAG_QBTest_Bar, TAG_QBTest_QuickSlot)));
	}

	// ---------------------------------------------------------------------------
	// HandleSlotChanged — manual-slot preservation tests (Step 2, Option A)
	// These call HandleSlotChanged directly, mirroring the live OnItemSlotChanged
	// rail. RunAutoSelect cannot drive the clear branch (always supplies valid tag).
	// ---------------------------------------------------------------------------

	// Config quickslot with ItemSlotId LEFT UNSET (manual slot).
	// Pre-create the active-slot entry via AddAndActivateSlot (explicit assign).
	// HandleSlotChanged with a valid NewSlot must PRESERVE the entry (not remove it).
	// This is the exact regression: fails before Step 1 fix, passes after.
	TEST_METHOD(AutoSelect_HandleSlotChanged_PreservesManuallyAssignedUnsetItemSlot)
	{
		UArcMassQuickBarConfig* Cfg = ArcMassItems::QuickBar::TestHelpers::BuildQuickBarConfig(
			[](UArcMassQuickBarConfig& C)
		{
			FArcMassQuickBarEntry& Bar = C.QuickBars.AddDefaulted_GetRef();
			Bar.BarId = TAG_QBTest_Bar;
			FArcMassQuickBarSlot& QSlot = Bar.Slots.AddDefaulted_GetRef();
			QSlot.QuickBarSlotId = TAG_QBTest_QuickSlot;
			// ItemSlotId intentionally left as default FGameplayTag() — manual slot.
			QSlot.bAutoSelect = true;
		});

		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, Cfg);

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ASSERT_THAT(IsTrue(ItemId.IsValid()));

		FGameplayTag ItemSlotTag = TAG_QBTest_ItemSlot.GetTag();
		ASSERT_THAT(IsTrue(ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, ItemSlotTag)));

		// Pre-create the active-slot entry exactly as FArcMassAddItemToQuickSlotCommand::Execute would.
		ArcMassItems::QuickBar::AddAndActivateSlot(*EntityManager, Entity,
			TAG_QBTest_Bar, TAG_QBTest_QuickSlot, ItemId);

		{
			FMassEntityView View(*EntityManager, Entity);
			const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
			ASSERT_THAT(IsNotNull(State));
			ASSERT_THAT(AreEqual(1, State->ActiveSlots.Num()));
		}

		// Assign/move event (valid NewSlot): manual slot must survive.
		ArcMassItems::QuickBar::AutoSelect::HandleSlotChanged(
			*EntityManager, Entity, ItemId, ItemSlotTag, FArcMassItemStoreFragment::StaticStruct());

		FMassEntityView View(*EntityManager, Entity);
		const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
		ASSERT_THAT(IsNotNull(State));
		ASSERT_THAT(AreEqual(1, State->ActiveSlots.Num()));
		ASSERT_THAT(IsTrue(State->ActiveSlots[0].AssignedItemId == ItemId));
	}

	// Same manual-slot setup + pre-created active slot.
	// HandleSlotChanged with NewSlot = FGameplayTag() (invalid → clear) must REMOVE the entry.
	// Manual slots still clear when the item is removed/unslotted.
	TEST_METHOD(AutoSelect_HandleSlotChanged_ClearRemovesManualSlot)
	{
		UArcMassQuickBarConfig* Cfg = ArcMassItems::QuickBar::TestHelpers::BuildQuickBarConfig(
			[](UArcMassQuickBarConfig& C)
		{
			FArcMassQuickBarEntry& Bar = C.QuickBars.AddDefaulted_GetRef();
			Bar.BarId = TAG_QBTest_Bar;
			FArcMassQuickBarSlot& QSlot = Bar.Slots.AddDefaulted_GetRef();
			QSlot.QuickBarSlotId = TAG_QBTest_QuickSlot;
			// ItemSlotId intentionally left as default FGameplayTag() — manual slot.
			QSlot.bAutoSelect = true;
		});

		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, Cfg);

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ASSERT_THAT(IsTrue(ItemId.IsValid()));

		FGameplayTag ItemSlotTag = TAG_QBTest_ItemSlot.GetTag();
		ASSERT_THAT(IsTrue(ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, ItemSlotTag)));

		ArcMassItems::QuickBar::AddAndActivateSlot(*EntityManager, Entity,
			TAG_QBTest_Bar, TAG_QBTest_QuickSlot, ItemId);

		{
			FMassEntityView View(*EntityManager, Entity);
			const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
			ASSERT_THAT(IsNotNull(State));
			ASSERT_THAT(AreEqual(1, State->ActiveSlots.Num()));
		}

		// Clear event (invalid NewSlot): must remove the manual-slot entry.
		ArcMassItems::QuickBar::AutoSelect::HandleSlotChanged(
			*EntityManager, Entity, ItemId, FGameplayTag(), FArcMassItemStoreFragment::StaticStruct());

		FMassEntityView View(*EntityManager, Entity);
		const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
		ASSERT_THAT(IsNotNull(State));
		ASSERT_THAT(AreEqual(0, State->ActiveSlots.Num()));
	}

	// Same manual-slot setup + pre-created active slot.
	// HandleSlotChanged with a DIFFERENT valid item-slot tag (item moves between slots)
	// must PRESERVE the manual active-slot entry and keep its AssignedItemId.
	// A manual slot tracks the item by id, not by item-slot tag, so the assignment survives a move.
	TEST_METHOD(AutoSelect_HandleSlotChanged_ManualSlotSurvivesMove)
	{
		UArcMassQuickBarConfig* Cfg = ArcMassItems::QuickBar::TestHelpers::BuildQuickBarConfig(
			[](UArcMassQuickBarConfig& C)
		{
			FArcMassQuickBarEntry& Bar = C.QuickBars.AddDefaulted_GetRef();
			Bar.BarId = TAG_QBTest_Bar;
			FArcMassQuickBarSlot& QSlot = Bar.Slots.AddDefaulted_GetRef();
			QSlot.QuickBarSlotId = TAG_QBTest_QuickSlot;
			// ItemSlotId intentionally left as default FGameplayTag() — manual slot.
			QSlot.bAutoSelect = true;
		});

		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, Cfg);

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ASSERT_THAT(IsTrue(ItemId.IsValid()));

		FGameplayTag OriginalSlotTag = TAG_QBTest_ItemSlot.GetTag();
		ASSERT_THAT(IsTrue(ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, OriginalSlotTag)));

		ArcMassItems::QuickBar::AddAndActivateSlot(*EntityManager, Entity,
			TAG_QBTest_Bar, TAG_QBTest_QuickSlot, ItemId);

		{
			FMassEntityView View(*EntityManager, Entity);
			const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
			ASSERT_THAT(IsNotNull(State));
			ASSERT_THAT(AreEqual(1, State->ActiveSlots.Num()));
		}

		// Move event: item moved to a DIFFERENT item-slot tag; manual entry must survive.
		FGameplayTag MovedToSlotTag = TAG_QBTest_NonMatchingSlot.GetTag();
		ArcMassItems::QuickBar::AutoSelect::HandleSlotChanged(
			*EntityManager, Entity, ItemId, MovedToSlotTag, FArcMassItemStoreFragment::StaticStruct());

		FMassEntityView View(*EntityManager, Entity);
		const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
		ASSERT_THAT(IsNotNull(State));
		ASSERT_THAT(AreEqual(1, State->ActiveSlots.Num()));
		ASSERT_THAT(IsTrue(State->ActiveSlots[0].AssignedItemId == ItemId));
	}

	// Config quickslot with a VALID ItemSlotId (tag-managed slot).
	// Seed item + pre-create the active slot, then HandleSlotChanged with NewSlot == that ItemSlotId.
	// Tag-managed remove-then-re-add still works — no regression from the fix.
	TEST_METHOD(AutoSelect_HandleSlotChanged_TagManagedSlotStillReassigns)
	{
		UArcMassQuickBarConfig* Cfg = ArcMassItems::QuickBar::TestHelpers::BuildQuickBarConfig(
			[](UArcMassQuickBarConfig& C)
		{
			FArcMassQuickBarEntry& Bar = C.QuickBars.AddDefaulted_GetRef();
			Bar.BarId = TAG_QBTest_Bar;
			FArcMassQuickBarSlot& QSlot = Bar.Slots.AddDefaulted_GetRef();
			QSlot.QuickBarSlotId = TAG_QBTest_QuickSlot;
			QSlot.ItemSlotId = TAG_QBTest_ItemSlot; // tag-managed
			QSlot.bAutoSelect = true;
		});

		FMassEntityHandle Entity = ArcMassItems::QuickBar::TestHelpers::CreateQuickBarEntity(*EntityManager, Cfg);

		FArcItemSpec Spec = FArcItemSpec::NewItem(MassSettings->SimpleBaseItem, 1, 1);
		FArcItemId ItemId = ArcMassItems::AddItem(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), Spec);
		ASSERT_THAT(IsTrue(ItemId.IsValid()));

		FGameplayTag ItemSlotTag = TAG_QBTest_ItemSlot.GetTag();
		ASSERT_THAT(IsTrue(ArcMassItems::AddItemToSlot(*EntityManager, Entity, FArcMassItemStoreFragment::StaticStruct(), ItemId, ItemSlotTag)));

		// Pre-create the active slot entry.
		ArcMassItems::QuickBar::AddAndActivateSlot(*EntityManager, Entity,
			TAG_QBTest_Bar, TAG_QBTest_QuickSlot, ItemId);

		{
			FMassEntityView View(*EntityManager, Entity);
			const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
			ASSERT_THAT(IsNotNull(State));
			ASSERT_THAT(AreEqual(1, State->ActiveSlots.Num()));
		}

		// Assign event matching the tag-managed slot: remove-then-re-add must still leave Num()==1.
		ArcMassItems::QuickBar::AutoSelect::HandleSlotChanged(
			*EntityManager, Entity, ItemId, ItemSlotTag, FArcMassItemStoreFragment::StaticStruct());

		FMassEntityView View(*EntityManager, Entity);
		const FArcMassQuickBarStateFragment* State = View.GetSparseFragmentDataPtr<FArcMassQuickBarStateFragment>();
		ASSERT_THAT(IsNotNull(State));
		ASSERT_THAT(AreEqual(1, State->ActiveSlots.Num()));
	}
};
