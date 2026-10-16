// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "UObject/PrimaryAssetId.h"
#include "ViewModels/ArcBuildingIngredientViewModel.h"

#include "ArcBuildingViewModel.generated.h"

struct FArcMassItemStoreFragment;
class UArcBuildingDefinition;

UCLASS(BlueprintType)
class ARCUIMODELS_API UArcBuildingViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText Description;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TSoftObjectPtr<UObject> Icon;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FGameplayTagContainer BuildingTags;

	/** Category leaf derived from BuildingTags, for the build-menu category label. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText CategoryText;

	/** Grid-cell footprint (X × Y) the building occupies, derived from the definition. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FIntPoint FootprintSize = FIntPoint(1, 1);

	/** Pre-formatted "{X} × {Y}" footprint text for direct display binding. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText FootprintText;

	/** Building health display text (placeholder until a real health source is bound). */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	FText HealthText;

	/** True when every ingredient slot is satisfied (trivially true with no ingredients). */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bCanAfford = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	bool bIsSelected = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
	TArray<TObjectPtr<UArcBuildingIngredientViewModel>> Ingredients;

	const FText& GetDisplayName() const { return DisplayName; }
	const FText& GetDescription() const { return Description; }
	const TSoftObjectPtr<UObject>& GetIcon() const { return Icon; }
	const FGameplayTagContainer& GetBuildingTags() const { return BuildingTags; }
	const FText& GetCategoryText() const { return CategoryText; }
	FIntPoint GetFootprintSize() const { return FootprintSize; }
	const FText& GetFootprintText() const { return FootprintText; }
	const FText& GetHealthText() const { return HealthText; }
	bool GetbCanAfford() const { return bCanAfford; }
	bool GetbIsSelected() const { return bIsSelected; }
	const TArray<TObjectPtr<UArcBuildingIngredientViewModel>>& GetIngredients() const { return Ingredients; }

	/** Non-const pointer — UArcBuilderSubsystem::StartPlacement takes non-const. */
	UArcBuildingDefinition* GetDefinition() const { return Definition; }

	/** Primary asset id cached at the last RefreshFrom; recycle key for the struct model. */
	const FPrimaryAssetId& GetBuildingAssetId() const { return BuildingAssetId; }

	/** Selection highlight, driven by UArcBuilderViewModel::SelectBuilding. */
	void SetbIsSelected(bool bNewIsSelected);

	/**
	 * Re-reads the building definition and player inventory snapshot, recycling the
	 * per-ingredient child VMs by slot index. bCanAfford = AND of ingredient bSatisfied.
	 */
	void RefreshFrom(UArcBuildingDefinition* Def, const FArcMassItemStoreFragment& Inventory);

private:
	void SetIngredients(TArray<TObjectPtr<UArcBuildingIngredientViewModel>>&& NewIngredients);
	void SetCategoryText(const FText& NewCategoryText);
	void SetFootprintSize(FIntPoint NewFootprintSize);
	void SetFootprintText(const FText& NewFootprintText);
	void SetHealthText(const FText& NewHealthText);

	UPROPERTY(Transient)
	TObjectPtr<UArcBuildingDefinition> Definition;

	FPrimaryAssetId BuildingAssetId;
};
