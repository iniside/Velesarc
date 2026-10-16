// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcNamedPrimaryAssetId.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "ViewModels/ArcRecipeIngredientViewModel.h"
#include "Items/ArcItemSpec.h"

#include "ArcRecipeViewModel.generated.h"

struct FArcRecipeDefinitionData;
class UArcTagDisplayRegistry;
class UArcStatDisplayRegistry;
class UArcItemViewModel;

UCLASS(BlueprintType)
class ARCUIMODELS_API UArcRecipeViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FArcNamedPrimaryAssetId RecipeId;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TSoftObjectPtr<UObject> Icon;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText Description;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	int32 OutputAmount = 1;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	float CraftTime = 0.0f;

	/** CraftTime formatted as HH:MM:SS for direct display binding. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText CraftTimeText;

	/** Every required slot is satisfied by the station input view; optional slots are ignored. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bCanCraft = false;

	/** Crafts the station input view (server input + preview) can fund, capped by the struct model. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	int32 CraftableFromInput = 0;

	/** Start rarity of the main output: the recipe's GuaranteedRarity, else the craft rules' DefaultRarity. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FGameplayTag OutputRarity;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TArray<TObjectPtr<UArcRecipeIngredientViewModel>> Ingredients;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TArray<TObjectPtr<UArcRecipeIngredientViewModel>> RequiredIngredients;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TArray<TObjectPtr<UArcRecipeIngredientViewModel>> OptionalIngredients;

	const FArcNamedPrimaryAssetId& GetRecipeId() const { return RecipeId; }
	const FText& GetDisplayName() const { return DisplayName; }
	const TSoftObjectPtr<UObject>& GetIcon() const { return Icon; }
	const FText& GetDescription() const { return Description; }
	int32 GetOutputAmount() const { return OutputAmount; }
	float GetCraftTime() const { return CraftTime; }
	const FText& GetCraftTimeText() const { return CraftTimeText; }
	bool GetbCanCraft() const { return bCanCraft; }
	int32 GetCraftableFromInput() const { return CraftableFromInput; }
	const FGameplayTag& GetOutputRarity() const { return OutputRarity; }
	const TArray<TObjectPtr<UArcRecipeIngredientViewModel>>& GetIngredients() const { return Ingredients; }
	const TArray<TObjectPtr<UArcRecipeIngredientViewModel>>& GetRequiredIngredients() const { return RequiredIngredients; }
	const TArray<TObjectPtr<UArcRecipeIngredientViewModel>>& GetOptionalIngredients() const { return OptionalIngredients; }

	/**
	 * Returns true if the given item satisfies at least one ingredient slot based on pure VM
	 * fields (no Mass access). MinRarityStep is skipped for both slot kinds — UArcItemViewModel exposes no
	 * rarity step. This is a UI early-out guard only.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc Crafting")
	bool DoesItemSatisfyAnySlot(const UArcItemViewModel* Item) const;

private:
	void SetIngredients(
		TArray<TObjectPtr<UArcRecipeIngredientViewModel>>&& NewIngredients,
		TArray<TObjectPtr<UArcRecipeIngredientViewModel>>&& NewRequired,
		TArray<TObjectPtr<UArcRecipeIngredientViewModel>>&& NewOptional);

public:

	/**
	 * Re-reads the recipe record against the inventory rows left after the preview and the station input view
	 * (ServerInput + PreviewInput). CraftableFromInput stops at MaxCraftable. Registry and StatReg may be null.
	 */
	void RefreshFrom(
		const FPrimaryAssetId& InRecipeId,
		const FArcRecipeDefinitionData& Recipe,
		TConstArrayView<FArcCraftStoreItemRow> InventoryItems,
		TConstArrayView<FArcItemSpec> ServerInput,
		TConstArrayView<FArcCraftStoreItemRow> PreviewInput,
		const UArcTagDisplayRegistry* Registry,
		const UArcStatDisplayRegistry* StatReg,
		int32 MaxCraftable);
};
