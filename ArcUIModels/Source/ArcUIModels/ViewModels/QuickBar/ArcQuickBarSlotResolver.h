// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "View/MVVMViewModelContextResolver.h"

#include "ArcQuickBarSlotResolver.generated.h"

class UArcFrontendMVVMModelComponent;
class UArcQuickBarSlotViewModel;

/**
 * MVVM viewmodel resolver for quick-bar slot widgets.
 *
 * Composes the (BarId, QuickSlotId) pair a slot widget needs: it looks up the
 * bar's UArcQuickBarViewModel in the local PlayerState's
 * UArcFrontendMVVMModelComponent (registered under its BarId by
 * FArcQuickBarStructModel), then returns that bar's matching
 * UArcQuickBarSlotViewModel via FindSlot.
 *
 * Per-tag fallback contract: each of the two tags is resolved independently.
 * For a given tag, the widget's IArcQuickBarSlotWidget value is used when the
 * widget implements the interface AND that value is valid; otherwise the
 * authored UPROPERTY fallback is used. This supports reusable slot widgets that
 * report their slot id from the widget class while the bar id is authored per
 * placement. Both final tags must be valid or CreateInstance returns nullptr.
 */
UCLASS(Blueprintable, EditInlineNew)
class ARCUIMODELS_API UArcQuickBarSlotResolver : public UMVVMViewModelContextResolver
{
	GENERATED_BODY()

public:
	/** Authored fallback bar id, used when the widget does not report a valid GetQuickBarId. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "QuickBar,BarId"))
	FGameplayTag BarId;

	/** Authored fallback quick-slot id, used when the widget does not report a valid GetQuickBarSlotId. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "QuickSlot"))
	FGameplayTag QuickSlotId;

	/** Composes the pair lookup: FindViewModel(BarId) -> Cast<UArcQuickBarViewModel> -> FindSlot(QuickSlotId). */
	static UArcQuickBarSlotViewModel* ResolveSlotViewModel(
		UArcFrontendMVVMModelComponent& Component, FGameplayTag InBarId, FGameplayTag InQuickSlotId);

	virtual UObject* CreateInstance(const UClass* ExpectedType, const UUserWidget* UserWidget, const UMVVMView* View) const override;
};
