// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemTypes.h"
#include "UObject/PrimaryAssetId.h"
#include "MVVMViewModelBase.h"

#include "ArcItemViewModel.generated.h"

struct FArcItemData;
struct FArcItemSpec;
struct FArcItemDefinitionData;
class UArcTagDisplayRegistry;
class UArcStatDisplayRegistry;
class UArcItemStatRowViewModel;

UCLASS(BlueprintType)
class ARCUIMODELS_API UArcItemViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FArcItemId ItemId;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FPrimaryAssetId ItemDefinition;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText Description;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TSoftObjectPtr<UObject> Icon;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FGameplayTag CurrentItemStoreSlot;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	int32 Stacks = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	int32 Level = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FGameplayTagContainer Tags;

	/** Rarity display label: the spec's rarity (or the craft rules' DefaultRarity) resolved via the tag registry. Empty for definition views. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText RarityLabel;

	/** Rarity accent color, resolved from the same tag registry entry as RarityLabel. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FLinearColor RarityColor = FLinearColor::White;

	/** True when the spec's instance tags contain Material.Property.Magic. False for definition views. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bEnchanted = false;

	/** Item category label (e.g. "Tool"). Left empty until an Item.Category.* convention exists. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText CategoryLabel;

	/** Per-stat display rows, built from the definition's FArcItemFragment_ItemStats::DefaultStats. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TArray<TObjectPtr<UArcItemStatRowViewModel>> StatRows;

	const FArcItemId& GetItemId() const { return ItemId; }
	const FPrimaryAssetId& GetItemDefinition() const { return ItemDefinition; }
	const FText& GetDisplayName() const { return DisplayName; }
	const FText& GetDescription() const { return Description; }
	const TSoftObjectPtr<UObject>& GetIcon() const { return Icon; }
	const FGameplayTag& GetCurrentItemStoreSlot() const { return CurrentItemStoreSlot; }
	int32 GetStacks() const { return Stacks; }
	int32 GetLevel() const { return Level; }
	const FGameplayTagContainer& GetTags() const { return Tags; }
	const FText& GetRarityLabel() const { return RarityLabel; }
	const FLinearColor& GetRarityColor() const { return RarityColor; }
	bool GetbEnchanted() const { return bEnchanted; }
	const FText& GetCategoryLabel() const { return CategoryLabel; }
	const TArray<TObjectPtr<UArcItemStatRowViewModel>>& GetStatRows() const { return StatRows; }

	// Re-reads the item data + fragments and fires FieldNotify only for fields that changed.
	// TagReg null leaves RarityLabel empty and RarityColor white.
	void RefreshFrom(const FArcItemData& Item, const UArcTagDisplayRegistry* TagReg);

	// Populates from a plan-snapshot spec — no FArcItemData exists for snapshot/station items.
	// InStacks is caller-supplied (e.g. the post-staging remainder), NOT Spec.Amount. The spec's
	// definition may be unresolved (null) — name/icon/tags are left empty in that case.
	void RefreshFrom(const FArcItemSpec& Spec, int32 InStacks, const UArcTagDisplayRegistry* TagReg);

	// Builds a design-time view from an item definition (required-ingredient tooltip): name/desc/
	// icon/tags and StatRows from the definition's FArcItemFragment_ItemStats::DefaultStats via
	// StatReg. Rarity is cleared (a definition has no rarity). Leaves ItemId/Stacks empty.
	// Def may be null (unresolved) — clears the definition-derived fields and returns.
	void RefreshFromDefinition(
		const FArcItemDefinitionData* Def,
		const UArcStatDisplayRegistry* StatReg);

private:
	void SetRarity(FGameplayTag RarityTag, const UArcTagDisplayRegistry* TagReg);

	// Shared push of the StatRows FieldNotify array (recycle-by-index pooled, two-pass prune).
	void SetStatRows(TArray<TObjectPtr<UArcItemStatRowViewModel>>&& NewStatRows);
};
