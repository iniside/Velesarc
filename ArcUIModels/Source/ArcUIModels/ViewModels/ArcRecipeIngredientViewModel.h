// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Items/ArcItemId.h"
#include "ArcNamedPrimaryAssetId.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "StructUtils/InstancedStruct.h"
#include "ViewModels/ArcIngredientCandidateViewModel.h"

#include "ArcRecipeIngredientViewModel.generated.h"

struct FArcItemSpec;
class UArcItemViewModel;
class UArcTagDisplayRegistry;
class UArcStatDisplayRegistry;

UENUM(BlueprintType)
enum class EArcRecipeIngredientSource : uint8
{
	ByItemDef,
	ByTag
};

UCLASS(BlueprintType)
class ARCUIMODELS_API UArcRecipeIngredientViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	EArcRecipeIngredientSource Source = EArcRecipeIngredientSource::ByItemDef;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TSoftObjectPtr<UObject> Icon;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	int32 Required = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	int32 Have = 0;

	/** HaveFromStation >= Required; always true for an optional slot. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bSatisfied = false;

	/** An optional slot never blocks a craft. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bOptional = false;

	/** HaveFromStation >= Required, for required and optional slots alike. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bFilled = false;

	/** An optional slot with nothing fitting in the station input view. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bOptionalEmpty = false;

	/** Formatted "N Available." text, where N = Have (inventory + station). */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText AvailableText;

	/** Formatted "Slotted / Required" text (property name kept stable for widget MVVM binding):
	 *  Slotted  = HaveFromStation (matching items currently in the station input);
	 *  Required = Required (amount this ingredient slot needs for one craft). */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText SlottedAvailableText;

	/** The exact item definition required by this slot (ByItemDef slots only; invalid for ByTag slots). */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FArcNamedPrimaryAssetId RequiredItemId;

	/** Tags an item must carry to satisfy this slot (ByTag slots only; empty for ByItemDef slots). */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FGameplayTagContainer RequiredTags;

	/** Tags that disqualify an item from satisfying this slot (ByTag and ByItemDef slots). */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FGameplayTagContainer DenyTags;

	/** Minimum item rarity step required to satisfy this slot (ByTag slots only; 0 for ByItemDef slots). */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	float MinRarityStep = 0.0f;

	/** Required - HaveFromStation, at least 0. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	int32 Missing = 0;

	/** Stacks in the player's inventory stores for this slot, minus the stacks already in the preview. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	int32 HaveFromInventory = 0;

	/** Stacks in the station input view for this slot: server input plus preview. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	int32 HaveFromStation = 0;

	/** Candidate items for this slot: station input view rows, then inventory rows, then unowned definitions. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TArray<TObjectPtr<UArcIngredientCandidateViewModel>> Candidates;

	/** Flavor/description text for a tag-based slot, from the tag registry's Description field. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText TagDescription;

	/** True when this slot is tag-based (Source == ByTag). */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bIsTagSource = false;

	/** True when this slot requires a specific item definition (Source == ByItemDef). */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bIsItemSource = true;

	/** Detail VM for the required item definition (ByItemDef slots only; null for ByTag). */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TObjectPtr<UArcItemViewModel> RequiredItemViewModel;

	EArcRecipeIngredientSource GetSource() const { return Source; }
	const FText& GetDisplayName() const { return DisplayName; }
	const TSoftObjectPtr<UObject>& GetIcon() const { return Icon; }
	int32 GetRequired() const { return Required; }
	int32 GetHave() const { return Have; }
	bool GetbSatisfied() const { return bSatisfied; }
	bool GetbOptional() const { return bOptional; }
	bool GetbFilled() const { return bFilled; }
	bool GetbOptionalEmpty() const { return bOptionalEmpty; }
	const FText& GetAvailableText() const { return AvailableText; }
	const FText& GetSlottedAvailableText() const { return SlottedAvailableText; }
	const FArcNamedPrimaryAssetId& GetRequiredItemId() const { return RequiredItemId; }
	const FGameplayTagContainer& GetRequiredTags() const { return RequiredTags; }
	const FGameplayTagContainer& GetDenyTags() const { return DenyTags; }
	float GetMinRarityStep() const { return MinRarityStep; }
	int32 GetMissing() const { return Missing; }
	int32 GetHaveFromInventory() const { return HaveFromInventory; }
	int32 GetHaveFromStation() const { return HaveFromStation; }
	const TArray<TObjectPtr<UArcIngredientCandidateViewModel>>& GetCandidates() const { return Candidates; }
	const FText& GetTagDescription() const { return TagDescription; }
	bool GetbIsTagSource() const { return bIsTagSource; }
	bool GetbIsItemSource() const { return bIsItemSource; }
	UArcItemViewModel* GetRequiredItemViewModel() const { return RequiredItemViewModel; }

	bool DoesItemSatisfy(const FArcItemSpec& Spec) const;

	/**
	 * Populates all FieldNotify properties from a single ingredient instanced struct.
	 * InventoryItems are the inventory rows left after the preview; ServerInput and PreviewInput
	 * together are the station input view. Registry and StatReg may be null.
	 */
	void RefreshFrom(
		const FInstancedStruct& IngredientStruct,
		TConstArrayView<FArcCraftStoreItemRow> InventoryItems,
		TConstArrayView<FArcItemSpec> ServerInput,
		TConstArrayView<FArcCraftStoreItemRow> PreviewInput,
		const UArcTagDisplayRegistry* Registry,
		const UArcStatDisplayRegistry* StatReg);

private:
	void SetCandidates(TArray<TObjectPtr<UArcIngredientCandidateViewModel>>&& NewCandidates);

	UPROPERTY(Transient)
	FInstancedStruct Ingredient;

	/** Recompute AvailableText ("N Available.") and SlottedAvailableText ("Slotted / Required")
	 *  from the current HaveFromStation / Required counts.
	 *  Called from RefreshFrom after counts are updated. */
	void RefreshFormattedText();
};
