// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcQuickBarTestHelpers.h"

#include "ArcItemTestsSettings.h"

// ---------------------------------------------------------------------------
// Gameplay tags for QuickBar tests
// ---------------------------------------------------------------------------
UE_DEFINE_GAMEPLAY_TAG(TAG_QBTest_Bar01, "QuickBar.Test.Bar01");
UE_DEFINE_GAMEPLAY_TAG(TAG_QBTest_Bar02, "QuickBar.Test.Bar02");
UE_DEFINE_GAMEPLAY_TAG(TAG_QBTest_Slot01, "QuickSlot.Test.Slot01");
UE_DEFINE_GAMEPLAY_TAG(TAG_QBTest_Slot02, "QuickSlot.Test.Slot02");
UE_DEFINE_GAMEPLAY_TAG(TAG_QBTest_Slot03, "QuickSlot.Test.Slot03");

// ---------------------------------------------------------------------------
// AArcQuickBarTestActor
// ---------------------------------------------------------------------------
AArcQuickBarTestActor::AArcQuickBarTestActor()
{
	ItemsStore = CreateDefaultSubobject<UArcItemsStoreComponent>(TEXT("ItemsStore"));
	ItemsStore->bUseSubsystemForItemStore = true;

	AbilitySystemComponent = CreateDefaultSubobject<UArcCoreAbilitySystemComponent>(TEXT("ASC"));

	QuickBarComponent = CreateDefaultSubobject<UArcQuickBarTestComponent>(TEXT("QuickBar"));
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace ArcQuickBarTestHelpers
{
	FArcQuickBar MakeBar(const FGameplayTag& BarId, EArcQuickSlotsMode Mode)
	{
		FArcQuickBar Bar;
		Bar.BarId = BarId;
		Bar.QuickSlotsMode = Mode;
		Bar.ItemsStoreClass = UArcItemsStoreComponent::StaticClass();
		return Bar;
	}

	FArcQuickBarSlot MakeSlot(const FGameplayTag& SlotId, bool bAutoSelect)
	{
		FArcQuickBarSlot Slot;
		Slot.QuickBarSlotId = SlotId;
		Slot.bAutoSelect = bAutoSelect;
		return Slot;
	}

	FArcItemId AddTestItem(UArcItemsStoreComponent* Store)
	{
		const UArcItemTestsSettings* Settings = GetDefault<UArcItemTestsSettings>();
		FArcItemSpec Spec;
		Spec.SetItemDefinition(Settings->SimpleBaseItem).SetAmount(1).SetItemLevel(1);
		return Store->AddItem(Spec, FArcItemId::InvalidId);
	}

	void ConfigureSingleBar(UArcQuickBarTestComponent* QB
		, const FGameplayTag& BarId
		, EArcQuickSlotsMode Mode
		, TArray<FArcQuickBarSlot> Slots)
	{
		FArcQuickBar Bar = MakeBar(BarId, Mode);
		Bar.Slots = MoveTemp(Slots);
		QB->GetMutableQuickBars().Reset();
		QB->GetMutableQuickBars().Add(MoveTemp(Bar));
	}

	void ConfigureTwoBars(UArcQuickBarTestComponent* QB
		, const FGameplayTag& Bar1Id, EArcQuickSlotsMode Mode1, TArray<FArcQuickBarSlot> Slots1
		, const FGameplayTag& Bar2Id, EArcQuickSlotsMode Mode2, TArray<FArcQuickBarSlot> Slots2)
	{
		QB->GetMutableQuickBars().Reset();

		FArcQuickBar Bar1 = MakeBar(Bar1Id, Mode1);
		Bar1.Slots = MoveTemp(Slots1);
		QB->GetMutableQuickBars().Add(MoveTemp(Bar1));

		FArcQuickBar Bar2 = MakeBar(Bar2Id, Mode2);
		Bar2.Slots = MoveTemp(Slots2);
		QB->GetMutableQuickBars().Add(MoveTemp(Bar2));
	}
}
