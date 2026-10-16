// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"

#include "ArcBuildingIngredientViewModel.generated.h"

struct FArcBuildIngredient;
struct FArcMassItemStoreFragment;

UCLASS(BlueprintType)
class ARCUIMODELS_API UArcBuildingIngredientViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText SlotName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	int32 Required = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	int32 Have = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bSatisfied = false;

	/** Formatted "Have / Required" text for direct display binding. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText HaveRequiredText;

	/** Resolved item display name, or the slot name when no item def is resolvable. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TSoftObjectPtr<UObject> Icon;

	const FText& GetSlotName() const { return SlotName; }
	int32 GetRequired() const { return Required; }
	int32 GetHave() const { return Have; }
	bool GetbSatisfied() const { return bSatisfied; }
	const FText& GetHaveRequiredText() const { return HaveRequiredText; }
	const FText& GetDisplayName() const { return DisplayName; }
	const TSoftObjectPtr<UObject>& GetIcon() const { return Icon; }

	/**
	 * Populates all FieldNotify properties from one ingredient slot and the player's
	 * item store. Have counts through FArcBuildIngredient::CountAvailable so the UI
	 * matches the authoritative consume path's DoesItemSatisfy predicate.
	 */
	void RefreshFrom(const FArcBuildIngredient& Ingredient, const FArcMassItemStoreFragment& Inventory);

private:
	void SetDisplayName(const FText& NewDisplayName);
	void SetIcon(const TSoftObjectPtr<UObject>& NewIcon);
};
