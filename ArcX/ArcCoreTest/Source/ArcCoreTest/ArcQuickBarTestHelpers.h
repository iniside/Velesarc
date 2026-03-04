// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemsStoreComponent.h"
#include "QuickBar/ArcQuickBarComponent.h"
#include "NativeGameplayTags.h"

#include "ArcQuickBarTestHelpers.generated.h"

// ---------------------------------------------------------------------------
// Test gameplay tags (defined in ArcQuickBarTestHelpers.cpp)
// ---------------------------------------------------------------------------
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_QBTest_Bar01);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_QBTest_Bar02);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_QBTest_Slot01);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_QBTest_Slot02);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_QBTest_Slot03);

// ---------------------------------------------------------------------------
// Test QuickBar component  -- exposes protected QuickBars for programmatic setup
// ---------------------------------------------------------------------------
UCLASS()
class UArcQuickBarTestComponent : public UArcQuickBarComponent
{
	GENERATED_BODY()

public:
	TArray<FArcQuickBar>& GetMutableQuickBars() { return QuickBars; }
};

// ---------------------------------------------------------------------------
// Minimal test actor: QuickBar + ItemsStore + ASC
// ---------------------------------------------------------------------------
UCLASS()
class AArcQuickBarTestActor : public AActor
	, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UArcItemsStoreComponent> ItemsStore;

	UPROPERTY()
	TObjectPtr<UArcCoreAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UArcQuickBarTestComponent> QuickBarComponent;

	AArcQuickBarTestActor();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override
	{
		return AbilitySystemComponent;
	}
};

// ---------------------------------------------------------------------------
// Helper utilities
// ---------------------------------------------------------------------------
namespace ArcQuickBarTestHelpers
{
	/** Build a FArcQuickBar configured for testing. ItemsStoreClass is set to UArcItemsStoreComponent. */
	FArcQuickBar MakeBar(const FGameplayTag& BarId, EArcQuickSlotsMode Mode);

	/** Build a FArcQuickBarSlot configured for testing. */
	FArcQuickBarSlot MakeSlot(const FGameplayTag& SlotId, bool bAutoSelect = false);

	/** Add a simple test item to the store and return its FArcItemId. */
	FArcItemId AddTestItem(UArcItemsStoreComponent* Store);

	/**
	 * Configure the component with a single bar containing the given slots.
	 * Replaces any previous QuickBars configuration.
	 */
	void ConfigureSingleBar(UArcQuickBarTestComponent* QB
		, const FGameplayTag& BarId
		, EArcQuickSlotsMode Mode
		, TArray<FArcQuickBarSlot> Slots);

	/**
	 * Configure the component with two bars.
	 */
	void ConfigureTwoBars(UArcQuickBarTestComponent* QB
		, const FGameplayTag& Bar1Id, EArcQuickSlotsMode Mode1, TArray<FArcQuickBarSlot> Slots1
		, const FGameplayTag& Bar2Id, EArcQuickSlotsMode Mode2, TArray<FArcQuickBarSlot> Slots2);
}
